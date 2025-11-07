/**
 * Linux风格等待队列
 * 用于select/poll等待事件
 */

#ifndef _WAIT_QUEUE_H
#define _WAIT_QUEUE_H

#include <types.h>

/* 前向声明 */
struct process;

/**
 * 等待队列条目
 */
struct wait_queue_entry {
    struct process *process;            /* 等待的进程 */
    struct wait_queue_entry *next;      /* 链表下一个节点 */
};

/**
 * 等待队列头
 */
struct wait_queue_head {
    struct wait_queue_entry *head;      /* 等待队列链表头 */
    uint32_t lock;                      /* 自旋锁保护 */
};

/* 类型别名（Linux风格）*/
typedef struct wait_queue_head wait_queue_head_t;

/**
 * 初始化等待队列头
 */
static inline void init_waitqueue_head(struct wait_queue_head *wq)
{
    wq->head = NULL;
    wq->lock = 0;
}

/**
 * 添加进程到等待队列
 */
void add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry);

/**
 * 从等待队列移除进程
 */
void remove_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry);

/**
 * 唤醒等待队列中的一个进程
 */
void wake_up(struct wait_queue_head *wq);

/**
 * 唤醒等待队列中的所有进程
 */
void wake_up_all(struct wait_queue_head *wq);

#endif /* _WAIT_QUEUE_H */

