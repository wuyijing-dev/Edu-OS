/**
 * oom.h - Out Of Memory Killer（Linux风格）
 * 
 * 当系统内存不足时，选择并终止进程以释放内存
 */

#ifndef _MM_OOM_H
#define _MM_OOM_H

#include <types.h>

/**
 * OOM统计信息
 */
struct oom_stats {
    uint64_t oom_count;          /* OOM发生次数 */
    uint64_t processes_killed;   /* 被杀死的进程数 */
    uint64_t pages_freed;        /* 释放的页面数 */
};

/* 全局OOM统计 */
extern struct oom_stats g_oom_stats;

/**
 * 初始化OOM killer
 */
void oom_init(void);

/**
 * 触发OOM killer
 * @return: 释放的页面数，0表示失败
 */
uint32_t oom_kill_process(void);

/**
 * 计算进程的OOM分数（分数越高越可能被杀死）
 * @param pid: 进程ID
 * @return: OOM分数（0-1000）
 */
uint32_t oom_score(pid_t pid);

/**
 * 检查是否需要触发OOM
 * @return: true表示需要触发OOM
 */
bool should_trigger_oom(void);

/**
 * 获取OOM统计信息
 */
void get_oom_stats(struct oom_stats *stats);

#endif /* _MM_OOM_H */

