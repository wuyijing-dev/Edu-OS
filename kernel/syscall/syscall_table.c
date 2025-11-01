/*
 * syscall_table.c - 系统调用表
 * 
 * 基于 Linux x86 系统调用表结构
 */

#include <syscall.h>
#include <types.h>

/* 外部系统调用函数声明 */

/* 进程管理 */
extern int sys_exit(int status);
extern int sys_fork(void);
extern int sys_execve(const char *path, char *const argv[], char *const envp[]);
extern int sys_getpid(void);
extern int sys_getppid(void);
extern int sys_waitpid(pid_t pid, int *status, int options);

/* 时间 */
extern int sys_time(uint32_t *tloc);

/* 文件I/O */
extern int sys_read(int fd, char *buf, size_t count);
extern int sys_write(int fd, const char *buf, size_t count);
extern int sys_open(const char *path, int flags, int mode);
extern int sys_close(int fd);
extern int sys_lseek(int fd, off_t offset, int whence);
extern int sys_creat(const char *path, mode_t mode);
extern int sys_unlink(const char *path);

/* 目录操作 */
extern int sys_mkdir(const char *path, mode_t mode);
extern int sys_rmdir(const char *path);
extern int sys_chdir(const char *path);

/* 内存管理 */
extern void *sys_brk(void *addr);
extern void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int sys_munmap(void *addr, size_t length);

/* ========== 系统调用表 ========== */

/*
 * 全局系统调用表
 * 
 * 索引 = 系统调用号
 * 值 = 系统调用函数指针
 */
syscall_func_t syscall_table[MAX_SYSCALLS] = {
    [0]             = NULL,                           /* 保留 */
    [SYS_exit]      = (syscall_func_t)sys_exit,       /* 1 */
    [SYS_fork]      = (syscall_func_t)sys_fork,       /* 2 */
    [SYS_read]      = (syscall_func_t)sys_read,       /* 3 */
    [SYS_write]     = (syscall_func_t)sys_write,      /* 4 */
    [SYS_open]      = (syscall_func_t)sys_open,       /* 5 */
    [SYS_close]     = (syscall_func_t)sys_close,      /* 6 */
    [SYS_waitpid]   = (syscall_func_t)sys_waitpid,    /* 7 */
    [SYS_creat]     = (syscall_func_t)sys_creat,      /* 8 */
    [SYS_unlink]    = (syscall_func_t)sys_unlink,     /* 10 */
    [SYS_execve]    = (syscall_func_t)sys_execve,     /* 11 */
    [SYS_chdir]     = (syscall_func_t)sys_chdir,      /* 12 */
    [SYS_time]      = (syscall_func_t)sys_time,       /* 13 */
    [SYS_lseek]     = (syscall_func_t)sys_lseek,      /* 19 */
    [SYS_getpid]    = (syscall_func_t)sys_getpid,     /* 20 */
    [SYS_mkdir]     = (syscall_func_t)sys_mkdir,      /* 39 */
    [SYS_rmdir]     = (syscall_func_t)sys_rmdir,      /* 40 */
    [SYS_brk]       = (syscall_func_t)sys_brk,        /* 45 */
    [SYS_getppid]   = (syscall_func_t)sys_getppid,    /* 64 */
    [SYS_mmap]      = (syscall_func_t)sys_mmap,       /* 90 */
    [SYS_munmap]    = (syscall_func_t)sys_munmap,     /* 91 */
};

