#include "syscall.h"

int incr (int a) {a = a + 1; return a; }

int main() { PutInt (incr (999)); }
