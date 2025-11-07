/**
 * mutex.h - 互斥锁（Mutex）
 * 
 * Linux风格的互斥锁实现
 */

#ifndef _SYNC_MUTEX_H
#define _SYNC_MUTEX_H

#include <types.h>
#include <list.h>
#include <wait_queue.h>
#include <time.h>

/**
 * 互斥锁类型
 */
#define MUTEX_NORMAL        0       /* 普通互斥锁 */
#define MUTEX_RECURSIVE     1       /* 递归互斥锁 */
#define MUTEX_ERRORCHECK    2       /* 错误检查互斥锁 */

/**
 * 互斥锁状态
 */
#define MUTEX_UNLOCKED      0       /* 未锁定 */
#define MUTEX_LOCKED        1       /* 已锁定 */

/**
 * 互斥锁结构
 */
struct mutex {
    volatile uint32_t locked;       /* 锁状态：0=未锁定，1=已锁定 */
    uint32_t type;                  /* 互斥锁类型 */
    uint32_t owner;                 /* 持有者进程ID */
    uint32_t recursion;             /* 递归计数（递归锁用）*/
    wait_queue_head_t wait_queue;   /* 等待队列 */
    
    /* 统计信息 */
    uint64_t lock_count;            /* 总加锁次数 */
    uint64_t unlock_count;          /* 总解锁次数 */
    uint64_t contention_count;      /* 竞争次数 */
};

/**
 * 互斥锁属性
 */
struct mutexattr {
    uint32_t type;                  /* 互斥锁类型 */
    uint32_t pshared;               /* 进程共享标志 */
};

/**
 * 全局互斥锁统计
 */
struct mutex_stats {
    uint64_t total_locks;           /* 系统总加锁次数 */
    uint64_t total_unlocks;         /* 系统总解锁次数 */
    uint64_t total_contentions;     /* 系统总竞争次数 */
};

extern struct mutex_stats g_mutex_stats;

/**
 * 互斥锁操作函数
 */

/* 初始化互斥锁系统 */
void mutex_init_system(void);

/* 初始化互斥锁 */
int mutex_init(struct mutex *mutex, const struct mutexattr *attr);

/* 销毁互斥锁 */
int mutex_destroy(struct mutex *mutex);

/* 加锁（阻塞）*/
int mutex_lock(struct mutex *mutex);

/* 尝试加锁（非阻塞）*/
int mutex_trylock(struct mutex *mutex);

/* 定时加锁 */
int mutex_timedlock(struct mutex *mutex, const struct timespec *abs_timeout);

/* 解锁 */
int mutex_unlock(struct mutex *mutex);

/* 获取互斥锁持有者 */
uint32_t mutex_get_owner(struct mutex *mutex);

/**
 * 互斥锁属性操作
 */
int mutexattr_init(struct mutexattr *attr);
int mutexattr_destroy(struct mutexattr *attr);
int mutexattr_settype(struct mutexattr *attr, int type);
int mutexattr_gettype(const struct mutexattr *attr, int *type);
int mutexattr_setpshared(struct mutexattr *attr, int pshared);
int mutexattr_getpshared(const struct mutexattr *attr, int *pshared);

/**
 * 内核互斥锁宏（简化版）
 */
#define DEFINE_MUTEX(name) \
    struct mutex name = { \
        .locked = MUTEX_UNLOCKED, \
        .type = MUTEX_NORMAL, \
        .owner = 0, \
        .recursion = 0, \
        .lock_count = 0, \
        .unlock_count = 0, \
        .contention_count = 0 \
    }

/* 静态初始化互斥锁 */
static inline void mutex_init_simple(struct mutex *mutex)
{
    mutex->locked = MUTEX_UNLOCKED;
    mutex->type = MUTEX_NORMAL;
    mutex->owner = 0;
    mutex->recursion = 0;
    init_waitqueue_head(&mutex->wait_queue);
    mutex->lock_count = 0;
    mutex->unlock_count = 0;
    mutex->contention_count = 0;
}

/**
 * 原子操作辅助函数
 */
static inline int atomic_cmpxchg(volatile uint32_t *ptr, uint32_t old_val, uint32_t new_val)
{
    uint32_t prev;
    __asm__ volatile(
        "lock cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(new_val), "0"(old_val)
        : "memory"
    );
    return prev == old_val;
}

/**
 * 打印互斥锁统计信息
 */
void mutex_print_stats(void);

/**
 * 系统调用接口
 */
int sys_mutex_init(struct mutex *mutex, const struct mutexattr *attr);
int sys_mutex_destroy(struct mutex *mutex);
int sys_mutex_lock(struct mutex *mutex);
int sys_mutex_trylock(struct mutex *mutex);
int sys_mutex_unlock(struct mutex *mutex);

#endif /* _SYNC_MUTEX_H */
