/*
 * wait.c - 进程等待机制（wait/waitpid系统调用）
 */

#include <process/process.h>
#include <kernel.h>
#include <string.h>

/* 外部函数：遍历进程列表 */
extern struct process *process_list_head;

/* 进程状态检查 */
static struct process *find_zombie_child(struct process *parent)
{
    if (!parent) {
        return NULL;
    }
    
    /* 遍历所有进程，找到已终止的子进程 */
    extern struct process *process_list_head;
    struct process *proc = process_list_head;
    
    while (proc) {
        if (proc->parent == parent && 
            proc->state == PROCESS_STATE_TERMINATED) {
            return proc;
        }
        proc = proc->next;
    }
    
    return NULL;
}

/*
 * sys_wait - 等待任意子进程退出
 * 
 * @param status: 存储子进程退出状态
 * @return: 子进程PID，出错返回-1
 */
pid_t sys_wait(int *status)
{
    struct process *current = process_get_current();
    
    if (!current) {
        return -ESRCH;
    }
    
    /* 查找已退出的子进程 */
    struct process *child = find_zombie_child(current);
    
    if (child) {
        /* 有子进程已退出 */
        if (status) {
            *status = child->exit_code;
        }
        
        pid_t child_pid = child->pid;
        
        /* 清理子进程资源 */
        process_destroy(child);
        
        kprintf("[WAIT] Parent %u reaped child %u\n", current->pid, child_pid);
        
        return child_pid;
    }
    
    /* 没有子进程退出，阻塞等待 */
    kprintf("[WAIT] Process %u waiting for children\n", current->pid);
    
    /* 阻塞当前进程 */
    current->state = PROCESS_STATE_BLOCKED;
    
    /* 触发调度，切换到其他进程 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    /* 被唤醒后重新检查（正常情况下不会返回到这里） */
    return -EINTR;  /* Interrupted */
}

/*
 * sys_waitpid - 等待指定子进程退出
 * 
 * @param pid: 要等待的进程ID
 * @param status: 存储子进程退出状态
 * @param options: 选项（WNOHANG等）
 * @return: 子进程PID，出错返回-1
 */
pid_t sys_waitpid(pid_t pid, int *status, int options)
{
    struct process *current = process_get_current();
    
    if (!current) {
        return -ESRCH;
    }
    
    kprintf("[WAITPID] Process %u waiting for child %u\n", current->pid, pid);
    
    /* TODO: 查找指定的子进程 */
    struct process *child = process_find_by_pid(pid);
    
    if (!child || child->parent != current) {
        return -ECHILD;  /* 不是自己的子进程 */
    }
    
    /* 检查子进程状态 */
    if (child->state == PROCESS_STATE_TERMINATED) {
        /* 子进程已退出 */
        if (status) {
            *status = child->exit_code;
        }
        
        /* 清理资源 */
        process_destroy(child);
        
        return pid;
    }
    
    /* WNOHANG: 非阻塞模式 */
    if (options & 0x1) {  // WNOHANG
        return 0;  /* 子进程还在运行 */
    }
    
    /* 阻塞等待 */
    kprintf("[WAITPID] Process %u blocked, waiting for child %u\n", current->pid, pid);
    
    current->state = PROCESS_STATE_BLOCKED;
    
    /* 触发调度 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    /* 被唤醒后返回 */
    return pid;
}
