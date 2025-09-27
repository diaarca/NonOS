#include "syscall.h"

int main()
{
    pid_t i = ForkExec("singleput");
    ProcessJoin(i);
}
