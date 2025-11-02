/*
 * signal.c - POSIX信号实现
 * 
 * Linux风格信号处理机制
 */

#include <signal.h>
#include <process/process.h>
#include <process/scheduler.h>
#include <kernel.h>
#include <string.h>

/* errno错误码定义（不包含errno.h避免宏冲突） */
#define EINVAL  22
#define ESRCH   3
#define EFAULT  14
#define EINTR   4

/*
 * 默认信号处理动作
 */
enum sig_default_action {
    SIG_ACT_TERM,    /* 终止进程 */
    SIG_ACT_IGN,     /* 忽略信号 */
    SIG_ACT_CORE,    /* 终止并生成core dump */
    SIG_ACT_STOP,    /* 停止进程 */
    SIG_ACT_CONT,    /* 继续进程 */
};

/* 默认信号动作表 */
static const enum sig_default_action default_actions[_NSIG] = {
    [0] = SIG_ACT_IGN,          /* 0号信号不存在 */
    [SIGHUP] = SIG_ACT_TERM,
    [SIGINT] = SIG_ACT_TERM,
    [SIGQUIT] = SIG_ACT_CORE,
    [SIGILL] = SIG_ACT_CORE,
    [SIGTRAP] = SIG_ACT_CORE,
    [SIGABRT] = SIG_ACT_CORE,
    [SIGBUS] = SIG_ACT_CORE,
    [SIGFPE] = SIG_ACT_CORE,
    [SIGKILL] = SIG_ACT_TERM,   /* SIGKILL不可捕获 */
    [SIGUSR1] = SIG_ACT_TERM,
    [SIGSEGV] = SIG_ACT_CORE,
    [SIGUSR2] = SIG_ACT_TERM,
    [SIGPIPE] = SIG_ACT_TERM,
    [SIGALRM] = SIG_ACT_TERM,
    [SIGTERM] = SIG_ACT_TERM,
    [SIGSTKFLT] = SIG_ACT_TERM,
    [SIGCHLD] = SIG_ACT_IGN,
    [SIGCONT] = SIG_ACT_CONT,
    [SIGSTOP] = SIG_ACT_STOP,   /* SIGSTOP不可捕获 */
    [SIGTSTP] = SIG_ACT_STOP,
    [SIGTTIN] = SIG_ACT_STOP,
    [SIGTTOU] = SIG_ACT_STOP,
    [SIGURG] = SIG_ACT_IGN,
    [SIGXCPU] = SIG_ACT_CORE,
    [SIGXFSZ] = SIG_ACT_CORE,
    [SIGVTALRM] = SIG_ACT_TERM,
    [SIGPROF] = SIG_ACT_TERM,
    [SIGWINCH] = SIG_ACT_IGN,
    [SIGIO] = SIG_ACT_TERM,
    [SIGPWR] = SIG_ACT_TERM,
    [SIGSYS] = SIG_ACT_CORE,
};

/*
 * init_process_signals - 初始化进程的信号相关字段
 */
void init_process_signals(struct process *proc)
{
    if (!proc) return;
    
    proc->pending_signals = 0;
    proc->blocked_signals = 0;
    
    /* 初始化所有信号处理器为默认 */
    for (int i = 0; i < 32; i++) {
        proc->signal_handlers[i] = SIG_DFL;
        proc->signal_flags[i] = 0;
    }
}

/*
 * send_signal - 向进程发送信号
 * 
 * @pid: 目标进程ID
 * @sig: 信号编号
 * 
 * 返回：0=成功，-1=失败
 */
int send_signal(pid_t pid, int sig)
{
    /* 参数验证 */
    if (sig < 1 || sig >= _NSIG) {
        return -1;
    }
    
    /* 查找目标进程 */
    extern struct process *process_find_by_pid(pid_t pid);
    struct process *target = process_find_by_pid(pid);
    
    if (!target) {
        return -1;  /* 进程不存在 */
    }
    
    return send_signal_to_process(target, sig);
}

/*
 * send_signal_to_process - 向指定进程发送信号
 */
int send_signal_to_process(struct process *proc, int sig)
{
    if (!proc || sig < 1 || sig >= _NSIG) {
        return -1;
    }
    
    kprintf("[SIGNAL] Sending signal %d to process %s (PID %u)\n", 
            sig, proc->name, proc->pid);
    
    /* 设置待处理信号位 */
    proc->pending_signals |= (1U << (sig - 1));
    
    /* 如果进程正在阻塞且信号未被阻塞，唤醒它 */
    if (proc->state == PROCESS_STATE_BLOCKED) {
        if (!(proc->blocked_signals & (1U << (sig - 1)))) {
            proc->state = PROCESS_STATE_READY;
            kprintf("[SIGNAL] Woke up process %s (PID %u)\n", proc->name, proc->pid);
        }
    }
    
    return 0;
}

/*
 * signal_pending - 检查进程是否有待处理的信号
 */
int signal_pending(struct process *proc)
{
    if (!proc) return 0;
    
    /* 检查是否有未阻塞的待处理信号 */
    uint32_t pending = proc->pending_signals & ~proc->blocked_signals;
    return pending != 0;
}

/*
 * do_signal_action - 执行信号的默认动作
 */
static void do_signal_action(struct process *proc, int sig)
{
    enum sig_default_action action = default_actions[sig];
    
    switch (action) {
        case SIG_ACT_TERM:
        case SIG_ACT_CORE:
            kprintf("[SIGNAL] Process %s (PID %u) terminated by signal %d\n",
                    proc->name, proc->pid, sig);
            proc->state = PROCESS_STATE_TERMINATED;
            proc->exit_code = sig;  /* 退出码设为信号编号 */
            break;
            
        case SIG_ACT_STOP:
            kprintf("[SIGNAL] Process %s (PID %u) stopped by signal %d\n",
                    proc->name, proc->pid, sig);
            proc->state = PROCESS_STATE_BLOCKED;  /* 使用BLOCKED作为停止状态 */
            break;
            
        case SIG_ACT_CONT:
            kprintf("[SIGNAL] Process %s (PID %u) continued by signal %d\n",
                    proc->name, proc->pid, sig);
            if (proc->state == PROCESS_STATE_BLOCKED) {
                proc->state = PROCESS_STATE_READY;
            }
            break;
            
        case SIG_ACT_IGN:
            /* 忽略信号 */
            break;
    }
}

/*
 * handle_signals - 处理进程的待处理信号
 * 
 * 在系统调用返回前或中断返回前调用
 */
void handle_signals(struct process *proc)
{
    if (!proc) return;
    
    /* 获取未阻塞的待处理信号 */
    uint32_t pending = proc->pending_signals & ~proc->blocked_signals;
    
    if (!pending) return;
    
    /* 处理每个待处理的信号 */
    for (int sig = 1; sig < _NSIG; sig++) {
        if (!(pending & (1U << (sig - 1)))) {
            continue;
        }
        
        /* 清除待处理位 */
        proc->pending_signals &= ~(1U << (sig - 1));
        
        /* 获取信号处理器 */
        sighandler_t handler = (sighandler_t)proc->signal_handlers[sig - 1];
        
        if (handler == SIG_IGN) {
            /* 忽略信号 */
            continue;
        } else if (handler == SIG_DFL) {
            /* 执行默认动作 */
            do_signal_action(proc, sig);
            
            /* 如果进程被终止，不再处理其他信号 */
            if (proc->state == PROCESS_STATE_TERMINATED) {
                break;
            }
        } else {
            /* 调用用户态信号处理函数 */
            kprintf("[SIGNAL] Calling user handler %p for signal %d\n", handler, sig);
            
            /* TODO: 设置用户栈帧并调用处理函数
             * 这需要：
             * 1. 保存当前上下文
             * 2. 修改用户栈，压入返回地址和信号号
             * 3. 设置EIP为信号处理函数
             * 4. 返回用户空间执行处理函数
             * 5. 处理函数返回后恢复原上下文
             * 
             * 简化实现：暂时只支持内核空间调用
             */
            
            /* SIGKILL和SIGSTOP不能被捕获 */
            if (sig == SIGKILL || sig == SIGSTOP) {
                do_signal_action(proc, sig);
            }
        }
    }
}

/*
 * sys_kill - 发送信号给进程
 */
int sys_kill(pid_t pid, int sig)
{
    /* 参数验证 */
    if (sig < 0 || sig >= _NSIG) {
        extern int set_errno(int error_code);
        return set_errno(EINVAL);
    }
    
    /* sig=0用于检查进程是否存在 */
    if (sig == 0) {
        extern struct process *process_find_by_pid(pid_t pid);
        struct process *target = process_find_by_pid(pid);
        return target ? 0 : set_errno(ESRCH);
    }
    
    /* 发送信号 */
    if (send_signal(pid, sig) < 0) {
        extern int set_errno(int error_code);
        return set_errno(ESRCH);  /* No such process */
    }
    
    return 0;
}

/*
 * sys_sigaction - 设置信号处理动作
 */
int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 参数验证 */
    if (signum < 1 || signum >= _NSIG || signum == SIGKILL || signum == SIGSTOP) {
        extern int set_errno(int error_code);
        return set_errno(EINVAL);
    }
    
    /* 保存旧的信号处理动作 */
    if (oldact) {
        oldact->sa_handler = (sighandler_t)current->signal_handlers[signum - 1];
        oldact->sa_flags = current->signal_flags[signum - 1];
        /* sa_mask暂不支持 */
    }
    
    /* 设置新的信号处理动作 */
    if (act) {
        current->signal_handlers[signum - 1] = (void*)act->sa_handler;
        current->signal_flags[signum - 1] = act->sa_flags;
    }
    
    return 0;
}

/*
 * sys_signal - 简化版信号处理设置（兼容BSD）
 */
sighandler_t sys_signal(int signum, sighandler_t handler)
{
    struct sigaction act, oldact;
    
    act.sa_handler = handler;
    act.sa_flags = SA_RESTART;  /* 默认重启系统调用 */
    sigemptyset(&act.sa_mask);
    
    if (sys_sigaction(signum, &act, &oldact) < 0) {
        return SIG_ERR;
    }
    
    return oldact.sa_handler;
}

/*
 * sys_sigprocmask - 设置/获取信号掩码
 */
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 保存旧的掩码 */
    if (oldset) {
        oldset->sig[0] = current->blocked_signals;
        oldset->sig[1] = 0;  /* 简化版只支持32个信号 */
    }
    
    /* 设置新的掩码 */
    if (set) {
        uint32_t new_mask = set->sig[0];
        
        /* SIGKILL和SIGSTOP不能被阻塞 */
        new_mask &= ~((1U << (SIGKILL - 1)) | (1U << (SIGSTOP - 1)));
        
        switch (how) {
            case SIG_BLOCK:
                current->blocked_signals |= new_mask;
                break;
            case SIG_UNBLOCK:
                current->blocked_signals &= ~new_mask;
                break;
            case SIG_SETMASK:
                current->blocked_signals = new_mask;
                break;
            default:
                extern int set_errno(int error_code);
                return set_errno(EINVAL);
        }
    }
    
    return 0;
}

/*
 * sys_pause - 暂停进程直到收到信号
 */
int sys_pause(void)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 进程进入阻塞状态 */
    current->state = PROCESS_STATE_BLOCKED;
    
    /* 触发调度 */
    extern void scheduler_schedule(void);
    scheduler_schedule();
    
    /* 被信号唤醒后返回 */
    extern int set_errno(int error_code);
    return set_errno(EINTR);  /* Always returns -1 with EINTR */
}

