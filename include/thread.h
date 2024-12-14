#ifndef THREAD_INCLUDED
#define THREAD_INCLUDED

#include <stddef.h>

extern void Thread_init(void);
extern int Thread_new(int func(void *, size_t), void *args, size_t nbytes, ...);
extern void Thread_exit(int code);
extern int Thread_self(void);
extern int Thread_join(int tid);
extern void Thread_pause(void);

#endif
