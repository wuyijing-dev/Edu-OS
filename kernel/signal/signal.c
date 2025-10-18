/*
 * signal.c - 信号处理机制
 * 
 * 实现类Unix信号系统
 */

#include <kernel.h>
#include <process/process.h>
#include <mm/kmalloc.h>
#include <string.h>

/* 信号定义 */
#define SIGHUP    1   /* Hangup */
#define SIGINT    2   /* Interrupt (Ctrl+C) */
#define SIGQUIT   3   /* Quit */
#define SIGILL    4   /* Illegal instruction */
#define SIGTRAP   5   /* Trace trap */
#define SIGABRT   6   /* Abort */
#define SIGBUS    7   /* Bus error */
#define SIGFPE    8   /* Floating point exception */
#define SIGKILL   9   /* Kill (不可捕获) */
#define SIGUSR1   10  /* User signal 1 */
#define SIGSEGV   11  /* Segmentation fault */
#define SIGUSR2   12  /* User signal 2 */
#define SIGPIPE   13  /* Broken pipe */
#define SIGALRM   14  /* Alarm clock */
#define SIGTERM   15  /* Termination */
#define SIGCHLD   17  /* Child status changed */
#define SIGCONT   18  /* Continue */
#define SIGSTOP   19  /* Stop (不可捕获) */
#define SIGTSTP   20  /* Keyboard stop */

#define NSIG      32  /* 最大信号数 */

/* 信号处理函数类型 */
typedef void (*sighandler_t)(int);

/* 特殊信号处理器 */
#define SIG_DFL  ((sighandler_t)0)   /* 默认处理 */
#define SIG_IGN  ((sighandler_t)1)   /* 忽略信号 */

/* 信号集 */
typedef uint32_t sigset_t;

/* 进程信号信息（添加到PCB） */
struct signal_struct {
    sighandler_t handlers[NSIG];  /* 信号处理函数表 */
    sigset_t pending;             /* 待处理信号 */
    sigset_t blocked;             /* 阻塞的信号 */
};

/*
 * 初始化进程信号结构
 */
void signal_init_process(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 分配信号结构 */
    struct signal_struct *sig = kmalloc(sizeof(struct signal_struct));
    if (!sig) {
        kprintf("[SIGNAL] Failed to allocate signal struct\n");
        return;
    }
    
    memset(sig, 0, sizeof(struct signal_struct));
    
    /* 设置所有信号为默认处理 */
    for (int i = 0; i < NSIG; i++) {
        sig->handlers[i] = SIG_DFL;
    }
    
    /* TODO: 将sig保存到proc中 */
    
    kprintf("[SIGNAL] Initialized signal handling for process %u\n", proc->pid);
}

/*
 * sys_signal - 设置信号处理函数
 * 
 * @param signum: 信号编号
 * @param handler: 处理函数
 * @return: 旧的处理函数
 */
sighandler_t sys_signal(int signum, sighandler_t handler)
{
    if (signum < 1 || signum >= NSIG) {
        kprintf("[SIGNAL] Invalid signal number: %d\n", signum);
        return SIG_DFL;
    }
    
    /* SIGKILL和SIGSTOP不能捕获 */
    if (signum == SIGKILL || signum == SIGSTOP) {
        kprintf("[SIGNAL] Cannot catch SIGKILL or SIGSTOP\n");
        return SIG_DFL;
    }
    
    struct process *proc = process_get_current();
    if (!proc) {
        return SIG_DFL;
    }
    
    /* TODO: 从proc获取signal_struct */
    /* TODO: 保存旧处理器 */
    /* TODO: 设置新处理器 */
    
    kprintf("[SIGNAL] Process %u set handler for signal %d\n", proc->pid, signum);
    
    return SIG_DFL;
}

/*
 * sys_kill - 向进程发送信号
 * 
 * @param pid: 目标进程ID
 * @param sig: 信号编号
 * @return: 成功返回0，失败返回-1
 */
int sys_kill(pid_t pid, int sig)
{
    if (sig < 0 || sig >= NSIG) {
        return -EINVAL;
    }
    
    kprintf("[SIGNAL] Sending signal %d to process %u\n", sig, pid);
    
    /* 查找目标进程 */
    struct process *target = process_find_by_pid(pid);
    if (!target) {
        return -ESRCH;  /* 进程不存在 */
    }
    
    /* TODO: 检查权限 */
    /* TODO: 将信号添加到目标进程的pending集 */
    
    /* 特殊信号立即处理 */
    switch (sig) {
        case SIGKILL:
            /* 强制终止 */
            kprintf("[SIGNAL] SIGKILL: terminating process %u\n", pid);
            target->state = PROCESS_STATE_TERMINATED;
            target->exit_code = 128 + sig;
            break;
            
        case SIGSTOP:
            /* 暂停进程 */
            kprintf("[SIGNAL] SIGSTOP: stopping process %u\n", pid);
            target->state = PROCESS_STATE_BLOCKED;
            break;
            
        case SIGCONT:
            /* 继续进程 */
            if (target->state == PROCESS_STATE_BLOCKED) {
                kprintf("[SIGNAL] SIGCONT: resuming process %u\n", pid);
                target->state = PROCESS_STATE_READY;
            }
            break;
            
        default:
            /* 其他信号：标记为pending，等待进程处理 */
            break;
    }
    
    return 0;
}

/*
 * 信号分发（在系统调用返回时检查）
 */
void signal_deliver(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* TODO: 从proc获取signal_struct */
    /* TODO: 检查pending信号 */
    /* TODO: 调用用户空间的信号处理函数 */
    
    kprintf("[SIGNAL] Checking signals for process %u\n", proc->pid);
}

/*
 * 默认信号处理
 */
static void signal_default_handler(struct process *proc, int sig)
{
    kprintf("[SIGNAL] Process %u received signal %d (default handler)\n", 
            proc->pid, sig);
    
    switch (sig) {
        case SIGINT:
        case SIGTERM:
        case SIGQUIT:
            /* 终止进程 */
            proc->state = PROCESS_STATE_TERMINATED;
            proc->exit_code = 128 + sig;
            break;
            
        case SIGSEGV:
        case SIGILL:
        case SIGFPE:
        case SIGBUS:
            /* 核心转储并终止 */
            kprintf("[SIGNAL] Fatal signal, dumping core\n");
            proc->state = PROCESS_STATE_TERMINATED;
            proc->exit_code = 128 + sig;
            break;
            
        case SIGCHLD:
        case SIGCONT:
            /* 忽略 */
            break;
            
        default:
            /* 其他信号：忽略 */
            break;
    }
}
