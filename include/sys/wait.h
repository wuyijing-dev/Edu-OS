/*
 * wait.h - POSIX wait 定义
 * 
 * 进程等待和退出状态宏
 */

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <types.h>

/* wait() 选项 */
#define WNOHANG    0x00000001  /* 非阻塞，如果没有子进程退出立即返回 */
#define WUNTRACED  0x00000002  /* 报告停止的子进程状态 */
#define WCONTINUED 0x00000008  /* 报告继续运行的子进程状态 */

/* 退出状态构造宏（Linux风格） */
#define __WEXITSTATUS(status)  (((status) & 0xff00) >> 8)
#define __WTERMSIG(status)     ((status) & 0x7f)
#define __WSTOPSIG(status)     __WEXITSTATUS(status)
#define __WIFEXITED(status)    (__WTERMSIG(status) == 0)
#define __WIFSIGNALED(status)  (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define __WIFSTOPPED(status)   (((status) & 0xff) == 0x7f)
#define __WIFCONTINUED(status) ((status) == 0xffff)
#define __WCOREDUMP(status)    ((status) & 0x80)

/* 用户空间退出状态宏 */
#ifndef __KERNEL__
#define WEXITSTATUS(status)  __WEXITSTATUS(status)
#define WTERMSIG(status)     __WTERMSIG(status)
#define WSTOPSIG(status)     __WSTOPSIG(status)
#define WIFEXITED(status)    __WIFEXITED(status)
#define WIFSIGNALED(status)  __WIFSIGNALED(status)
#define WIFSTOPPED(status)   __WIFSTOPPED(status)
#define WIFCONTINUED(status) __WIFCONTINUED(status)
#define WCOREDUMP(status)    __WCOREDUMP(status)
#endif

/* 构造退出状态 */
#define W_EXITCODE(ret, sig)  ((ret) << 8 | (sig))
#define W_STOPCODE(sig)       ((sig) << 8 | 0x7f)

/* 特殊PID值 */
#define WAIT_ANY       (-1)  /* 等待任何子进程 */
#define WAIT_MYPGRP    0     /* 等待同组进程 */

/* 系统调用原型 */
#ifndef __KERNEL__
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
pid_t wait3(int *status, int options, void *rusage);
pid_t wait4(pid_t pid, int *status, int options, void *rusage);
#endif

#endif /* _SYS_WAIT_H */

