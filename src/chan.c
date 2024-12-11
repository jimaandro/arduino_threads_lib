#include "chan.h"
#include "sem.h"
#include "threadsafe_libc.h"

#define T Chan_T
struct T {                 /* channels: */
    void *ptr;             /* message address */
    size_t *size;          /* pointer to the message size */
    Sem_T send, rec, sync; /* associated semaphores */
};

T Chan_new(void) {
    T c = calloc(1, sizeof *c);

    if (c) {
        Sem_init(&c->send, 1);
        Sem_init(&c->rec, 0);
        Sem_init(&c->sync, 0);
    }
    return c;
}

size_t Chan_send(Chan_T c, void *ptr, size_t size) {
    threadsafe_assert(c);
    threadsafe_assert(ptr);
    Sem_wait(&c->send);
    c->ptr = ptr;
    c->size = &size;
    Sem_signal(&c->rec);
    Sem_wait(&c->sync);
    return size;
}

size_t Chan_receive(Chan_T c, void *ptr, size_t size) {
    size_t n;
    long *src;

    threadsafe_assert(c);
    threadsafe_assert(ptr);
    Sem_wait(&c->rec);
    n = *c->size;
    if (size < n)
        n = size;
    *c->size = n;
    if (n > 0)
        memcpy(ptr, c->ptr, n);
    Sem_signal(&c->sync);
    Sem_signal(&c->send);
    return n;
}
