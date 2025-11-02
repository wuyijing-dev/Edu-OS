/*
 * errno.c - POSIX errno支持
 * 
 * Linux风格：每个进程有独立的errno
 */

#include <process/process.h>
#include <kernel.h>

/* 不包含errno.h，避免errno宏与结构体字段冲突 */

/*
 * set_errno - 设置当前进程的errno并返回-1
 * 
 * POSIX标准：系统调用出错时返回-1并设置errno
 */
int set_errno(int error_code)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (current) {
        current->errno = error_code;
    }
    
    return -1;
}

/*
 * get_errno - 获取当前进程的errno
 */
int get_errno(void)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (current) {
        return current->errno;
    }
    
    return 0;
}

/*
 * clear_errno - 清除errno（系统调用成功时调用）
 */
void clear_errno(void)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (current) {
        current->errno = 0;
    }
}

/*
 * sys_get_errno - 获取errno的系统调用（用于libc）
 * 
 * 用户空间通过此系统调用访问内核空间的errno
 */
int sys_get_errno(void)
{
    return get_errno();
}

