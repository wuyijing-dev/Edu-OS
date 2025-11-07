/**
 * semaphore.c - POSIX信号量实现
 * 
 * Linux风格的信号量系统，支持命名和匿名信号量
 */

#include <sync/semaphore.h>
#include <mm/kmalloc.h>
#include <process/process.h>
#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <fs/vfs.h>

/* 全局信号量管理器 */
struct semaphore_manager g_sem_manager;

/**
 * 初始化信号量系统
 */
void semaphore_init(void)
{
    kprintf("[Semaphore] Initializing semaphore system...\n");
    
    INIT_LIST_HEAD(&g_sem_manager.named_sems);
    g_sem_manager.named_count = 0;
    g_sem_manager.total_waits = 0;
    g_sem_manager.total_posts = 0;
    
    kprintf("[Semaphore] Semaphore system initialized\n");
    kprintf("[Semaphore]   Max named semaphores: %u\n", SEM_MAX_NAMED);
    kprintf("[Semaphore]   Max value: %u\n", SEM_VALUE_MAX);
}

/**
 * 初始化匿名信号量
 */
int sem_init(struct semaphore *sem, int pshared, uint32_t value)
{
    if (!sem) {
        return -EINVAL;
    }
    
    if (value > SEM_VALUE_MAX) {
        return -EINVAL;
    }
    
    sem->count = value;
    sem->flags = pshared ? SEM_FLAG_SHARED : 0;
    sem->name[0] = '\0';
    sem->refcount = 0;
    sem->wait_count = 0;
    sem->post_count = 0;
    
    init_waitqueue_head(&sem->wait_queue);
    INIT_LIST_HEAD(&sem->list);
    
    return 0;
}

/**
 * 销毁匿名信号量
 */
int sem_destroy(struct semaphore *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    if (sem->flags & SEM_FLAG_NAMED) {
        return -EINVAL;  /* 不能销毁命名信号量 */
    }
    
    /* TODO: 检查是否有进程在等待 */
    
    return 0;
}

/**
 * 查找命名信号量
 */
struct semaphore *sem_find_named(const char *name)
{
    struct semaphore *sem;
    
    list_for_each_entry(sem, &g_sem_manager.named_sems, list) {
        if (strcmp(sem->name, name) == 0) {
            return sem;
        }
    }
    
    return NULL;
}

/**
 * 打开/创建命名信号量
 */
struct semaphore *sem_open(const char *name, int oflag, mode_t mode, uint32_t value)
{
    if (!name || name[0] != '/') {
        return NULL;  /* 名称必须以'/'开头 */
    }
    
    if (value > SEM_VALUE_MAX) {
        return NULL;
    }
    
    /* 查找是否已存在 */
    struct semaphore *sem = sem_find_named(name);
    
    if (sem) {
        /* 信号量已存在 */
        if (oflag & O_CREAT && oflag & O_EXCL) {
            return NULL;  /* 独占创建失败 */
        }
        sem->refcount++;
        return sem;
    }
    
    /* 创建新信号量 */
    if (!(oflag & O_CREAT)) {
        return NULL;  /* 信号量不存在且未指定创建 */
    }
    
    if (g_sem_manager.named_count >= SEM_MAX_NAMED) {
        kprintf("[Semaphore] ERROR: Too many named semaphores\n");
        return NULL;
    }
    
    /* 分配信号量结构 */
    sem = (struct semaphore *)kmalloc(sizeof(struct semaphore));
    if (!sem) {
        return NULL;
    }
    
    /* 初始化信号量 */
    strncpy(sem->name, name, sizeof(sem->name) - 1);
    sem->name[sizeof(sem->name) - 1] = '\0';
    sem->count = value;
    sem->flags = SEM_FLAG_NAMED | SEM_FLAG_SHARED;
    sem->refcount = 1;
    sem->wait_count = 0;
    sem->post_count = 0;
    
    init_waitqueue_head(&sem->wait_queue);
    INIT_LIST_HEAD(&sem->list);
    
    /* 添加到全局链表 */
    list_add_tail(&sem->list, &g_sem_manager.named_sems);
    g_sem_manager.named_count++;
    
    kprintf("[Semaphore] Created named semaphore: %s (value=%u)\n", sem->name, value);
    
    return sem;
}

/**
 * 关闭命名信号量
 */
int sem_close(struct semaphore *sem)
{
    if (!sem || !(sem->flags & SEM_FLAG_NAMED)) {
        return -EINVAL;
    }
    
    if (sem->refcount > 0) {
        sem->refcount--;
    }
    
    return 0;
}

/**
 * 删除命名信号量
 */
int sem_unlink(const char *name)
{
    struct semaphore *sem = sem_find_named(name);
    if (!sem) {
        return -ENOENT;
    }
    
    /* 如果还有引用，标记为待删除 */
    if (sem->refcount > 0) {
        /* 简化实现：等待所有引用关闭后才删除 */
        return 0;
    }
    
    /* 从全局链表中删除 */
    list_del(&sem->list);
    g_sem_manager.named_count--;
    
    /* 释放信号量结构 */
    kfree(sem);
    
    kprintf("[Semaphore] Unlinked named semaphore: %s\n", name);
    
    return 0;
}

/**
 * 等待信号量（P操作）
 */
int sem_wait(struct semaphore *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    /* 禁用中断（简化的临界区保护）*/
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    while (sem->count <= 0) {
        /* 信号量不可用，需要等待 */
        sem->wait_count++;
        g_sem_manager.total_waits++;
        
        /* TODO: 真正的阻塞等待 */
        /* 简化实现：自旋等待 */
        __asm__ volatile("sti; hlt; cli");
    }
    
    /* 获取信号量 */
    sem->count--;
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 尝试等待信号量（非阻塞）
 */
int sem_trywait(struct semaphore *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    /* 禁用中断 */
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    if (sem->count <= 0) {
        /* 信号量不可用 */
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return -EAGAIN;
    }
    
    /* 获取信号量 */
    sem->count--;
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 定时等待信号量
 */
int sem_timedwait(struct semaphore *sem, const struct timespec *abs_timeout)
{
    /* TODO: 实现定时等待 */
    (void)abs_timeout;
    return sem_wait(sem);
}

/**
 * 发布信号量（V操作）
 */
int sem_post(struct semaphore *sem)
{
    if (!sem) {
        return -EINVAL;
    }
    
    /* 禁用中断 */
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags));
    
    if (sem->count >= SEM_VALUE_MAX) {
        /* 信号量已达最大值 */
        __asm__ volatile("push %0; popf" :: "r"(flags));
        return -EOVERFLOW;
    }
    
    /* 释放信号量 */
    sem->count++;
    sem->post_count++;
    g_sem_manager.total_posts++;
    
    /* TODO: 唤醒等待的进程 */
    
    /* 恢复中断 */
    __asm__ volatile("push %0; popf" :: "r"(flags));
    
    return 0;
}

/**
 * 获取信号量值
 */
int sem_getvalue(struct semaphore *sem, int *sval)
{
    if (!sem || !sval) {
        return -EINVAL;
    }
    
    *sval = sem->count;
    return 0;
}

/**
 * 内核信号量down操作
 */
void down(struct semaphore *sem)
{
    sem_wait(sem);
}

/**
 * 内核信号量down操作（可中断）
 */
int down_interruptible(struct semaphore *sem)
{
    return sem_wait(sem);
}

/**
 * 内核信号量trydown操作
 */
int down_trylock(struct semaphore *sem)
{
    return sem_trywait(sem);
}

/**
 * 内核信号量up操作
 */
void up(struct semaphore *sem)
{
    sem_post(sem);
}

/**
 * 打印信号量信息
 */
void semaphore_print_info(void)
{
    kprintf("\n=== Semaphore Statistics ===\n");
    kprintf("Named semaphores: %u / %u\n", g_sem_manager.named_count, SEM_MAX_NAMED);
    kprintf("Total waits:      %llu\n", g_sem_manager.total_waits);
    kprintf("Total posts:      %llu\n", g_sem_manager.total_posts);
    
    if (g_sem_manager.named_count > 0) {
        kprintf("\nNamed semaphores:\n");
        struct semaphore *sem;
        list_for_each_entry(sem, &g_sem_manager.named_sems, list) {
            kprintf("  %s: value=%d (waits=%llu, posts=%llu, refs=%u)\n",
                    sem->name, sem->count, sem->wait_count, 
                    sem->post_count, sem->refcount);
        }
    }
    
    kprintf("============================\n\n");
}

/**
 * 系统调用实现
 */

int sys_sem_init(struct semaphore *sem, int pshared, uint32_t value)
{
    /* TODO: 用户空间指针验证 */
    return sem_init(sem, pshared, value);
}

int sys_sem_destroy(struct semaphore *sem)
{
    /* TODO: 用户空间指针验证 */
    return sem_destroy(sem);
}

int sys_sem_open(const char *name, int oflag, mode_t mode, uint32_t value)
{
    /* TODO: 用户空间指针验证 */
    struct semaphore *sem = sem_open(name, oflag, mode, value);
    if (!sem) {
        return -1;
    }
    return (int)(uintptr_t)sem;  /* 简化：直接返回指针 */
}

int sys_sem_close(struct semaphore *sem)
{
    return sem_close(sem);
}

int sys_sem_unlink(const char *name)
{
    /* TODO: 用户空间指针验证 */
    return sem_unlink(name);
}

int sys_sem_wait(struct semaphore *sem)
{
    return sem_wait(sem);
}

int sys_sem_trywait(struct semaphore *sem)
{
    return sem_trywait(sem);
}

int sys_sem_timedwait(struct semaphore *sem, const struct timespec *abs_timeout)
{
    /* TODO: 用户空间指针验证 */
    return sem_timedwait(sem, abs_timeout);
}

int sys_sem_post(struct semaphore *sem)
{
    return sem_post(sem);
}

int sys_sem_getvalue(struct semaphore *sem, int *sval)
{
    /* TODO: 用户空间指针验证 */
    return sem_getvalue(sem, sval);
}
