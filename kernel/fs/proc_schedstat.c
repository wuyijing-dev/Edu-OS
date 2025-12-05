/**
 * proc_schedstat.c - /proc/schedstat 实现
 * 
 * 提供详细的调度器统计信息
 */

#include <fs/vfs.h>
#include <process/process.h>
#include <process/scheduler.h>
#include <kernel.h>
#include <string.h>
#include <errno.h>

/* 调度器统计缓冲区 */
static char schedstat_buffer[4096];
static size_t schedstat_size = 0;

/**
 * 更新调度统计信息
 */
static void update_schedstat(void)
{
    char *buf = schedstat_buffer;
    size_t remaining = sizeof(schedstat_buffer);
    int written;
    
    /* 标题 */
    written = snprintf(buf, remaining,
        "=== EduOS Scheduler Statistics ===\n\n");
    buf += written;
    remaining -= written;
    
    /* 获取调度器统计 */
    extern struct process *scheduler_get_current(void);
    struct process *curr = scheduler_get_current();
    
    if (curr) {
        written = snprintf(buf, remaining,
            "Current Process:\n"
            "  Name:           %s\n"
            "  PID:            %u\n"
            "  State:          %d\n"
            "  Policy:         %d\n"
            "  Priority:       %u\n"
            "  Nice:           %d\n"
            "  Time slice:     %u\n"
            "  Total runtime:  %llu\n\n",
            curr->name,
            curr->pid,
            curr->state,
            curr->policy,
            curr->priority,
            curr->nice,
            curr->time_slice,
            curr->total_runtime);
        buf += written;
        remaining -= written;
    }
    
    /* CFS统计 */
    written = snprintf(buf, remaining,
        "CFS Scheduler:\n"
        "  Target latency:   6 ms\n"
        "  Min granularity:  750 us\n\n");
    buf += written;
    remaining -= written;
    
    /* 实时调度器统计 */
    written = snprintf(buf, remaining,
        "Real-Time Scheduler:\n"
        "  Priority range:   0-99\n"
        "  RR timeslice:     100 ms\n\n");
    buf += written;
    remaining -= written;
    
    /* 进程列表统计 */
    written = snprintf(buf, remaining,
        "Process Statistics:\n");
    buf += written;
    remaining -= written;
    
    /* 遍历所有进程 */
    extern struct process *process_list_head;  /* EduOS使用简单双向链表 */
    struct process *proc;
    int process_count = 0;
    int running_count = 0;
    int ready_count = 0;
    int blocked_count = 0;
    
    /* 收集每个进程的详细信息 */
    proc = process_list_head;
    while (proc) {
        process_count++;
        switch (proc->state) {
            case PROCESS_STATE_RUNNING:
                running_count++;
                break;
            case PROCESS_STATE_READY:
                ready_count++;
                break;
            case PROCESS_STATE_BLOCKED:
                blocked_count++;
                break;
            default:
                break;
        }
        proc = proc->next;  /* 移动到下一个进程 */
    }
    
    written = snprintf(buf, remaining,
        "  Total processes:  %d\n"
        "  Running:          %d\n"
        "  Ready:            %d\n"
        "  Blocked:          %d\n\n",
        process_count,
        running_count,
        ready_count,
        blocked_count);
    buf += written;
    remaining -= written;
    
    /* 详细进程信息 */
    written = snprintf(buf, remaining,
        "Process Details:\n"
        "%-6s %-20s %-8s %-8s %-6s %-10s %-12s\n",
        "PID", "NAME", "STATE", "POLICY", "NICE", "RUNTIME", "VRUNTIME");
    buf += written;
    remaining -= written;
    
    proc = process_list_head;
    while (proc) {
        const char *state_str;
        switch (proc->state) {
            case PROCESS_STATE_NEW:        state_str = "NEW"; break;
            case PROCESS_STATE_READY:      state_str = "READY"; break;
            case PROCESS_STATE_RUNNING:    state_str = "RUNNING"; break;
            case PROCESS_STATE_BLOCKED:    state_str = "BLOCKED"; break;
            case PROCESS_STATE_TERMINATED: state_str = "TERM"; break;
            default:                       state_str = "UNKNOWN"; break;
        }
        
        const char *policy_str;
        switch (proc->policy) {
            case SCHED_NORMAL: policy_str = "NORMAL"; break;
            case SCHED_FIFO:   policy_str = "FIFO"; break;
            case SCHED_RR:     policy_str = "RR"; break;
            case SCHED_BATCH:  policy_str = "BATCH"; break;
            case SCHED_IDLE:   policy_str = "IDLE"; break;
            default:           policy_str = "?"; break;
        }
        
        written = snprintf(buf, remaining,
            "%-6u %-20s %-8s %-8s %-6d %-10llu %-12llu\n",
            proc->pid,
            proc->name,
            state_str,
            policy_str,
            proc->nice,
            proc->total_runtime,
            proc->se.vruntime);
        
        if (written < 0 || (size_t)written >= remaining) {
            break;  /* 缓冲区满 */
        }
        
        buf += written;
        remaining -= written;
        
        proc = proc->next;  /* 移动到下一个进程 */
    }
    
    written = snprintf(buf, remaining,
        "\n=================================\n");
    buf += written;
    remaining -= written;
    
    schedstat_size = sizeof(schedstat_buffer) - remaining;
}

/**
 * 读取 /proc/schedstat
 */
int proc_schedstat_read(struct vfs_file *file, char *buffer, size_t count)
{
    (void)file;
    
    /* 更新统计信息 */
    if (file->pos == 0) {
        update_schedstat();
    }
    
    /* 检查偏移量 */
    if (file->pos >= (off_t)schedstat_size) {
        return 0;  /* EOF */
    }
    
    /* 计算可读取的字节数 */
    size_t available = schedstat_size - file->pos;
    size_t to_read = (count < available) ? count : available;
    
    /* 复制数据到用户缓冲区 */
    memcpy(buffer, schedstat_buffer + file->pos, to_read);
    file->pos += to_read;
    
    return to_read;
}

/**
 * /proc/schedstat 文件操作
 */
static struct vfs_file_operations proc_schedstat_fops = {
    .open = NULL,
    .release = NULL,
    .read = proc_schedstat_read,
    .write = NULL,  /* 只读 */
    .ioctl = NULL,
    .poll = NULL,
};

/*
 * 不需要单独的初始化函数
 * 在EduOS中，proc文件通过procfs.c中的全局文件列表自动注册
 * 我们已经在procfs.c的global_files数组中添加了schedstat条目
 */

