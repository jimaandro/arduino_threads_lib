#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int in_libc_flag = 0;

void threadsafe_free(void *ptr) {
    in_libc_flag = 1;
    free(ptr);
    in_libc_flag = 0;
}

void *threadsafe_malloc(size_t __size) {
    in_libc_flag = 1;
    void *ptr = malloc(__size);
    in_libc_flag = 0;

    return ptr;
}

void *threadsafe_calloc(size_t __nmemb, size_t __size) {
    in_libc_flag = 1;
    void *ptr = calloc(__nmemb, __size);
    in_libc_flag = 0;

    return ptr;
}

void *threadsafe_memset(void *__s, int __c, size_t __n) {
    in_libc_flag = 1;
    void *ptr = memset(__s, __c, __n);
    in_libc_flag = 0;

    return ptr;
}

void *threadsafe_memcpy(void *__restrict__ __dest, const void *__restrict__ __src, size_t __n) {
    in_libc_flag = 1;
    void *ptr = memcpy(__dest, __src, __n);
    in_libc_flag = 0;

    return ptr;
}

void threadsafe_exit(int __status) {
    in_libc_flag = 1;
    exit(__status);
    in_libc_flag = 0;
}

int threadsafe_printf(const char *__restrict__ __format, ...) {
    int ret_val;
    va_list args;

    in_libc_flag = 1;

    va_start(args, __format);
    ret_val = vprintf(__format, args);
    va_end(args);

    in_libc_flag = 0;
    return ret_val;
}