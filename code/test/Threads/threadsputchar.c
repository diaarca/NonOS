#include "syscall.h"

void thread_routine(void *arg){
  for (int i = 0; i < 99999; i++) {
        PutChar('a');
  }
  ThreadExit();
}

int main ()
{
    int a = 15;
    // PutString ("Main thread test\n", 50);
    ThreadCreate (thread_routine, &a);
    for (int i = 0; i < 99999; i++)
    {
        PutChar('b');
    }
}
