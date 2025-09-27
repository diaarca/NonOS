#include "syscall.h"

int main ()
{
    int n;
    for (;;)
    {
        PutString ("Reading: ", 10);
        GetInt(&n);
        if (n == -1){
            PutString("-1 readed, stop the program !\n", 31);
            break;
        }
        PutString ("Readed : ", 10);
        PutInt(n);
        PutChar('\n');
    }
}