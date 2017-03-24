#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/telnet.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>

#include "main.h"


#define ADDRBUFFSIZE 256


char progver[] = "0.01.00 ALPHA";
int endprog = 0;


void handle_sigchld(int sig)
{
  while (waitpid((pid_t) -1, 0, WNOHANG) > 0) {}
}

void handle_sigint(int sig)
{
  if (endprog) printf("Already shutting down!\n");
  else printf("Shutting down...\n");
  
  endprog = 1;
}

void handle_sigterm(int sig)
{
  printf("Terminating Immediately!\n");
  exit(4);
}

int setnonblocking(int fd)
{
  int flags;
#ifdef O_NONBLOCK
  if ((flags = fcntl(fd, F_GETFL, 0)) == -1) flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
  flags = 1;
  return ioctl(fd, FIOBIO &flags);
#endif
}

int main( int argc, char *argv[])
{
  const char *hostname = NULL;
  const char portname[] = "23";
  struct addrinfo hints, *res = NULL;
  char hname[ADDRBUFFSIZE] = "";
  char ipaddr[ADDRBUFFSIZE] = "";
  char remoteipaddr[ADDRBUFFSIZE] = "";
  int err;
  
  endprog = 0;
  
  err = gethostname(hname, ADDRBUFFSIZE);
  if (!err) printf("Hostname: %s\n",hname);
  else fprintf(stderr, "Hostname Error: %d\n", err);
  
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = 0;
  hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
  err = getaddrinfo(hostname, portname, &hints, &res);
  if (err)
  {
    fprintf(stderr, "Failed to resolve local socket address!  Error %d.\n",err);
    exit(1);
  }
  
  int server_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (server_fd == -1)
  {
    fprintf(stderr, "Failed to create server socket - %s\n",strerror(errno));
    freeaddrinfo(res);
    exit(1);
  }
  int reuseaddr = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1)
  {
    fprintf(stderr, "%s\n", strerror(errno));
    freeaddrinfo(res);
    exit(1);
  }
  
  if (bind(server_fd, res->ai_addr, res->ai_addrlen) == -1)
  {
    fprintf(stderr, "%s\n", strerror(errno));
    freeaddrinfo(res);
    exit(1);
  }
  freeaddrinfo(res);
  if (listen(server_fd, SOMAXCONN))
  {
    fprintf(stderr, "Failed to listen for connections!  Error %d: %s\n", errno, strerror(errno));
    exit(1);
  }
  
  printf("Entering Listen Loop...");
  
  int session_fd = -1;
  struct sockaddr_storage anaddr;
  struct sockaddr_in *ipv4addr;
  socklen_t anaddr_size;
  struct sockaddr_in localaddr;
  socklen_t localaddr_size;
  
  /* Register any signal handlers here */
  struct sigaction sa;
  sa.sa_handler = &handle_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, 0) == -1)
  {
    perror(0);
    fprintf(stderr, "Failed to register SIGCHLD handler!\n");
    close(server_fd);
    exit(1);
  }
  
  if (signal(SIGINT, handle_sigint) == SIG_ERR)
  {
    perror(0);
    fprintf(stderr, "Failed to register SIGINT handler!\n");
    close(server_fd);
    exit(1);
  }
  
  setnonblocking(server_fd);
  
  if (signal(SIGTERM, handle_sigterm) == SIG_ERR)
  {
    perror(0);
    fprintf(stderr, "Failed to register SIGTERM handler!\n");
    close(server_fd);
    exit(1);
  }
  
  while (endprog == 0)
  {
    anaddr_size = sizeof(anaddr);
    localaddr_size = sizeof(localaddr);
    session_fd = accept(server_fd, (struct sockaddr *) &anaddr, &anaddr_size);
    ipv4addr = (struct sockaddr_in *) &anaddr;
    if (session_fd == -1)
    {
      if (errno==EINTR || errno==EWOULDBLOCK) continue;
      fprintf(stderr, "Failed to accept connection!  Error %d: %s\n", errno, strerror(errno));
      close(server_fd);
      exit(2);
    }
    getsockname(session_fd, (struct sockaddr *) &localaddr, &localaddr_size);
    printf("Connected!\n");
    strcpy(remoteipaddr, (char *) inet_ntoa(ipv4addr->sin_addr));
    printf("Remote IP=%s, Port=%ld\n", remoteipaddr, (long) ntohs(ipv4addr->sin_port));
    printf("Local IP=%s, Port=%ld\n", inet_ntoa(localaddr.sin_addr), (long) ntohs(localaddr.sin_port));
    
    printf("Time to split...\n");
    pid_t pid = fork();
    if (pid == -1)
    {
      fprintf(stderr, "Failed to create child process!  Error %d: %s\n", errno, strerror(errno));
      close(server_fd);
      exit(3);
    }
    else if (pid == 0)
    {
      printf("Forked: Child.\n");
      /* Reset any non-child signals to SIG_DFL */
      /* Save ipaddr? */
      close(server_fd);
      /* Do something with session_fd */
      
      printf("Ending session...\n");
      close(session_fd);
      printf("Session Ended!\n");
      
      fclose(stderr);
      fclose(stdin);
      fclose(stdout);
      wait(0);
      _Exit(0);
      exit(0);
      return 0;
    }
    else
    {
      printf("Forked: Parent.\n");
      close(session_fd);
    }
  }
  
  printf("Closing server...\n");
  close(server_fd);
  printf("Waiting for children...\n");
  waitpid(-1,0,0);
  return 0;
}
