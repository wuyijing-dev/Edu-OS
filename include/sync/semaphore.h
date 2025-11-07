/**
 * semaphore.h - POSIX信号量（Semaphore）
 * 
 * 支持命名信号量和匿名信号量
 */

#ifndef _SYNC_SEMAPHORE_H
#define _SYNC_SEMAPHORE_H

#include <types.h>
#include <list.h>
#include <wait_queue.h>
#include <time.h>

/**
 * 信号量配置
 */
#define SEM_VALUE_MAX       32767   /* 信号量最大值 */
#define SEM_MAX_NAMED       256     /* 最大命名信号量数 */
#define SEM_NAME_MAX        64      /* 信号量名称最大长度 */

/**
 * 信号量标志
 */
#define SEM_FLAG_NAMED      0x01    /* 命名信号量 */
#define SEM_FLAG_SHARED     0x02    /* 进程间共享 */

/**
 * 信号量结构（Linux风格）
 */
struct semaphore {
    int32_t count;                  /* 信号量计数值 */
    uint32_t flags;                 /* 标志位 */
    wait_queue_head_t wait_queue;   /* 等待队列 */
    
    /* 命名信号量专用 */
    char name[SEM_NAME_MAX];        /* 信号量名称 */
    uint32_t refcount;              /* 引用计数 */
    struct list_head list;          /* 全局链表节点 */
    
    /* 统计信息 */
    uint64_t wait_count;            /* 总等待次数 */
    uint64_t post_count;            /* 总发布次数 */
};

/**
 * 信号量管理器
 */
struct semaphore_manager {
    struct list_head named_sems;    /* 命名信号量链表 */
    uint32_t named_count;           /* 命名信号量数量 */
    
    /* 统计信息 */
    uint64_t total_waits;           /* 系统总等待次数 */
    uint64_t total_posts;           /* 系统总发布次数 */
};

/**
 * 全局信号量管理器
 */
extern struct semaphore_manager g_sem_manager;

/**
 * 信号量操作函数
 */

/* 初始化信号量系统 */
void semaphore_init(void);

/* 初始化匿名信号量 */
int sem_init(struct semaphore *sem, int pshared, uint32_t value);

/* 销毁匿名信号量 */
int sem_destroy(struct semaphore *sem);

/* 打开/创建命名信号量 */
struct semaphore *sem_open(const char *name, int oflag, mode_t mode, uint32_t value);

/* 关闭命名信号量 */
int sem_close(struct semaphore *sem);

/* 删除命名信号量 */
int sem_unlink(const char *name);

/* 等待信号量（P操作，阻塞）*/
int sem_wait(struct semaphore *sem);

/* 尝试等待信号量（非阻塞）*/
int sem_trywait(struct semaphore *sem);

/* 定时等待信号量 */
int sem_timedwait(struct semaphore *sem, const struct timespec *abs_timeout);

/* 发布信号量（V操作）*/
int sem_post(struct semaphore *sem);

/* 获取信号量值 */
int sem_getvalue(struct semaphore *sem, int *sval);

/* 查找命名信号量 */
struct semaphore *sem_find_named(const char *name);

/* 打印信号量信息（调试用）*/
void semaphore_print_info(void);

/**
 * 内核内部信号量操作（简化版）
 */

/* 初始化内核信号量 */
static inline void sema_init(struct semaphore *sem, int val)
{
    sem->count = val;
    sem->flags = 0;
    init_waitqueue_head(&sem->wait_queue);
    sem->wait_count = 0;
    sem->post_count = 0;
}

/* 内核信号量down操作（可能阻塞）*/
void down(struct semaphore *sem);

/* 内核信号量down操作（可中断）*/
int down_interruptible(struct semaphore *sem);

/* 内核信号量trydown操作（非阻塞）*/
int down_trylock(struct semaphore *sem);

/* 内核信号量up操作 */
void up(struct semaphore *sem);

/**
 * 系统调用接口
 */
int sys_sem_init(struct semaphore *sem, int pshared, uint32_t value);
int sys_sem_destroy(struct semaphore *sem);
int sys_sem_open(const char *name, int oflag, mode_t mode, uint32_t value);
int sys_sem_close(struct semaphore *sem);
int sys_sem_unlink(const char *name);
int sys_sem_wait(struct semaphore *sem);
int sys_sem_trywait(struct semaphore *sem);
int sys_sem_timedwait(struct semaphore *sem, const struct timespec *abs_timeout);
int sys_sem_post(struct semaphore *sem);
int sys_sem_getvalue(struct semaphore *sem, int *sval);

#endif /* _SYNC_SEMAPHORE_H */
