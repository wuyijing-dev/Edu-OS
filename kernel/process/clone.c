/*
 * clone.c - clone系统调用实现
 * 对标Linux kernel/fork.c中的clone实现
 */

#include <process/clone.h>
#include <process/sched.h>
#include <mm/vmm.h>
#include <mm/kmalloc.h>
#include <fs/vfs.h>
#include <signal.h>
#include <errno.h>
#include <kernel.h>

/* 线程本地存储结构 */
struct thread_struct {
    /* 线程特定数据 */
    void *tls_base;             /* TLS基地址 */
    struct user_desc tls_desc;  /* TLS描述符 */
    
    /* 线程状态 */
    uint32_t flags;
    pid_t tid;                  /* 线程ID */
    pid_t tgid;                 /* 线程组ID */
    
    /* 线程组链表 */
    struct list_head thread_group;
    struct task_struct *group_leader;
};

/* 进程/线程结构体扩展 */
struct task_struct {
    /* 基础进程信息 */
    pid_t pid;                  /* 进程ID */
    pid_t tgid;                 /* 线程组ID */
    
    /* 线程组管理 */
    struct list_head thread_group;
    struct task_struct *group_leader;
    int thread_count;
    
    /* 线程本地存储 */
    struct thread_struct thread;
    
    /* 其他进程信息... */
    struct mm_struct *mm;       /* 内存管理 */
    struct fs_struct *fs;       /* 文件系统 */
    struct files_struct *files; /* 文件描述符 */
    struct signal_struct *signal; /* 信号处理 */
    
    /* 状态信息 */
    int state;
    int exit_code;
    
    /* 链表节点 */
    struct list_head tasks;
};

/* 复制进程信息 */
static int copy_process(unsigned long clone_flags, unsigned long stack_start,
                       struct pt_regs *regs, unsigned long stack_size,
                       int *parent_tidptr, int *child_tidptr)
{
    struct task_struct *tsk;
    int retval;
    
    /* 分配新任务结构 */
    tsk = alloc_task_struct();
    if (!tsk) {
        return -ENOMEM;
    }
    
    /* 初始化基本字段 */
    memset(tsk, 0, sizeof(struct task_struct));
    tsk->state = TASK_UNINTERRUPTIBLE;
    
    /* 分配PID */
    tsk->pid = alloc_pid();
    if (tsk->pid < 0) {
        retval = tsk->pid;
        goto bad_fork_cleanup_task;
    }
    
    /* 处理clone标志 */
    
    /* 复制或共享虚拟内存 */
    if (clone_flags & CLONE_VM) {
        /* 线程：共享虚拟内存 */
        tsk->mm = current->mm;
        atomic_inc(&tsk->mm->mm_users);
        tsk->thread.group_leader = current->group_leader ? current->group_leader : current;
    } else {
        /* 进程：复制虚拟内存 */
        tsk->mm = dup_mm(tsk);
        if (!tsk->mm) {
            retval = -ENOMEM;
            goto bad_fork_cleanup_pid;
        }
    }
    
    /* 复制或共享文件系统信息 */
    if (clone_flags & CLONE_FS) {
        tsk->fs = current->fs;
        atomic_inc(&tsk->fs->count);
    } else {
        tsk->fs = copy_fs_struct(current->fs);
        if (!tsk->fs) {
            retval = -ENOMEM;
            goto bad_fork_cleanup_mm;
        }
    }
    
    /* 复制或共享文件描述符 */
    if (clone_flags & CLONE_FILES) {
        tsk->files = current->files;
        atomic_inc(&tsk->files->count);
    } else {
        tsk->files = dup_fd(current->files);
        if (!tsk->files) {
            retval = -ENOMEM;
            goto bad_fork_cleanup_fs;
        }
    }
    
    /* 复制或共享信号处理 */
    if (clone_flags & CLONE_SIGHAND) {
        tsk->signal = current->signal;
        atomic_inc(&tsk->signal->count);
    } else {
        tsk->signal = copy_signal(current->signal);
        if (!tsk->signal) {
            retval = -ENOMEM;
            goto bad_fork_cleanup_files;
        }
    }
    
    /* 设置线程组信息 */
    if (clone_flags & CLONE_THREAD) {
        /* 属于同一线程组 */
        tsk->tgid = current->tgid;
        tsk->group_leader = current->group_leader ? current->group_leader : current;
        
        /* 添加到线程组 */
        list_add_tail(&tsk->thread_group, &tsk->group_leader->thread_group);
        tsk->group_leader->thread_count++;
    } else {
        /* 新进程组 */
        tsk->tgid = tsk->pid;
        tsk->group_leader = tsk;
        INIT_LIST_HEAD(&tsk->thread_group);
        tsk->thread_count = 1;
    }
    
    /* 设置线程本地存储 */
    if (clone_flags & CLONE_SETTLS) {
        /* TODO: 设置TLS */
        if (child_tidptr) {
            tsk->thread.tls_base = child_tidptr;
        }
    }
    
    /* 设置父进程和子进程TID */
    if (parent_tidptr) {
        *parent_tidptr = tsk->pid;
    }
    
    if (child_tidptr && (clone_flags & CLONE_CHILD_SETTID)) {
        tsk->thread.tls_desc.base_addr = (unsigned int)child_tidptr;
        *child_tidptr = tsk->pid;
    }
    
    /* 设置栈 */
    if (stack_start) {
        tsk->thread.esp = stack_start;
    }
    
    /* 复制寄存器状态 */
    copy_thread(tsk, regs);
    
    /* 设置任务状态为就绪 */
    tsk->state = TASK_RUNNING;
    
    /* 添加到调度器 */
    wake_up_new_task(tsk);
    
    return tsk->pid;
    
bad_fork_cleanup_files:
    put_files_struct(tsk->files);
bad_fork_cleanup_fs:
    put_fs_struct(tsk->fs);
bad_fork_cleanup_mm:
    if (!(clone_flags & CLONE_VM)) {
        mmput(tsk->mm);
    }
bad_fork_cleanup_pid:
    free_pid(tsk->pid);
bad_fork_cleanup_task:
    free_task_struct(tsk);
    return retval;
}

/* clone系统调用 */
int sys_clone(unsigned long clone_flags, unsigned long newsp,
              void *parent_tid, void *child_tid, struct user_desc *tls)
{
    struct pt_regs *regs = task_pt_regs(current);
    
    /* 验证clone标志 */
    if (clone_flags & ~(CSIGNAL | CLONE_VM | CLONE_FS | CLONE_FILES | 
                       CLONE_SIGHAND | CLONE_PID | CLONE_PTRACE |
                       CLONE_VFORK | CLONE_PARENT | CLONE_THREAD |
                       CLONE_NEWNS | CLONE_SYSVSEM | CLONE_SETTLS |
                       CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID |
                       CLONE_DETACHED | CLONE_UNTRACED |
                       CLONE_CHILD_SETTID | CLONE_NEWUTS |
                       CLONE_NEWIPC | CLONE_NEWUSER | CLONE_NEWPID |
                       CLONE_NEWNET | CLONE_IO)) {
        return -EINVAL;
    }
    
    /* 检查线程组标志 */
    if ((clone_flags & (CLONE_NEWUSER | CLONE_NEWPID)) &&
        !(clone_flags & CLONE_THREAD)) {
        return -EINVAL;
    }
    
    /* 设置新的栈指针 */
    if (newsp) {
        regs->sp = newsp;
    }
    
    /* 调用核心fork函数 */
    return do_fork(clone_flags, regs->sp, regs, 0, parent_tid, child_tid);
}

/* fork系统调用 */
int sys_fork(void)
{
    return sys_clone(SIGCHLD, 0, NULL, NULL, NULL);
}

/* vfork系统调用 */
int sys_vfork(void)
{
    return sys_clone(CLONE_VFORK | CLONE_VM | SIGCHLD, 0, NULL, NULL, NULL);
}

/* 线程退出 */
void exit_thread(void)
{
    struct task_struct *tsk = current;
    
    /* 如果是线程组最后一个成员 */
    if (tsk->group_leader && tsk->group_leader != tsk) {
        /* 从线程组移除 */
        list_del(&tsk->thread_group);
        tsk->group_leader->thread_count--;
        
        /* 如果线程组leader已退出，且这是最后一个线程 */
        if (tsk->group_leader->state == TASK_ZOMBIE && 
            tsk->group_leader->thread_count == 0) {
            /* 释放线程组资源 */
            release_thread_group(tsk->group_leader);
        }
    }
}

/* 线程组退出 */
void thread_group_exit(struct task_struct *tsk, int exit_code)
{
    struct task_struct *g, *t;
    
    /* 向线程组所有成员发送SIGKILL */
    do_each_thread(g, t) {
        if (t->tgid == tsk->tgid && t != tsk) {
            send_sig(SIGKILL, t, 1);
        }
    } while_each_thread(g, t);
}

/* 复制线程状态 */
static void copy_thread(struct task_struct *tsk, struct pt_regs *regs)
{
    struct pt_regs *childregs = task_pt_regs(tsk);
    
    /* 复制寄存器状态 */
    *childregs = *regs;
    
    /* 设置子进程返回值 */
    childregs->ax = 0;
    
    /* 设置内核栈 */
    tsk->thread.sp = (unsigned long)childregs;
    tsk->thread.ip = (unsigned long)ret_from_fork;
}

/* 新任务唤醒 */
static void wake_up_new_task(struct task_struct *tsk)
{
    /* 设置调度参数 */
    tsk->prio = current->prio;
    tsk->static_prio = current->static_prio;
    tsk->normal_prio = current->normal_prio;
    
    /* 添加到运行队列 */
    activate_task(tsk);
}