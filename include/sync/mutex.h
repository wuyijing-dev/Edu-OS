/*
 * mutex.h - 互斥锁（Mutex）
 * 
 * 基于 Linux 的互斥锁实现
 */

#ifndef MUTEX_H
#define MUTEX_H

#include <types.h>
#include <process/process.h>

/* 互斥锁状态 */
enum mutex_state {
    MUTEX_UNLOCKED = 0,
    MUTEX_LOCKED   = 1
};

/* 互斥锁结构 */
struct mutex {
    volatile uint32_t lock;         /* 锁状态：0=未锁，1=已锁 */
    struct process *owner;          /* 持有者进程 */
    struct process *wait_list;      /* 等待队列 */
    uint32_t recursion_count;       /* 递归计数（如果支持递归锁） */
    const char *name;               /* 锁名称（调试用） */
};

/* ========== 互斥锁操作 ========== */

/*
 * 初始化互斥锁
 */
void mutex_init(struct mutex *m, const char *name);

/*
 * 销毁互斥锁
 */
void mutex_destroy(struct mutex *m);

/*
 * 获取互斥锁（阻塞）
 */
void mutex_lock(struct mutex *m);

/*
 * 尝试获取互斥锁（非阻塞）
 * 
 * @return: 0=成功获取，-1=锁已被占用
 */
int mutex_trylock(struct mutex *m);

/*
 * 释放互斥锁
 */
void mutex_unlock(struct mutex *m);

/*
 * 检查锁是否被持有
 */
bool mutex_is_locked(struct mutex *m);

/* ========== 静态初始化宏 ========== */

#define MUTEX_INITIALIZER(lockname) \
    { .lock = 0, .owner = NULL, .wait_list = NULL, .recursion_count = 0, .name = #lockname }

/* ========== 便捷宏 ========== */

#define DEFINE_MUTEX(name) \
    struct mutex name = MUTEX_INITIALIZER(name)

#endif // MUTEX_H

