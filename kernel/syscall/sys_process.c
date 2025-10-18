/*
 * sys_process.c - 进程管理相关系统调用
 */

#include <syscall.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <process/process.h>

/*
 * sys_getpid - 获取当前进程ID
 */
int sys_getpid(void)
{
    struct process *current = process_get_current();
    
    if (!current) {
        return -1;
    }
    
    return current->pid;
}

/*
 * sys_getppid - 获取父进程ID
 */
int sys_getppid(void)
{
    struct process *current = process_get_current();
    
    if (!current || !current->parent) {
        return -1;
    }
    
    return current->parent->pid;
}

/*
 * sys_exit - 退出当前进程
 */
int sys_exit(int status)
{
    kprintf("\n[SYSCALL] User program exited with status %d\n", status);
    kprintf("[SYSCALL] Returning to kernel...\n");
    
    /* 简化：直接停止（真正的实现会切换到其他进程） */
    kprintf("\n[SUCCESS] User mode program executed successfully!\n");
    kprintf("[INFO] In a full implementation, would return to scheduler\n");
    
    /* 停止系统（演示用） */
    while (1) {
        asm("hlt");
    }
    
    return 0;
}

/*
 * sys_fork - 创建子进程
 */
int sys_fork(void)
{
    extern int do_fork(void);
    return do_fork();
}

/*
 * sys_execve - 执行程序
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
    extern int do_execve(const char *path, char *const argv[], char *const envp[]);
    return do_execve(path, argv, envp);
}

/* 查找已终止的子进程 */
static struct process *find_zombie_child(struct process *parent)
{
    if (!parent) {
        return NULL;
    }
    
    /* 遍历所有进程，找到已终止的子进程 */
    struct process *proc = parent;
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
        
        return child_pid;
    }
    
    /* 没有子进程退出，阻塞等待 */
    current->state = PROCESS_STATE_BLOCKED;
    
    /* 触发调度，切换到其他进程 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    return -EINTR;
}

/*
 * sys_waitpid - 等待子进程
 */
int sys_waitpid(pid_t pid, int *status, int options)
{
    struct process *current = process_get_current();
    
    if (!current) {
        return -ESRCH;
    }
    
    /* 查找指定的子进程 */
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
    current->state = PROCESS_STATE_BLOCKED;
    
    /* 触发调度 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    /* 被唤醒后返回 */
    return pid;
}

