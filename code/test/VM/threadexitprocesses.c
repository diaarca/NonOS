#include "syscall.h"

int main()
{
    ForkExec("Threads/threadsexit");
    for(int i = 0; i < 1000000; i++)
    {
    }
    PutString("Hello, world!\n", 60);
}
