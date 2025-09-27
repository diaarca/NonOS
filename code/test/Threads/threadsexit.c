#include "syscall.h"
void thread_routine(void *arg)
{
    PutString("Sub thread started\n", 50);
    PutString("Thread terminate: ", 20);
    PutInt(*(int *)arg);
    PutChar('\n');
    Halt(2);
}

int main()
{
    PutString("Main thread started\n", 50);
    int arg = 2;
    tid_t id;
    id = ThreadCreate(thread_routine, &arg);
    ThreadJoin(id);

    PutString("End of the main (anormal)\n", 30);
}
