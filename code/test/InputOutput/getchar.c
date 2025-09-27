#include "syscall.h"

int main ()
{
    char c;
    while ((c = GetChar ()) != -1)
    {
        PutChar (c);
    }
    Exit (0);
}
