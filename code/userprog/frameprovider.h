#ifndef FRAMEPROVIDER_H
#define FRAMEPROVIDER_H

#include "bitmap.h"
#include "synch.h"

class FrameProvider
{
  public:
    ~FrameProvider();
    
    static FrameProvider* GetInstance(); // Get the instance of the frameProvider
    int GetEmptyFrame(); // Get an empty frame
    void ReleaseFrame(int frame); // Release a frame
    int NumAvailFrame(); // Return the number of frame available
    void AcquireFpLock(); // Acquire the lock
    void ReleaseFpLock(); // Release the lock

    BitMap* frameMap;
  private:
    FrameProvider();
    
    static FrameProvider* inst;
    int nAvailFrame; // Number of available frames
    Lock *fpLock; // Lock for the frameProviders' critic sections
    int FindFreeFrame(); // Find a free frame
};

#endif // FRAMEPROVIDER_H
