#include "syscall.h"
#include "mem_alloc.h"

struct arg_t {
    int fd;
    char s[1];
};


void thread_routine(void *arg)
{
    struct arg_t a = *(struct arg_t *)arg;
    int value, i = 0;
    while ((value = Write(a.s, 1, a.fd)) > 0 && i < 1000) {
        i++;
    }
}

int main() {
    int fd, value, id1, id2;
    char buffer[100];
    mem_init(100);

    if (!Create("Test")) {
        PutString("The file Test can't be created\n", 50);
        Exit(1);
    }

    if ((fd = Open("Test")) == -1) {
        PutString("The file Test can't be opened\n", 50);
        Exit(1);
    }

    struct arg_t *arg1 = (struct arg_t *)mem_alloc(sizeof(struct arg_t));
    arg1->fd = fd;
    arg1->s[0] = 'a';

    struct arg_t *arg2 = (struct arg_t *)mem_alloc(sizeof(struct arg_t));
    arg2->fd = fd;
    arg2->s[0] = 'b';

    id1 = ThreadCreate(thread_routine, arg1);
    id2 = ThreadCreate(thread_routine, arg2);
    ThreadJoin(id1);
    ThreadJoin(id2);

    Seek(0, fd);

    while ((value = Read(buffer, 100, fd)) > 0) {
        PutString(buffer, value);
    }
    PutChar('\n');

    Close(fd);

    return 0;
}
