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
 * sys_exit - 退出当前进程（Linux风格完整实现）
 */
int sys_exit(int status)
{
    struct process *current = process_get_current();
    
    if (!current) {
        panic("sys_exit: no current process");
    }
    
    current->exit_code = status;
    current->state = PROCESS_STATE_TERMINATED;
    
    /* 释放用户空间资源 */
    if (current->vma_list) {
        extern void vma_destroy_all(struct vma *list);
        vma_destroy_all(current->vma_list);
        current->vma_list = NULL;
    }
    
    /* 释放页表 */
    if (current->page_dir) {
        extern void vmm_destroy_page_directory(struct page_directory *pd);
        vmm_destroy_page_directory(current->page_dir);
        current->page_dir = NULL;
    }
    
    /* 唤醒等待的父进程 */
    if (current->parent && current->parent->state == PROCESS_STATE_BLOCKED) {
        current->parent->state = PROCESS_STATE_READY;
    }
    
    /* 将子进程重新父亲化给init进程 */
    extern struct process *process_list_head;
    struct process *child = process_list_head;
    while (child) {
        if (child->parent == current) {
            child->parent = NULL;
        }
        child = child->next;
    }
    
    /* 切换到其他进程 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    /* 不会返回 */
    panic("sys_exit: returned from scheduler");
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

