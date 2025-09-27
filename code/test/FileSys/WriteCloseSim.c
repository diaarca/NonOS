#include "syscall.h"

#define a "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"

int fd;

void thread_routineWrite(void *arg) {
    int value, i = 0;
    while ((value = Write(a, 42, fd)) > 0 && i < 1000) {
        i++;
    }
}

void thread_routineClose(void *arg) {
    if (Close(fd) == -1) {
        PutString("Close didn't work\n", 50);
    }
}

int main() {
    int id1, id2, value, nb;
    char buffer[100];

    if (!Create("Test")) {
        PutString("The file Test can't be created\n", 50);
        Exit(1);
    }

    if ((fd = Open("Test")) == -1) {
        PutString("The file Test can't be opened\n", 50);
        Exit(1);
    }

    id1 = ThreadCreate(thread_routineWrite, 0);
    id2 = ThreadCreate(thread_routineClose, 0);
    ThreadJoin(id1);
    ThreadJoin(id2);

    if ((fd = Open("Test")) == -1) {
        PutString("The file Test can't be opened\n", 50);
        Exit(1);
    }

    nb = 0;
    while ((value = Read(buffer, 42, fd)) > 0) {
        if (value != 42) {
            nb++;
        }
    }
    if (nb != 0) {
        PutString("The file has been closed during the write\n", 50);
    } else {
        PutString("The file has not been corrupted!\n", 50);
    }
    PutChar('\n');

    Close(fd);

    return 0;
}
