// exception.cc
//      Entry point into the Nachos kernel from user programs.
//      There are two kinds of things that can cause control to
//      transfer back to here from user code:
//
//      syscall -- The user code explicitly requests to call a procedure
//      in the Nachos kernel.  Right now, the only function we support is
//      "Halt".
//
//      exceptions -- The user code does something that the CPU can't handle.
//      For instance, accessing memory that doesn't exist, arithmetic errors,
//      etc.
//
//      Interrupts (which can also cause control to transfer from user
//      code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "addrspace.h"
#include "copyright.h"
#include "migrate.h"
#include "synchconsole.h"
#include "syscall.h"
#include "system.h"
#include "userthread.h"

extern bool FTPClientAction(int servAddr, char readwrite, char *fileName);
extern void startFTPserver();

//----------------------------------------------------------------------
// UpdatePC : Increments the Program Counter register in order to resume
// the user program immediately after the "syscall" instruction.
//----------------------------------------------------------------------

static void UpdatePC()
{
    int pc = machine->ReadRegister(PCReg);
    machine->WriteRegister(PrevPCReg, pc);
    pc = machine->ReadRegister(NextPCReg);
    machine->WriteRegister(PCReg, pc);
    pc += 4;
    machine->WriteRegister(NextPCReg, pc);
}

//----------------------------------------------------------------------
//  copyStringFromMachine
//      Copy a string from the MIPS mode to the Linux mode.
//
//      "from" the starting address of the string
//      "to" the final string that we return
//      "size" the size we want to copy from the string 'from'
/*//----------------------------------------------------------------------*/

void copyStringFromMachine(int from, char *to, unsigned size)
{
    unsigned i = 0;
    while(i < size)
    {
        // Read the i-th character of the string 'from'
        // and write it to the index i of the final string 'to'
        machine->ReadMem(from + i, 1, (int *)(to + i));
        if(to[i] == '\0')
        {
            break;
        }
        i++;
    }

    // Write the character '\0' to the end of the string
    to[i] = '\0';
}

//----------------------------------------------------------------------
//  synchThreadsMainExit
//      Synchronize the termination of the main thread with the exit of the
//      created threads.
//----------------------------------------------------------------------

void synchThreadsMainExit()
{
    threadsLock->Acquire();
    AddrSpace *space = currentThread->space;
    pidMap->Clear(space->pid);
    space->processJoinCond->Broadcast(threadsLock);
    while(currentThread->space->nThreads != 0)
    {
        printf("Main go wait for the others\n");
        currentThread->space->nThreadsCond->Wait(threadsLock);
    }
    threadsLock->Release();
}

//----------------------------------------------------------------------
//  exitCode
//      Print an error message corresponding to the exit code.
//      exit_code is equal to 0 if it corresponds to a proper exit
//      otherwise it corresponds to an abnormal exit of code 'exit_code'
//----------------------------------------------------------------------

void exitCode(int exit_code)
{
    char exit_code_str[MAX_STRING_SIZE];
    if(exit_code == 0)
    {
        snprintf(exit_code_str, MAX_STRING_SIZE, "\nProper exit of thread %d\n",
                 currentThread->GetThreadId());
        synchconsole->SynchPutString(exit_code_str);
    }
    else
    {
        snprintf(exit_code_str, MAX_STRING_SIZE, "\nAbnormal exit of thread %d, exit code: %d\n",
                 currentThread->GetThreadId(), exit_code);
        synchconsole->SynchPutString(exit_code_str);
    }
}

//----------------------------------------------------------------------
//  endProcess
//      Realize the end of the process.
//      The function release the frames used by the process and end the program.
//----------------------------------------------------------------------

extern void endProcess()
{
    AddrSpace::nUsedAddrSpaceLock->Acquire();
    threadsLock->Acquire();
    if(currentThread->space)
    {
        currentThread->space->ReleaseFrames();
    }
    thread_info_t *t_info = GetThreadInfoFromTid(currentThread->GetThreadId());
    if(AddrSpace::nUsedAddrSpace == 0)
    {
        delete currentThread->space;
        threadsLock->Release();
        AddrSpace::nUsedAddrSpaceLock->Release();
        delete AddrSpace::nUsedAddrSpaceLock;
        interrupt->Halt();
        // Stop (never returns from Halt)
    }
    if(t_info)
    {
        tidMap->Clear(currentThread->GetThreadId());
        t_info->threadCond->Broadcast(threadsLock);
        delete t_info;
    }
    AddrSpace::nUsedAddrSpace--;
    threadsLock->Release();
    delete currentThread->space;
    AddrSpace::nUsedAddrSpaceLock->Release();
    currentThread->Finish();
}

//----------------------------------------------------------------------
// ExceptionHandler
//      Entry point into the Nachos kernel.  Called when a user program
//      is executing, and either does a syscall, or generates an addressing
//      or arithmetic exception.
//
//      For system calls, the following is the calling convention:
//
//      system call code -- r2
//              arg1 -- r4
//              arg2 -- r5
//              arg3 -- r6
//              arg4 -- r7
//
//      The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//      "which" is the kind of exception.  The list of possible exceptions
//      are in machine.h.
//----------------------------------------------------------------------

void ExceptionHandler(ExceptionType which)
{
    char ch;
    int size, net_addr, start_addr, exit_code, value, f, f_wrapper, arg, newThreadId, semAddr, tid,
        semId, fd, userThreadId, pid, shouldStop;
    unsigned int addr;
    int type = machine->ReadRegister(2);
    char put_str[MAX_STRING_SIZE];
    char get_str[MAX_STRING_SIZE];
    bool sent;
    if(which == SyscallException)
    {
        switch(type)
        {
        case SC_Halt:
            DEBUG('a', "Shutdown, initiated by user program with tid %d.\n",
                  currentThread->GetThreadId());
            interrupt->Halt();
            break;
        case SC_Sendprocess:
            UpdatePC();
            net_addr = machine->ReadRegister(4);
            shouldStop = machine->ReadRegister(5);
            sent = SendProcess(net_addr);
            if(!sent)
            {
                machine->WriteRegister(2, -1);
                return; // return instead of break because we already update the PC
            }
            if(!shouldStop)
            {
                machine->WriteRegister(2, 0);
                return; // return instead of break because we already update the PC
            }
            // else the process should be stopped, letting it go to the exit
        case SC_Exit:
        case SC_Threadexit:
            DEBUG('a', "Exit of thread %d, initiated by user program.\n",
                  currentThread->GetThreadId());
            if(currentThread->isMain)
            {
                DEBUG('a', "Thread %d is a main thread\n", currentThread->GetThreadId());
                synchThreadsMainExit();
            }
            else
            {
                DEBUG('a', "Thread %d isn't a main thread\n", currentThread->GetThreadId());
                do_UserThreadExit(); // Never get out of this
                ASSERT(false);
            }
            exit_code = machine->ReadRegister(4);
            exitCode(exit_code);
            endProcess();
            break;
        case SC_Create:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->Create(put_str, 0);
            machine->WriteRegister(2, value);
            break;
        case SC_Remove:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->Remove(put_str);
            machine->WriteRegister(2, value);
            break;
        case SC_Open:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            fd = fileSystem->OpenUser(put_str);
            machine->WriteRegister(2, fd);
            break;
        case SC_Close:
            fd = machine->ReadRegister(4);
            value = fileSystem->CloseUser(fd);
            machine->WriteRegister(2, value);
            break;
        case SC_Write:
            start_addr = machine->ReadRegister(4);
            size = machine->ReadRegister(5);
            fd = machine->ReadRegister(6);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->WriteUser(put_str, size, fd);
            machine->WriteRegister(2, value);
            break;
        case SC_Read:
            start_addr = machine->ReadRegister(4);
            size = machine->ReadRegister(5);
            fd = machine->ReadRegister(6);
            value = fileSystem->ReadUser(get_str, size, fd);
            machine->WriteRegister(2, value);
            size = 0;
            while(get_str[size] != '\0')
            {
                machine->WriteMem(start_addr + size, 1, (int)get_str[size]);
                size++;
            }
            machine->WriteMem(start_addr + size, 1, (int)'\0');
            break;
        case SC_Seek:
            fd = machine->ReadRegister(4);
            value = machine->ReadRegister(5);
            fileSystem->SeekUser(fd, value);
            break;
        case SC_Putchar:
            ch = machine->ReadRegister(4);
            DEBUG('a', "PutChar, put a char %c in stdout.\n", ch);
            synchconsole->SynchPutChar(ch);
            break;
        case SC_Putstring:
            DEBUG('a', "PutString, initiated by user program.\n");
            start_addr = machine->ReadRegister(4);
            size = machine->ReadRegister(5);
            size = size < MAX_STRING_SIZE ? size : MAX_STRING_SIZE - 1;
            copyStringFromMachine(start_addr, put_str, size);
            DEBUG('a', "PutString, put the string %d.\n", put_str);
            synchconsole->SynchPutString(put_str);
            break;
        case SC_Getchar:
            ch = synchconsole->SynchGetChar();
            DEBUG('a', "GetChar, get the char %c.\n", ch);
            machine->WriteRegister(2, (int)ch);
            break;
        case SC_Getstring:
            DEBUG('a', "GetString, initiated by user program.\n");
            start_addr = machine->ReadRegister(4);
            size = machine->ReadRegister(5);
            synchconsole->SynchGetString(get_str, size);
            size = 0;
            while(get_str[size] != '\0')
            {
                machine->WriteMem(start_addr + size, 1, (int)get_str[size]);
                size++;
            }
            machine->WriteMem(start_addr + size, 1, (int)'\0');
            break;
        case SC_Putint:
            DEBUG('a', "PutInt, initiated by user program.\n");
            char putint_str[MAX_STRING_SIZE];
            value = machine->ReadRegister(4);
            snprintf(putint_str, MAX_STRING_SIZE, "%d", value);
            synchconsole->SynchPutString(putint_str);
            break;
        case SC_Getint:
            DEBUG('a', "GetInt, initiated by user program.\n");
            char get_int_str[MAX_STRING_SIZE];
            start_addr = machine->ReadRegister(4);
            synchconsole->SynchGetString(get_int_str, MAX_STRING_SIZE);
            sscanf(get_int_str, "%d", &value);
            machine->WriteMem(start_addr, 4, value);
            break;
        case SC_Threadcreate:
            DEBUG('a', "ThreadCreate, initiated by user program.\n");
            f = machine->ReadRegister(4);
            arg = machine->ReadRegister(5);
            f_wrapper = machine->ReadRegister(6);
            userThreadId = do_UserThreadCreate_wrapper(f_wrapper, f, arg);
            machine->WriteRegister(2, userThreadId);
            break;
        case SC_Threadjoin:
            DEBUG('a', "ThreadJoin, initiated by user program\n");
            userThreadId = machine->ReadRegister(4);
            do_UserThreadJoin(userThreadId);
            break;
        case SC_Processjoin:
            DEBUG('a', "ProcessJoin, initiated by user program\n");
            pid = machine->ReadRegister(4);
            do_ProcessJoin(pid);
            break;
        case SC_Seminit:
            semAddr = machine->ReadRegister(4);
            value = machine->ReadRegister(5);
            semId = do_SemInit(value);
            machine->WriteMem(semAddr, 4, semId);
            break;
        case SC_Sempost:
            semAddr = machine->ReadRegister(4);
            machine->ReadMem(semAddr, 4, &semId);
            do_SemPost(semId);
            break;
        case SC_Semwait:
            semAddr = machine->ReadRegister(4);
            machine->ReadMem(semAddr, 4, &semId);
            do_SemWait(semId);
            break;
        case SC_Semdestroy:
            semAddr = machine->ReadRegister(4);
            machine->ReadMem(semAddr, 4, &semId);
            do_SemDestroy(semId);
            break;
        case SC_Forkexec:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            newThreadId = do_ForkExec(put_str);
            machine->WriteRegister(2, newThreadId);
            break;
        case SC_Sbrk:
            size = machine->ReadRegister(4);
            addr = currentThread->space->do_Sbrk((unsigned int)size);
            machine->WriteRegister(2, addr);
            break;
        case SC_Mkdir:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->CreateDir(put_str);
            machine->WriteRegister(2, value);
            break;
        case SC_Rmdir:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->RemoveDir(put_str);
            machine->WriteRegister(2, value);
            break;
        case SC_Listfiles:
            fileSystem->List();
            break;
        case SC_Changedir:
            start_addr = machine->ReadRegister(4);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            value = fileSystem->ChangeDir(put_str);
            machine->WriteRegister(2, value);
            break;
        case SC_Listenprocess:
            tid = ListenProcess();
            machine->WriteRegister(2, tid);
            break;
        case SC_Startftpserver:
            startFTPserver();
            break;
        case SC_Sendfile:
            net_addr = machine->ReadRegister(4);
            start_addr = machine->ReadRegister(5);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            sent = FTPClientAction(net_addr, 'w', put_str);
            machine->WriteRegister(2, sent);
            break;
        case SC_Receivefile:
            net_addr = machine->ReadRegister(4);
            start_addr = machine->ReadRegister(5);
            copyStringFromMachine(start_addr, put_str, MAX_STRING_SIZE);
            sent = FTPClientAction(net_addr, 'r', put_str);
            machine->WriteRegister(2, sent);
            break;
        default:
            fprintf(stderr, "Unknow Syscall Exception %d\n", type);
            break;
        }
    }
    // LB: Do not forget to increment the pc before returning!
    UpdatePC();
    // End of addition
}
