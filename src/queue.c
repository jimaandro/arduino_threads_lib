/* Development : Nikos Boumakis, 4346
 * Email : csd4346 @csd.uoc.gr */

#include "queue.h"
#include "threadsafe_libc.h"

#define MAX_FREE_LIST 16

struct Queue_node {
    void *elem;
    struct Queue_node *next;
};

struct Queue {
    struct Queue_node *head;
    struct Queue_node *tail;

    struct Queue_node *free_list;
    int free_list_size;

    int count;
};

static struct Queue_node *get_new_node(Queue_t queue) {
    if (queue->free_list) {
        struct Queue_node *node = queue->free_list;
        queue->free_list = queue->free_list->next;
        --queue->free_list_size;

        return node;
    } else {
        return malloc(sizeof(struct Queue_node));
    }
}

static void free_node(Queue_t queue, struct Queue_node *node) {
    if (queue->free_list_size < MAX_FREE_LIST) {
        node->next = queue->free_list;
        queue->free_list = node;

        ++queue->free_list_size;
    } else {
        free(node);
    }
}

/* Creates a new, empty queue. Memory the queue will be allocated dynamically */
Queue_t new_queue() {
    Queue_t queue = malloc(sizeof(struct Queue));
    queue->count = 0;
    queue->head = NULL;
    queue->tail = NULL;

    queue->free_list = NULL;
    queue->free_list_size = 0;

    return queue;
}

/* Completely deletes the queue. After this operation all accesses to the queue will
 * result in undefined behavior. Note that the elements contained in the queue are
 * unaffected. Advised to use this only when the elements are either statically
 * allocated, the queue is empty or the pointers to dynamically allocated elements
 * are held elseware too. */
void delete_queue(Queue_t queue) {
    while (queue->count) {
        dequeue(queue);
    }

    while (queue->free_list) {
        struct Queue_node *head = queue->free_list;
        queue->free_list = queue->free_list->next;
        free(head);
    }

    free(queue);
}

/* Append elem to the end of the queue */
void enqueue(Queue_t queue, void *elem) {
    struct Queue_node *node = get_new_node(queue);
    node->elem = elem;

    node->next = NULL;
    queue->count++;

    if (queue->head == NULL) {
        queue->head = node;
        queue->tail = node;

        return;
    }

    queue->tail->next = node;
    queue->tail = node;
}

/* Remove and return the element at the start of the queue. The queue is shortened
 * and memory might be freed. Returns NULL if the queue was already empty. */
void *dequeue(Queue_t queue) {
    struct Queue_node *tmp = queue->head;
    void *elem;
    if (tmp == NULL) {
        return NULL;
    }

    queue->head = queue->head->next;

    if (queue->head == NULL) {
        queue->tail = NULL;
    }

    elem = tmp->elem;
    free_node(queue, tmp);
    queue->count--;

    return elem;
}

/* Return the element at the start of the queue. The queue is unaffected, i.e.
 * multiple consecutive calls will always return the same element. If the queue is
 * empty, NULL will be returned. */
void *queue_head(Queue_t queue) {
    if (queue->head == NULL) {
        return NULL;
    }

    return queue->head->elem;
}

/* Return the number of elements in the queue, or zero if the queue is empty */
int queue_count(Queue_t queue) { return queue->count; }

/* Return 1 if there is at least one element in the queue, 0 otherwise */
int queue_isEmpty(Queue_t queue) { return queue->count == 0; }

/* Extend queue queue dest with the elements from queue src. After this, src is empty */
void queue_extend(Queue_t src, Queue_t dest) {
    if (queue_isEmpty(src))
        return;

    if (queue_isEmpty(dest)) {
        dest->head = src->head;
        dest->tail = src->tail;
    } else {
        dest->tail->next = src->head;
        dest->tail = src->tail;
    }

    dest->count += src->count;

    src->count = 0;
    src->head = src->tail = NULL;
}