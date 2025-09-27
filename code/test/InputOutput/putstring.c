#include "syscall.h"
#define STR_SIZE 200

int main ()
{
    PutString ("perfect size", 12);
    PutChar ('\n');
    PutString ("too short string", 200);
    PutChar ('\n');
    PutString ("too long string", 1);
    PutChar ('\n');
    char str[STR_SIZE];
    for (int i = 0; i < STR_SIZE - 1; i++)
    {
        str[i] = 'X';
    }
    str[STR_SIZE - 1] = 'O';
    PutString (str, STR_SIZE);
    PutChar ('\n');

}
