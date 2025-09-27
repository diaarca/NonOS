#include "syscall.h"
#define STR_SIZE 10
int main ()
{
    char s[STR_SIZE];
    for (;;)
    {
        PutString ("\nReading: ", 10);
        GetString (s, STR_SIZE);
        if (s[0] == '\0')
        {
            break;
        }
        PutString ("\nReaded : ", 10);
        PutString (s, STR_SIZE);
    }
    Exit (0);
}
