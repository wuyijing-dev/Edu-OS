/*
 * syscall.c - 系统调用包装函数
 */

#include "unistd.h"

/* 系统调用号 */
#define SYS_exit     1
#define SYS_fork     2
#define SYS_read     3
#define SYS_write    4
#define SYS_open     5
#define SYS_close    6
#define SYS_waitpid  7
#define SYS_execve   11
#define SYS_getpid   20
#define SYS_getppid  64
#define SYS_mmap     90
#define SYS_munmap   91

/* 系统调用宏 */
#define SYSCALL0(name, num) \
int name(void) { \
    int ret; \
    asm volatile("int $0x80" : "=a"(ret) : "a"(num)); \
    return ret; \
}

#define SYSCALL1(name, num) \
int name(int arg1) { \
    int ret; \
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1)); \
    return ret; \
}

#define SYSCALL2(name, num) \
int name(int arg1, int arg2) { \
    int ret; \
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2)); \
    return ret; \
}

#define SYSCALL3(name, num) \
int name(int arg1, int arg2, int arg3) { \
    int ret; \
    asm volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)); \
    return ret; \
}

/* 进程控制 */
void _exit(int status)
{
    asm volatile("int $0x80" :: "a"(SYS_exit), "b"(status));
    while(1);  /* 永远不返回 */
}

SYSCALL0(_fork_impl, SYS_fork)
SYSCALL0(_getpid_impl, SYS_getpid)
SYSCALL0(_getppid_impl, SYS_getppid)

int _waitpid_impl(int pid, int *status, int options)
{
    int ret;
    asm volatile("int $0x80" 
        : "=a"(ret) 
        : "a"(SYS_waitpid), "b"(pid), "c"(status), "d"(options));
    return ret;
}

int _execve_impl(const char *path, char *const argv[], char *const envp[])
{
    int ret;
    asm volatile("int $0x80" 
        : "=a"(ret) 
        : "a"(SYS_execve), "b"(path), "c"(argv), "d"(envp));
    return ret;
}

/* I/O 操作 */
SYSCALL3(_read_impl, SYS_read)
SYSCALL3(_write_impl, SYS_write)
SYSCALL2(_open_impl, SYS_open)
SYSCALL1(_close_impl, SYS_close)

/* 包装函数 */
pid_t fork(void)
{
    return _fork_impl();
}

pid_t getpid(void)
{
    return _getpid_impl();
}

pid_t getppid(void)
{
    return _getppid_impl();
}

pid_t wait(int *status)
{
    return _waitpid_impl(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    return _waitpid_impl(pid, status, options);
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    return _execve_impl(path, argv, envp);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return _read_impl(fd, (int)buf, (int)count);
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return _write_impl(fd, (int)buf, (int)count);
}

int open(const char *path, int flags)
{
    return _open_impl((int)path, flags);
}

int close(int fd)
{
    return _close_impl(fd);
}

/* mmap系统调用（6个参数）
 * Linux old_mmap风格：ebx指向栈上的参数数组
 */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    /* 在栈上构建参数数组 */
    unsigned long args[6] = {
        (unsigned long)addr,
        (unsigned long)length,
        (unsigned long)prot,
        (unsigned long)flags,
        (unsigned long)fd,
        (unsigned long)offset
    };
    
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_mmap), "b"(args)
        : "memory"
    );
    return (void*)ret;
}

/* munmap系统调用（2个参数） */
int munmap(void *addr, size_t length)
{
    int ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(SYS_munmap), "b"(addr), "c"(length));
    return ret;
}
