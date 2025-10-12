/*
 * mutex.c - 互斥锁实现
 * 
 * 支持优先级继承，防止优先级反转
 */

#include <sync/mutex.h>
#include <process/process.h>
#include <process/scheduler.h>
#include <kernel.h>
#include <string.h>

/*
 * 原子性测试并设置（Test-And-Set）
 */
static inline int atomic_test_and_set(volatile uint32_t *lock)
{
    uint32_t result;
    asm volatile(
        "lock xchg %0, %1"
        : "=r"(result), "+m"(*lock)
        : "0"(1)
        : "memory"
    );
    return result;
}

/*
 * 原子性清除
 */
static inline void atomic_clear(volatile uint32_t *lock)
{
    asm volatile(
        "movl $0, %0"
        : "=m"(*lock)
        :
        : "memory"
    );
}

/*
 * 初始化互斥锁
 */
void mutex_init(struct mutex *m, const char *name)
{
    if (!m) return;
    
    m->lock = MUTEX_UNLOCKED;
    m->owner = NULL;
    m->wait_list = NULL;
    m->recursion_count = 0;
    m->name = name ? name : "unnamed";
}

/*
 * 销毁互斥锁
 */
void mutex_destroy(struct mutex *m)
{
    if (!m) return;
    
    if (m->lock == MUTEX_LOCKED) {
        kprintf("[MUTEX] Warning: Destroying locked mutex '%s'\n", m->name);
    }
    
    m->lock = MUTEX_UNLOCKED;
    m->owner = NULL;
    m->wait_list = NULL;
}

/*
 * 获取互斥锁（阻塞）
 */
void mutex_lock(struct mutex *m)
{
    if (!m) return;
    
    struct process *current = process_get_current();
    
    /* 禁用中断（临界区） */
    asm volatile("cli");
    
    /* 尝试获取锁 */
    while (atomic_test_and_set(&m->lock) != 0) {
        /* 锁已被占用 */
        
        /* 优先级继承：如果持有者优先级低，临时提升 */
        if (m->owner && current) {
            if (m->owner->priority > current->priority) {
                /* 持有者优先级低，提升到当前进程优先级 */
                kprintf("[MUTEX] Priority inheritance: %s (%d) -> %d\n",
                        m->owner->name, m->owner->priority, current->priority);
                m->owner->priority = current->priority;
            }
        }
        
        /* 加入等待队列 */
        if (current) {
            current->next_waiting = m->wait_list;
            m->wait_list = current;
        }
        
        /* 重新启用中断 */
        asm volatile("sti");
        
        /* 让出 CPU，等待锁释放 */
        /* 简化版：忙等待 */
        asm volatile("pause");  // CPU 休息一下
        
        /* 禁用中断继续尝试 */
        asm volatile("cli");
    }
    
    /* 成功获取锁 */
    m->owner = current;
    m->recursion_count = 1;
    
    /* 重新启用中断 */
    asm volatile("sti");
}

/*
 * 尝试获取互斥锁（非阻塞）
 */
int mutex_trylock(struct mutex *m)
{
    if (!m) return -1;
    
    /* 禁用中断 */
    asm volatile("cli");
    
    /* 尝试获取 */
    if (atomic_test_and_set(&m->lock) == 0) {
        /* 成功 */
        m->owner = process_get_current();
        m->recursion_count = 1;
        asm volatile("sti");
        return 0;
    }
    
    /* 失败 */
    asm volatile("sti");
    return -1;
}

/*
 * 释放互斥锁
 */
void mutex_unlock(struct mutex *m)
{
    if (!m) return;
    
    struct process *current = process_get_current();
    
    /* 禁用中断 */
    asm volatile("cli");
    
    /* 检查是否是持有者 */
    if (m->owner != current) {
        kprintf("[MUTEX] Warning: Process %s trying to unlock mutex '%s' not owned by it\n",
                current ? current->name : "unknown", m->name);
        asm volatile("sti");
        return;
    }
    
    /* 递归锁处理 */
    if (m->recursion_count > 1) {
        m->recursion_count--;
        asm volatile("sti");
        return;
    }
    
    /* 恢复持有者的原始优先级（如果使用了优先级继承） */
    /* TODO: 保存和恢复原始优先级 */
    
    /* 唤醒等待队列中的一个进程 */
    if (m->wait_list) {
        struct process *wake = m->wait_list;
        m->wait_list = wake->next_waiting;
        wake->next_waiting = NULL;
        
        /* 将进程设置为就绪状态 */
        if (wake->state == PROCESS_STATE_BLOCKED) {
            wake->state = PROCESS_STATE_READY;
        }
    }
    
    /* 清除所有者 */
    m->owner = NULL;
    m->recursion_count = 0;
    
    /* 释放锁 */
    atomic_clear(&m->lock);
    
    /* 重新启用中断 */
    asm volatile("sti");
}

/*
 * 检查锁是否被持有
 */
bool mutex_is_locked(struct mutex *m)
{
    if (!m) return false;
    return m->lock == MUTEX_LOCKED;
}

