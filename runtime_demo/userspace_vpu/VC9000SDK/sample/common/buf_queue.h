#ifndef __BUF_QUEUE_H_
#define __BUF_QUEUE_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX_BUFFER_COUNT 1024

// #define    LOCK(a)    pthread_mutex_lock(a)
// #define    UNLOCK(a)  pthread_mutex_unlock(a)

typedef struct
{
    uint8_t* buffer;

    uint32_t length;
    uint32_t index;
} buf_t;

typedef struct
{
    buf_t* buf[MAX_BUFFER_COUNT];
    uint32_t write;
    uint32_t read;

    uint32_t max_buffer_size;
    // pthread_mutex_t m_mutex;
} queue_t;

queue_t* init_buffer_queue(uint32_t buf_size);

void free_buffer_queue(queue_t* queue);

buf_t* write_buffer(queue_t* queue);

void move_to_next_write_buffer(queue_t* queue);

buf_t* read_buffer(queue_t* queue);

void move_to_next_read_buffer(queue_t* queue);

void print_queue(queue_t* queue);

#endif
