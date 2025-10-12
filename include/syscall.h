/*
 * syscall.h - 系统调用接口定义
 * 
 * 基于 Linux x86 系统调用规范
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <types.h>

/* ========== 系统调用号（Linux 兼容）========== */

#define SYS_exit        1
#define SYS_fork        2
#define SYS_read        3
#define SYS_write       4
#define SYS_open        5
#define SYS_close       6
#define SYS_waitpid     7
#define SYS_creat       8
#define SYS_unlink      10
#define SYS_execve      11
#define SYS_chdir       12
#define SYS_time        13
#define SYS_mknod       14
#define SYS_chmod       15
#define SYS_lseek       19
#define SYS_getpid      20
#define SYS_mkdir       39
#define SYS_rmdir       40
#define SYS_dup         41
#define SYS_pipe        42
#define SYS_brk         45
#define SYS_signal      48
#define SYS_ioctl       54
#define SYS_fcntl       55
#define SYS_dup2        63
#define SYS_getppid     64
#define SYS_setsid      66
#define SYS_sigaction   67
#define SYS_readdir     89
#define SYS_mmap        90
#define SYS_munmap      91
#define SYS_fstat       108
#define SYS_clone       120
#define SYS_nanosleep   162

/* 最大系统调用号 */
#define MAX_SYSCALLS    256

/* ========== 系统调用函数类型 ========== */

typedef int (*syscall_func_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/* ========== 中断帧结构 ========== */

struct syscall_frame {
    /* 段寄存器 */
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;
    
    /* pusha 保存的通用寄存器 */
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;  /* pusha 压入但被忽略 */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    
    /* 中断号和错误码 */
    uint32_t int_no;
    uint32_t err_code;
    
    /* 中断时 CPU 自动压入 */
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
    uint32_t user_esp;
    uint32_t ss;
} __attribute__((packed));

/* ========== 内核系统调用接口 ========== */

/*
 * 初始化系统调用
 */
void syscall_init(void);

/*
 * 系统调用分发器
 */
int syscall_dispatch(struct syscall_frame *frame);

/*
 * 地址验证
 */
bool is_user_address(const void *addr);
bool is_user_buffer(const void *buf, size_t size);

/* ========== 具体系统调用实现 ========== */

/* 进程管理 */
int sys_exit(int status);
int sys_fork(void);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
int sys_getpid(void);
int sys_getppid(void);
int sys_waitpid(pid_t pid, int *status, int options);

/* 文件I/O */
int sys_read(int fd, char *buf, size_t count);
int sys_write(int fd, const char *buf, size_t count);
int sys_open(const char *path, int flags, int mode);
int sys_close(int fd);
int sys_lseek(int fd, off_t offset, int whence);

/* 文件系统 */
int sys_creat(const char *path, mode_t mode);
int sys_unlink(const char *path);
int sys_mkdir(const char *path, mode_t mode);
int sys_rmdir(const char *path);
int sys_chdir(const char *path);

/* 内存管理 */
void *sys_brk(void *addr);
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);

#endif // SYSCALL_H

