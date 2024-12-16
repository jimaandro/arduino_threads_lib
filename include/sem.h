#ifndef SEM_INCLUDED
#define SEM_INCLUDED

#define T Sem_T

typedef struct T { /* opaque! */
    int id;
    int count;
} T;

extern void Sem_init(T *s, int count);
extern void Sem_wait(T *s);
extern void Sem_signal(T *s);

#undef T
#endif
