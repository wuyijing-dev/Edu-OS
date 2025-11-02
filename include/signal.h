/*
 * signal.h - POSIX信号定义
 * 
 * 符合POSIX.1-2017标准
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stdint.h>

/* POSIX标准信号编号 */
#define SIGHUP    1   /* Hangup (POSIX) */
#define SIGINT    2   /* Interrupt (ANSI) */
#define SIGQUIT   3   /* Quit (POSIX) */
#define SIGILL    4   /* Illegal instruction (ANSI) */
#define SIGTRAP   5   /* Trace trap (POSIX) */
#define SIGABRT   6   /* Abort (ANSI) */
#define SIGIOT    6   /* IOT trap (4.2 BSD) */
#define SIGBUS    7   /* BUS error (4.2 BSD) */
#define SIGFPE    8   /* Floating-point exception (ANSI) */
#define SIGKILL   9   /* Kill, unblockable (POSIX) */
#define SIGUSR1   10  /* User-defined signal 1 (POSIX) */
#define SIGSEGV   11  /* Segmentation violation (ANSI) */
#define SIGUSR2   12  /* User-defined signal 2 (POSIX) */
#define SIGPIPE   13  /* Broken pipe (POSIX) */
#define SIGALRM   14  /* Alarm clock (POSIX) */
#define SIGTERM   15  /* Termination (ANSI) */
#define SIGSTKFLT 16  /* Stack fault */
#define SIGCHLD   17  /* Child status has changed (POSIX) */
#define SIGCLD    SIGCHLD  /* Same as SIGCHLD (System V) */
#define SIGCONT   18  /* Continue (POSIX) */
#define SIGSTOP   19  /* Stop, unblockable (POSIX) */
#define SIGTSTP   20  /* Keyboard stop (POSIX) */
#define SIGTTIN   21  /* Background read from tty (POSIX) */
#define SIGTTOU   22  /* Background write to tty (POSIX) */
#define SIGURG    23  /* Urgent condition on socket (4.2 BSD) */
#define SIGXCPU   24  /* CPU limit exceeded (4.2 BSD) */
#define SIGXFSZ   25  /* File size limit exceeded (4.2 BSD) */
#define SIGVTALRM 26  /* Virtual alarm clock (4.2 BSD) */
#define SIGPROF   27  /* Profiling alarm clock (4.2 BSD) */
#define SIGWINCH  28  /* Window size change (4.3 BSD, Sun) */
#define SIGIO     29  /* I/O now possible (4.2 BSD) */
#define SIGPOLL   SIGIO  /* Pollable event occurred (System V) */
#define SIGPWR    30  /* Power failure restart (System V) */
#define SIGSYS    31  /* Bad system call */
#define SIGUNUSED 31

#define _NSIG     32  /* 信号总数（实际支持1-31） */

/* 信号集合类型 */
typedef struct {
    uint32_t sig[2];  /* 支持64个信号 (1-63) */
} sigset_t;

/* 信号处理函数类型 */
typedef void (*sighandler_t)(int);

/* 特殊信号处理器值 */
#define SIG_DFL ((sighandler_t)0)  /* 默认处理 */
#define SIG_IGN ((sighandler_t)1)  /* 忽略信号 */
#define SIG_ERR ((sighandler_t)-1) /* 错误返回 */

/* 前置声明 */
struct siginfo;

/* sigaction结构（POSIX标准） */
struct sigaction {
    union {
        sighandler_t sa_handler;    /* 简单信号处理函数 */
        void (*sa_sigaction)(int, struct siginfo *, void *);  /* 高级处理函数 */
    };
    sigset_t sa_mask;               /* 执行处理函数时要阻塞的信号 */
    int sa_flags;                   /* 标志 */
    void (*sa_restorer)(void);      /* 已废弃，但保留兼容性 */
};

/* sa_flags标志位 */
#define SA_NOCLDSTOP  1        /* 子进程停止时不产生SIGCHLD */
#define SA_NOCLDWAIT  2        /* 子进程终止时不产生僵尸进程 */
#define SA_SIGINFO    4        /* 使用sa_sigaction而不是sa_handler */
#define SA_ONSTACK    0x08000000  /* 在备用信号栈上执行 */
#define SA_RESTART    0x10000000  /* 重启被中断的系统调用 */
#define SA_NODEFER    0x40000000  /* 不阻塞当前信号 */
#define SA_RESETHAND  0x80000000  /* 执行后重置为SIG_DFL */

/* siginfo结构（简化版） */
struct siginfo {
    int si_signo;       /* 信号编号 */
    int si_errno;       /* 错误码 */
    int si_code;        /* 信号代码 */
    pid_t si_pid;       /* 发送信号的进程ID */
    uid_t si_uid;       /* 发送信号的用户ID */
    int si_status;      /* 退出状态或信号 */
    void *si_addr;      /* 引起fault的地址 */
    int si_value;       /* 信号值 */
};

/* sigprocmask的how参数 */
#define SIG_BLOCK     0  /* 阻塞信号集中的信号 */
#define SIG_UNBLOCK   1  /* 解除阻塞 */
#define SIG_SETMASK   2  /* 设置阻塞集合 */

/* 信号集合操作宏（内核和用户空间都可用） */
static inline int sigemptyset(sigset_t *set) {
    set->sig[0] = 0;
    set->sig[1] = 0;
    return 0;
}

static inline int sigfillset(sigset_t *set) {
    set->sig[0] = ~0U;
    set->sig[1] = ~0U;
    return 0;
}

static inline int sigaddset(sigset_t *set, int signum) {
    set->sig[(signum-1)/32] |= (1U << ((signum-1)%32));
    return 0;
}

static inline int sigdelset(sigset_t *set, int signum) {
    set->sig[(signum-1)/32] &= ~(1U << ((signum-1)%32));
    return 0;
}

static inline int sigismember(const sigset_t *set, int signum) {
    return (set->sig[(signum-1)/32] & (1U << ((signum-1)%32))) != 0;
}

/* 系统调用原型（用户空间） */
#ifndef __KERNEL__

/* 信号处理 */
sighandler_t signal(int signum, sighandler_t handler);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

/* 信号发送 */
int kill(pid_t pid, int sig);
int raise(int sig);

/* 信号掩码 */
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigsuspend(const sigset_t *mask);
int sigpending(sigset_t *set);

/* 信号等待 */
int pause(void);

#endif /* __KERNEL__ */

/* 内核空间函数原型 */
#ifdef __KERNEL__

/* 信号发送（内核使用） */
int send_signal(pid_t pid, int sig);
int send_signal_to_process(struct process *proc, int sig);

/* 信号处理（内核使用） */
void handle_signals(struct process *proc);
int signal_pending(struct process *proc);

#endif /* __KERNEL__ */

#endif /* _SIGNAL_H */
