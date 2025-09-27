#include "libgcc.h"
/* Recent gcc can emit call to such functions, even when -fno-builtin is used
 * So, this file provides a basic implementation of them
 *
 * For reference: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=56888#c8
 */

void *memset(void *s, int c, size_t n) {
    char *ptr = s;
    for (int i = 0; i < n; i++) {
        *(ptr++) = c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *ptr_d = dest;
    const char *ptr_s = src;
    for (int i = 0; i < n; i++) {
        *(ptr_d++) = *(ptr_s++);
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    if (src >= dest) {
        return memcpy(dest, src, n);
    } else {
        char *ptr_d = dest;
        const char *ptr_s = src;
        ptr_d += n;
        ptr_s += n;
        for (int i = 0; i < n; i++) {
            *(--ptr_d) = *(--ptr_s);
        }
        return dest;
    }
    return dest;
}
