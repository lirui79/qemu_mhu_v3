#include "queue.h"
#include "log.h"
#include <stdlib.h>

struct queue_node {
    void *val;
    struct queue_node *next;
};

struct queue_root {
    struct queue_node *node;
    volatile int size;
};

struct vmpp_queue {
    struct queue_root *worker_queue;
    struct queue_root *idle_queue;
};

static inline int init_queue(struct queue_root **root)
{
    *root = (struct queue_root *)malloc(sizeof(struct queue_root));
    if (*root == NULL) {
        LOG_ERROR("Fail to malloc queue.");
        return -1;
    }
    (*root)->node = NULL;
    (*root)->size = 0;
    return 0;
}

static inline int queue_push_back(struct queue_root *root, struct queue_node *node)
{
    if (!root || !node) {
        return -1;
    }

    struct queue_node *tmp = root->node;
    if (!tmp) {
        /* first node */
        root->node = node;
    } else {
        while (tmp->next) {
            tmp = tmp->next;
        }
        /* append to the tail */
        tmp->next = node;
    }
    node->next = NULL;
    ++root->size;
    return root->size;
}

static inline int queue_insert(struct queue_root *root, struct queue_node *node, int index)
{
    if (!root || !node || index < 0 || index > root->size) {
        return -1;
    }

    struct queue_node *tmp = root->node;
    struct queue_node *prev = NULL;
    for (int i = 0; i < index; i++) {
        prev = tmp;
        tmp = tmp->next;
    }
    if (prev) {
        prev->next = node;
    } else {
        root->node = node;
    }
    node->next = tmp;

    ++root->size;
    return root->size;
}

static inline void *queue_pop_front(struct queue_root *root)
{
    if (!root || !root->size) {
        return NULL;
    }

    struct queue_node *tmp = NULL;
    void *val = NULL;
    do {
        tmp = root->node;
        root->node = tmp->next;
        val = tmp->val;
        free(tmp);
    } while (0);
    --root->size;
    return val;
}

static inline struct queue_node *queue_pop_front_node(struct queue_root *root)
{
    if (!root || !root->size) {
        return NULL;
    }

    struct queue_node *tmp = NULL;
    do {
        tmp = root->node;
        root->node = tmp->next;
    } while (0);
    --root->size;
    return tmp;
}

static inline struct queue_node *queue_pop_tail_node(struct queue_root *root)
{
    if (!root || !root->size) {
        return NULL;
    }

    struct queue_node *tmp = root->node;
    struct queue_node *prev = NULL;
    while (tmp->next) {
        prev = tmp;
        tmp = tmp->next;
    }
    if (prev) {
        prev->next = NULL;
    } else {
        // empty
        root->node = NULL;
    }

    --root->size;
    return tmp;
}

static inline void *queue_peek(struct queue_root *root, int index)
{
    if (!root || index < 0 || index >= root->size) {
        return NULL;
    }

    struct queue_node *tmp = root->node;
    for (int i = 0; i < index; i++) {
        tmp = tmp->next;
    }
    return tmp->val;
}

static inline struct queue_node *queue_get(struct queue_root *root, int index)
{
    if (!root || index < 0 || index >= root->size) {
        LOG_ERROR("[queue] invalid parameter.");
        return NULL;
    }

    struct queue_node *tmp = root->node;
    struct queue_node *prev = NULL;
    for (int i = 0; i < index; i++) {
        prev = tmp;
        tmp = tmp->next;
    }
    if (prev) {
        prev->next = tmp->next;
    } else {
        root->node = tmp->next;
    }
    --root->size;
    return tmp;
}

static inline void free_queue(struct queue_root **root)
{
    if (root && *root) {
        do {
            queue_pop_front(*root);
        } while ((*root)->size);
        free(*root);
        *root = NULL;
    }
}

int vmpp_queue_init(struct vmpp_queue **queue)
{
    if (!queue) {
        return -1;
    }

    struct vmpp_queue *tmp = (struct vmpp_queue *)malloc(sizeof(struct vmpp_queue));
    if (!tmp) {
        LOG_ERROR("Fail to alloc a queue");
        *queue = NULL;
        return -1;
    }

    memset(tmp, 0, sizeof(struct vmpp_queue));
    if (init_queue(&tmp->idle_queue) < 0) {
        LOG_ERROR("Fail to init idle queue");
        goto fail;
    }

    if (init_queue(&tmp->worker_queue) < 0) {
        LOG_ERROR("Fail to init worker queue");
        goto fail;
    }
    *queue = tmp;
    return 0;
fail:
    if (tmp->idle_queue) {
        free_queue(&tmp->idle_queue);
    }
    if (tmp->worker_queue) {
        free_queue(&tmp->worker_queue);
    }
    *queue = NULL;
    return -1;
}

int vmpp_queue_push_back(struct vmpp_queue *queue, void *val)
{
    if (!queue || !val) {
        return -1;
    }
    struct queue_node *node = NULL;
    if (queue->idle_queue->size) {
        node = queue_pop_front_node(queue->idle_queue);
    } else {
        node = (struct queue_node *)malloc(sizeof(struct queue_node));
    }
    if (node == NULL) {
        LOG_ERROR("Fail to malloc new node.");
        return -1;
    }
    node->val = val;
    node->next = NULL;
    return queue_push_back(queue->worker_queue, node);
}

int vmpp_queue_insert_front(struct vmpp_queue *queue, void *val)
{
    if (!queue || !val) {
        return -1;
    }

    return vmpp_queue_insert(queue, val, 0);
}

int vmpp_queue_insert(struct vmpp_queue *queue, void *val, int index)
{
    if (!queue || !val || index > vmpp_queue_size(queue)) {
        return -1;
    }

    struct queue_node *node = NULL;
    if (queue->idle_queue->size) {
        node = queue_pop_front_node(queue->idle_queue);
    } else {
        node = (struct queue_node *)malloc(sizeof(struct queue_node));
    }
    if (node == NULL) {
        LOG_ERROR("Fail to malloc new node.");
        return -1;
    }
    node->val = val;
    node->next = NULL;
    return queue_insert(queue->worker_queue, node, index);
}

void *vmpp_queue_pop_front(struct vmpp_queue *queue)
{
    if (!queue || !queue->worker_queue->size) {
        return NULL;
    }

    void *val = NULL;
    struct queue_node *node = queue_pop_front_node(queue->worker_queue);
    if (node) {
        val = node->val;
        queue_push_back(queue->idle_queue, node);
    }
    return val;
}

void *vmpp_queue_pop_tail(struct vmpp_queue *queue)
{
    if (!queue || !queue->worker_queue->size) {
        return NULL;
    }

    void *val = NULL;
    struct queue_node *node = queue_pop_tail_node(queue->worker_queue);
    if (node) {
        val = node->val;
        queue_push_back(queue->idle_queue, node);
    }
    return val;
}

void *vmpp_queue_peek(struct vmpp_queue *queue, int index)
{
    if (!queue) {
        return NULL;
    }
    return queue_peek(queue->worker_queue, index);
}

void *vmpp_queue_get(struct vmpp_queue *queue, int index)
{
    if (!queue || index >= vmpp_queue_size(queue)) {
        return NULL;
    }
    void *val = NULL;
    struct queue_node *node = queue_get(queue->worker_queue, index);
    if (node) {
        val = node->val;
        queue_push_back(queue->idle_queue, node);
    }
    return val;
}

int vmpp_queue_size(struct vmpp_queue *queue)
{
    if (!queue || !queue->worker_queue) {
        return -1;
    }

    return queue->worker_queue->size;
}

void vmpp_queue_free(struct vmpp_queue **queue)
{
    if (queue && *queue) {
        struct vmpp_queue *tmp = *queue;
        free_queue(&tmp->worker_queue);
        free_queue(&tmp->idle_queue);
        free(tmp);
        *queue = NULL;
    }
}
