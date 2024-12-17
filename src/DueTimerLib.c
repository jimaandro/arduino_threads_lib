#include "DueTimerLib.h"
#include "Arduino.h"
#include "sam3xa/include/sam3x8e.h"
#include <assert.h>
// Converted to C from ivanseidel's DueTimer library, found at https://github.com/ivanseidel/DueTimer

#ifdef USING_SERVO_LIB
#warning "HEY! You have set flag USING_SERVO_LIB. Timer0, 2,3,4 and 5 are not available"
#endif

#if defined TC2
#define NUM_TIMERS 9
#else
#define NUM_TIMERS 6
#endif

struct Timer {
    Tc *tc;
    uint32_t channel;
    IRQn_Type irq;
};

static const Timer_t Timers[NUM_TIMERS] = {
    {TC0, 0, TC0_IRQn},
    {TC0, 1, TC1_IRQn},
    {TC0, 2, TC2_IRQn},
    {TC1, 0, TC3_IRQn},
    {TC1, 1, TC4_IRQn},
    {TC1, 2, TC5_IRQn},
#if NUM_TIMERS > 6
    {TC2, 0, TC6_IRQn},
    {TC2, 1, TC7_IRQn},
    {TC2, 2, TC8_IRQn},
#endif
};

static void (*callbacks[NUM_TIMERS])(Context *);

int is_available(IRQn_Type irq) {
    return !(NVIC->ISER[(uint32_t)((int32_t)irq) >> 5] & (uint32_t)(1 << ((uint32_t)((int32_t)irq) & (uint32_t)0x1F)));
}

const Timer_t *get_available_timer() {
    for (int i = 0; i < NUM_TIMERS; ++i) {
        if (is_available(Timers[i].irq)) {
            return &Timers[i];
        }
    }

    return NULL;
}

uint8_t best_clock(double frequency, uint32_t *retRC) {
    /*
        Pick the best Clock, thanks to Ogle Basil Hall!

        Timer		Definition
        TIMER_CLOCK1	MCK /  2
        TIMER_CLOCK2	MCK /  8
        TIMER_CLOCK3	MCK / 32
        TIMER_CLOCK4	MCK /128
    */
    const struct {
        uint8_t flag;
        uint8_t divisor;
    } clockConfig[] = {
        {TC_CMR_TCCLKS_TIMER_CLOCK1, 2},
        {TC_CMR_TCCLKS_TIMER_CLOCK2, 8},
        {TC_CMR_TCCLKS_TIMER_CLOCK3, 32},
        {TC_CMR_TCCLKS_TIMER_CLOCK4, 128}};
    float ticks;
    float error;
    int clkId = 3;
    int bestClock = 3;
    float bestError = 9.999e99;
    do {
        ticks = (float)SystemCoreClock / frequency / (float)clockConfig[clkId].divisor;
        // error = abs(ticks - round(ticks));
        error = clockConfig[clkId].divisor * abs(ticks - round(ticks)); // Error comparison needs scaling
        if (error < bestError) {
            bestClock = clkId;
            bestError = error;
        }
    } while (clkId-- > 0);
    ticks = (float)SystemCoreClock / frequency / (float)clockConfig[bestClock].divisor;
    *retRC = (uint32_t)round(ticks);
    return clockConfig[bestClock].flag;
}

void set_frequency(Timer_t *timer, float freq) {
    assert(timer);
    pmc_set_writeprotect(false);

    // Enable clock for the timer
    pmc_enable_periph_clk((uint32_t)timer->irq);

    uint32_t rc = 0;

    // Find the best clock for the wanted frequency
    uint8_t clock = best_clock(freq, &rc);

    // Set up the Timer in waveform mode which creates a PWM
    // in UP mode with automatic trigger on RC Compare
    // and sets it up with the determined internal clock as clock input.
    TC_Configure(timer->tc, timer->channel, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | clock);
    // Reset counter and fire interrupt when RC value is matched:
    TC_SetRC(timer->tc, timer->channel, rc);
    // Enable the RC Compare Interrupt...
    timer->tc->TC_CHANNEL[timer->channel].TC_IER = TC_IER_CPCS;
    // ... and disable all others.
    timer->tc->TC_CHANNEL[timer->channel].TC_IDR = ~TC_IER_CPCS;
}

void set_timer(Timer_t *timer, int period_us, void (*handler)(Context *)) {
    assert(timer);

    int timer_index = timer - Timers;
    set_frequency(timer, 1000000.0 / period_us);

    NVIC_ClearPendingIRQ(timer->irq);
    NVIC_EnableIRQ(timer->irq);

    TC_Start(timer->tc, timer->channel);

    callbacks[timer_index] = handler;
}

extern void (*handler)(Context *);

#ifndef USING_SERVO_LIB
void TC0_Handler(void) {
    TC_GetStatus(TC0, 0);

    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[0](&ctx);
}
#endif
void TC1_Handler(void) {
    TC_GetStatus(TC0, 1);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[1](&ctx);
}

// Fix for compatibility with Servo library
#ifndef USING_SERVO_LIB
void TC2_Handler(void) {
    TC_GetStatus(TC0, 2);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[2](&ctx);
}
void TC3_Handler(void) {
    TC_GetStatus(TC1, 0);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[3](&ctx);
}
void TC4_Handler(void) {
    TC_GetStatus(TC1, 1);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[4](&ctx);
}
void TC5_Handler(void) {
    TC_GetStatus(TC1, 2);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[5](&ctx);
}
#endif

#if NUM_TIMERS > 6
void TC6_Handler(void) {
    TC_GetStatus(TC2, 0);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[6](&ctx);
}
void TC7_Handler(void) {
    TC_GetStatus(TC2, 1);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[7](&ctx);
}
void TC8_Handler(void) {
    TC_GetStatus(TC2, 2);
    Context ctx;

    asm volatile(
        "ldr %0, [sp, #56]\n"
        "ldr %1, [sp, #60]\n"
        "ldr %2, [sp, #64]\n"
        "ldr %3, [sp, #68]\n"
        "ldr %4, [sp, #72]\n"
        "ldr %5, [sp, #76]\n"
        "ldr %6, [sp, #80]\n"
        "ldr %7, [sp, #84]\n"
        : "=r"(ctx.R0),
          "=r"(ctx.R1),
          "=r"(ctx.R2),
          "=r"(ctx.R3),
          "=r"(ctx.R12),
          "=r"(ctx.return_PC),
          "=r"(ctx.PSR),
          "=r"(ctx.LR)::"memory");

    callbacks[8](&ctx);
}
#endif
