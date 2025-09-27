#include "userthread.h"
#include "addrspace.h"
#include "bitmap.h"
#include <cstdint>

Semaphore *semFORK = new Semaphore("Fork sem", -1);

extern void StartProcess(char *file);

extern void endProcess();
//----------------------------------------------------------------------
// StartUserThread
//      Initialize the environment of the new thread.
//      Set the PC, NextPC to the function, set the SP to a new address.
//      Set the register 4 to the arg of the function f, for the function f.
//
//      "f" is the struct with the address of the function and the
//      address of the arg of the function
//----------------------------------------------------------------------

static void StartUserThread(int f)
{
    struct thread_arg args = *(struct thread_arg *)f;
    thread_info_t *t_info = GetThreadInfoFromTid(currentThread->GetThreadId());
    DEBUG('u', "Start user thread with id: %d\n", currentThread->GetThreadId());
    int newSP = currentThread->space->GetStackStartAddr(t_info->addrspace_idx);

    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();

    machine->WriteRegister(PCReg, args.f_wrapper);
    machine->WriteRegister(NextPCReg, args.f_wrapper + 4);
    machine->WriteRegister(StackReg, newSP);

    machine->WriteRegister(4, args.f_arg.arg);
    machine->WriteRegister(5, args.f_arg.f);
    machine->Run();
}

//----------------------------------------------------------------------
// do_UserThreadCreate_wrapper
//      Wrapper to create a new user thread.
//
//      "f_wrapper" is the address of the wrapper function to allow thread_exit
//                  at the return of the function.
//      "f" is the address of the function used by the thread
//      "arg" is the arg sent to the thread function (f)
//
// returns:
//      The id of the new created thread, or -1 if there is an error during the creation
//----------------------------------------------------------------------

int do_UserThreadCreate_wrapper(int f_wrapper, int f, int arg)
{
    struct thread_f_arg *f_arg = new thread_f_arg;
    f_arg->f = f;
    f_arg->arg = arg;

    return do_UserThreadCreate(f_wrapper, (int)f_arg);
}

//----------------------------------------------------------------------
// do_UserThreadCreate
//      Create a new user thread.
//
//      "f" is the address of the function used by the thread
//      "arg" is the arg sent to the thread function (f)
//
// returns:
//      The id of the new created thread, or -1 if there is an error during the creation
//----------------------------------------------------------------------

extern int do_UserThreadCreate(int f, int arg)
{
    struct thread_arg *args = new thread_arg;
    args->f_wrapper = f;
    args->f_arg = *(struct thread_f_arg *)arg;

    threadsLock->Acquire();
    Thread *newThread = new Thread("");
    newThread->isMain = false;

    args->bitmapIdx = currentThread->space->AllocateThreadData(newThread->GetThreadId());
    if(args->bitmapIdx == -1 || newThread->GetThreadId() == -1)
    {
        threadsLock->Release();
        return -1;
    }

    // Increment the number of the thread
    currentThread->space->nThreads++;
    newThread->Fork(StartUserThread, (int)args);
    threadsLock->Release();

    // Create a new user thread
    return newThread->GetUserThreadId(); // Return the id of the thread
}

//----------------------------------------------------------------------
//  do_UserThreadExit
//      Exit the thread.
//----------------------------------------------------------------------

extern void do_UserThreadExit()
{
    threadsLock->Acquire();
    int tid = currentThread->GetThreadId();

    thread_info *tinfo = GetThreadInfoFromTid(tid);
    ASSERT(tinfo != NULL);

    // Wakeup the thread waiting using UserThreadJoin
    tinfo->threadCond->Broadcast(threadsLock);
    currentThread->space->DeleteThreadData(tid);
    currentThread->space->nThreads--;

    // If the current thread is the last thread,
    if(currentThread->space->nThreads == 0)
    {
        // Wake up the main
        currentThread->space->nThreadsCond->Signal(threadsLock);
    }
    threadsLock->Release();
    currentThread->Finish();
    // We do not delete his space because it is shared with others threads
}

//----------------------------------------------------------------------
//  do_UserThreadJoin
//      Block the calling thread until the thread terminate
//
//      int threadId: the thread's id to wait .
//----------------------------------------------------------------------

extern void do_UserThreadJoin(int userThreadId)
{
    threadsLock->Acquire();
    thread_info *tinfo;
    DEBUG('u', "Thread %d start join the thread %d\n", currentThread->GetUserThreadId(),
          userThreadId);
    tinfo = currentThread->space->GetThreadInfoFromUserthreadId(userThreadId);
    while(tinfo && tinfo->thread_id < MaxThreads && tidMap->Test(tinfo->thread_id))
    {
        DEBUG('u', "Thread %d go to sleep (ZZZ)\n", currentThread->GetUserThreadId());
        tinfo = currentThread->space->GetThreadInfoFromUserthreadId(userThreadId);
        tinfo->threadCond->Wait(threadsLock);
    }

    DEBUG('u', "Thread %d end join the thread %d\n", currentThread->GetUserThreadId(),
          userThreadId);
    threadsLock->Release();
}

//----------------------------------------------------------------------
//  do_SemInit
//      Initialize a semaphore.
//      int initValue: inital value of the semaphore
//
//      Return the identifier of the new semaphore, -1 if the limit is reached  .
//----------------------------------------------------------------------

extern int do_SemInit(int initValue)
{
    BitMap *semBitmap = currentThread->space->semBitmap;
    Semaphore **semList = currentThread->space->semList;
    int semId = semBitmap->Find();
    if(semId != -1)
    {
        semList[semId] = new Semaphore("user semaphore", initValue);
    }
    return semId;
}

//----------------------------------------------------------------------
//  do_SemPost
//      Increment a semaphore
//      Doesn't do anything if the semaphore is invalid
//      int semId: the id of the semahpore to increment
//
//----------------------------------------------------------------------

extern void do_SemPost(int semId)
{
    BitMap *semBitmap = currentThread->space->semBitmap;
    Semaphore **semList = currentThread->space->semList;
    if(semId < MAX_SEM && semBitmap->Test(semId) && semList[semId] != NULL)
    {
        semList[semId]->V();
    }
    return;
}

//----------------------------------------------------------------------
//  do_SemWait
//      Wait until a semaphore is > 0 and decrement it
//      Doesn't do anything if the semaphore is invalid
//      int semId: the id of the semahpore to call
//
//----------------------------------------------------------------------

extern void do_SemWait(int semId)
{
    BitMap *semBitmap = currentThread->space->semBitmap;
    Semaphore **semList = currentThread->space->semList;
    if(semId < MAX_SEM && semBitmap->Test(semId) && semList[semId] != NULL)
    {
        semList[semId]->P();
    }
    return;
}

//----------------------------------------------------------------------
//  do_SemDestroy
//      Deallocate a semaphore
//      Doesn't do anything if the semaphore is invalid
//      int semId: the id of the semahpore to deallocate
//
//----------------------------------------------------------------------

extern void do_SemDestroy(int semId)
{
    BitMap *semBitmap = currentThread->space->semBitmap;
    Semaphore **semList = currentThread->space->semList;
    if(semId < MAX_SEM && semBitmap->Test(semId) && semList[semId] != NULL)
    {
        delete semList[semId];
        semList[semId] = NULL;
        semBitmap->Clear(semId);
    }
    return;
}

//----------------------------------------------------------------------
//  run_ForkExec
//      Run the process in the new created thread.
//
//      "arg" the arguments corresponding to the program to launch
//----------------------------------------------------------------------

void run_ForkExec(int arg)
{
    char *s = (char *)arg;
    char s_cpy[strlen(s)];
    strcpy(s_cpy, s);
    delete s;
    DEBUG('u', "Thread %d run the file %s\n", currentThread->GetThreadId(), s_cpy);
    StartProcess(s_cpy);
    // Return only if the loading of the programm failed
    endProcess();
}

//----------------------------------------------------------------------
//  do_ForkExec
//      Create a new thread that correspond to the new process to run.
//
//  Return:
//      The id of the thread created that corresponds to the process
//----------------------------------------------------------------------

int do_ForkExec(char *s)
{
    Thread *newThread = new Thread("main of another process");
    char *s_cpy = new char[strlen(s)];
    strcpy(s_cpy, s);

    AddrSpace::nUsedAddrSpaceLock->Acquire();

    if(AddrSpace::nUsedAddrSpace >= MaxProcesses)
    {
        AddrSpace::nUsedAddrSpaceLock->Release();
        return -1;
    }

    // Increment the number of running process
    AddrSpace::nUsedAddrSpace++;
    AddrSpace::nUsedAddrSpaceLock->Release();

    // Realize the allocation for the process
    threadsLock->Acquire();

    int tid = newThread->GetThreadId();
    threadsInfos[tid] = new thread_info_t;
    threadsInfos[tid]->threadCond = new Condition("Thread cond");

    newThread->Fork(run_ForkExec, (int)s_cpy);
    threadsLock->Release();

    semFORK->P();
    if(!newThread->space)
    {
        return -1;
    }
    DEBUG('u', "Thread %d fork to create process %d of the file %s\n", currentThread->GetThreadId(),
          newThread->GetThreadId(), s);
    return newThread->space->pid;
}

extern void do_ProcessJoin(int pid)
{
    threadsLock->Acquire();
    if(pid < 0 || pid > MaxProcesses)
    {
        DEBUG('u', "Thread %d start join the process %d (invalid pid)\n",
              currentThread->GetUserThreadId(), pid);
        threadsLock->Release();
        return;
    }
    DEBUG('u', "Thread %d start join the process %d\n", currentThread->GetUserThreadId(), pid);
    AddrSpace *space = addrspaces[pid];
    while(space && pidMap->Test(pid))
    {
        DEBUG('u', "Thread %d go to sleep (ZZZ)\n", currentThread->GetUserThreadId());
        space->processJoinCond->Wait(threadsLock);
    }

    DEBUG('u', "Thread %d end join the process %d\n", currentThread->GetUserThreadId(), pid);
    threadsLock->Release();
}
