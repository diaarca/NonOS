#ifndef LIBGCC_H
#define LIBGCC_H
typedef unsigned int size_t;

typedef int sem_t;
typedef signed long int int32_t;
typedef unsigned long int uint32_t;
void *memset(void *s, int c, size_t n);

void *memcpy(void *dest, const void *src, size_t n);

void *memmove(void *dest, const void *src, size_t n);
#endif // !i
