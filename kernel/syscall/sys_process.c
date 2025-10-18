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

