#include "frameprovider.h"
#include "bitmap.h"
#include "machine.h"
#include "system.h"
#include <cstdlib>

FrameProvider *FrameProvider::inst = nullptr;

//----------------------------------------------------------------------
// FrameProvider::FrameProvider
//      Initialize the frame provider for the system.
//      Initialize the BitMap for defining if a page is used or not. 
//----------------------------------------------------------------------

FrameProvider::FrameProvider()
{
    frameMap = new BitMap(NumPhysPages);
    nAvailFrame = NumPhysPages;
    inst = nullptr;
    fpLock = new Lock("Frame Provider Lock");
}

//----------------------------------------------------------------------
// FrameProvider::GetInstance
//      Allows to get the instance of the frame provider of the actual system.
//----------------------------------------------------------------------

FrameProvider *FrameProvider::GetInstance()
{
    if(inst == nullptr)
    {
        inst = new FrameProvider();
    }
    return inst;
}

//----------------------------------------------------------------------
// FrameProvider::~FrameProvider
//      Delete the BitMap of the frames and the lock.
//----------------------------------------------------------------------

FrameProvider::~FrameProvider()
{
    delete frameMap;
    delete fpLock;
}

//----------------------------------------------------------------------
// FrameProvider::FindFreeFrame
//      Find the fisrt frame that is free in the frameMap.
//
// Return:
//      the index of the empty frame that can be used.
//----------------------------------------------------------------------

int FrameProvider::FindFreeFrame()
{

    int idx = frameMap->Find();
    /*if(nAvailFrame == 0)
    {
        return -1;
    }
    int x = rand() % NumPhysPages;
    while(frameMap->Test(x))
    {
        x = rand() % NumPhysPages;
    }
    frameMap->Mark(x);
    return x;
*/ return idx;
}

//----------------------------------------------------------------------
// FrameProvider::GetEmptyFrame
//      Get the empty frame of the system and clear the memory of the frame.
//
// Return:
//      the index of the empty frame.
//----------------------------------------------------------------------

int FrameProvider::GetEmptyFrame()
{
    int idx = FindFreeFrame();
    if(idx != -1)
    {
        bzero(machine->mainMemory + (PageSize * idx), PageSize);
        nAvailFrame--;
    }
    // printf("Allocate %d frame\n", idx);
    return idx;
}

//----------------------------------------------------------------------
// FrameProvider::ReleaseFrame
//      Release the wanted frame.
//
//      "frame" the frame that we want to release
//----------------------------------------------------------------------

void FrameProvider::ReleaseFrame(int frame)
{
    frameMap->Clear(frame);
    // printf("Free %d frame\n", frame);
    nAvailFrame++;
}

//----------------------------------------------------------------------
// FrameProvider::numAvailFrame
//      Return the number of available frames
//----------------------------------------------------------------------

int FrameProvider::NumAvailFrame() { return nAvailFrame; }

//----------------------------------------------------------------------
// FrameProvider::AcquireFpLock
//      Allow to acquire the lock of the frame provider.
//----------------------------------------------------------------------

void FrameProvider::AcquireFpLock() { fpLock->Acquire(); }

//----------------------------------------------------------------------
// FrameProvider::~FrameProvider
//      Allow to release the lock of the frame proider.
//----------------------------------------------------------------------

void FrameProvider::ReleaseFpLock() { fpLock->Release(); }
