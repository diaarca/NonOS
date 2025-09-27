#include "mem_alloc.h"
#include "syscall.h"

void incr(int *x) { (*x)++; }

int main()
{
    mem_init(400);
    int *x = mem_alloc(sizeof(int) * 100);
    if(x == NULL)
    {
        Exit(1);
    }
    x[0] = 5;
    mem_free(x);
    int *y = mem_alloc(sizeof(int) * 100);
    if(y == NULL)
    {
        Exit(2);
    }
    y[0] = 1;
    incr(&x[0]);
    incr(&y[0]);
    PutInt(x[0]);
    PutChar('\n');
    PutInt(y[0]);
    return 0;
}
