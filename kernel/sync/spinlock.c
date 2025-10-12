/*
 * spinlock.c - 自旋锁实现
 */

#include <sync/spinlock.h>
#include <kernel.h>

/*
 * 初始化自旋锁
 */
void spin_lock_init(struct spinlock *lock, const char *name)
{
    if (!lock) return;
    
    lock->lock = 0;
    lock->name = name ? name : "unnamed";
}

/*
 * 获取自旋锁
 */
void spin_lock(struct spinlock *lock)
{
    if (!lock) return;
    
    /* 自旋等待 */
    while (__sync_lock_test_and_set(&lock->lock, 1)) {
        /* 忙等待，使用 pause 降低功耗 */
        asm volatile("pause");
    }
    
    /* 内存屏障，确保锁操作完成 */
    asm volatile("" ::: "memory");
}

/*
 * 释放自旋锁
 */
void spin_unlock(struct spinlock *lock)
{
    if (!lock) return;
    
    /* 内存屏障 */
    asm volatile("" ::: "memory");
    
    /* 释放锁 */
    __sync_lock_release(&lock->lock);
}

/*
 * 尝试获取自旋锁
 */
int spin_trylock(struct spinlock *lock)
{
    if (!lock) return -1;
    
    if (__sync_lock_test_and_set(&lock->lock, 1) == 0) {
        asm volatile("" ::: "memory");
        return 0;
    }
    
    return -1;
}

/*
 * 获取锁并保存中断状态
 */
void spin_lock_irqsave(struct spinlock *lock, unsigned long *flags)
{
    if (!lock || !flags) return;
    
    /* 保存 EFLAGS 并禁用中断 */
    asm volatile(
        "pushf\n"
        "pop %0\n"
        "cli\n"
        : "=r"(*flags)
        :
        : "memory"
    );
    
    /* 获取锁 */
    spin_lock(lock);
}

/*
 * 释放锁并恢复中断状态
 */
void spin_unlock_irqrestore(struct spinlock *lock, unsigned long flags)
{
    if (!lock) return;
    
    /* 释放锁 */
    spin_unlock(lock);
    
    /* 恢复中断状态 */
    asm volatile(
        "push %0\n"
        "popf\n"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

