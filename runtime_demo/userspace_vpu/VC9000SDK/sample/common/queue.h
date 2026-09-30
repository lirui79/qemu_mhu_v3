#ifndef __QUEUE_H__
#define __QUEUE_H__

/* This queue is not thread safe, caller must ensure it's safe in multi-thread conditions */
struct vmpp_queue;
int vmpp_queue_init(struct vmpp_queue **queue);
int vmpp_queue_push_back(struct vmpp_queue *queue, void *val);
int vmpp_queue_insert_front(struct vmpp_queue *queue, void *val);
int vmpp_queue_insert(struct vmpp_queue *queue, void *val, int index);
void *vmpp_queue_pop_front(struct vmpp_queue *queue);
void *vmpp_queue_pop_tail(struct vmpp_queue *queue);
void *vmpp_queue_peek(struct vmpp_queue *queue, int index);
void *vmpp_queue_get(struct vmpp_queue *queue, int index);
int vmpp_queue_size(struct vmpp_queue *queue);
void vmpp_queue_free(struct vmpp_queue **queue);

#endif