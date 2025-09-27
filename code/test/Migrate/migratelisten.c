#include "syscall.h"

int main(){
  pid_t p; 
  PutString("\nReady to listen !\n", 30);
  while ((p = ListenProcess())){
    ProcessJoin(p);
    PutString("\nReady to listen !\n", 30);
  }
  
}
