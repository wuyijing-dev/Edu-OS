/*
 * proc_pid.c - /proc/[pid]/ 进程文件实现
 * 
 * 显示进程相关信息（Linux风格）
 */

#include <fs/procfs.h>
#include <kernel.h>
#include <process/process.h>
#include <string.h>

/* ========== 辅助函数 ========== */

/*
 * 获取进程状态字符
 */
static char proc_state_to_char(process_state_t state)
{
    switch (state) {
        case PROCESS_STATE_RUNNING:     return 'R';
        case PROCESS_STATE_READY:       return 'R';
        case PROCESS_STATE_BLOCKED:     return 'S';
        case PROCESS_STATE_TERMINATED:  return 'Z';
        default:                        return '?';
    }
}

/*
 * 获取进程状态名称
 */
static const char *proc_state_to_name(process_state_t state)
{
    switch (state) {
        case PROCESS_STATE_RUNNING:     return "running";
        case PROCESS_STATE_READY:       return "sleeping";
        case PROCESS_STATE_BLOCKED:     return "disk sleep";
        case PROCESS_STATE_TERMINATED:  return "zombie";
        default:                        return "unknown";
    }
}

/* ========== /proc/[pid]/status ========== */

/*
 * 读取进程状态信息
 * 
 * 格式类似 Linux /proc/[pid]/status
 */
int proc_pid_status_read(char *buf, size_t size, off_t *offset, void *data)
{
    /* data 包含 PID */
    pid_t pid = (pid_t)(uintptr_t)data;
    
    /* 查找进程 */
    struct process *proc = procfs_find_process(pid);
    if (!proc) {
        return -ESRCH;  // No such process
    }
    
    /* 获取父进程 PID */
    uint32_t parent_pid = proc->parent ? proc->parent->pid : 0;
    
    /* 生成状态信息 */
    char content[2048];
    int len = snprintf(content, sizeof(content),
        "Name:\t%s\n"
        "Umask:\t0022\n"
        "State:\t%c (%s)\n"
        "Tgid:\t%u\n"
        "Ngid:\t0\n"
        "Pid:\t%u\n"
        "PPid:\t%u\n"
        "TracerPid:\t0\n"
        "Uid:\t0\t0\t0\t0\n"
        "Gid:\t0\t0\t0\t0\n"
        "FDSize:\t64\n"
        "Groups:\t0\n"
        "NStgid:\t%u\n"
        "NSpid:\t%u\n"
        "NSpgid:\t0\n"
        "NSsid:\t0\n"
        "VmPeak:\t    %8u kB\n"
        "VmSize:\t    %8u kB\n"
        "VmLck:\t           0 kB\n"
        "VmPin:\t           0 kB\n"
        "VmHWM:\t    %8u kB\n"
        "VmRSS:\t    %8u kB\n"
        "RssAnon:\t    %8u kB\n"
        "RssFile:\t           0 kB\n"
        "RssShmem:\t           0 kB\n"
        "VmData:\t    %8u kB\n"
        "VmStk:\t           8 kB\n"
        "VmExe:\t           4 kB\n"
        "VmLib:\t           0 kB\n"
        "VmPTE:\t           4 kB\n"
        "VmSwap:\t           0 kB\n"
        "Threads:\t1\n"
        "SigQ:\t0/0\n"
        "SigPnd:\t0000000000000000\n"
        "ShdPnd:\t0000000000000000\n"
        "SigBlk:\t0000000000000000\n"
        "SigIgn:\t0000000000000000\n"
        "SigCgt:\t0000000000000000\n"
        "CapInh:\t0000000000000000\n"
        "CapPrm:\t000001ffffffffff\n"
        "CapEff:\t000001ffffffffff\n"
        "CapBnd:\t000001ffffffffff\n"
        "CapAmb:\t0000000000000000\n"
        "voluntary_ctxt_switches:\t%u\n"
        "nonvoluntary_ctxt_switches:\t%u\n",
        proc->name,
        proc_state_to_char(proc->state),
        proc_state_to_name(proc->state),
        proc->pid,
        proc->pid,
        parent_pid,
        proc->pid,
        proc->pid,
        8192,  // 假设每个进程 8MB
        8192,
        8192,
        8192,
        8192,
        8192,
        0,  // voluntary context switches
        0   // nonvoluntary context switches
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

/* ========== /proc/[pid]/cmdline ========== */

/*
 * 读取进程命令行
 * 
 * 格式：以 \0 分隔的参数列表
 * 例如："/bin/sh\0-i\0"
 */
int proc_pid_cmdline_read(char *buf, size_t size, off_t *offset, void *data)
{
    /* data 包含 PID */
    pid_t pid = (pid_t)(uintptr_t)data;
    
    /* 查找进程 */
    struct process *proc = procfs_find_process(pid);
    if (!proc) {
        return -ESRCH;
    }
    
    /* 生成命令行（简化：只有进程名） */
    char content[256];
    int len = snprintf(content, sizeof(content), "%s", proc->name);
    
    /* cmdline 使用 \0 结尾 */
    if (len < (int)sizeof(content) - 1) {
        content[len] = '\0';
        len++;
    }
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

/* ========== /proc/[pid]/stat ========== */

/*
 * 读取进程统计信息
 * 
 * 格式：单行，空格分隔的字段
 */
int proc_pid_stat_read(char *buf, size_t size, off_t *offset, void *data)
{
    /* data 包含 PID */
    pid_t pid = (pid_t)(uintptr_t)data;
    
    /* 查找进程 */
    struct process *proc = procfs_find_process(pid);
    if (!proc) {
        return -ESRCH;
    }
    
    /* 获取父进程 PID */
    uint32_t parent_pid = proc->parent ? proc->parent->pid : 0;
    
    /* 生成 stat 信息 */
    char content[512];
    int len = snprintf(content, sizeof(content),
        "%u "           // pid
        "(%s) "         // comm (进程名)
        "%c "           // state
        "%u "           // ppid
        "0 "            // pgrp
        "0 "            // session
        "0 "            // tty_nr
        "0 "            // tpgid
        "0 "            // flags
        "0 "            // minflt
        "0 "            // cminflt
        "0 "            // majflt
        "0 "            // cmajflt
        "0 "            // utime
        "0 "            // stime
        "0 "            // cutime
        "0 "            // cstime
        "%d "           // priority
        "0 "            // nice
        "1 "            // num_threads
        "0 "            // itrealvalue
        "0 "            // starttime
        "8388608 "      // vsize (8MB)
        "2048 "         // rss (2MB in pages)
        "4294967295 "   // rsslim
        "0 "            // startcode
        "0 "            // endcode
        "0 "            // startstack
        "0 "            // kstkesp
        "0 "            // kstkeip
        "0 "            // signal
        "0 "            // blocked
        "0 "            // sigignore
        "0 "            // sigcatch
        "0 "            // wchan
        "0 "            // nswap
        "0 "            // cnswap
        "0 "            // exit_signal
        "0 "            // processor
        "0 "            // rt_priority
        "0 "            // policy
        "0 "            // delayacct_blkio_ticks
        "0 "            // guest_time
        "0\n",          // cguest_time
        proc->pid,
        proc->name,
        proc_state_to_char(proc->state),
        parent_pid,
        proc->priority
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

