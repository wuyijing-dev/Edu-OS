/**
 * sys_nice.c - nice/setpriority/getpriority系统调用
 * 
 * 支持POSIX标准的进程优先级调整
 */

#include <process/process.h>
#include <process/cfs_sched.h>
#include <kernel.h>
#include <errno.h>

/**
 * nice系统调用
 * 调整进程的nice值（相对调整）
 * 
 * @param increment nice值增量（-20到+19）
 * @return 成功返回新的nice值，失败返回-1
 */
int sys_nice(int increment)
{
    struct process *curr = process_get_current();
    if (!curr) {
        return -ESRCH;
    }
    
    /* 计算新的nice值 */
    int new_nice = curr->nice + increment;
    
    /* 限制范围 */
    if (new_nice < -20) new_nice = -20;
    if (new_nice > 19) new_nice = 19;
    
    /* 更新nice值 */
    cfs_set_task_nice(curr, new_nice);
    
    kprintf("[NICE] Process '%s' (PID %u) nice: %d -> %d\n",
            curr->name, curr->pid, curr->nice - increment, new_nice);
    
    return new_nice;
}

/**
 * setpriority系统调用
 * 设置进程、进程组或用户的调度优先级（绝对设置）
 * 
 * @param which 目标类型（PRIO_PROCESS/PRIO_PGRP/PRIO_USER）
 * @param who 目标ID（0表示当前进程）
 * @param prio 新的nice值（-20到+19）
 * @return 成功返回0，失败返回-1
 */
int sys_setpriority(int which, int who, int prio)
{
    /* 限制nice值范围 */
    if (prio < -20) prio = -20;
    if (prio > 19) prio = 19;
    
    struct process *target = NULL;
    
    switch (which) {
        case 0:  /* PRIO_PROCESS */
            if (who == 0) {
                /* 当前进程 */
                target = process_get_current();
            } else {
                /* 指定PID */
                target = process_find_by_pid(who);
            }
            
            if (!target) {
                return -ESRCH;
            }
            
            cfs_set_task_nice(target, prio);
            
            kprintf("[SETPRIORITY] Process '%s' (PID %u) set nice to %d\n",
                    target->name, target->pid, prio);
            break;
            
        case 1:  /* PRIO_PGRP */
            kprintf("[SETPRIORITY] PRIO_PGRP not yet implemented\n");
            return -ENOSYS;
            
        case 2:  /* PRIO_USER */
            kprintf("[SETPRIORITY] PRIO_USER not yet implemented\n");
            return -ENOSYS;
            
        default:
            return -EINVAL;
    }
    
    return 0;
}

/**
 * getpriority系统调用
 * 获取进程、进程组或用户的调度优先级
 * 
 * @param which 目标类型（PRIO_PROCESS/PRIO_PGRP/PRIO_USER）
 * @param who 目标ID（0表示当前进程）
 * @return 成功返回nice值（偏移20，避免负数），失败返回-1
 */
int sys_getpriority(int which, int who)
{
    struct process *target = NULL;
    
    switch (which) {
        case 0:  /* PRIO_PROCESS */
            if (who == 0) {
                /* 当前进程 */
                target = process_get_current();
            } else {
                /* 指定PID */
                target = process_find_by_pid(who);
            }
            
            if (!target) {
                return -ESRCH;
            }
            
            /* 返回nice值+20（0-40），避免返回负数 */
            return target->nice + 20;
            
        case 1:  /* PRIO_PGRP */
            kprintf("[GETPRIORITY] PRIO_PGRP not yet implemented\n");
            return -ENOSYS;
            
        case 2:  /* PRIO_USER */
            kprintf("[GETPRIORITY] PRIO_USER not yet implemented\n");
            return -ENOSYS;
            
        default:
            return -EINVAL;
    }
}

/**
 * sched_setscheduler系统调用
 * 设置进程的调度策略和参数
 * 
 * @param pid 进程ID（0表示当前进程）
 * @param policy 调度策略（SCHED_NORMAL/SCHED_FIFO/SCHED_RR等）
 * @param param 调度参数（实时优先级等）
 * @return 成功返回0，失败返回-1
 */
int sys_sched_setscheduler(pid_t pid, int policy, const void *param)
{
    struct process *target;
    
    if (pid == 0) {
        target = process_get_current();
    } else {
        target = process_find_by_pid(pid);
    }
    
    if (!target) {
        return -ESRCH;
    }
    
    /* 验证调度策略 */
    if (policy < SCHED_NORMAL || policy > SCHED_IDLE) {
        return -EINVAL;
    }
    
    target->policy = policy;
    
    kprintf("[SCHED] Process '%s' (PID %u) policy set to %d\n",
            target->name, target->pid, policy);
    
    /* TODO: 处理param参数（实时优先级等）*/
    (void)param;
    
    return 0;
}

/**
 * sched_getscheduler系统调用
 * 获取进程的调度策略
 * 
 * @param pid 进程ID（0表示当前进程）
 * @return 成功返回调度策略，失败返回-1
 */
int sys_sched_getscheduler(pid_t pid)
{
    struct process *target;
    
    if (pid == 0) {
        target = process_get_current();
    } else {
        target = process_find_by_pid(pid);
    }
    
    if (!target) {
        return -ESRCH;
    }
    
    return target->policy;
}

/**
 * sched_yield系统调用
 * 主动放弃CPU，让其他进程运行
 * 
 * @return 总是返回0
 */
int sys_sched_yield(void)
{
    extern void scheduler_yield(void);
    scheduler_yield();
    return 0;
}

