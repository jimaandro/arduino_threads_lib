#ifndef __DUETIMERLIB_H
#define __DUETIMERLIB_H

#include "Arduino.h"

typedef struct Timer Timer_t;

typedef struct {
    uint32_t R0;
    uint32_t R1;
    uint32_t R2;
    uint32_t R3;
    uint32_t R12;
    uint32_t return_PC;
    uint32_t PSR;
    uint32_t LR;
} Context;

const Timer_t *get_available_timer();
void set_timer(Timer_t *timer, int period_us, void (*handler)(Context *));

#endif /* __DUETIMERLIB_H */