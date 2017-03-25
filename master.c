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

#include "master.h"

iofdnode *iofdroot = NULL, *iofdptr = NULL;
int mainfdi[2], mainfdo[2]; /* FDs for communicating with the main process */

sesspropsnode *sproot = NULL, *spptr = NULL;

int inloop = 0;


iofdnode *newiofdnode(int sessionnum)
{
  iofdnode *aniofdnode = (iofdnode *) malloc(sizeof(iofdnode));
  if (aniofdnode == NULL) return NULL;
  aniofdnode->sessionnum = sessionnum;
  aniofdnode->isopen = 0;
  aniofdnode->next = NULL;
  return aniofdnode;
}

sesspropsnode *newspn(int sessionnum)
{
  sesspropsnode *aspn = (sesspropsnode *) malloc( sizeof(sesspropsnode));
  if (aspn == NULL) return NULL;
  aspn->sessionnum = sessionnum;
  aspn->doecho = 0;
  aspn->nick = NULL;
  aspn->next = NULL;
}

iofdnode *createiofd(int sessionnum)
{
  if (iofdroot == NULL)
  {
    iofdroot = newiofdnode(sessionnum);
    if (iofdroot == NULL) return NULL;
    iofdptr = iofdroot;
  }
  else
  {
    for (iofdptr = iofdroot; iofdptr->next != NULL; iofdptr = iofdptr->next) {}
    iofdptr->next = newiofdnode(sessionum);
    if (iofdptr->next == NULL) return NULL;
    iofdptr = iofdptr->next;
  }
  return iofdptr;
}

sesspropsnode *createspn(int sessionnum)
{
  if (sproot == NULL)
  {
    sproot = newspn(sessionnum);
    if (sproot == NULL) return NULL;
    spptr = sproot;
  }
  else
  {
    for (spptr = sproot; spptr->next != NULL; spptr = spptr->next) {}
    spptr->next = newspn(sessionnum);
    if (spptr->next == NULL) return NULL;
    spptr = spptr->next;
  }
  return spptr;
}

int destroyiofd(iofdnode *aniofd)
{
  iofdnode *tiofd = NULL;
  if (aniofd == NULL) return 0;
  for (iofdptr = iofdroot; iofdptr != NULL; iofdptr = iofdptr->next)
  {
    if (iofdptr == aniofd) break;
    tiofd = iofdptr;
  }
  if (iofdptr == NULL) return 0;
  if (iofdptr->isopen != 0) return 0;
  if (tiofd == NULL) iofdroot = iofdptr->next;
  else tiofd->next = iofdptr->next;
  free(iofdptr);
  iofdptr = NULL;
  return 1;
}

int destroyspn(sesspropnode *aspn)
{
  sesspropnode *tspn = NULL;
  if (aspn == NULL) return 0;
  for (spptr = sproot; spptr != NULL; spptr = spptr->next)
  {
    if (spptr == aspn) break;
    tspn = spptr;
  }
  if (spptr == NULL) return 0;
  if (tspn == NULL) sproot = spptr->next;
  else tspn->next = spptr->next;
  
  if (spptr->nick != NULL) free(spptr->nick);
  free(spptr);
  spptr = NULL;
  return 1;
}

iofdnode *getsessioniofd(int sessionnum)
{
  for (iofdptr = iofdroot; iofdptr != NULL; iofdptr = iofdptr->next)
  {
    if (iofdptr->sessionnum == sessionnum) return iofdptr;
  }
  return NULL;
}

sesspropsnode *getsessionprops(int sessionnum)
{
  for (spptr = sproot; spptr != NULL; spptr = spptr->next)
  {
    if (spptr->sessionnum == sessionnum) return spptr;
  }
  return NULL;
}

int openmainfds()
{
  pipe(mainfdo);
  pipe(mainfdi);
  setnonblocking(mainfdo[FD_OUTN]);
  setnonblocking(mainfdi[FD_OUTN]);
}

int sendtomain(char *sdata)
{
  long wret = 0;
  if (sdata == NULL) return 0;
  wret = write(mainfdi[FD_INN], sdata, strlen(sdata));
  if (wret == strlen(sdata)) return 1;
  return 0;
}

int sendtomaster(char *sdata)
{
  long wret = 0;
  if (sdata == NULL) return 0;
  wret = write(mainfdo[FD_INN], sdata, strlen(sdata));
  if (wret == strlen(sdata)) return 1;
  return 0;
}

int readfrommain(char rdbuff, long buffsize)
{
  /* Returns: 1 = worked, 0 = Nothing there, -1 = Error, -2 = Bad Buffer */
  char rddat[2] = " ";
  long j = 0;
  int rdo = -1;
  if (buffsize <2) return -2;
/*  rdo = read(mainfdo[FD_OUTN], rdbuff, buffsize-1); */
  rdbuff[buffsize-1] = 0;
  rdbuff[0] = 0;
  do
  {
    rdo = read(mainfdo[FD_OUTN], rddat, 1);
    rdbuff[j] = rddat[0];
    j++;
    if (j>=buffsize) break;
    rdbuff[j] = 0;
    if (rddat[0]=='\n') break;
  } while (rdo>0);
  if (rdo == -1)
  {
    if (errno == EAGAIN) return 0;
    return -1;
  }
  else if (rdo>0)
  {
    return 1;
  }
  else return 0;
}

int readfrommaster(char rdbuff, long buffsize)
{
  /* Returns: 1 = worked, 0 = Nothing there, -1 = Error, -2 = Bad Buffer */
  int rdo = -1;
  char rddat[2] = " ";
  long j = 0;
  if (buffsize <2) return -2;
  /*rdo = read(mainfdi[FD_OUTN], rdbuff, buffsize-1);*/
  rdbuff[buffsize-1] = 0;
  rdbuff[0] = 0;
  do
  {
    rdo = read(mainfdi[FD_OUTN], rddat, 1);
    rdbuff[j] = rddat[0];
    j++;
    if (j>=buffsize) break;
    rdbuff[j] = 0;
    if (rddat[0]=='\n') break;
  } while (rdo>0);
  if (rdo == -1)
  {
    if (errno == EAGAIN) return 0;
    return -1;
  }
  else if (rdo>0)
  {
    return 1;
  }
  else return 0;
}

int readfromsession(int sessionnum, char rdbuff, long buffsize)
{
  /* Only master fork should call this! */
  /* Returns: 1 = worked, 0 = Nothing there, -1 = Error, -2 = Bad Buffer, -3 = Invalid Sessionnum */
  char rddat[2] = " ";
  long j = 0;
  int rdo = -1;
  iofdnode *siofd = getsessioniofd(sessionnum);
  if (siofd == NULL) return -3;
  if (buffsize <2) return -2;
/*  rdo = read(mainfdo[FD_OUTN], rdbuff, buffsize-1); */
  rdbuff[buffsize-1] = 0;
  rdbuff[0] = 0;
  do
  {
    rdo = read(siofd->fdo[FD_OUTN], rddat, 1);
    rdbuff[j] = rddat[0];
    j++;
    if (j>=buffsize) break;
    rdbuff[j] = 0;
    if (rddat[0]=='\n') break;
  } while (rdo>0);
  if (rdo == -1)
  {
    if (errno == EAGAIN) return 0;
    return -1;
  }
  else if (rdo>0)
  {
    return 1;
  }
  else return 0;
}

void handle_master_sigint( int sig )
{
  if (inloop) printf("Closing master...\n");
  else printf("Master is closing!\n");
  inloop = 0;
}

int masterloop()
{
  /* Only the Master fork should open this! */
  char tbuf[1024] = "";
  int rdret = 0;
  
  if (signal(SIGINT, handle_master_sigint) == SIG_ERR)
  {
    perror(0);
    return 0;
  }
  inloop = 1;
  while (inloop == 1)
  {
    rdret = readfrommain(tbuf,1024);
    if (rdret == -1)
    {
      perror(0);
      inloop = 0;
      break;
    }
    else if (rdret > 0)
    {
      /* Do something with info */
      long snum = 0;
      iofdnode *siofd;
      sesspropsnode *spn;
      if (memcmp(tbuf,"OS ",3*sizeof(char))==0)
      {
        /* Opened session */
        snum = atol(tbuf+(3*sizeof(char)));
        siofd = createiofd((int) snum);
        if (siofd != NULL)
        {
          pipe(siofd->fdi);
          pipe(siofd->fdo);
          sprintf(tbuf, "OS/ %ld\nSInf[ %ld\nSFDI= %d\nSFDO= %d\nSInf] %ld\n",snum,snum,siofd->fdo[FD_INN], siofd->fdi[FD_OUTN],snum);
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
        else
        {
          sprintf(tbuf, "OS~ %ld\nErr Out of Memory!\n",snum);
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
      }
      else if (memcmp(tbuf,"OSP ",4*sizeof(char))==0)
      {
        /* Opened session properties */
        snum = atol(tbuf+(4*sizeof(char)));
        spn = createspn((int) snum);
        if (spn != NULL)
        {
          sprintf(tbuf, "OSP/ %ld\n",snum);
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
        else
        {
          sprintf(tbuf, "OSP~ %ld\nErr Out of Memory!\n",snum);
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
      }
      else if (memcmp(tbuf,"SFrk ",5*sizeof(char))==0)
      {
        /* Session has Forked */
        snum = atol(tbuf+(5*sizeof(char)));
        siofd = getsessioniofd(snum);
        if (siofd != NULL)
        {
          close(siofd->fdo[FD_INN]);
          close(siofd->fdi[FD_OUTN]);
          sprintf(tbuf,"SFrk/ %ld\n", snum); /* SFrk Success */
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
        else
        {
          sprintf(tbuf,"SFrk~ %ld\nErr Could not find session!\n", snum); /* SFrk Failure */
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
          
        }
      }
      else if (memcmp(tbuf,"CS ",3*sizeof(char))==0)
      {
        /* Closed Session */
        snum = atol(tbuf+(3*sizeof(char)));
        spn = getsessionprops(snum);
        if (spn != NULL)
        {
          if (!destroyspn(spn))
          {
            sprintf(tbuf,"SInf[ %ld\nErr Could not close session properties!\nSInf] %ld\n", snum,snum); /* CS Error */
            rdret = sendtomain(tbuf);
            if (rdret == 0)
            {
              /* ? */
            }
          }
        }
        else
        {
          sprintf(tbuf,"SInf[ %ld\nWar Could not find session properties.\nSInf] %ld\n", snum,snum); /* CS Warning */
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
        siofd = getsessioniofd(snum);
        if (siofd != NULL)
        {
          if (siofd->isopen != 0)
          {
            close(siofd->fdi[FD_INN]);
            close(siofd->fdo[FD_OUTN]);
            siofd->isopen = 0;
          }
          if (destroyiofd(siofd))
          {
            sprintf(tbuf,"CS/ %ld\n", snum); /* CS Success */
            rdret = sendtomain(tbuf);
            if (rdret == 0)
            {
              /* ? */
            }
          }
          else
          {
            sprintf(tbuf,"CS~ %ld\nErr Could not destroy descriptor.\n", snum); /* CS Failure */
            rdret = sendtomain(tbuf);
            if (rdret == 0)
            {
              /* ? */
            }
          }
        }
        else
        {
          /* No Session */
          sprintf(tbuf,"CS~ %ld\nErr Could not find session!\n", snum); /* CS Failure */
          rdret = sendtomain(tbuf);
          if (rdret == 0)
          {
            /* ? */
          }
        }
      }
      else if (memcmp(tbuf,"MB ",3*sizeof(char))==0)
      {
        /* Message Broadcast! */
      }
      else if (memcmp(tbuf,"AYT? ",5*sizeof(char))==0 || memcmp(tbuf,"AYT?\n",5*sizeof(char))==0)
      {
        /* Are You There? */
        rdret = sendtomain("AYT.\n");
        if (rdret == 0)
        {
          /* ? */
        }
      }
      
    }
    /* Read from other fds! */
    
  }
  
  if (signal(SIGINT, SIG_DFL) == SIG_ERR)
  {
    perror(0);
    return 0;
  }
  return 1;
}
