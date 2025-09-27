// synch.cc
//      Routines for synchronizing threads.  Three kinds of
//      synchronization routines are defined here: semaphores, locks
//      and condition variables (the implementation of the last two
//      are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "synch.h"
#include "copyright.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
//      Initialize a semaphore, so that it can be used for synchronization.
//
//      "debugName" is an arbitrary name, useful for debugging.
//      "initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(const char *debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
//      De-allocate semaphore, when no longer needed.  Assume no one
//      is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore() { delete queue; }

//----------------------------------------------------------------------
// Semaphore::P
//      Wait until semaphore value > 0, then decrement.  Checking the
//      value and decrementing must be done atomically, so we
//      need to disable interrupts before checking the value.
//
//      Note that Thread::Sleep assumes that interrupts are disabled
//      when it is called.
//----------------------------------------------------------------------

void Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // disable interrupts

    while(value == 0)
    {                                         // semaphore not available
        queue->Append((void *)currentThread); // so go to sleep
        currentThread->Sleep();
    }
    value--; // semaphore available,
    // consume its value

    (void)interrupt->SetLevel(oldLevel); // re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
//      Increment semaphore value, waking up a waiter if necessary.
//      As with P(), this operation must be atomic, so we need to disable
//      interrupts.  Scheduler::ReadyToRun() assumes that threads
//      are disabled when it is called.
//----------------------------------------------------------------------

void Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if(thread != NULL) // make thread ready, consuming the V immediately
        scheduler->ReadyToRun(thread);
    value++;
    (void)interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments
// Note -- without a correct implementation of Condition::Wait(),
// the test case in the network assignment won't work!

//----------------------------------------------------------------------
// Lock::Lock
//      Initialize a lock, so that it can be used for synchronization.
//
//      "debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------

Lock::Lock(const char *debugName)
{
    name = debugName;
    lockOwner = -1;
    sem = new Semaphore(debugName, 1);
}

//----------------------------------------------------------------------
// Lock::~Lock
//      De-allocate lock when no longer needed.
//----------------------------------------------------------------------

Lock::~Lock() { delete sem; }

//----------------------------------------------------------------------
// Lock::Acquire
//      Acquire the lock. If it's already held, the calling thread will
//      block until the lock becomes available.
//      Must be atomic, so we disable interrupts temporarily.
//----------------------------------------------------------------------

void Lock::Acquire()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // Disable interrupts

    sem->P();                                 // Wait (P) on semaphore
    lockOwner = currentThread->GetThreadId(); // Set the current thread as the lock owner

    (void)interrupt->SetLevel(oldLevel); // Re-enable interrupts
}

//----------------------------------------------------------------------
// Lock::Release
//      Release the lock. If other threads are waiting, one will acquire it.
//      Only the thread that holds the lock should release it.
//----------------------------------------------------------------------

void Lock::Release()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // Disable interrupts

    ASSERT(isHeldByCurrentThread()); // Ensure current thread holds the lock
    lockOwner = -1;                  // Clear lock ownership
    sem->V();                        // Signal (V) on semaphore

    (void)interrupt->SetLevel(oldLevel); // Re-enable interrupts
}

//----------------------------------------------------------------------
// Lock::isHeldByCurrentThread
//      Check if the current thread holds this lock.
//----------------------------------------------------------------------

bool Lock::isHeldByCurrentThread() { return lockOwner == currentThread->GetThreadId(); }

//----------------------------------------------------------------------
// Condition::Condition
//      Initialize a condition variable, so that it can be used for
//      synchronization.
//
//      "debugName" is an arbitrary name, useful for debugging.
//----------------------------------------------------------------------

Condition::Condition(const char *debugName)
{
    name = debugName;
    waitQueue = new List; // Queue of threads waiting on the condition
}

//----------------------------------------------------------------------
// Condition::~Condition
//      De-allocate condition variable when no longer needed.
//----------------------------------------------------------------------

Condition::~Condition() { delete waitQueue; }

//----------------------------------------------------------------------
// Condition::Wait
//      Release the lock, and go to sleep waiting for the condition.
//      When awakened, re-acquire the lock.
//      The lock must be held by the calling thread when this function is called.
//----------------------------------------------------------------------

void Condition::Wait(Lock *conditionLock)
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // Disable interrupts

    ASSERT(conditionLock->isHeldByCurrentThread()); // Ensure lock is held
    waitQueue->Append((void *)currentThread);       // Add thread to wait queue
    conditionLock->Release();                       // Release the lock
    currentThread->Sleep();                         // Go to sleep
    conditionLock->Acquire();                       // Re-acquire the lock when awakened

    (void)interrupt->SetLevel(oldLevel); // Re-enable interrupts
}

//----------------------------------------------------------------------
// Condition::Signal
//      Wake up a thread waiting on this condition, if any. The thread woken up is the first
//      thread of the waiting list (in the front of the list).
//      The lock must be held by the calling thread when this function is called.
//----------------------------------------------------------------------

void Condition::Signal(Lock *conditionLock)
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // Disable interrupts

    ASSERT(conditionLock->isHeldByCurrentThread()); // Ensure lock is held
    Thread *thread = (Thread *)waitQueue->Remove(); // Remove one thread
    if(thread != NULL)
    {
        scheduler->ReadyToRun(
            thread); // Make a thread (the first thread of the waitQueue) to the state Ready
    }

    (void)interrupt->SetLevel(oldLevel); // Re-enable interrupts
}

//----------------------------------------------------------------------
// Condition::Broadcast
//      Wake up all threads waiting on this condition.
//      The lock must be held by the calling thread when this function is called.
//----------------------------------------------------------------------

void Condition::Broadcast(Lock *conditionLock)
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff); // Disable interrupts

    ASSERT(conditionLock->isHeldByCurrentThread()); // Ensure lock is held
    Thread *thread;
    while((thread = (Thread *)waitQueue->Remove()) != NULL)
    {
        scheduler->ReadyToRun(thread); // Make each waiting threads to the state Ready
    }

    (void)interrupt->SetLevel(oldLevel); // Re-enable interrupts
}
