#include "syscall.h"

int n = 0;
sem_t sem;
void Incr(void *args)
{
    for(int i = 0; i < 500; i++)
    {
        SemWait(&sem);
        n++;
        SemPost(&sem);
    }
    ThreadExit();
}

int main() {
  SemInit(&sem, 1);
  tid_t id1 = ThreadCreate(Incr, 0);
  tid_t id2 = ThreadCreate(Incr, 0);
  ThreadJoin(id1);
  ThreadJoin(id2);
  SemDestroy(&sem);
  PutInt(n);
  PutChar('\n');
}
