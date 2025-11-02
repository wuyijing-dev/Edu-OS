/**
 * oom.c - Out Of Memory Killer实现
 * 
 * Linux风格的OOM killer，当内存不足时选择并终止进程
 */

#include <mm/oom.h>
#include <mm/pmm.h>
#include <mm/page_reclaim.h>
#include <mm/vma.h>
#include <process/process.h>
#include <kernel.h>
#include <signal.h>
#include <string.h>

/* 全局OOM统计 */
struct oom_stats g_oom_stats;

/* OOM阈值：当可用内存低于此值时触发OOM */
#define OOM_THRESHOLD_PAGES 256  /* 1MB */

/**
 * 初始化OOM killer
 */
void oom_init(void)
{
    kprintf("[OOM] Initializing OOM killer...\n");
    memset(&g_oom_stats, 0, sizeof(g_oom_stats));
    kprintf("[OOM] OOM killer initialized (threshold: %u pages)\n", OOM_THRESHOLD_PAGES);
}

/**
 * 检查是否需要触发OOM
 */
bool should_trigger_oom(void)
{
    struct pmm_stats stats;
    pmm_get_stats(&stats);
    
    return (stats.free_frames < OOM_THRESHOLD_PAGES);
}

/**
 * 计算进程的OOM分数
 * 
 * 评分标准（Linux风格）：
 * 1. 内存使用量（主要因素）
 * 2. 运行时间（时间越短分数越高）
 * 3. 特权进程（init等）分数降低
 * 4. 用户可调整的oom_score_adj
 */
uint32_t oom_score(pid_t pid)
{
    struct process *proc = process_find_by_pid(pid);
    
    if (!proc) {
        return 0;
    }
    
    uint32_t score = 0;
    
    /* 1. init进程（PID 1）永不杀死 */
    if (pid == 1) {
        return 0;
    }
    
    /* 2. 内核线程不杀死 */
    if (proc->page_dir == NULL) {
        return 0;
    }
    
    /* 3. 基于内存使用量评分（0-500分）*/
    /* 统计进程的VMA总大小 */
    uint32_t total_pages = 0;
    struct vma *vma = proc->vma_list;
    while (vma) {
        total_pages += (vma->end - vma->start) / 4096;
        vma = vma->next;
    }
    
    /* 每1MB内存增加10分 */
    score += (total_pages / 256) * 10;
    if (score > 500) score = 500;
    
    /* 4. 基于运行时间评分（0-200分）*/
    /* 运行时间越短，分数越高（新进程更容易被杀）*/
    /* 这里简化：假设所有进程运行时间相同 */
    score += 100;
    
    /* 5. 优先级调整（0-300分）*/
    /* 优先级越低的进程越容易被杀 */
    if (proc->priority > 10) {
        score += 50;  /* 低优先级进程 */
    }
    
    /* 确保分数在0-1000范围内 */
    if (score > 1000) score = 1000;
    
    return score;
}

/**
 * 选择要杀死的进程
 */
static pid_t select_bad_process(void)
{
    pid_t victim_pid = 0;
    uint32_t max_score = 0;
    
    extern struct process *process_list_head;
    struct process *proc = process_list_head;
    
    kprintf("[OOM] Scanning processes for victim...\n");
    
    while (proc) {
        uint32_t score = oom_score(proc->pid);
        
        kprintf("[OOM]   PID %u (%s): score=%u\n", 
                proc->pid, proc->name, score);
        
        if (score > max_score) {
            max_score = score;
            victim_pid = proc->pid;
        }
        
        proc = proc->next;
    }
    
    return victim_pid;
}

/**
 * 触发OOM killer
 */
uint32_t oom_kill_process(void)
{
    kprintf("\n");
    kprintf("╔════════════════════════════════════════════════════════════╗\n");
    kprintf("║              OUT OF MEMORY KILLER                         ║\n");
    kprintf("╚════════════════════════════════════════════════════════════╝\n");
    kprintf("\n");
    
    g_oom_stats.oom_count++;
    
    /* 获取内存统计 */
    struct pmm_stats pmm_stats;
    pmm_get_stats(&pmm_stats);
    
    kprintf("[OOM] System is out of memory!\n");
    kprintf("[OOM] Free memory: %u KB (%u pages)\n", 
            pmm_stats.free_frames * 4, pmm_stats.free_frames);
    kprintf("[OOM] Total memory: %u KB\n", pmm_stats.total_frames * 4);
    
    /* 首先尝试页面回收 */
    kprintf("[OOM] Attempting page reclaim first...\n");
    uint32_t freed = try_to_free_pages(OOM_THRESHOLD_PAGES);
    
    if (freed >= OOM_THRESHOLD_PAGES / 2) {
        kprintf("[OOM] Page reclaim successful, freed %u pages\n", freed);
        return freed;
    }
    
    kprintf("[OOM] Page reclaim insufficient, selecting victim process...\n");
    
    /* 选择要杀死的进程 */
    pid_t victim_pid = select_bad_process();
    
    if (victim_pid == 0) {
        kprintf("[OOM] ERROR: No suitable process found to kill!\n");
        kprintf("[OOM] System may be unstable!\n");
        return 0;
    }
    
    struct process *victim = process_find_by_pid(victim_pid);
    
    if (!victim) {
        kprintf("[OOM] ERROR: Victim process disappeared!\n");
        return 0;
    }
    
    kprintf("\n");
    kprintf("[OOM] ⚠️  Killing process: %s (PID %u)\n", victim->name, victim_pid);
    kprintf("[OOM]     OOM score: %u\n", oom_score(victim_pid));
    
    /* 统计进程占用的内存 */
    uint32_t victim_pages = 0;
    struct vma *vma = victim->vma_list;
    while (vma) {
        victim_pages += (vma->end - vma->start) / 4096;
        vma = vma->next;
    }
    
    kprintf("[OOM]     Memory usage: ~%u KB\n", victim_pages * 4);
    
    /* 发送SIGKILL信号终止进程 */
    extern int sys_kill(pid_t pid, int sig);
    sys_kill(victim_pid, SIGKILL);
    
    g_oom_stats.processes_killed++;
    g_oom_stats.pages_freed += victim_pages;
    
    kprintf("[OOM] ✓ Process killed, expected to free ~%u pages\n", victim_pages);
    kprintf("\n");
    
    return victim_pages;
}

/**
 * 获取OOM统计信息
 */
void get_oom_stats(struct oom_stats *stats)
{
    if (stats) {
        memcpy(stats, &g_oom_stats, sizeof(struct oom_stats));
    }
}

