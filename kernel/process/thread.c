/*
 * thread.c - 线程组管理实现
 * 对标Linux kernel/exit.c中的线程组管理
 */

#include <process/clone.h>
#include <process/sched.h>
#include <list.h>
#include <errno.h>
#include <kernel.h>

/* 线程组管理 */
struct thread_group {
    struct list_head list;
    pid_t tgid;
    int nr_threads;
    struct task_struct *leader;
    struct rwlock_t lock;
};

/* 全局线程组哈希表 */
#define THREAD_GROUP_HASH_SIZE  64
static struct thread_group *thread_group_hash[THREAD_GROUP_HASH_SIZE];
static DEFINE_SPINLOCK(thread_group_lock);

/* 查找线程组 */
static struct thread_group *find_thread_group(pid_t tgid)
{
    struct thread_group *tg;
    unsigned int hash = tgid & (THREAD_GROUP_HASH_SIZE - 1);
    
    read_lock(&thread_group_lock);
    tg = thread_group_hash[hash];
    while (tg && tg->tgid != tgid) {
        tg = tg->next;
    }
    read_unlock(&thread_group_lock);
    
    return tg;
}

/* 创建线程组 */
static struct thread_group *create_thread_group(struct task_struct *leader)
{
    struct thread_group *tg;
    unsigned int hash = leader->tgid & (THREAD_GROUP_HASH_SIZE - 1);
    
    tg = kmalloc(sizeof(struct thread_group), GFP_KERNEL);
    if (!tg) {
        return NULL;
    }
    
    tg->tgid = leader->tgid;
    tg->nr_threads = 1;
    tg->leader = leader;
    rwlock_init(&tg->lock);
    
    write_lock(&thread_group_lock);
    tg->next = thread_group_hash[hash];
    thread_group_hash[hash] = tg;
    write_unlock(&thread_group_lock);
    
    return tg;
}

/* 添加线程到线程组 */
int thread_group_add(struct task_struct *tsk)
{
    struct thread_group *tg;
    
    if (tsk->tgid == tsk->pid) {
        /* 新进程组leader */
        tg = create_thread_group(tsk);
        if (!tg) {
            return -ENOMEM;
        }
    } else {
        /* 添加到现有线程组 */
        tg = find_thread_group(tsk->tgid);
        if (!tg) {
            return -ESRCH;
        }
        
        write_lock(&tg->lock);
        tg->nr_threads++;
        write_unlock(&tg->lock);
    }
    
    return 0;
}

/* 从线程组移除线程 */
void thread_group_remove(struct task_struct *tsk)
{
    struct thread_group *tg;
    
    if (tsk->tgid == tsk->pid) {
        /* 进程组leader */
        tg = find_thread_group(tsk->tgid);
        if (tg) {
            write_lock(&thread_group_lock);
            /* 从哈希表移除 */
            unsigned int hash = tsk->tgid & (THREAD_GROUP_HASH_SIZE - 1);
            struct thread_group **pp = &thread_group_hash[hash];
            while (*pp) {
                if (*pp == tg) {
                    *pp = tg->next;
                    break;
                }
                pp = &(*pp)->next;
            }
            write_unlock(&thread_group_lock);
            
            kfree(tg);
        }
    } else {
        /* 普通线程 */
        tg = find_thread_group(tsk->tgid);
        if (tg) {
            write_lock(&tg->lock);
            tg->nr_threads--;
            if (tg->nr_threads == 0) {
                /* 线程组为空，移除 */
                write_unlock(&tg->lock);
                thread_group_remove(tg->leader);
            } else {
                write_unlock(&tg->lock);
            }
        }
    }
}

/* 获取线程组信息 */
int thread_group_info(pid_t tgid, struct thread_group_info *info)
{
    struct thread_group *tg;
    
    tg = find_thread_group(tgid);
    if (!tg) {
        return -ESRCH;
    }
    
    read_lock(&tg->lock);
    info->tgid = tg->tgid;
    info->nr_threads = tg->nr_threads;
    info->leader_pid = tg->leader->pid;
    read_unlock(&tg->lock);
    
    return 0;
}

/* 遍历线程组 */
#define do_each_thread(g, t) \
    for (g = &init_task; (g = next_task(g)) != &init_task; ) \
        for_each_thread(g, t)

#define for_each_thread(leader, t) \
    list_for_each_entry(t, &leader->thread_group, thread_group)

/* 向线程组发送信号 */
int thread_group_send_sig_info(int sig, struct siginfo *info, struct task_struct *tsk)
{
    struct task_struct *g, *p;
    int ret = 0;
    
    /* 向线程组所有成员发送信号 */
    do_each_thread(g, p) {
        if (p->tgid == tsk->tgid) {
            ret = send_sig_info(sig, info, p);
            if (ret) {
                break;
            }
        }
    } while_each_thread(g, p);
    
    return ret;
}

/* 等待线程组 */
int thread_group_wait(struct task_struct *tsk, int options, struct siginfo *infop)
{
    struct thread_group *tg;
    long timeout = MAX_SCHEDULE_TIMEOUT;
    int ret;
    
    tg = find_thread_group(tsk->tgid);
    if (!tg) {
        return -ESRCH;
    }
    
    /* 等待线程组中的子线程 */
    for (;;) {
        ret = wait_task_zombie(tsk, timeout);
        if (ret != -EAGAIN) {
            break;
        }
        
        /* 检查是否还有活动的线程 */
        read_lock(&tg->lock);
        if (tg->nr_threads == 0) {
            read_unlock(&tg->lock);
            ret = 0;
            break;
        }
        read_unlock(&tg->lock);
        
        /* 等待事件 */
        set_current_state(TASK_INTERRUPTIBLE);
        timeout = schedule_timeout(timeout);
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            break;
        }
    }
    
    return ret;
}

/* 释放线程组资源 */
void release_thread_group(struct task_struct *leader)
{
    struct thread_group *tg;
    
    tg = find_thread_group(leader->tgid);
    if (tg) {
        /* 释放线程组占用的资源 */
        thread_group_remove(leader);
    }
}

/* 线程组退出处理 */
void do_thread_group_exit(struct task_struct *tsk, int exit_code)
{
    struct thread_group *tg;
    
    tg = find_thread_group(tsk->tgid);
    if (!tg) {
        return;
    }
    
    /* 设置退出码 */
    tsk->exit_code = exit_code;
    tsk->exit_state = EXIT_ZOMBIE;
    
    /* 唤醒等待的父进程 */
    wake_up_parent(tsk->parent);
    
    /* 如果这是leader，标记线程组退出 */
    if (tsk == tg->leader) {
        tg->flags |= THREAD_GROUP_EXIT;
        tg->exit_code = exit_code;
    }
}

/* 初始化线程组管理 */
void __init thread_group_init(void)
{
    int i;
    
    for (i = 0; i < THREAD_GROUP_HASH_SIZE; i++) {
        thread_group_hash[i] = NULL;
    }
    
    /* 创建init任务的线程组 */
    create_thread_group(&init_task);
}