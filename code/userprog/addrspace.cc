// addrspace.cc
//      Routines to manage address spaces (executing user programs).
//
//      In order to run a user program, you must:
//
//      1. link with the -N -T 0 option
//      2. run coff2noff to convert the object file to Nachos format
//              (Nachos object code format is essentially just a simpler
//              version of the UNIX executable object code format)
//      3. load the NOFF file into the Nachos file system
//              (if you haven't implemented the file system yet, you
//              don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "addrspace.h"
#include "copyright.h"
#include "frameprovider.h"
#include "noff.h"
#include "system.h"

int AddrSpace::nUsedAddrSpace = 0;
Lock *AddrSpace::nUsedAddrSpaceLock = new Lock("n used addr space lock");

Semaphore *semPID = new Semaphore("Pid sem", 1);
int GetNewPid()
{
    semPID->P();
    static int lastId = 0;
    int pid = pidMap->FindStart(lastId);
    lastId = pid + 1;
    semPID->V();
    return pid;
}

//----------------------------------------------------------------------
// AddrSpace::ReleaseFrames
//      Release all the frames used by the address space
//----------------------------------------------------------------------

void AddrSpace::ReleaseFrames()
{
    if(!pageTable)
    {
        return;
    }
    FrameProvider *fp = FrameProvider::GetInstance();

    for(unsigned int i = 0; i < numPages; i++)
    {
        fp->ReleaseFrame(pageTable[i].physicalPage);
    }
}

//----------------------------------------------------------------------
// GetThreadInfoFromTid
//      Get the thread info for a thread
//----------------------------------------------------------------------

thread_info_t *GetThreadInfoFromTid(int tid)
{
    if(tid > 0 && tid <= MaxThreads && tidMap->Test(tid))
    {
        return threadsInfos[tid];
    }
    return NULL;
}

//----------------------------------------------------------------------
// ReadAtVirtual
//      We read at the virtual address n bytes (numBytes) at a position (position)
//      in the VA. And we write it into the memory.
//
//      "executable" -- the OpenFile class for ReadAt
//	    "virtualaddr" -- the virtual address containing the data to be written into the memory
//	    "numBytes" -- the number of bytes to transfer
//	    "position" -- the offset within of the VA of the first byte to be read/written
//      "pageTable" -- the page table used for the translation
//      "numPages" -- the number of page in our page table
//----------------------------------------------------------------------

static void ReadAtVirtual(OpenFile *executable,
                          int virtualaddr,
                          int numBytes,
                          int position,
                          TranslationEntry *pageTable,
                          unsigned numPages)
{
    char *into = new char[numBytes];

    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;

    executable->ReadAt(into, numBytes, position);

    for(int i = 0; i < numBytes; i++)
    {
        machine->WriteMem(virtualaddr + i, 1, (int)into[i]);
    }

    delete[] into;
}

//----------------------------------------------------------------------
// SwapHeader
//      Do little endian to big endian conversion on the bytes in the
//      object file header, in case the file was generated on a little
//      endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader(NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
//      Create an address space to run a user program.
//      Load the program from a file "executable", and set everything
//      up so that we can start executing user instructions.
//
//      Assumes that the object code file is in NOFF format.
//
//      First, set up the translation from program memory to physical
//      memory.  For now, this is really simple (1:1), since we are
//      only uniprogramming, and we have a single unsegmented page table
//
//      "executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------
AddrSpace::AddrSpace(unsigned int nPages)
{
    unsigned int i, size;
    processJoinCond = new Condition("Process Join Condition");
    processJoinLock = new Lock("Process Join lock");
    ;
    pid = GetNewPid();
    addrspaces[pid] = this;
    nextUserThreadid = 1;
    numPages = nPages;
    size = numPages * PageSize;
    FrameProvider *fp = FrameProvider::GetInstance();
    fp->AcquireFpLock();
    ASSERT((unsigned int)fp->NumAvailFrame() >= numPages);
    ASSERT(numPages <= NumPhysPages);
    // check we're not trying
    // to run anything too big --
    // at least until we have
    // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
    nThreadsCond = new Condition("n threads cond");
    // set up the translation
    pageTable = new TranslationEntry[numPages];
    for(i = 0; i < numPages; i++)
    {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = fp->GetEmptyFrame();
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE; // if the code segment was entirely on
                                       // a separate page, we could set its
                                       // pages to be read-only
    }
    brk = size;
    fp->ReleaseFpLock();

    InitializeThreadData();
    semBitmap = new BitMap(MAX_SEM);
    semList = new Semaphore *[MAX_SEM];
}

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;
    unsigned int i, size;
    processJoinCond = new Condition("Process Join Condition");
    processJoinLock = new Lock("Process Join lock");
    numPages = 0;

    pid = GetNewPid();
    addrspaces[pid] = this;
    nextUserThreadid = 1;
    nThreads = 0;
    currentThread->space = this;
    FrameProvider *fp = FrameProvider::GetInstance();
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);

    if((noffH.noffMagic != NOFFMAGIC) && (WordToHost(noffH.noffMagic) == NOFFMAGIC))
        SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

    // how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size +
           UserStackSize; // we need to increase the size
    // to leave room for the stack
    numPages = divRoundUp(size, PageSize);

    size = numPages * PageSize;
    fp->AcquireFpLock();

    ASSERT((unsigned int)fp->NumAvailFrame() >= numPages);
    ASSERT(numPages <= NumPhysPages);
    // check we're not trying
    // to run anything too big --
    // at least until we have
    // virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", numPages, size);
    nThreadsCond = new Condition("n threads cond");
    // set up the translation
    pageTable = new TranslationEntry[numPages];
    for(i = 0; i < numPages; i++)
    {
        pageTable[i].virtualPage = i;
        pageTable[i].physicalPage = fp->GetEmptyFrame();
        pageTable[i].valid = TRUE;
        pageTable[i].use = FALSE;
        pageTable[i].dirty = FALSE;
        pageTable[i].readOnly = FALSE; // if the code segment was entirely on
                                       // a separate page, we could set its
                                       // pages to be read-only
    }
    brk = size;
    fp->ReleaseFpLock();

    // then, copy in the code and data segments into memory
    if(noffH.code.size > 0)
    {
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", noffH.code.virtualAddr,
              noffH.code.size);
        ReadAtVirtual(executable, noffH.code.virtualAddr, noffH.code.size, noffH.code.inFileAddr,
                      pageTable, numPages);
    }

    if(noffH.initData.size > 0)
    {
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", noffH.initData.virtualAddr,
              noffH.initData.size);
        ReadAtVirtual(executable, noffH.initData.virtualAddr, noffH.initData.size,
                      noffH.initData.inFileAddr, pageTable, numPages);
    }

    // Initialize the threadsBitmap for the physical memory
    InitializeThreadData();
    semBitmap = new BitMap(MAX_SEM);
    semList = new Semaphore *[MAX_SEM];
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
//      Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
    // LB: Missing [] for delete
    // delete pageTable;
    delete[] pageTable;
    delete nThreadsCond;
    delete threadsBitmap;
    delete semBitmap;
    // End of modification
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
//      Set the initial values for the user-level register set.
//
//      We write these directly into the "machine" registers, so
//      that we can immediately jump to user code.  Note that these
//      will be saved/restored into the currentThread->userRegisters
//      when this thread is context switched out.
//----------------------------------------------------------------------

void AddrSpace::InitRegisters()
{
    int i;

    for(i = 0; i < NumTotalRegs; i++)
        machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
//      On a context switch, save any machine state, specific
//      to this address space, that needs saving.
//
//      For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState() {}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
//      On a context switch, restore the machine state so that
//      this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()
{
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
}

//----------------------------------------------------------------------
// AddrSpace::GetSize
//      Compute the size of the address space
//
//  returns:
//      The size of the address space
//----------------------------------------------------------------------

int AddrSpace::GetSize() { return numPages * PageSize; }

//----------------------------------------------------------------------
// AddrSpace::InitializeBitmap
//      Initialize the threadsBitmap for the physical memory
//----------------------------------------------------------------------

void AddrSpace::InitializeThreadData()
{
    // the first available thread stack above the main thread stack
    int curr_addr = this->GetSize() - 16 - ThreadStackSize;
    threadsBitmap = new BitMap(MaxThreadsPerProcess);
    localThreadsInfos = new thread_info_t *[MaxThreadsPerProcess];
    for(int i = 0; i < MaxThreadsPerProcess; i++)
    {
        localThreadsInfos[i] = NULL;
        stackStartAddrs[i] = curr_addr;
        curr_addr -= ThreadStackSize;
    }
}

//----------------------------------------------------------------------
// AddrSpace::AllocateThreadData
//      Allocate a thread stack for a new thread.
//
//      "tid" the ID of the thread we want to allocate the stack
//
//  returns;
//      The index on the struct threadsInfos of the threads (tid)
//----------------------------------------------------------------------

int AddrSpace::AllocateThreadData(int tid)
{
    int free_idx = this->threadsBitmap->Find();
    if(free_idx != -1)
    {
        threadsInfos[tid] = new thread_info_t;
        threadsInfos[tid]->userthread_id = nextUserThreadid;
        threadsInfos[tid]->addrspace_idx = free_idx;
        threadsInfos[tid]->threadCond = new Condition("Thread cond");
        threadsInfos[tid]->thread_id = tid;
        localThreadsInfos[free_idx] = threadsInfos[tid];
        nextUserThreadid++;
    }
    return free_idx;
}

//----------------------------------------------------------------------
// GetThreadInfoFromTid
//      Get the thread info for a thread
//----------------------------------------------------------------------

thread_info_t *AddrSpace::GetThreadInfoFromUserthreadId(int utid)
{
    for(int i = 0; i < MaxThreadsPerProcess; i++)
    {
        if(threadsBitmap->Test(i) && localThreadsInfos[i]->userthread_id == utid)
        {
            return localThreadsInfos[i];
        }
    }
    return NULL;
}

//----------------------------------------------------------------------
// AddrSpace::DeleteThreadData
//      Delete a thread stack for a thread.
//
//      "tid" the ID of the thread
//----------------------------------------------------------------------

void AddrSpace::DeleteThreadData(int tid)
{
    ASSERT(tidMap->Test(tid));
    DEBUG('t', "Delete thread data %d\n", tid);
    ASSERT(threadsBitmap->Test(threadsInfos[tid]->addrspace_idx));
    localThreadsInfos[threadsInfos[tid]->addrspace_idx] = NULL;
    threadsBitmap->Clear(threadsInfos[tid]->addrspace_idx);
    tidMap->Clear(tid);
    delete threadsInfos[tid]->threadCond;
}

//----------------------------------------------------------------------
// AddrSpace::GetStackStartAddr
//      Get the start of the stack from an index.
//
//      "index" the index of the stack
//
// returns:
//      the start address of the stack.
//----------------------------------------------------------------------

int AddrSpace::GetStackStartAddr(int index) { return stackStartAddrs[index]; }

//----------------------------------------------------------------------
// AddrSpace::do_Sbrk
//      Update the page table to add the n new pages for the heap.
//
//      "n" the number of pages that we want to allocate
//
// returns:
//      the starting address of the new memory block allocated.
//      If n = 0: the we return the address of the break of the program
//----------------------------------------------------------------------

unsigned int AddrSpace::do_Sbrk(unsigned int n)
{
    FrameProvider *fp = FrameProvider::GetInstance();

    fp->AcquireFpLock();
    int oldBrk = brk; // The first page of the start of the memory block (or the break one)
    if((unsigned int)fp->NumAvailFrame() >= n)
    {
        TranslationEntry *newPageTable = new TranslationEntry[numPages + n];
        // Copy the old page table to the new one
        for(unsigned int i = 0; i < numPages; i++)
        {
            newPageTable[i].virtualPage = pageTable[i].virtualPage;
            newPageTable[i].physicalPage = pageTable[i].physicalPage;
            newPageTable[i].valid = pageTable[i].valid;
            newPageTable[i].use = pageTable[i].use;
            newPageTable[i].dirty = pageTable[i].dirty;
            newPageTable[i].readOnly = pageTable[i].readOnly;
        }

        // Add the new pages we want to allocate
        for(unsigned int i = numPages; i < numPages + n; i++)
        {
            newPageTable[i].virtualPage = i;
            newPageTable[i].physicalPage = fp->GetEmptyFrame();
            newPageTable[i].valid = TRUE;
            newPageTable[i].use = FALSE;
            newPageTable[i].dirty = FALSE;
            newPageTable[i].readOnly = FALSE;
        }
        // Swap the old page table to the new one
        TranslationEntry *tmp = pageTable;
        numPages = numPages + n;
        pageTable = newPageTable;

        delete tmp;

        brk = numPages * PageSize; // The address
        machine->pageTableSize = numPages;
        machine->pageTable = pageTable;
        fp->ReleaseFpLock();
        return oldBrk;
    }
    else
    {
        fp->ReleaseFpLock();
        return 0;
    }
}
