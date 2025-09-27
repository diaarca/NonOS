#include "migrate.h"
#include <climits>
#include <cstddef>

bool SendProcess(int farAddr)
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    AddrSpace *space = currentThread->space;
    Connection *c = postOffice->Connect(farAddr);
    DEBUG('m', "Receive connection from %d %d\n", c->pIn->pktHdr.from, c->pIn->mailHdr.from);
    // Sending the memory
    // Sending the number of pages
    DEBUG('m', "Sending the number of pages : %d\n", space->numPages);
    postOffice->Send(c, (char *)&space->numPages, sizeof(int));
    // char buffer[PageSize];
    //  Sending pages
    for(unsigned int nPage = 0; nPage < space->numPages; nPage++)
    {
        for(int j = 0; j < PageSize; j++)
        {
            int value;
            machine->ReadMem(nPage * PageSize + j, 1, &value);
            if(!postOffice->Send(c, (char *)&value, sizeof(char)))
            {
                (void)interrupt->SetLevel(oldLevel);
                return false;
            }
        }
    }
    // Sending registers of the current thread:
    for(int r = 0; r < NumTotalRegs; r++)
    {
        if(!postOffice->Send(c, (char *)&currentThread->userRegisters[r], sizeof(int)))
        {
            (void)interrupt->SetLevel(oldLevel);
            return false;
        }
    }

    // Sending semList
    int noDataValue = INT_MAX;
    for(int i = 0; i < MAX_SEM; i++)
    {
        if(space->semBitmap->Test(i))
        {
            if(!postOffice->Send(c, (char *)&space->semList[i]->value, sizeof(int)))
            {
                (void)interrupt->SetLevel(oldLevel);
                return false;
            }
        }
        else
        {
            if(!postOffice->Send(c, (char *)&noDataValue, sizeof(int)))
            {
                (void)interrupt->SetLevel(oldLevel);
                return false;
            }
        }
    }

    // sending nThreads
    if(!postOffice->Send(c, (char *)&space->nThreads, sizeof(int)))
    {
        (void)interrupt->SetLevel(oldLevel);
        return false;
    }

    // Sending other threads info
    for(int i = 0; i < MaxThreadsPerProcess; i++)
    {
        thread_info_t *info = space->localThreadsInfos[i];
        if(info && info->thread_id < MaxThreads && threads[info->thread_id] &&
           tidMap->Test(info->thread_id))
        {
            if(!postOffice->Send(c, (char *)&info->userthread_id, sizeof(int)))
            {
                (void)interrupt->SetLevel(oldLevel);
                return false;
            }
            for(int r = 0; r < NumTotalRegs; r++)
            {
                if(!postOffice->Send(c, (char *)&threads[info->thread_id]->userRegisters[r],
                                     sizeof(int)))
                {
                    (void)interrupt->SetLevel(oldLevel);
                    return false;
                }
            }
        }
        else
        {
            if(!postOffice->Send(c, (char *)&noDataValue, sizeof(int)))
            {
                (void)interrupt->SetLevel(oldLevel);
                return false;
            }
        }
    }

    postOffice->Disconnect(c);
    (void)interrupt->SetLevel(oldLevel);

    return true;
}

static void RunListenedProcess(int arg)
{
    currentThread->space->RestoreState();

    machine->Run();
}

int ListenProcess()
{

    char buffer[PageSize];
    AddrSpace *tmp = currentThread->space;
    Connection *c = postOffice->Listen();
    if(!c)
    {
        return -1;
    }
    AddrSpace::nUsedAddrSpaceLock->Acquire();
    if(AddrSpace::nUsedAddrSpace >= MaxProcesses)
    {
        AddrSpace::nUsedAddrSpaceLock->Release();
        return -1;
    }
    // Increment the number of running process
    AddrSpace::nUsedAddrSpace++;
    AddrSpace::nUsedAddrSpaceLock->Release();

    DEBUG('m', "Receive connection from %d %d\n", c->pIn->pktHdr.from, c->pIn->mailHdr.from);
    unsigned int nPages;
    // Receive the number of pages
    postOffice->Receive(c, (char *)&nPages);
    DEBUG('m', "Need %d pages\n", nPages);
    AddrSpace *newSpace = new AddrSpace(nPages);
    int newPid = newSpace->pid;
    newSpace->RestoreState();
    currentThread->space = newSpace;
    // Receive pages
    for(unsigned int nPage = 0; nPage < nPages; nPage++)
    {
        for(int j = 0; j < PageSize; j++)
        {
            postOffice->Receive(c, &buffer[0]);
            machine->WriteMem(nPage * PageSize + j, 1, buffer[0]);
        }
    }
    Thread *newThread = new Thread("Listen process thread");

    // Receive registers of the current thread:
    for(int r = 0; r < NumTotalRegs; r++)
    {
        int value;
        postOffice->Receive(c, (char *)&value);
        newThread->userRegisters[r] = value;
    }
    newThread->userRegisters[2] = 1;

    int semValue;

    // Receive sem list
    for(int i = 0; i < MAX_SEM; i++)
    {
        postOffice->Receive(c, (char *)&semValue);
        if(semValue != INT_MAX)
        {
            printf("Receiving sem %d %d\n", i, semValue);
            newSpace->semList[i] = new Semaphore("sem list", semValue);
            newSpace->semBitmap->Mark(i);
        }
    }

    // receive nThreads
    postOffice->Receive(c, (char *)&newSpace->nThreads);

    unsigned int userThreadid;
    for(int i = 0; i < MaxThreadsPerProcess; i++)
    {
        postOffice->Receive(c, (char *)&userThreadid);
        if(userThreadid != INT_MAX)
        {
            printf("Receive data about threads userid: %d, emplacement %d\n", userThreadid, i);
            Thread *nT = new Thread("Listen process sub-thread");
            nT->space = newSpace;
            nT->isMain = false;
            int ntid = nT->GetThreadId();
            newSpace->threadsBitmap->Mark(i);
            threadsInfos[ntid] = new thread_info_t;
            threadsInfos[ntid]->userthread_id = userThreadid;
            threadsInfos[ntid]->addrspace_idx = i;
            threadsInfos[ntid]->threadCond = new Condition("Thread cond");
            threadsInfos[ntid]->thread_id = ntid;
            newSpace->localThreadsInfos[i] = threadsInfos[ntid];
            newSpace->nextUserThreadid = (newSpace->nextUserThreadid > userThreadid + 1)
                                             ? newSpace->nextUserThreadid
                                             : (userThreadid + 1);
            for(int r = 0; r < NumTotalRegs; r++)
            {
                int value;
                postOffice->Receive(c, (char *)&value);
                nT->userRegisters[r] = value;
            }
            nT->Fork(RunListenedProcess, 0);
        }
    }

    newThread->space = newSpace;
    newThread->Fork(RunListenedProcess, 0);
    currentThread->space = tmp;
    currentThread->space->RestoreState();
    postOffice->Disconnect(c);
    return newPid;
}
