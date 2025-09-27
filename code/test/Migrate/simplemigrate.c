#include "syscall.h"

int main()
{
    int p = 50; 
    int migrate = SendProcess(0, 0);
    PutString("Process sent/received, should show this string in the Sender and the Receiver\n", 80);
    if (migrate == 0){
        SendProcess(0, 1);
        PutString("Process received, should show this string only in the Receiver because Sender has been stopped\n", 100);
    }
    PutString("Testing that the stack has been well copied, must show 50: ", 80);
    PutInt(p);
}
