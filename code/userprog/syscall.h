/* syscalls.h
 * 	Nachos system call interface.  These are Nachos kernel operations
 * 	that can be invoked from user programs, by trapping to the kernel
 *	via the "syscall" instruction.
 *
 *	This file is included by user programs and by the Nachos kernel.
 *
 * Copyright (c) 1992-1993 The Regents of the University of California.
 * All rights reserved.  See copyright.h for copyright notice and limitation
 * of liability and disclaimer of warranty provisions.
 */

#ifndef SYSCALLS_H
#define SYSCALLS_H
#include "copyright.h"

/* system call codes -- used by the stubs to tell the kernel which system call
 * is being asked for
 */
#define SC_Halt 0
#define SC_Exit 1
#define SC_Exec 2
#define SC_Join 3
#define SC_Create 4
#define SC_Open 5
#define SC_Read 6
#define SC_Write 7
#define SC_Close 8
#define SC_Fork 9
#define SC_Putchar 11
#define SC_Putstring 12
#define SC_Getchar 13
#define SC_Getstring 14
#define SC_Putint 15
#define SC_Getint 16
#define SC_Threadcreate 17
#define SC_Threadexit 18
#define SC_Threadjoin 19
#define SC_Seminit 20
#define SC_Sempost 21
#define SC_Semwait 22
#define SC_Semdestroy 23
#define SC_Forkexec 24
#define SC_Sbrk 25
#define SC_Mkdir 26
#define SC_Rmdir 27
#define SC_Listfiles 28
#define SC_Changedir 29
#define SC_Remove 30
#define SC_Seek 31
#define SC_Sendprocess 32 
#define SC_Listenprocess 33
#define SC_Processjoin 34
#define SC_Sendfile 35
#define SC_Receivefile 36 
#define SC_Startftpserver 37 

#ifdef IN_USER_MODE

// LB: This part is read only on compiling the test/*.c files.
// It is *not* read on compiling test/start.S

/* The system call interface.  These are the operations the Nachos
 * kernel needs to support, to be able to run user programs.
 *
 * Each of these is invoked by a user program by simply calling the
 * procedure; an assembly language stub stuffs the system call code
 * into a register, and traps to the kernel.  The kernel procedures
 * are then invoked in the Nachos kernel, after appropriate error checking,
 * from the system call entry point in exception.cc.
 */

typedef int sem_t;
typedef int pid_t;
typedef int tid_t;

/* Stop Nachos, and print out performance stats */
void Halt() __attribute__((noreturn));

/* Address space control operations: Exit, Exec, and Join */

/* This user program is done (status = 0 means exited normally). */
void Exit(int status) __attribute__((noreturn));


/* File system operations: Create, Open, Read, Write, Close
 * These functions are patterned after UNIX -- files represent
 * both files *and* hardware I/O devices.
 *
 * If this assignment is done before doing the file system assignment,
 * note that the Nachos file system has a stub implementation, which
 * will work for the purposes of testing out these routines.
 */

/* when an address space starts up, it has two open files, representing
 * keyboard input and display output (in UNIX terms, stdin and stdout).
 * Read and Write can be used directly on these, without first opening
 * the console device.
 */

/* Create a Nachos file, with "name" */
int Create(char *name);

int Remove(char *name);
    
/* Open the Nachos file "name", and return an "OpenFileId" that can
 * be used to read and write to the file.
 */
int Open(char *name);

/* Write "size" bytes from "buffer" to the open file. */
int Write(char *buffer, int size, int id);

/* Read "size" bytes from the open file into "buffer".
 * Return the number of bytes actually read -- if the open file isn't
 * long enough, or if it is an I/O device, and there aren't enough
 * characters to read, return whatever is available (for I/O devices,
 * you should always wait until you can return at least one character).
 */
int Read(char *buffer, int size, int id);

/* Close the file, we're done reading and writing to it. */
int Close(int id);

/* User-level thread operations: Fork and Yield.  To allow multiple
 * threads to run within a user program.
 */

/* Fork a thread to run a procedure ("func") in the *same* address space
 * as the current thread.
 */
void Fork(void (*func)());

/* Write a char in stdout
 */
void PutChar(char c);

/* Put a string in stdout
 * char *c: pointer to the string, size : number of char of the string
 */
void PutString(char *c, int size);

int GetChar();

void GetString(char *s, int n);

void PutInt(int n);

void GetInt(int *n);

tid_t ThreadCreate(void f(void *arg), void *arg);

void ThreadExit();

void ThreadJoin(tid_t threadId);

void ProcessJoin(pid_t processId);

void SemInit(sem_t *sem, unsigned int v);

void SemPost(sem_t *sem);

void SemWait(sem_t *sem);

void SemDestroy(sem_t *sem);

pid_t ForkExec(char *s);

void *Sbrk(unsigned int n);

int Mkdir(char *s);

int Rmdir(char *s);

char *Listfiles();

int Changedir(char *s);

void Seek(int fd, int offset);

// should only be called by main
// if stopAfter != 0 : stop the calling thread after sending the process 
// return : -1 if the process hasn't been send,
//           0 for the sender 
//           1 for the receiver  
// should be synchronized with other threads syscall 
int SendProcess(int farAddr, int stopAfter);

tid_t ListenProcess();

int SendFile(int farAddr, char *filename);

int ReceiveFile(int farAddr, char *filename);

void StartFTPServer();


#endif // IN_USER_MODE

#endif /* SYSCALL_H */
