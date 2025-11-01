/*
 * signal.h - POSIX信号处理
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

/* 信号类型 */
typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);

/* 标准信号 */
#define SIGHUP      1   /* Hangup */
#define SIGINT      2   /* Interrupt */
#define SIGQUIT     3   /* Quit */
#define SIGILL      4   /* Illegal instruction */
#define SIGTRAP     5   /* Trace trap */
#define SIGABRT     6   /* Abort */
#define SIGBUS      7   /* Bus error */
#define SIGFPE      8   /* Floating point exception */
#define SIGKILL     9   /* Kill */
#define SIGUSR1     10  /* User defined signal 1 */
#define SIGSEGV     11  /* Segmentation violation */
#define SIGUSR2     12  /* User defined signal 2 */
#define SIGPIPE     13  /* Broken pipe */
#define SIGALRM     14  /* Alarm clock */
#define SIGTERM     15  /* Termination */
#define SIGCHLD     17  /* Child status changed */
#define SIGCONT     18  /* Continue */
#define SIGSTOP     19  /* Stop */

/* 特殊信号处理器 */
#define SIG_DFL     ((sighandler_t)0)  /* Default action */
#define SIG_IGN     ((sighandler_t)1)  /* Ignore signal */
#define SIG_ERR     ((sighandler_t)-1) /* Error return */

/* 函数声明 */
sighandler_t signal(int signum, sighandler_t handler);
int kill(int pid, int sig);
int raise(int sig);

#endif /* _SIGNAL_H */

