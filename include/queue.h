/* Development : Nikos Boumakis, 4346
 * Email : csd4346 @csd.uoc.gr */

typedef struct Queue *Queue_t;

/* Creates a new, empty queue. Memory the queue will be allocated dynamically */
Queue_t new_queue();

/* Completely deletes the queue. After this operation all accesses to the queue will
 * result in undefined behavior. Note that the elements contained in the queue are
 * unaffected. Advised to use this only when the elements are either statically
 * allocated, the queue is empty or the pointers to dynamically allocated elements
 * are held elseware too. */
void delete_queue(Queue_t queue);

/* Append elem to the end of the queue */
void enqueue(Queue_t queue, void *elem);

/* Remove and return the element at the start of the queue. The queue is shortened
 * and memory is freed. Returns NULL if the queue was already empty. */
void *dequeue(Queue_t queue);

/* Return the element at the start of the queue. The queue is unaffected, i.e.
 * multiple consecutive calls will always return the same element. If the queue is
 * empty, NULL will be returned. */
void *queue_head(Queue_t queue);

/* Return the number of elements in the queue, or zero if the queue is empty */
int queue_count(Queue_t queue);

/* Return 1 if there is at least one element in the queue, 0 otherwise */
int queue_isEmpty(Queue_t queue);

/* Extend queue queue dest with the elements from queue src. After this, src is empty */
void queue_extend(Queue_t src, Queue_t dest);
