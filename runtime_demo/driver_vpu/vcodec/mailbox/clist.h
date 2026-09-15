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
**                        include c double list header                          **
*********************************************************************************/

#ifndef _C_DOUBLE_LIST_H_
#define _C_DOUBLE_LIST_H_

#include "inc.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct cnode_t {
    struct cnode_t    *prev;
    struct cnode_t    *next;
} cnode_t;

typedef struct clist_t {
    cnode_t           head; // 哨兵节点，head.next指向第一个元素，head.prev指向最后一个元素
    uint32_t          size;
} clist_t;

/**
 * @brief 初始化一个节点
 * @param node 待初始化的节点指针
 * @param data 节点携带的数据指针
 * @return 0 成功, -1 失败
 */
int32_t    cnode_init(cnode_t *node);

/**
 * @brief 在指定节点之后插入新节点
 * @param node 目标节点
 * @param new_node 待插入的新节点
 * @return 0 成功, -1 失败
 */
int32_t    cnode_insert_after(cnode_t *node, cnode_t *new_node);

/**
 * @brief 初始化链表
 * @param list 待初始化的链表指针
 * @return 0 成功, -1 失败
 */
int32_t    clist_init(clist_t *list);

/**
 * @brief 从链表头部插入节点
 * @param list 链表指针
 * @param node 待插入节点
 * @return 0 成功, -1 失败
 */
int32_t    clist_push_front(clist_t *list, cnode_t *node);

/**
 * @brief 从链表尾部插入节点
 * @param list 链表指针
 * @param node 待插入节点
 * @return 0 成功, -1 失败
 */
int32_t    clist_push_back(clist_t *list, cnode_t *node);

/**
 * @brief 从链表头部弹出节点
 * @param list 链表指针
 * @return 弹出的节点指针，若链表为空则返回NULL
 */
cnode_t*   clist_pop_front(clist_t *list);

/**
 * @brief 从链表尾部弹出节点
 * @param list 链表指针
 * @return 弹出的节点指针，若链表为空则返回NULL
 */
cnode_t*   clist_pop_back(clist_t *list);

/**
 * @brief 在链表中指定节点后插入新节点
 * @param list 链表指针
 * @param node 目标节点
 * @param new_node 待插入节点
 * @return 0 成功, -1 失败
 */
int32_t    clist_insert_after(clist_t *list, cnode_t *node, cnode_t *new_node);

/**
 * @brief 从链表中删除指定节点
 * @param list 链表指针
 * @param node 待删除节点
 * @return 0 成功, -1 失败
 */
int32_t    clist_erase(clist_t *list, cnode_t *node);

/**
 * @brief 获取链表大小
 * @param list 链表指针
 * @return 链表节点数量
 */
uint32_t   clist_size(clist_t *list);

/**
 * @brief 判断链表是否为空
 * @param list 链表指针
 * @return 1 为空, 0 非空
 */
uint32_t   clist_empty(clist_t *list);


#ifdef __cplusplus
}
#endif

#endif /*_C_DOUBLE_LIST_H_*/
