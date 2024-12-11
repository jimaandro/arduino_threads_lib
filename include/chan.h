#ifndef CHAN_INCLUDED
#define CHAN_INCLUDED

#include <stddef.h>

#define T Chan_T
typedef struct T *T;

extern T Chan_new(void);
extern size_t Chan_send(T c, void *ptr, size_t size);
extern size_t Chan_receive(T c, void *ptr, size_t size);

#undef T
#endif
