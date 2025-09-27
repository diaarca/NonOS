#include "syscall.h"
#include "libgcc.h"
#define NB_PROCESS 20

int main()
{
    tid_t ids[NB_PROCESS];
    for(int i = 0; i < 20; i++)
    {

        for(int j = 0; j < NB_PROCESS; j++)
        {
            ids[j] = ForkExec("incr");
        }
        for(int j = 0; j < NB_PROCESS; j++)
        {
            ProcessJoin(ids[j]);
        }
    }
    PutString("Time to stop!\n", 100);
}
