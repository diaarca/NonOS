#include "syscall.h"
void thread_routine (void *arg)
{
    // PutString ("Yes it runs !", 12);
    // PutInt (*(int *)arg);
    // PutChar ('\n');
    // PutString ("Let's wait a bit before terminating the thread\n", 50);
    for (int i = 0; i < 100000; i++)
        ;
    PutString ("Thread terminate:", 20);
    PutInt (*(int *)arg);
    PutChar ('\n');
    ThreadExit ();
}

void thread_routine_create (void *arg)
{
    PutString ("Starting create routine\n", 30);
    int i = -1;
    tid_t tid_t_ = ThreadCreate (thread_routine, &i);
    if (tid_t_ != -1)
    {
        PutString ("Thread created with arg: ", 25);
        PutInt (i);
        PutChar ('\n');
    }
    else
    {
        PutString ("Impossible to create a new thread in routine create!\n", 60);
    }
    ThreadExit ();
}

int main ()
{
    PutString ("Main thread started\n", 50);
    tid_t tid_t_;
    int args[10];
    ThreadCreate (thread_routine_create, 0);

    for (int i = 0; i < 10; i++)
    {
        args[i] = i;
        tid_t_ = ThreadCreate (thread_routine, &args[i]);
        if (tid_t_ != -1)
        {
            PutString ("Thread created with arg: ", 25);
            PutInt (i);
            PutChar ('\n');
        }
        else
        {
            PutString ("Impossible to create a new thread !\n", 50);
        }
    }
    Halt ();
}
