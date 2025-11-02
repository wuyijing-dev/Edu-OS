/**
 * Linux风格等待队列实现
 */

#include <wait_queue.h>
#include <process/process.h>
#include <kernel.h>

/* 简单的自旋锁实现 */
static inline void spin_lock(uint32_t *lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* 忙等待 */
    }
}

static inline void spin_unlock(uint32_t *lock)
{
    __sync_lock_release(lock);
}

/**
 * 添加进程到等待队列
 */
void add_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry)
{
    if (!wq || !entry) {
        return;
    }
    
    spin_lock(&wq->lock);
    
    /* 添加到链表头 */
    entry->next = wq->head;
    wq->head = entry;
    
    spin_unlock(&wq->lock);
}

/**
 * 从等待队列移除进程
 */
void remove_wait_queue(struct wait_queue_head *wq, struct wait_queue_entry *entry)
{
    if (!wq || !entry) {
        return;
    }
    
    spin_lock(&wq->lock);
    
    /* 从链表中移除 */
    struct wait_queue_entry **prev = &wq->head;
    struct wait_queue_entry *curr = wq->head;
    
    while (curr) {
        if (curr == entry) {
            *prev = curr->next;
            break;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    
    spin_unlock(&wq->lock);
}

/**
 * 唤醒等待队列中的一个进程
 */
void wake_up(struct wait_queue_head *wq)
{
    if (!wq) {
        return;
    }
    
    spin_lock(&wq->lock);
    
    if (wq->head && wq->head->process) {
        struct process *proc = wq->head->process;
        
        /* 将进程状态从BLOCKED改为READY */
        if (proc->state == PROCESS_STATE_BLOCKED) {
            proc->state = PROCESS_STATE_READY;
            
            /* 添加回调度队列 */
            extern void scheduler_add_process(struct process *proc);
            scheduler_add_process(proc);
        }
    }
    
    spin_unlock(&wq->lock);
}

/**
 * 唤醒等待队列中的所有进程
 */
void wake_up_all(struct wait_queue_head *wq)
{
    if (!wq) {
        return;
    }
    
    spin_lock(&wq->lock);
    
    struct wait_queue_entry *entry = wq->head;
    while (entry) {
        if (entry->process) {
            struct process *proc = entry->process;
            
            /* 将进程状态从BLOCKED改为READY */
            if (proc->state == PROCESS_STATE_BLOCKED) {
                proc->state = PROCESS_STATE_READY;
                
                /* 添加回调度队列 */
                extern void scheduler_add_process(struct process *proc);
                scheduler_add_process(proc);
            }
        }
        entry = entry->next;
    }
    
    spin_unlock(&wq->lock);
}

