/*
 * vfork.c - vfork系统调用实现
 * 对标Linux kernel/fork.c中的vfork实现
 */

#include <process/clone.h>
#include <process/sched.h>
#include <mm/vmm.h>
#include <kernel.h>

/* vfork完成等待结构 */
struct vfork_wait {
    struct completion completion;
    struct mm_struct *mm;
};

/* vfork系统调用 */
int sys_vfork(void)
{
    struct vfork_wait vfork;
    struct pt_regs *regs = task_pt_regs(current);
    int pid;
    
    /* 初始化完成量 */
    init_completion(&vfork.completion);
    
    /* 调用clone创建子进程 */
    pid = sys_clone(CLONE_VFORK | CLONE_VM | SIGCHLD, 0, NULL, NULL, NULL);
    
    if (pid < 0) {
        return pid;
    }
    
    if (pid == 0) {
        /* 子进程 */
        return 0;
    }
    
    /* 父进程：等待子进程退出或执行exec */
    wait_for_completion(&vfork.completion);
    
    return pid;
}

/* vfork子进程完成 */
void vfork_done(struct task_struct *child)
{
    struct vfork_wait *vfork;
    
    /* 获取vfork等待结构 */
    vfork = &current->vfork_done;
    
    /* 唤醒等待的父进程 */
    complete(&vfork->completion);
}

/* execve时处理vfork */
void vfork_execve_done(struct task_struct *tsk)
{
    struct task_struct *parent;
    
    /* 检查是否是vfork子进程 */
    if (tsk->flags & PF_VFORK) {
        parent = tsk->parent;
        
        /* 分离内存空间 */
        if (tsk->mm == parent->mm) {
            tsk->mm = allocate_mm();
            if (tsk->mm) {
                memcpy(tsk->mm, parent->mm, sizeof(struct mm_struct));
                atomic_set(&tsk->mm->mm_users, 1);
            }
        }
        
        /* 完成vfork */
        vfork_done(tsk);
    }
}