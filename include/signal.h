/*
 * signal.h - 信号处理接口
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <types.h>

/* 信号编号 */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGBUS    7
#define SIGFPE    8
#define SIGKILL   9
#define SIGUSR1   10
#define SIGSEGV   11
#define SIGUSR2   12
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19
#define SIGTSTP   20

#define NSIG      32

/* 信号处理函数类型 */
typedef void (*sighandler_t)(int);

/* 特殊处理器 */
#define SIG_DFL  ((sighandler_t)0)
#define SIG_IGN  ((sighandler_t)1)

/* 信号集类型 */
typedef uint32_t sigset_t;

/* 前向声明 */
struct process;

/* 信号管理函数 */
void signal_init_process(struct process *proc);
void signal_deliver(struct process *proc);

/* 系统调用 */
sighandler_t sys_signal(int signum, sighandler_t handler);
int sys_kill(pid_t pid, int sig);
int sys_sigaction(int signum, const void *act, void *oldact);
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

#endif /* _SIGNAL_H */
