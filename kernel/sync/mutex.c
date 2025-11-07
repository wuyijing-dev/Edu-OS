/**
 * mutex.c - 互斥锁实现
 * 
 * Linux风格的互斥锁，支持递归锁和错误检查
 */

#include <sync/mutex.h>
#include <process/process.h>
#include <kernel.h>
#include <errno.h>

/* 全局互斥锁统计 */
struct mutex_stats g_mutex_stats;

/**
 * 初始化互斥锁系统
 */
void mutex_init_system(void)
{
    kprintf("[Mutex] Initializing mutex system...\n");
    
    g_mutex_stats.total_locks = 0;
    g_mutex_stats.total_unlocks = 0;
    g_mutex_stats.total_contentions = 0;
    
    kprintf("[Mutex] Mutex system initialized\n");
}

/**
 * 获取当前进程ID
 */
static uint32_t get_current_pid(void)
{
    /* TODO: 从进程管理器获取当前PID */
    return 1;  /* 简化：返回固定值 */
}

/**
 * 初始化互斥锁
 */
int mutex_init(struct mutex *mutex, const struct mutexattr *attr)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    mutex->locked = MUTEX_UNLOCKED;
    mutex->type = attr ? attr->type : MUTEX_NORMAL;
    mutex->owner = 0;
    mutex->recursion = 0;
    mutex->lock_count = 0;
    mutex->unlock_count = 0;
    mutex->contention_count = 0;
    
    init_waitqueue_head(&mutex->wait_queue);
    
    return 0;
}

/**
 * 销毁互斥锁
 */
int mutex_destroy(struct mutex *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    if (mutex->locked == MUTEX_LOCKED) {
        return -EBUSY;  /* 互斥锁仍被持有 */
    }
    
    return 0;
}

/**
 * 加锁
 */
int mutex_lock(struct mutex *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    uint32_t current_pid = get_current_pid();
    
    /* 禁用中断 */
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    /* 检查递归锁 */
    if (mutex->type == MUTEX_RECURSIVE && mutex->owner == current_pid) {
        mutex->recursion++;
        mutex->lock_count++;
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return 0;
    }
    
    /* 检查错误 */
    if (mutex->type == MUTEX_ERRORCHECK && mutex->owner == current_pid) {
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return -EDEADLK;  /* 死锁检测 */
    }
    
    /* 尝试获取锁 */
    while (mutex->locked == MUTEX_LOCKED) {
        mutex->contention_count++;
        g_mutex_stats.total_contentions++;
        
        /* TODO: 真正的阻塞等待 */
        /* 简化实现：自旋等待 */
        __asm__ volatile("sti; pause; cli");
    }
    
    /* 获取锁 */
    mutex->locked = MUTEX_LOCKED;
    mutex->owner = current_pid;
    mutex->recursion = 1;
    mutex->lock_count++;
    g_mutex_stats.total_locks++;
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 尝试加锁（非阻塞）
 */
int mutex_trylock(struct mutex *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    uint32_t current_pid = get_current_pid();
    
    /* 禁用中断 */
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    /* 检查递归锁 */
    if (mutex->type == MUTEX_RECURSIVE && mutex->owner == current_pid) {
        mutex->recursion++;
        mutex->lock_count++;
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return 0;
    }
    
    /* 尝试获取锁 */
    if (mutex->locked == MUTEX_LOCKED) {
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return -EBUSY;
    }
    
    /* 获取锁 */
    mutex->locked = MUTEX_LOCKED;
    mutex->owner = current_pid;
    mutex->recursion = 1;
    mutex->lock_count++;
    g_mutex_stats.total_locks++;
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 定时加锁
 */
int mutex_timedlock(struct mutex *mutex, const struct timespec *abs_timeout)
{
    /* TODO: 实现定时加锁 */
    (void)abs_timeout;
    return mutex_lock(mutex);
}

/**
 * 解锁
 */
int mutex_unlock(struct mutex *mutex)
{
    if (!mutex) {
        return -EINVAL;
    }
    
    uint32_t current_pid = get_current_pid();
    
    /* 禁用中断 */
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    /* 检查是否是持有者 */
    if (mutex->owner != current_pid) {
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return -EPERM;  /* 不是锁的持有者 */
    }
    
    /* 处理递归锁 */
    if (mutex->type == MUTEX_RECURSIVE && mutex->recursion > 1) {
        mutex->recursion--;
        mutex->unlock_count++;
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return 0;
    }
    
    /* 释放锁 */
    mutex->owner = 0;
    mutex->recursion = 0;
    mutex->locked = MUTEX_UNLOCKED;
    mutex->unlock_count++;
    g_mutex_stats.total_unlocks++;
    
    /* TODO: 唤醒等待的进程 */
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 获取互斥锁持有者
 */
uint32_t mutex_get_owner(struct mutex *mutex)
{
    return mutex ? mutex->owner : 0;
}

/**
 * 互斥锁属性操作
 */

int mutexattr_init(struct mutexattr *attr)
{
    if (!attr) {
        return -EINVAL;
    }
    
    attr->type = MUTEX_NORMAL;
    attr->pshared = 0;
    
    return 0;
}

int mutexattr_destroy(struct mutexattr *attr)
{
    (void)attr;
    return 0;
}

int mutexattr_settype(struct mutexattr *attr, int type)
{
    if (!attr) {
        return -EINVAL;
    }
    
    if (type != MUTEX_NORMAL && type != MUTEX_RECURSIVE && type != MUTEX_ERRORCHECK) {
        return -EINVAL;
    }
    
    attr->type = type;
    return 0;
}

int mutexattr_gettype(const struct mutexattr *attr, int *type)
{
    if (!attr || !type) {
        return -EINVAL;
    }
    
    *type = attr->type;
    return 0;
}

int mutexattr_setpshared(struct mutexattr *attr, int pshared)
{
    if (!attr) {
        return -EINVAL;
    }
    
    attr->pshared = pshared;
    return 0;
}

int mutexattr_getpshared(const struct mutexattr *attr, int *pshared)
{
    if (!attr || !pshared) {
        return -EINVAL;
    }
    
    *pshared = attr->pshared;
    return 0;
}

/**
 * 打印互斥锁统计信息
 */
void mutex_print_stats(void)
{
    kprintf("\n=== Mutex Statistics ===\n");
    kprintf("Total locks:       %llu\n", g_mutex_stats.total_locks);
    kprintf("Total unlocks:     %llu\n", g_mutex_stats.total_unlocks);
    kprintf("Total contentions: %llu\n", g_mutex_stats.total_contentions);
    kprintf("========================\n\n");
}

/**
 * 系统调用实现
 */

int sys_mutex_init(struct mutex *mutex, const struct mutexattr *attr)
{
    /* TODO: 用户空间指针验证 */
    return mutex_init(mutex, attr);
}

int sys_mutex_destroy(struct mutex *mutex)
{
    /* TODO: 用户空间指针验证 */
    return mutex_destroy(mutex);
}

int sys_mutex_lock(struct mutex *mutex)
{
    /* TODO: 用户空间指针验证 */
    return mutex_lock(mutex);
}

int sys_mutex_trylock(struct mutex *mutex)
{
    /* TODO: 用户空间指针验证 */
    return mutex_trylock(mutex);
}

int sys_mutex_unlock(struct mutex *mutex)
{
    /* TODO: 用户空间指针验证 */
    return mutex_unlock(mutex);
}
