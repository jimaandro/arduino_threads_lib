#include "thread.h"
#include "DueTimerLib.h"
#include "sem.h"
#include "threadsafe_libc.h"
#include <limits.h>
#include <signal.h>
#include <stdint.h>

#define T Sem_T

#ifndef STACK_SIZE
#define STACK_SIZE (8L * 1024)
#endif

#define R0_OFFSET 0
#define R1_OFFSET 1
#define R2_OFFSET 2
#define LR_OFFSET 13
#define THRSTART_FRAME_SIZE (14 * 4)

#ifndef MAX_THREADS
#define MAX_THREADS 8
#endif

#define BYTE_OFFSET_TO_WORD(offset) ((offset) / sizeof(uint32_t))

#define PREEMPT_INTERVAL 100

typedef enum {
    INVALID,      // This thread is not valid and shouldn't run
    RUNNING,      // Running or able to run
    WAIT_AT_JOIN, // Waiting at Thread_join for some thread(s) to exit
    WAIT_FOR_SEM  // Waiting for a semaphore to be raised
} ThreadState;

void _STARTMONITOR() {}
extern void _ENDMONITOR();

extern void _swtch(void *from, void *to);
extern void _thrstart(void);

typedef struct Thread {
    int id;
    ThreadState status; // (1) Ready (2) Running (3) Waiting (4) Delayed (5) Blocked

    uint32_t wait_for_ID; // waiting for thread with ID = wait_for_ID
    uint32_t waiting_for_sem;

    uint32_t *sp;
    uint32_t *stack; // used for free();

    int returned_value;
} Thread;

static Thread thread_table[MAX_THREADS]; // ALL THREADS

static Thread *current_thread = NULL; /* The currently running thread */
static Thread *pending_free = NULL;   /* A thread that has finished but hasn't been freed yet to allow for switching */

static int existing_threads; // num of threads not INVALID
static int waiting_for_zero;

static Timer_t *timer;

static Thread *select_runnable_thread() {
    static int last_I = 0;
    Thread *sel_thread = NULL;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[(i + last_I) % MAX_THREADS].status == RUNNING) {
            sel_thread = &thread_table[(i + last_I) % MAX_THREADS];
            last_I = (i + last_I + 1) % MAX_THREADS;
            return sel_thread;
        }
    }

    return NULL;
}

/* Return a free ID: Currently just return the last ID+1 */
static int get_new_tid() {
    static int counter = 1;

    return counter++;
}

static int get_new_sid() {
    static int counter = 1;

    return counter++;
}

/* Deallocate a thread descriptor  */
static void Thread_destroy(Thread *thr) {
    free(thr->stack);
    thr->stack = NULL;
}

/* Shutdown the threading system. Should be called before exiting  */
static void Thread_shutdown() {}

/* Return 1 if the thread `tid` exists, otherwise 0. */
static int Thread_exists(int tid) {
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].id == tid && thread_table[i].status == RUNNING) {
            return 1;
        }
    }

    return 0;
}

/* Runs every PREEMPT_INTERVAL usecs to switch between threads.
 * Doesn't run if at the time of the timer signal a thread library
 * function is still executing or while executing a threadsafe_libc function.
 */
static void handler(Context *ctx) {
    if (in_libc_flag)
        return;
    if ((int)_STARTMONITOR <= ctx->return_PC && ctx->return_PC <= (int)_ENDMONITOR)
        return;

    Thread_pause();
}

void Thread_init() {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].status = INVALID;
    }

    waiting_for_zero = 0;

    thread_table[0].id = get_new_tid();
    thread_table[0].status = RUNNING;
    thread_table[0].stack = NULL;
    existing_threads = 1;

    current_thread = &thread_table[0];

    timer = get_available_timer();
    set_timer(timer, 100000, handler);
}

int Thread_new(int func(void *, size_t), void *args, size_t nbytes, ...) {
    Thread *thread_descriptor = NULL;

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].status == INVALID) {
            thread_descriptor = &thread_table[i];

            break;
        }
    }

    if (!thread_descriptor)
        return -1;

    thread_descriptor->id = get_new_tid();
    thread_descriptor->status = RUNNING;
    thread_descriptor->waiting_for_sem = 0;
    ++existing_threads;

    if (!thread_descriptor->stack) {
        thread_descriptor->stack = malloc(STACK_SIZE);
    }
    threadsafe_assert(thread_descriptor->stack && "Cannot allocate stack");

    // Allocate stack frame
    thread_descriptor->sp = &thread_descriptor->stack[BYTE_OFFSET_TO_WORD(STACK_SIZE - THRSTART_FRAME_SIZE)];

    /* Save address of args to the location that will be restored in R0 after context switch */
    thread_descriptor->sp[R0_OFFSET] = (uint32_t)args;

    /* Save nbytes to the location that will be restored in R1 after context switch */
    thread_descriptor->sp[R1_OFFSET] = (uint32_t)nbytes;

    /* Save address of func to the location that will be restored in R2 after context switch */
    thread_descriptor->sp[R2_OFFSET] = ((uint32_t)func) | 1;

    /* Save address of _thrstart to the location that will be used as return after context switch */
    thread_descriptor->sp[LR_OFFSET] = ((uint32_t)_thrstart) | 1;

    return thread_descriptor->id;
}

void Thread_exit(int code) {
    if (pending_free && pending_free->stack && pending_free->status == INVALID) {
        Thread_destroy(pending_free);
        pending_free = NULL;
    }

    current_thread->status = INVALID;
    --existing_threads;

    // Put all threads waiting for the current thread back into the run queue
    for (int i = 0; i < MAX_THREADS; i++) {
        if ((thread_table[i].status == WAIT_AT_JOIN) && (current_thread->id == thread_table[i].wait_for_ID)) {
            thread_table[i].returned_value = code;
            thread_table[i].status = RUNNING;
        }
    }

    Thread *next_thread = select_runnable_thread();

    // If there is no thread to run, either exit or restore the one thread that has called join(0)
    // Otherwise, switch to the next thread
    if (!next_thread) {
        if (existing_threads == 0) {
            Thread_shutdown();
            exit(code);
        } else if (existing_threads == 1) {
            for (int i = 0; i < MAX_THREADS; i++) {
                if ((thread_table[i].status == WAIT_AT_JOIN) && (thread_table[i].wait_for_ID == 0)) {
                    waiting_for_zero--;

                    thread_table[i].returned_value = 0;
                    thread_table[i].status = RUNNING;

                    uint32_t **curr_sp = &current_thread->sp;

                    pending_free = current_thread;
                    current_thread = &thread_table[i];
                    _swtch(curr_sp, &current_thread->sp);
                }
            }
            threadsafe_assert(0 && "Deadlock detected: No threads in run queue, one task remaining, not joining with 0");
        } else {
            threadsafe_assert(0 && "Deadlock detected: No threads in run queue, many threads remaining sleeping");
        }
    } else {
        uint32_t **curr_sp = &current_thread->sp;

        pending_free = current_thread;
        current_thread = next_thread;
        _swtch(curr_sp, &next_thread->sp);
    }
}

int Thread_self() {
    return current_thread->id;
}

void Thread_pause() {
    uint32_t **curr_sp = &current_thread->sp;
    current_thread = select_runnable_thread();

    // Runqueue should have at least one element, the thread that called Thread_pause itself
    threadsafe_assert(current_thread && "Something went REALLY wrong, contact the library developer");

    _swtch(curr_sp, &current_thread->sp);
}

int Thread_join(int tid) {
    threadsafe_assert((tid || Thread_self() != tid) && "Runtime error: A non-zero tid cannot name the calling thread");

    // If tid doesn't exist, return -1
    if (tid && !Thread_exists(tid)) {
        return -1;
    }

    // If tid is 0 and the only existing thread, return 0 immediately
    if (!tid && existing_threads == 1) {
        return 0;
    }

    // If there is such a queue, insert the current queue in that. If tid == 0 and there is already a thread in that queue, raise error
    // Otherwise create a new queue an the insert

    threadsafe_assert((tid || waiting_for_zero == 0) && "Runtime error: Only a single thread can call join(0)");

    current_thread->status = WAIT_AT_JOIN;
    current_thread->wait_for_ID = tid;
    if (tid == 0) {
        waiting_for_zero++;
    }

    uint32_t **curr_sp = &current_thread->sp;

    current_thread = select_runnable_thread();

    threadsafe_assert(current_thread && "Deadlock detected: No threads in run queue");

    _swtch(curr_sp, &current_thread->sp);

    return current_thread->returned_value;
}

void Sem_init(T *s, int count) {
    threadsafe_assert(s && "Semaphore cannot be NULL");
    s->count = count;
    s->id = get_new_sid();
}

void Sem_wait(T *s) {
    // While the semaphore's count isn't greater than 0, the current thread blocks
    while (!(s->count > 0)) {
        current_thread->status = WAIT_FOR_SEM;
        current_thread->waiting_for_sem = s->id;

        uint32_t **curr_sp = &current_thread->sp;
        current_thread = select_runnable_thread();

        threadsafe_assert(current_thread && "Deadlock detected: No threads in run queue");
        current_thread->status = RUNNING;
        _swtch(curr_sp, &current_thread->sp);
    }

    --s->count;
}

void Sem_signal(T *s) {
    threadsafe_assert(s && "Semaphore cannot be NULL");
    ++s->count;

    // Put all threads wait'ing on the semaphore back in the run queue
    for (int i = 0; i < MAX_THREADS; i++) {
        if ((thread_table[i].status == WAIT_FOR_SEM) && (s->id == thread_table[i].waiting_for_sem)) {
            thread_table[i].status = RUNNING;
        }
    }
}
