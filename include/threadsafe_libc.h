#ifndef __THREADSAFE_LIBC_H
#define __THREADSAFE_LIBC_H

#include <assert.h>
#include <stddef.h>

extern int in_libc_flag;

void threadsafe_free(void *ptr);

void *threadsafe_malloc(size_t __size);

void *threadsafe_calloc(size_t __nmemb, size_t __size);

void *threadsafe_memset(void *__s, int __c, size_t __n);

void *threadsafe_memcpy(void *__restrict__ __dest, const void *__restrict__ __src, size_t __n);

int threadsafe_printf(const char *__restrict__ __format, ...);

void threadsafe_exit(int __status);

#define free threadsafe_free
#define malloc threadsafe_malloc
#define calloc threadsafe_calloc
#define threadsafe_assert(expr) \
    do {                        \
        in_libc_flag = 1;       \
        assert(expr);           \
        in_libc_flag = 0;       \
    } while (0)

#define memset threadsafe_memset
#define memcpy threadsafe_memcpy
#define printf threadsafe_printf
#define exit threadsafe_exit

#endif /* __THREADSAFE_LIBC_H */
