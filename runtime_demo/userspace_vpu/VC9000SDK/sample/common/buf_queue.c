#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buf_queue.h"

queue_t* init_buffer_queue(uint32_t buf_size)
{
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));

    for (uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
        buf_t* buf = (buf_t*)malloc(sizeof(buf_t));
        buf->buffer = (uint8_t*)malloc(buf_size);
        buf->index = i;
        buf->length = 0;
        queue->buf[i] = buf;
    }

    queue->max_buffer_size = buf_size;
    queue->write = 0;
    queue->read = 0;

    // pthread_mutex_init(&queue->m_mutex, NULL);

    return queue;
}

void free_buffer_queue(queue_t* queue)
{
    if (!queue) {
        return;
    }

     for (uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
        free(queue->buf[i]->buffer);
        free(queue->buf[i]);
    }
    free(queue);
}

buf_t* write_buffer(queue_t* queue)
{
    if (!queue) {
        return NULL;
    }

    // printf("write_buffer, queue->write %d, queue->read %d.\n", queue->write, queue->read);
    if ((queue->write + 1) % MAX_BUFFER_COUNT == queue->read) {  /* queue is full. */
        // printf("write_buffer, queue is full, need write read thread to read.\n");
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
        if (queue->buf[i]->index == (queue->write % MAX_BUFFER_COUNT)) {
            //queue->write++;
            return queue->buf[i];
        }
    }

    return NULL;
}

void move_to_next_write_buffer(queue_t* queue) {
    if (!queue) {
        return;
    }
    queue->write = (queue->write + 1) % MAX_BUFFER_COUNT;
}

buf_t* read_buffer(queue_t* queue)
{
    if (!queue) {
        return NULL;
    }

    // printf("read_buffer, queue->read %d, queue->write %d.\n", queue->read, queue->write);
    if (queue->read == queue->write) {/* queue is empty. */
        // printf("read_buffer, read is equals to write, should return null.\n");
        return NULL;
    }

    for (uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
        if (queue->buf[i]->index == (queue->read % MAX_BUFFER_COUNT)) {
            if (queue->buf[i]->length == 0) {
                return NULL;
            }
            //queue->read++;
            return queue->buf[i];
        }
    }

    return NULL;
}

void move_to_next_read_buffer(queue_t* queue) {
    if (!queue) {
        return;
    }
    queue->read = (queue->read + 1) % MAX_BUFFER_COUNT;
}

void print_queue(queue_t* queue)
{
    if (!queue) {
        return;
    }

    for (uint32_t i = 0; i < MAX_BUFFER_COUNT; i++) {
        printf("queue->buf[%d] index %d.\n", i, queue->buf[i]->index);
    }
}