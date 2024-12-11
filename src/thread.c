#include "thread.h"
#include "queue.h"
#include "sem.h"
#include "symtable.h"
#include "threadsafe_libc.h"
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <sys/time.h>
// #include <ucontext.h>

#define T Sem_T

#define STACK_SIZE (8L * 1024 * 1024)
#define ESI_OFFSET 4
#define EDI_OFFSET 8
#define EBP_OFFSET 12
#define RIP_OFFSET 16
#define THRSTART_FRAME_SIZE 20

#ifndef MAX_THREADS
     #define MAX_THREADS 8
#endif

#define BYTE_OFFSET_TO_WORD(offset) ((offset) / sizeof(uint32_t))

#define PREEMPT_INTERVAL 100

typedef enum {
    INVALID = 1,    // Ready state
    RUNNING,      // Running state
    WAITING      // Waiting state
} ThreadState;

void _STARTMONITOR() {}
extern void _ENDMONITOR();

extern void _swtch(void *from, void *to);
extern void _thrstart(void);

typedef struct Thread {
    int id;
    ThreadState  status; // (1) Ready (2) Running (3) Waiting (4) Delayed (5) Blocked 
    uint32_t wait_for_ID; // waiting for thread with ID = wait_for_ID

    uint32_t *sp;
    uint32_t *stack;    // used for free();

    int returned_value;

    void *args;
} Thread;

static Thread thread_table [MAX_THREADS];    // ALL THREADS

static Queue_t run_queue = NULL;       /* A queue holding the runnable threads */
static Thread *current_thread = NULL;  /* The currently running thread */
static Thread *pending_free = NULL;    /* A thread that has finished but hasn't been freed yet to allow for switching */
static SymTable_T wait_queues = NULL;  /* A hashmap, Key: the thread being waited on, Value: a queue of threads waiting for the key */
// static SymTable_T thread_table = NULL; /* A hashmap (really a set) holding the currently registered threads */
static Queue_t free_tid_queue = NULL;

static int existing_threads; // num of threads not INVALID
static int waiting_for_zero;

static Thread *select_runnable_thread()
{
    static int last_I=0;
    Thread *sel_thread= NULL;

    for (int i = 0; i < MAX_THREADS; i++)
    {
        if(thread_table[( i + last_I ) % MAX_THREADS].status==RUNNING)
        {
           sel_thread =  &thread_table[( i + last_I) % MAX_THREADS];
           last_I = ( i + last_I + 1) % MAX_THREADS;
           return sel_thread;
        }

    }
    return NULL;
}



/* Return a free ID: Currently just return the last ID+1 */
static int get_new_tid() {
    static int counter = 1;

    assert((queue_count(free_tid_queue) > 0 || counter < INT_MAX) && "Failed to get new tid");

    if (queue_count(free_tid_queue) > 0) {
        return (int)dequeue(free_tid_queue);
    }

    return counter++;
}

static void deallocate_tid(int tid) {
    enqueue(free_tid_queue, (void *)tid);
}

/* Deallocate a thread descriptor. After calling this, `thr` is NULL.  */
static void Thread_destroy(Thread **thr) {
    free((*thr)->args);
    free(*thr);

    *thr = NULL;
}

static void delete_queue_wrapper(const int pcKey, void *pvValue, void *pvExtra) {
    (void)pcKey;
    (void)pvExtra;
    if (pvValue) {
        delete_queue(pvValue);
    }
}

/* Shutdown the threading system. Should be called before exiting  */
static void Thread_shutdown() {
    delete_queue(run_queue);
    delete_queue(free_tid_queue);
    SymTable_map(wait_queues, delete_queue_wrapper, NULL);
    SymTable_free(wait_queues);
    SymTable_free(thread_table);
}

/* Return 1 if the thread `tid` exists, otherwise 0. */
static int Thread_exists(int tid) {
    return SymTable_contains(thread_table, tid);
}

/* Runs every PREEMPT_INTERVAL usecs to switch between threads.
 * Doesn't run if at the time of the timer signal a thread library
 * function is still executing or while executing a threadsafe_libc function.
 *
 * Doesn't reenable the SIGVTALRM signal after exiting; The running
 * thread might change from a different function call. To avoid completely
 * disabling the signal after an timer tick, set option SA_NODEFER */
static void timer_handler(int sig, siginfo_t *info, void *ucontext) {
    threadsafe_assert(sig == SIGVTALRM);
    threadsafe_assert(info->si_signo == SIGVTALRM);

    ucontext_t *context = (ucontext_t *)ucontext;

    int pc = context->uc_mcontext.gregs[REG_EIP];

    if ((int)_STARTMONITOR <= pc && pc <= (int)_ENDMONITOR)
        return;

    if (in_libc_flag)
        return;

    Thread_pause();
}

/* Initialize the preemption timer */
static void set_preemption_timer() {
    struct itimerval tv;
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = PREEMPT_INTERVAL;
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = PREEMPT_INTERVAL;

    setitimer(ITIMER_VIRTUAL, &tv, NULL);

    struct sigaction sa;

    memset(&sa, 0, sizeof(struct sigaction));
    // SIGINFO to use sigaction instead of sighandler, NODEFER to not disable signals after each interrupt
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sa.sa_sigaction = timer_handler;

    sigaction(SIGVTALRM, &sa, NULL);
}

void Thread_init() {
    // run_queue = new_queue();
    // wait_queues = SymTable_new();
    // thread_table = SymTable_new();
    
    for (int i = 0; i < MAX_THREADS; i++)
    {
        thread_table[i].status=INVALID;
        
    }
    
    existing_threads = 1;
    waiting_for_zero = 0;
    // free_tid_queue = new_queue();


    // memset(thread_descriptor->stack, 0, STACK_SIZE);
    thread_table[0].id = get_new_tid();
    thread_table[0].status = RUNNING;
    thread_table[0].args = NULL;

    set_preemption_timer();

    // SymTable_put(thread_table, thread_descriptor->id, NULL);

    current_thread = &thread_table[0];
}

int Thread_new(int func(void *), void *args, size_t nbytes, ...) {

    Thread *thread_descriptor = NULL;

    for (int i = 0; i < MAX_THREADS; i++)
    {
       if(thread_table[i].status==INVALID)
       {
           thread_descriptor = &thread_table[i];
       }
    }
    
    if (!thread_descriptor)
        return -1;

    thread_descriptor->id = get_new_tid();
    thread_descriptor->status = RUNNING; 

    thread_descriptor->args = args;

    thread_descriptor->stack = malloc(STACK_SIZE);

    // Allocate stack frame
    thread_descriptor->sp = &thread_descriptor->stack[BYTE_OFFSET_TO_WORD(STACK_SIZE - THRSTART_FRAME_SIZE)];
    // TODO

    // Save sp in the stack
    // thread_descriptor->sp[0] = (uint32_t)thread_descriptor->sp;

    /* Save address of func to the location that will be restored in esi */
    // thread_descriptor->sp[BYTE_OFFSET_TO_WORD(ESI_OFFSET)] = (uint32_t)func;

    // /* Save address of thread_descriptor->args to the location that will be restored in edi */
    // thread_descriptor->sp[BYTE_OFFSET_TO_WORD(EDI_OFFSET)] = (uint32_t)thread_descriptor->args;

    // /* Save EBP to the stack */
    // thread_descriptor->sp[BYTE_OFFSET_TO_WORD(EBP_OFFSET)] = (uint32_t)&thread_descriptor->sp[BYTE_OFFSET_TO_WORD(THRSTART_FRAME_SIZE) - 1];

    // /* Save address of _thrstart to the location that will be used as return after context switch */
    // thread_descriptor->sp[BYTE_OFFSET_TO_WORD(RIP_OFFSET)] = (uint32_t)_thrstart;


    existing_threads++;

    return thread_descriptor->id;
}

void Thread_exit(int code) {
    if (pending_free) {
        Thread_destroy(&pending_free);
    }

    current_thread->status = INVALID;
    free(current_thread->stack);

    // SymTable_remove(thread_table, current_thread->id);
    // deallocate_tid(current_thread->id);

    // Put all threads waiting for the current thread back into the run queue
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if((thread_table[i].status==WAITING) && (current_thread->id== thread_table[i].wait_for_ID ))
        {
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
            
            for (int i = 0; i < MAX_THREADS; i++)
            {
                if((thread_table[i].status==WAITING) && (thread_table[i].wait_for_ID == 0))
                {
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

    // run_queue should have at least one element, the thread that called Thread_pause itself
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
    
    current_thread->status = WAITING;
    current_thread->wait_for_ID = tid;
    if (tid==0)
    {
        waiting_for_zero++;
    }


    uint32_t **curr_sp = &current_thread->sp;

    current_thread = select_runnable_thread();

    threadsafe_assert(current_thread && "Deadlock detected: No threads in run queue");
    _swtch(curr_sp, &current_thread->sp);

    // When we come back from the switch, the return value will have been placed in our struct
    // Special case: Waiting for tid 0 returns 0.
    if (tid)
        return current_thread->returned_value;
    else
        return 0;
}

void Sem_init(T *s, int count) {
    threadsafe_assert(s && "Semaphore cannot be NULL");
    s->count = count;
    s->queue = new_queue();
}

void Sem_wait(T *s) {
    // While the semaphore's count isn't greater than 0, the current thread blocks
    while (!(s->count > 0)) {
        current_thread->status = WAITING;
        enqueue(s->queue, current_thread);

        uint32_t **curr_sp = &current_thread->sp;

        current_thread = dequeue(run_queue);
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
    if (!queue_isEmpty(s->queue))
        queue_extend(s->queue, run_queue);
}
