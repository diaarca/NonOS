#include "syscall.h"

int main() {
    int fd, value;
    char buffer[100];

    if (Create("Test") == -1) {
        PutString("The file Test can't be created\n", 50);
        Exit(1);
    }

    if ((fd = Open("Test")) == -1) {
        PutString("The file Test can't be opened\n", 50);
        Exit(1);
    }

    value = Write("Hello, World!", 14, fd);
    if (value < 0) {
        PutString("We got an error during the write\n", 50);
        Exit(1);
    }

    Seek(fd, 0);

    value = Read(buffer, 14, fd);
    if (value < 0) {
        PutString("We got an error during the read\n", 50);
        Exit(1);
    }

    PutString(buffer, value);
    PutChar('\n');

    Close(fd);
    
    return 0;
}
