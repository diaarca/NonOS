#include "syscall.h"

void thread_routine(void *arg) {
    int fd;
    if ((fd = Open("Test")) >= 0) {
        PutString("The file Test has been opened on the file descriptor ", 60);
        PutInt(fd);
        PutChar('\n');
        for (int i = 0; i<100000; i++);
        Close(fd);
    } else {
        PutString("The file Test has not been opened\n", 50);
    }
}

int main() {
    int id[5];

    if (!Create("Test")) {
        PutString("The file Test can't be created\n", 50);
        Exit(1);
    }
    
    for (int i = 0; i < 5; i++) {
        if ((id[i] = ThreadCreate(thread_routine, 0)) == -1) {
            PutString("Create threads Fail (Too many threads)\n", 50);
        }
    }
    for (int i = 0; i < 5; i++)
         if (id[i] != -1) 
            ThreadJoin(id[i]);

    return 0;
}
