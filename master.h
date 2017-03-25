#ifndef __INC_MASTER_H__
#define __INC_MASTER_H__ 1

#define FD_OUTN 0
#define FD_INN 1

typedef struct iofdnode_struct {
  int fdi[2]; /* To the subprocess */
  int fdo[2]; /* From the subprocess */
  int sessionnum;
  int isopen;
  struct iofdnode_struct *next;
} iofdnode; /* Holds the I/O File Descriptors for each session. */

typedef struct sessionpropsnode_struct {
  int sessionnum;
  int doecho;
  char *nick;
  struct sessionpropsnode_struct *next;
} sesspropsnode; /* Holds the session properties */

iofdnode *newiofdnode(int sessionnum);
sesspropsnode *newspn(int sessionnum);
iofdnode *createiofd(int sessionnum);
sesspropsnode *createspn(int sessionnum);
int destroyiofd(iofdnode *aniofd);
int destroyspn(sesspropnode *aspn);
iofdnode *getsessioniofd(int sessionnum);
sesspropsnode *getsessionprops(int sessionnum);

int openmainfds();
int sendtomain(char *sdata);
int sendtomaster(char *sdata);
int readfrommain(char rdbuff, long buffsize);
int readfrommaster(char rdbuff, long buffsize);
  /* Read Returns: 1 = worked, 0 = Nothing there, -1 = Error, -2 = Bad Buffer */
int masterloop();

/*
 * Main/Master Communication Protocol:
 * ==================================
 * ? = Question
 * . = Confirmation
 * / = Success
 * ~ = Failure (Follow with error message)
 * = = Value
 * (Nothing) = Instruction
 * ! = Forced instruction
 * ?! = Forced question
 * [ = Block begin
 * ] = Block end
 * --------------------
 * AYT = Are You There (?/., No argument)
 * CS = Close Session (I///~, Argument is Session Number)
 * Err = Error Message (I, Argument is Message)
 * MB = Message Broadcast (I///~, Argument is Message)
 * OS = Open Session (I///~, Argument is Session Number)
 * SFDI = Session File Descriptor Input (=, Argument is FD)
 * SFDO = Session File Descriptor Output (=, Argument is FD)
 * SInf = Session Info Block ([/], Argument is Session Number)
 */

#endif
