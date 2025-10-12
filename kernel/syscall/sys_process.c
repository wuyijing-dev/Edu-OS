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
    kprintf("[SYSCALL] Process %d exiting with status %d\n", 
            sys_getpid(), status);
    
    process_exit(status);
    
    /* 不会执行到这里 */
    return 0;
}

/*
 * sys_fork - 创建子进程
 */
int sys_fork(void)
{
    /* TODO: 实现 fork */
    kprintf("[SYSCALL] sys_fork not yet implemented\n");
    return -ENOSYS;
}

/*
 * sys_execve - 执行程序
 */
int sys_execve(const char *path, char *const argv[], char *const envp[])
{
    /* TODO: 实现 execve */
    (void)path;
    (void)argv;
    (void)envp;
    kprintf("[SYSCALL] sys_execve not yet implemented\n");
    return -ENOSYS;
}

/*
 * sys_waitpid - 等待子进程
 */
int sys_waitpid(pid_t pid, int *status, int options)
{
    /* TODO: 实现 waitpid */
    (void)pid;
    (void)status;
    (void)options;
    return -ENOSYS;
}

