/*
 * rwlock.h - 读写锁
 * 
 * 允许多个读者或一个写者
 */

#ifndef RWLOCK_H
#define RWLOCK_H

#include <types.h>
#include <sync/spinlock.h>

/* 读写锁结构 */
struct rwlock {
    struct spinlock lock;           /* 保护内部状态的自旋锁 */
    volatile int32_t readers;       /* 当前读者数量 */
    volatile bool writer;           /* 是否有写者 */
    struct process *read_waiters;   /* 读等待队列 */
    struct process *write_waiters;  /* 写等待队列 */
    const char *name;               /* 名称 */
};

/* ========== 读写锁操作 ========== */

/*
 * 初始化读写锁
 */
void rwlock_init(struct rwlock *rw, const char *name);

/*
 * 读锁定
 */
void rwlock_read_lock(struct rwlock *rw);

/*
 * 读解锁
 */
void rwlock_read_unlock(struct rwlock *rw);

/*
 * 写锁定
 */
void rwlock_write_lock(struct rwlock *rw);

/*
 * 写解锁
 */
void rwlock_write_unlock(struct rwlock *rw);

/*
 * 尝试读锁定
 */
int rwlock_try_read_lock(struct rwlock *rw);

/*
 * 尝试写锁定
 */
int rwlock_try_write_lock(struct rwlock *rw);

#endif // RWLOCK_H

