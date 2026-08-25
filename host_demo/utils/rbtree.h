#ifndef __RBTREE_H
#define __RBTREE_H

#include <stdint.h>
#include <stdbool.h>


#ifndef NULL
#ifdef __cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* 红黑树颜色 */
#define RB_RED     0
#define RB_BLACK   1

struct rb_node
{
    uint8_t rb_color;
    struct rb_node *rb_parent;
    struct rb_node *rb_left;
    struct rb_node *rb_right;
};

struct rb_root
{
    struct rb_node *rb_node;
};

/* 容器偏移宏，类似内核 container_of */
//#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

//#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

/* 初始化根节点 */
#define RB_ROOT (struct rb_root){NULL}
static inline void rb_init_root(struct rb_root *root)
{
    root->rb_node = NULL;
}

/* 初始化树节点，默认红色 */
static inline void rb_init_node(struct rb_node *node)
{
    node->rb_color = RB_RED;
    node->rb_parent = NULL;
    node->rb_left = NULL;
    node->rb_right = NULL;
}

static inline void rb_link_node(struct rb_node *node, struct rb_node *parent, struct rb_node **rb_link)
{
	node->rb_parent = parent;
	node->rb_left   = node->rb_right = NULL;

	*rb_link = node;
}

/* 判断节点颜色 */
static inline bool rb_is_red(struct rb_node *node)
{
    return node && (node->rb_color == RB_RED);
}

static inline bool rb_is_black(struct rb_node *node)
{
    return !node || (node->rb_color == RB_BLACK);
}

static inline void rb_set_red(struct rb_node *node)
{
    node->rb_color = RB_RED;
}

static inline void rb_set_black(struct rb_node *node)
{
    node->rb_color = RB_BLACK;
}

static inline uint8_t rb_color(struct rb_node *node)
{
    return node ? node->rb_color : RB_BLACK;
}

static inline void rb_set_color(struct rb_node *node, uint8_t color)
{
    node->rb_color = color;
}

/* 左旋、右旋、插入修复、删除修复 */
void rb_left_rotate(struct rb_node *node, struct rb_root *root);
void rb_right_rotate(struct rb_node *node, struct rb_root *root);
void rb_insert_color(struct rb_node *node, struct rb_root *root);

void rb_erase(struct rb_node *node, struct rb_root *root);

/* 查找最小、最大节点 */
struct rb_node *rb_first(const struct rb_root *root);
struct rb_node *rb_last(const struct rb_root *root);

/* 中序遍历：前驱、后继 */
struct rb_node *rb_next(const struct rb_node *node);
struct rb_node *rb_prev(const struct rb_node *node);

#ifdef __cplusplus
}
#endif

#endif //__RBTREE_H