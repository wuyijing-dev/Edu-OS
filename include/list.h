/**
 * list.h - Linux风格的双向循环链表
 * 
 * 这是Linux内核最常用的数据结构之一
 * 特点：侵入式设计，链表节点嵌入到数据结构中
 */

#ifndef _LIST_H
#define _LIST_H

#include <types.h>

/**
 * 链表节点结构
 * 
 * Linux风格：这是一个通用的链表节点，可以嵌入到任何结构体中
 */
struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

/**
 * 静态初始化链表头
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * 定义并初始化链表头
 */
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * 初始化链表头（运行时）
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * 在两个节点之间插入新节点（内部函数）
 */
static inline void __list_add(struct list_head *new,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/**
 * 在指定节点后插入新节点
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/**
 * 在指定节点前插入新节点（添加到链表尾部）
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/**
 * 删除节点（内部函数）
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * 从链表中删除节点
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

/**
 * 删除节点并重新初始化
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/**
 * 替换节点
 */
static inline void list_replace(struct list_head *old, struct list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

/**
 * 移动节点到另一个链表头部
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/**
 * 移动节点到另一个链表尾部
 */
static inline void list_move_tail(struct list_head *list, struct list_head *head)
{
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/**
 * 检查链表是否为空
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * 检查节点是否是链表的最后一个节点
 */
static inline int list_is_last(const struct list_head *list,
                               const struct list_head *head)
{
    return list->next == head;
}

/**
 * 拼接两个链表
 */
static inline void __list_splice(const struct list_head *list,
                                 struct list_head *prev,
                                 struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

/**
 * 将list拼接到head之后
 */
static inline void list_splice(const struct list_head *list,
                               struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

/**
 * 获取包含链表节点的结构体指针（核心宏）
 * 
 * @param ptr:    指向struct list_head的指针
 * @param type:   包含链表节点的结构体类型
 * @param member: 链表节点在结构体中的成员名
 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * 获取链表的第一个元素
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * 获取链表的最后一个元素
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/**
 * 遍历链表
 * 
 * @param pos:  用于遍历的struct list_head *指针
 * @param head: 链表头
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * 安全遍历链表（可以在遍历中删除节点）
 * 
 * @param pos:  用于遍历的struct list_head *指针
 * @param n:    临时存储下一个节点的指针
 * @param head: 链表头
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
         pos = n, n = pos->next)

/**
 * 反向遍历链表
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * 反向安全遍历链表
 */
#define list_for_each_safe_reverse(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
         pos != (head); \
         pos = n, n = pos->prev)

/**
 * 遍历链表中的每个元素（获取包含链表节点的结构体）
 * 
 * @param pos:    用于遍历的结构体指针
 * @param head:   链表头
 * @param member: 链表节点在结构体中的成员名
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * 安全遍历链表中的每个元素
 */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member), \
         n = list_entry(pos->member.next, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * 反向遍历链表中的每个元素
 */
#define list_for_each_entry_reverse(pos, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * 安全反向遍历链表中的每个元素
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member), \
         n = list_entry(pos->member.prev, typeof(*pos), member); \
         &pos->member != (head); \
         pos = n, n = list_entry(n->member.prev, typeof(*n), member))

#endif /* _LIST_H */

