#include "syscall.h"
void thread_routine (void *arg)
{
    PutString ("Thread created:", 20);
    PutInt (*(int *)arg);
    PutChar ('\n');
    for (int i = 0; i < 100000; i++)
        ;
    PutString ("Thread terminate:", 20);
    PutInt (*(int *)arg);
    PutChar ('\n');
    ThreadExit ();
}

int main ()
{
    PutString ("Main thread started\n", 50);
    tid_t tid_t_;
    int args[20];

    for (int i = 0; i < 20; i++)
    {
        args[i] = i;
        tid_t_ = ThreadCreate (thread_routine, &args[i]);
        if (tid_t_ != -1)
        {
            ThreadJoin (tid_t_);
        }
        else
        {
            PutString ("Impossible to create a new thread !\n", 50);
        }
    }
    PutString("We try to wait for a already terminated thread\n", 50);
    ThreadJoin(tid_t_);
    PutString("We try to wait for a non-existant thread\n", 50);
    ThreadJoin(100);
    Halt ();
}
