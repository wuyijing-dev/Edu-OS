/*
 * spinlock.h - 自旋锁
 * 
 * 用于短临界区，不会睡眠
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <types.h>

/* 自旋锁结构 */
struct spinlock {
    volatile uint32_t lock;     /* 锁状态 */
    const char *name;           /* 名称 */
};

/* ========== 自旋锁操作 ========== */

/*
 * 初始化自旋锁
 */
void spin_lock_init(struct spinlock *lock, const char *name);

/*
 * 获取自旋锁
 */
void spin_lock(struct spinlock *lock);

/*
 * 释放自旋锁
 */
void spin_unlock(struct spinlock *lock);

/*
 * 尝试获取自旋锁
 */
int spin_trylock(struct spinlock *lock);

/* ========== 中断安全的自旋锁 ========== */

/*
 * 获取锁并禁用中断
 */
void spin_lock_irqsave(struct spinlock *lock, unsigned long *flags);

/*
 * 释放锁并恢复中断
 */
void spin_unlock_irqrestore(struct spinlock *lock, unsigned long flags);

/* ========== 静态初始化 ========== */

#define SPINLOCK_INITIALIZER(lockname) \
    { .lock = 0, .name = #lockname }

#define DEFINE_SPINLOCK(name) \
    struct spinlock name = SPINLOCK_INITIALIZER(name)

#endif // SPINLOCK_H

