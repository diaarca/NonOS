#ifndef USERTHREAD_H
#define USERTHREAD_H
#include "system.h"
#include <climits>

extern Semaphore *semFORK;

struct thread_f_arg {
    int f;
    int arg;
};

struct thread_arg
{
    int f_wrapper;
    struct thread_f_arg f_arg;
    int bitmapIdx;
};


int do_UserThreadCreate(int f, int arg);
int do_UserThreadCreate_wrapper(int f_wrapper, int f, int arg);
void do_UserThreadExit();
void do_UserThreadJoin(int userThreadId); 
int do_SemInit(int initValue);
void do_SemPost(int semid);
void do_SemWait(int semid);
void do_SemDestroy(int semid);
int do_ForkExec(char *s);
void do_ProcessJoin(int pid);
#endif
