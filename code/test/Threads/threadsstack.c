#include "syscall.h"

int a;
void thread_routine (void *arg)
{
    PutString ("Final thread : ", 30);
    PutInt (*(int *)arg);
    PutChar ('\n');
    
    for (int i = 0; i < 500000; i++);
    
    ThreadExit ();
}

void thread_routine_create (void *arg)
{
    tid_t id;
    int i;
    PutString ("Starting create routine ", 30);
    PutInt(*(int *)arg);
    PutChar('\n');
    if (a < 1) {
        a++;
        i = 1;
        id = ThreadCreate (thread_routine_create, &i);
        if (id != -1)
        {
            PutString ("Thread created with arg: ", 25);
            PutInt (i);
            PutChar ('\n');
            ThreadJoin(id);
        }
        else
        {
            PutString ("Impossible to create a new thread in routine create!\n", 60);
        }
    } else {
        i = -1;
        id = ThreadCreate (thread_routine, &i);
        if (id != -1)
        {
            PutString ("Thread created with arg: ", 25);
            PutInt (i);
            PutChar ('\n');
            ThreadJoin(id);
        }
        else
        {
            PutString ("Impossible to create a new thread in routine create!\n", 60);
        }
    }
    ThreadExit ();
}

int main ()
{
    PutString ("Main thread started\n", 50);
    int i = 0;
    a = 0;
    ThreadCreate (thread_routine_create, &i);
    PutString("End of threads\n", 50);
}
