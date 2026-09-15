/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                        include c double list source                          **
*********************************************************************************/

#include "clist.h"

int32_t cnode_init(cnode_t *node) {
    if (node == NULL) {
        return -1;
    }
    node->prev = NULL;
    node->next = NULL;
    return 0;
}

int32_t cnode_insert_after(cnode_t *node, cnode_t *new_node) {
    if (node == NULL || new_node == NULL) {
        return -1;
    }

    new_node->next = node->next;
    new_node->prev = node;

    if (node->next != NULL) {
        node->next->prev = new_node;
    }

    node->next = new_node;
    return 0;
}

int32_t clist_init(clist_t *list) {
    if (list == NULL) {
        return -1;
    }

    list->head.prev = &list->head;
    list->head.next = &list->head;
    list->size = 0;
    return 0;
}

int32_t clist_push_front(clist_t *list, cnode_t *node) {
    int32_t code = 0;
    if (list == NULL || node == NULL) {
        return -1;
    }
    // 在头节点（哨兵）之后插入，即链表头部
    code = cnode_insert_after(&list->head, node);
    if (code >= 0) {
        list->size++;
    }

    return code;
}

int32_t clist_push_back(clist_t *list, cnode_t *node) {
    int32_t code = 0;
    if (list == NULL || node == NULL) {
        return -1;
    }
    // 在尾节点（head.prev）之后插入，即链表尾部
    code = cnode_insert_after(list->head.prev, node);
    if (code >= 0) {
        list->size++;
    }

    return code;
}

cnode_t* clist_pop_front(clist_t *list) {
    cnode_t *first_node = NULL;
    if (list == NULL || clist_empty(list)) {
        return NULL;
    }

    first_node = list->head.next;
    clist_erase(list, first_node);

    // 断开节点与链表的联系，但不释放内存，由调用者管理
    first_node->prev = NULL;
    first_node->next = NULL;

    return first_node;
}

cnode_t* clist_pop_back(clist_t *list) {    
    cnode_t *last_node = NULL;
    if (list == NULL || clist_empty(list)) {
        return NULL;
    }

    last_node = list->head.prev;
    clist_erase(list, last_node);

    last_node->prev = NULL;
    last_node->next = NULL;

    return last_node;
}

int32_t clist_insert_after(clist_t *list, cnode_t *node, cnode_t *new_node) {
    int32_t code = 0;
    if (list == NULL || node == NULL || new_node == NULL) {
        return -1;
    }
    // 这里可以添加额外的检查，确保node属于该list，但为了性能通常省略
    code = cnode_insert_after(node, new_node);
    if (code >= 0) {
        list->size++;
    }

    return code;
}

int32_t clist_erase(clist_t *list, cnode_t *node) {
    if (list == NULL || node == NULL) {
        return -1;
    }
    
    // 如果节点不在链表中（prev/next为NULL或指向自己且不是哨兵），则忽略
    // 简单实现假设节点确实在链表中
    if (node->prev == NULL && node->next == NULL) {
         return 0; 
    }

    node->prev->next = node->next;
    node->next->prev = node->prev;
    
    node->prev = NULL;
    node->next = NULL;
    
    list->size--;
    return 0;
}

uint32_t clist_size(clist_t *list) {
    if (list == NULL) {
        return 0;
    }
    return list->size;
}

uint32_t clist_empty(clist_t *list) {
    if (list == NULL) {
        return 1;
    }
    return (list->size == 0) ? 1 : 0;
}
