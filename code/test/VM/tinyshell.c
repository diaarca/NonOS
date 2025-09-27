#include "syscall.h"

int strCmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int main()
{
    tid_t newProc;
    char buffer[60];
    int i;
    PutString("Starting the shell !\n", 25);
    while (1)
    {
        PutString("->", 2);

        i = 0;

        GetString(buffer, 60);
        do
        {

        } while (buffer[i++] != '\n');

        buffer[--i] = '\0';
        PutString(buffer, 60);
        PutChar('\n');
        if (strCmp(buffer, "quit") == 0)
        {
            break;
        }
        else if (i > 0)
        {
            newProc = ForkExec(buffer);
            ProcessJoin(newProc);
        }
    }
}
