#include "syscall.h"

void thread_routine(void *arg) {
    if (Create("Test") == 0) {
        PutString("The file Test can't be created\n", 50);
    } else {
        PutString("The file Test has been created\n", 50);
    }
}

int main() {
    int id[5];

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
