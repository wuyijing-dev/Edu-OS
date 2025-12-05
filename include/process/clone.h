/*
 * clone.h - clone系统调用头文件
 * 对标Linux include/linux/sched.h中的clone相关定义
 */

#ifndef _CLONE_H
#define _CLONE_H

#include <stdint.h>
#include <sys/types.h>

/* clone标志位 */
#define CSIGNAL         0x000000FF      /* 信号掩码 */
#define CLONE_VM        0x00000100      /* 共享虚拟内存 */
#define CLONE_FS        0x00000200      /* 共享文件系统信息 */
#define CLONE_FILES     0x00000400      /* 共享文件描述符 */
#define CLONE_SIGHAND   0x00000800      /* 共享信号处理 */
#define CLONE_PID       0x00001000      /* 共享PID (废弃) */
#define CLONE_PTRACE    0x00002000      /* 继续ptrace */
#define CLONE_VFORK     0x00004000      /* 父进程睡眠直到子进程退出 */
#define CLONE_PARENT    0x00008000      /* 共享父进程 */
#define CLONE_THREAD    0x00010000      /* 同一线程组 */
#define CLONE_NEWNS     0x00020000      /* 新命名空间 */
#define CLONE_SYSVSEM   0x00040000      /* 共享System V SEM_UNDO */
#define CLONE_SETTLS    0x00080000      /* 设置TLS */
#define CLONE_PARENT_SETTID 0x00100000  /* 设置父进程TID */
#define CLONE_CHILD_CLEARTID 0x00200000 /* 清除子进程TID */
#define CLONE_DETACHED  0x00400000      /* 分离 (废弃) */
#define CLONE_UNTRACED  0x00800000      /* 不可ptrace */
#define CLONE_CHILD_SETTID 0x01000000   /* 设置子进程TID */
#define CLONE_NEWUTS    0x04000000      /* 新UTS命名空间 */
#define CLONE_NEWIPC    0x08000000      /* 新IPC命名空间 */
#define CLONE_NEWUSER   0x10000000      /* 新用户命名空间 */
#define CLONE_NEWPID    0x20000000      /* 新PID命名空间 */
#define CLONE_NEWNET    0x40000000      /* 新网络命名空间 */
#define CLONE_IO        0x80000000      /* 共享IO上下文 */

/* 线程本地存储结构 */
struct user_desc {
    unsigned int entry_number;
    unsigned int base_addr;
    unsigned int limit;
    unsigned int seg_32bit:1;
    unsigned int contents:2;
    unsigned int read_exec_only:1;
    unsigned int limit_in_pages:1;
    unsigned int seg_not_present:1;
    unsigned int useable:1;
};

/* clone系统调用 */
int sys_clone(unsigned long clone_flags, unsigned long newsp, 
              void *parent_tid, void *child_tid, struct user_desc *tls);

/* 线程创建 */
int do_fork(unsigned long clone_flags, unsigned long stack_start,
            struct pt_regs *regs, unsigned long stack_size,
            int *parent_tidptr, int *child_tidptr);

/* 线程组管理 */
struct task_struct;

void thread_group_exit(struct task_struct *tsk, int exit_code);
int thread_group_cputime(struct task_struct *tsk, struct task_cputime *times);
void exit_thread(void);

#endif /* _CLONE_H */