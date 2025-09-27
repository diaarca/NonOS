/* Start.s 
 *	Assembly language assist for user programs running on top of Nachos.
 *
 *	Since we don't want to pull in the entire C library, we define
 *	what we need for a user program here, namely Start and the system
 *	calls.
 */

#include "syscall.h"

        .text   
        .align  2

/* -------------------------------------------------------------
 * __start
 *	Initialize running a C program, by calling "main". 
 *
 * 	NOTE: This has to be first, so that it gets loaded at location 0.
 *	The Nachos kernel always starts a program by jumping to location 0.
 * -------------------------------------------------------------
 */

	.globl __start
	.ent	__start
__start:
	jal	main
	move $4, $2		
	jal	Exit	 /* if we return from main, exit(0) */
	.end __start

/* -------------------------------------------------------------
 * System call stubs:
 *	Assembly language assist to make system calls to the Nachos kernel.
 *	There is one stub per system call, that places the code for the
 *	system call into register r2, and leaves the arguments to the
 *	system call alone (in other words, arg1 is in r4, arg2 is 
 *	in r5, arg3 is in r6, arg4 is in r7)
 *
 * 	The return value is in r2. This follows the standard C calling
 * 	convention on the MIPS.
 * -------------------------------------------------------------
 */

	.globl Halt
	.ent	Halt
Halt:
	addiu $2,$0,SC_Halt
	syscall
	j	$31
	.end Halt

	.globl Exit
	.ent	Exit
Exit:
	addiu $2,$0,SC_Exit
	syscall
	j	$31
	.end Exit

	.globl Exec
	.ent	Exec
Exec:
	addiu $2,$0,SC_Exec
	syscall
	j	$31
	.end Exec

	.globl Join
	.ent	Join
Join:
	addiu $2,$0,SC_Join
	syscall
	j	$31
	.end Join

	.globl Create
	.ent	Create
Create:
	addiu $2,$0,SC_Create
	syscall
	j	$31
	.end Create

	.globl Open
	.ent	Open
Open:
	addiu $2,$0,SC_Open
	syscall
	j	$31
	.end Open

	.globl Read
	.ent	Read
Read:
	addiu $2,$0,SC_Read
	syscall
	j	$31
	.end Read

	.globl Write
	.ent	Write
Write:
	addiu $2,$0,SC_Write
	syscall
	j	$31
	.end Write

	.globl Close
	.ent	Close
Close:
	addiu $2,$0,SC_Close
	syscall
	j	$31
	.end Close

	.globl Seek
	.ent	Seek
Seek:
	addiu $2,$0,SC_Seek
	syscall
	j	$31
	.end Seek

	.globl Fork
	.ent	Fork
Fork:
	addiu $2,$0,SC_Fork
	syscall
	j	$31
	.end Fork

	.globl PutChar
	.ent	PutChar
PutChar:
	addiu $2,$0,SC_Putchar
	syscall
	j	$31
	.end PutChar
	
	.globl PutString
	.ent	PutString
PutString:
	addiu $2,$0,SC_Putstring
	syscall
	j	$31
	.end PutString

	.globl GetChar
	.ent	GetChar
GetChar:
	addiu $2,$0,SC_Getchar
	syscall
	j	$31
	.end GetChar

	.globl GetString
	.ent	GetString
GetString:
	addiu $2,$0,SC_Getstring
	syscall
	j	$31
	.end GetString

	.globl PutInt
	.ent	PutInt
PutInt:
	addiu $2,$0,SC_Putint
	syscall
	j	$31
	.end PutInt

	.globl GetInt
	.ent	GetInt
GetInt:
	addiu $2,$0,SC_Getint
	syscall
	j	$31
	.end GetInt

    .globl ThreadWrapper
    .ent ThreadWrapper
ThreadWrapper:
    // r4 has the arg of the function of the thread
    // r5 has the function of the thread
    jal $5
	jal	ThreadExit	 /* if we return from a thread, threadExit */
    .end ThreadWrapper

	.globl ThreadCreate
	.ent	ThreadCreate
ThreadCreate:
	addiu $2,$0,SC_Threadcreate
    addiu $6, $0, ThreadWrapper // Add the address of the wrapper
	syscall
    j $31
	.end ThreadCreate

	.globl ThreadExit
	.ent	ThreadExit
ThreadExit:
	addiu $2,$0,SC_Threadexit
	syscall
	j	$31
	.end ThreadExit

	.globl ThreadJoin
	.ent	ThreadJoin
ThreadJoin:
	addiu $2,$0,SC_Threadjoin
	syscall
	j	$31
	.end ThreadJoin

	.globl SemInit
	.ent SemInit	
SemInit:
	addiu $2,$0,SC_Seminit
	syscall
	j	$31
	.end SemInit 

	.globl SemPost
	.ent	SemPost
SemPost:
	addiu $2,$0,SC_Sempost
	syscall
	j	$31
	.end SemPost

	.globl SemWait
	.ent	SemWait
SemWait:
	addiu $2,$0,SC_Semwait
	syscall
	j	$31
	.end SemWait

	.globl SemDestroy
	.ent	SemDestroy
SemDestroy:
	addiu $2,$0,SC_Semdestroy
	syscall
	j	$31
	.end SemDestroy
  
  .globl ForkExec
  .ent ForkExec
ForkExec:
  addiu $2, $0, SC_Forkexec
  syscall
  j $31 
  .end ForkExec

  .globl Sbrk
  .ent Sbrk
Sbrk:
  addiu $2, $0, SC_Sbrk
  syscall
  j $31
  .end Sbrk

  .globl Mkdir
  .ent Mkdir
Mkdir:
  addiu $2, $0, SC_Mkdir
  syscall
  j $31
  .end Mkdir

  .globl Rmdir
  .ent Rmdir
Rmdir:
  addiu $2, $0, SC_Rmdir
  syscall
  j $31
  .end Rmdir

  .globl Listfiles
  .ent Listfiles
Listfiles:
  addiu $2, $0, SC_Listfiles
  syscall
  j $31
  .end Listfiles

  .globl Changedir
  .ent Changedir
Changedir:
  addiu $2, $0, SC_Changedir
  syscall
  j $31
  .end Changedir

  .globl Remove
  .ent Remove
Remove:
  addiu $2, $0, SC_Remove
  syscall
  j $31
  .end Remove


  .globl SendProcess
  .ent SendProcess
SendProcess:
  addiu $2, $0, SC_Sendprocess
  syscall
  j $31
  .end SendProcess

  .globl ListenProcess
  .ent ListenProcess
ListenProcess:
  addiu $2, $0, SC_Listenprocess
  syscall
  j $31
  .end ListenProcess

	.globl ProcessJoin
	.ent	ProcessJoin
ProcessJoin:
	addiu $2,$0,SC_Processjoin
	syscall
	j	$31
	.end ProcessJoin
	
	.globl SendFile
	.ent	SendFile
SendFile:
	addiu $2,$0,SC_Sendfile
	syscall
	j	$31
	.end SendFile

	.globl ReceiveFile
	.ent	ReceiveFile
ReceiveFile:
	addiu $2,$0,SC_Receivefile
	syscall
	j	$31
	.end ReceiveFile

	.globl StartFTPServer
	.ent	StartFTPServer
StartFTPServer:
	addiu $2,$0,SC_Startftpserver
	syscall
	j	$31
	.end StartFTPServer

/* dummy function to keep gcc happy */
        .globl  __main
        .ent    __main
__main:
        j       $31
        .end    __main

