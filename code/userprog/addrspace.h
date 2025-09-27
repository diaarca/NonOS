// addrspace.h
//      Data structures to keep track of executing user programs
//      (address spaces).
//
//      For now, we don't keep any information about address spaces.
//      The user level CPU state is saved and restored in the thread
//      executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "bitmap.h"
#include "copyright.h"
#include "filesys.h"
#include "synch.h"
#include "translate.h"
#include "machine.h"

#define MAX_SEM 128
#define UserStackSize 2048                 // increase this as necessary!
#define ThreadStackSize ((PageSize) * (2)) // number of pages in the stack of one thread
#define MaxThreadsPerProcess                                                                       \
    (((UserStackSize)-16) / (ThreadStackSize)) // maximum number of threads that can be created per process
#define MaxProcesses NumPhysPages // maximum number of processes that can be created
#define MaxThreads ((MaxThreadsPerProcess) * (MaxProcesses)) // maximum number of threads that can be created in the system

// -----------------------------------------------------
// Structure dedicated to the thread information :
// - the ID of the starting address space
// - address of the beginning of the thread stack
// -----------------------------------------------------
//
typedef struct thread_info
{
    int addrspace_idx;
    int userthread_id;
    int thread_id;
    Condition *threadCond;
} thread_info_t;

thread_info_t *GetThreadInfoFromTid(int tid); // Get the information of a thread from its ID

class AddrSpace
{
  public:
    AddrSpace(unsigned int nPages); // Create an empty address space,
    AddrSpace(OpenFile *executable); // Create an address space,
    // initializing it with the program
    // stored in the file "executable"
    ~AddrSpace(); // De-allocate an address space

    void InitRegisters(); // Initialize user-level CPU registers,
    // before jumping to user code

    void ReleaseFrames(); // Release all the frames used in the address space

    void SaveState();    // Save/restore address space-specific
    void RestoreState(); // info on a context switch
    int GetSize();       // Get the size of the addr space

    void InitializeThreadData();    // Initialize the thread structures
    int AllocateThreadData(int id); // Allocate the structures of a thread
    void DeleteThreadData(int id);  // Deallocate the structures of a thread
    bool CanCreateThread();         // Check if a new thread can be created

    int GetStackStartAddr(int index); // Get the information of a thread from its index in the threadsBitmap
    
    int pid;
    thread_info_t *GetThreadInfoFromUserthreadId(int utid);
    Condition *nThreadsCond;
    BitMap *semBitmap;
    Semaphore **semList;
    int nThreads; // Number of thread that didn't terminate
    static int nUsedAddrSpace; // Number of frames used by the address space
    unsigned int do_Sbrk(unsigned int n); // Try to allocate n new frames
    static Lock *nUsedAddrSpaceLock;
    Condition *processJoinCond;
    Lock *processJoinLock;
    unsigned int numPages;
    thread_info_t **localThreadsInfos;
    BitMap *threadsBitmap; // BitMap used to indicate whether a thread stack is free or not
    unsigned int nextUserThreadid;

  private:
    int stackStartAddrs[(MaxThreadsPerProcess)]; // arstack start addrs
    TranslationEntry *pageTable;                 // Assume linear page table translation
    // for now!
    // address space
    unsigned int brk;
};

#endif // ADDRSPACE_H
