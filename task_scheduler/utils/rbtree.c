#include "rbtree.h"

static inline void __rb_change_child(struct rb_root *root,
                                     struct rb_node *old, struct rb_node *new,
                                     struct rb_node *parent)
{
    if (!parent)
    {
        root->rb_node = new;
    }
    else if (parent->rb_left == old)
    {
        parent->rb_left = new;
    }
    else
    {
        parent->rb_right = new;
    }

    if (new)
        new->rb_parent = parent;
}

/* 左旋 */
void rb_left_rotate(struct rb_node *x, struct rb_root *root)
{
    struct rb_node *y = x->rb_right;
    x->rb_right = y->rb_left;

    if (y->rb_left)
        y->rb_left->rb_parent = x;

    y->rb_parent = x->rb_parent;
    __rb_change_child(root, x, y, x->rb_parent);

    y->rb_left = x;
    x->rb_parent = y;
}

/* 右旋 */
void rb_right_rotate(struct rb_node *y, struct rb_root *root)
{
    struct rb_node *x = y->rb_left;
    y->rb_left = x->rb_right;

    if (x->rb_right)
        x->rb_right->rb_parent = y;

    x->rb_parent = y->rb_parent;
    __rb_change_child(root, y, x, y->rb_parent);

    x->rb_right = y;
    y->rb_parent = x;
}

/* 插入后平衡修复 */
void rb_insert_color(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *parent, *gparent;

    while ((parent = node->rb_parent) && rb_is_red(parent))
    {
        gparent = parent->rb_parent;

        if (parent == gparent->rb_left)
        {
            struct rb_node *uncle = gparent->rb_right;
            if (rb_is_red(uncle))
            {
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;
                continue;
            }

            if (node == parent->rb_right)
            {
                node = parent;
                rb_left_rotate(node, root);
            }
            rb_set_black(parent);
            rb_set_red(gparent);
            rb_right_rotate(gparent, root);
        }
        else
        {
            struct rb_node *uncle = gparent->rb_left;
            if (rb_is_red(uncle))
            {
                rb_set_black(parent);
                rb_set_black(uncle);
                rb_set_red(gparent);
                node = gparent;
                continue;
            }

            if (node == parent->rb_left)
            {
                node = parent;
                rb_right_rotate(node, root);
            }
            rb_set_black(parent);
            rb_set_red(gparent);
            rb_left_rotate(gparent, root);
        }
    }

    rb_set_black(root->rb_node);
}

static struct rb_node *__rb_erase_color(struct rb_node *node,
                                       struct rb_node *parent,
                                       struct rb_root *root)
{
    struct rb_node *sibling;

    while ((!node || rb_is_black(node)) && node != root->rb_node)
    {
        if (parent->rb_left == node)
        {
            sibling = parent->rb_right;
            if (rb_is_red(sibling))
            {
                rb_set_black(sibling);
                rb_set_red(parent);
                rb_left_rotate(parent, root);
                sibling = parent->rb_right;
            }

            if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                (!sibling->rb_right || rb_is_black(sibling->rb_right)))
            {
                rb_set_red(sibling);
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if (!sibling->rb_right || rb_is_black(sibling->rb_right))
                {
                    rb_set_black(sibling->rb_left);
                    rb_set_red(sibling);
                    rb_right_rotate(sibling, root);
                    sibling = parent->rb_right;
                }
                rb_set_color(sibling, rb_color(parent));
                rb_set_black(parent);
                rb_set_black(sibling->rb_right);
                rb_left_rotate(parent, root);
                node = root->rb_node;
            }
        }
        else
        {
            sibling = parent->rb_left;
            if (rb_is_red(sibling))
            {
                rb_set_black(sibling);
                rb_set_red(parent);
                rb_right_rotate(parent, root);
                sibling = parent->rb_left;
            }

            if ((!sibling->rb_left || rb_is_black(sibling->rb_left)) &&
                (!sibling->rb_right || rb_is_black(sibling->rb_right)))
            {
                rb_set_red(sibling);
                node = parent;
                parent = node->rb_parent;
            }
            else
            {
                if (!sibling->rb_left || rb_is_black(sibling->rb_left))
                {
                    rb_set_black(sibling->rb_right);
                    rb_set_red(sibling);
                    rb_left_rotate(sibling, root);
                    sibling = parent->rb_left;
                }
                rb_set_color(sibling, rb_color(parent));
                rb_set_black(parent);
                rb_set_black(sibling->rb_left);
                rb_right_rotate(parent, root);
                node = root->rb_node;
            }
        }
    }
    if (node)
        rb_set_black(node);
    return node;
}

/* 删除节点 */
void rb_erase(struct rb_node *node, struct rb_root *root)
{
    struct rb_node *rebalance;
    struct rb_node *child, *parent;
    uint8_t old_color = node->rb_color;

    if (!node->rb_left)
    {
        child = node->rb_right;
        parent = node->rb_parent;
        __rb_change_child(root, node, child, parent);
    }
    else if (!node->rb_right)
    {
        child = node->rb_left;
        parent = node->rb_parent;
        __rb_change_child(root, node, child, parent);
    }
    else
    {
        struct rb_node *successor = node->rb_right;
        while (successor->rb_left)
            successor = successor->rb_left;

        child = successor->rb_right;
        parent = successor->rb_parent;
        old_color = successor->rb_color;

        if (parent == node)
        {
            if (child)
                child->rb_parent = successor;
        }
        else
        {
            __rb_change_child(root, successor, child, parent);
            successor->rb_right = node->rb_right;
            node->rb_right->rb_parent = successor;
        }

        __rb_change_child(root, node, successor, node->rb_parent);
        successor->rb_left = node->rb_left;
        node->rb_left->rb_parent = successor;
        rb_set_color(successor, node->rb_color);
    }

    if (old_color == RB_BLACK)
        __rb_erase_color(child, parent, root);
}

/* 最小节点（最左） */
struct rb_node *rb_first(const struct rb_root *root)
{
    struct rb_node *n = root->rb_node;
    if (!n)
        return NULL;
    while (n->rb_left)
        n = n->rb_left;
    return n;
}

/* 最大节点（最右） */
struct rb_node *rb_last(const struct rb_root *root)
{
    struct rb_node *n = root->rb_node;
    if (!n)
        return NULL;
    while (n->rb_right)
        n = n->rb_right;
    return n;
}

/* 中序后继 */
struct rb_node *rb_next(const struct rb_node *node)
{
    struct rb_node *parent;
    if (node->rb_right)
    {
        node = node->rb_right;
        while (node->rb_left)
            node = node->rb_left;
        return (struct rb_node *)node;
    }

    while ((parent = node->rb_parent) && node == parent->rb_right)
        node = parent;
    return parent;
}

/* 中序前驱 */
struct rb_node *rb_prev(const struct rb_node *node)
{
    struct rb_node *parent;
    if (node->rb_left)
    {
        node = node->rb_left;
        while (node->rb_right)
            node = node->rb_right;
        return (struct rb_node *)node;
    }

    while ((parent = node->rb_parent) && node == parent->rb_left)
        node = parent;
    return parent;
}