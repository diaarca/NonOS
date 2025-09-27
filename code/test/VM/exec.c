#include "syscall.h"

int main()
{
    pid_t ids[10];
    for(int i = 0; i < 5; i++)
    {

        for(int j = 0; j < 1; j++)
        {
            ids[j] = ForkExec("incr");
        }
        for(int j = 0; j < 1; j++)
        {
            ProcessJoin(ids[j]);
        }
    }
}
