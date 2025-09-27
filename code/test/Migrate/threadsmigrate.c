#include "syscall.h"

#define BUFFER_SIZE 10

int buffer[BUFFER_SIZE];
int in = 0, out = 0;
sem_t empty, full, mutex;

void Producer(void *args)
{
    for(int i = 0; i < 50; i++)
    {
        SemWait(&empty);
        SemWait(&mutex);
        PutString("Produce: ", 9);
        PutInt(i);
        PutChar('\n');

        buffer[in] = i;
        in = (in + 1) % BUFFER_SIZE;

        SemPost(&mutex);
        SemPost(&full);
    }
}

void Consumer(void *args)
{
    for(int i = 0; i < 50; i++)
    {
        SemWait(&full);
        SemWait(&mutex);

        int item = buffer[out];
        out = (out + 1) % BUFFER_SIZE;

        PutString("Consume: ", 9);
        PutInt(item);
        PutChar('\n');

        SemPost(&mutex);
        SemPost(&empty);
    }
}

int main()
{
    SemInit(&empty, BUFFER_SIZE);
    SemInit(&full, 0);
    SemInit(&mutex, 1);

    tid_t producer = ThreadCreate(Producer, 0);
    tid_t consumer = ThreadCreate(Consumer, 0);
    SemWait(&mutex);
    SendProcess(0, 0);
    PutString("Process sent\n", 20);
    SemPost(&mutex);
    ThreadJoin(producer);
    ThreadJoin(consumer);

    SemDestroy(&empty);
    SemDestroy(&full);
    SemDestroy(&mutex);

    PutChar('\n');
    Exit(0);
}
