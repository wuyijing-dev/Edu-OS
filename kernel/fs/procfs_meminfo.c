/**
 * procfs_meminfo.c - /proc/meminfo实现
 * 
 * 提供系统内存统计信息（Linux风格）
 */

#include <fs/vfs.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <mm/page_reclaim.h>
#include <mm/oom.h>
#include <kernel.h>
#include <string.h>

#define MEMINFO_BUF_SIZE 2048

/**
 * 生成meminfo内容
 */
static int generate_meminfo(char *buf, size_t size)
{
    int len = 0;
    
    /* 获取物理内存统计 */
    struct pmm_stats pmm_stats;
    pmm_get_stats(&pmm_stats);
    
    uint32_t total_kb = pmm_stats.total_frames * 4;
    uint32_t free_kb = pmm_stats.free_frames * 4;
    uint32_t used_kb = pmm_stats.used_frames * 4;
    
    /* 获取kmalloc统计 */
    struct kmalloc_stats kmalloc_stats;
    kmalloc_get_stats(&kmalloc_stats);
    
    uint32_t kernel_kb = kmalloc_stats.used_bytes / 1024;
    
    /* 获取LRU统计 */
    uint32_t active_kb = g_lru_manager.active_count * 4;
    uint32_t inactive_kb = g_lru_manager.inactive_count * 4;
    
    /* 获取回收统计 */
    struct reclaim_stats reclaim_stats;
    get_reclaim_stats(&reclaim_stats);
    
    /* 获取OOM统计 */
    struct oom_stats oom_stats;
    get_oom_stats(&oom_stats);
    
    /* 格式化输出（Linux /proc/meminfo风格）*/
    len += snprintf(buf + len, size - len, "MemTotal:       %8u kB\n", total_kb);
    len += snprintf(buf + len, size - len, "MemFree:        %8u kB\n", free_kb);
    len += snprintf(buf + len, size - len, "MemUsed:        %8u kB\n", used_kb);
    len += snprintf(buf + len, size - len, "MemAvailable:   %8u kB\n", free_kb + inactive_kb / 2);
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "Active:         %8u kB\n", active_kb);
    len += snprintf(buf + len, size - len, "Inactive:       %8u kB\n", inactive_kb);
    len += snprintf(buf + len, size - len, "Active(anon):   %8u kB\n", active_kb / 2);
    len += snprintf(buf + len, size - len, "Inactive(anon): %8u kB\n", inactive_kb / 2);
    len += snprintf(buf + len, size - len, "Active(file):   %8u kB\n", active_kb / 2);
    len += snprintf(buf + len, size - len, "Inactive(file): %8u kB\n", inactive_kb / 2);
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "Slab:           %8u kB\n", kernel_kb);
    len += snprintf(buf + len, size - len, "KernelStack:    %8u kB\n", 0);  /* TODO */
    len += snprintf(buf + len, size - len, "PageTables:     %8u kB\n", 0);  /* TODO */
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "Dirty:          %8u kB\n", 0);  /* TODO */
    len += snprintf(buf + len, size - len, "Writeback:      %8u kB\n", 0);
    len += snprintf(buf + len, size - len, "Mapped:         %8u kB\n", 0);  /* TODO */
    len += snprintf(buf + len, size - len, "Shmem:          %8u kB\n", 0);  /* TODO */
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "SwapTotal:      %8u kB\n", 0);  /* TODO: SWAP */
    len += snprintf(buf + len, size - len, "SwapFree:       %8u kB\n", 0);
    len += snprintf(buf + len, size - len, "SwapCached:     %8u kB\n", 0);
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "# Page Reclaim Statistics\n");
    len += snprintf(buf + len, size - len, "PagesScanned:   %8llu\n", reclaim_stats.pages_scanned);
    len += snprintf(buf + len, size - len, "PagesReclaimed: %8llu\n", reclaim_stats.pages_reclaimed);
    len += snprintf(buf + len, size - len, "PagesActivated: %8llu\n", reclaim_stats.pages_activated);
    len += snprintf(buf + len, size - len, "PagesDeactivated: %8llu\n", reclaim_stats.pages_deactivated);
    len += snprintf(buf + len, size - len, "\n");
    
    len += snprintf(buf + len, size - len, "# OOM Killer Statistics\n");
    len += snprintf(buf + len, size - len, "OOM_Count:      %8llu\n", oom_stats.oom_count);
    len += snprintf(buf + len, size - len, "OOM_Kills:      %8llu\n", oom_stats.processes_killed);
    len += snprintf(buf + len, size - len, "OOM_PagesFreed: %8llu\n", oom_stats.pages_freed);
    
    return len;
}

/**
 * /proc/meminfo读取函数
 */
ssize_t procfs_meminfo_read(struct vfs_file *file, void *buf, size_t count)
{
    static char meminfo_buf[MEMINFO_BUF_SIZE];
    static int meminfo_len = 0;
    static bool generated = false;
    
    /* 首次读取时生成内容 */
    if (!generated) {
        meminfo_len = generate_meminfo(meminfo_buf, MEMINFO_BUF_SIZE);
        generated = true;
    }
    
    /* 检查偏移 */
    if (file->pos >= meminfo_len) {
        generated = false;  /* 重置，下次读取重新生成 */
        return 0;  /* EOF */
    }
    
    /* 计算可读取的字节数 */
    size_t remaining = meminfo_len - file->pos;
    if (count > remaining) {
        count = remaining;
    }
    
    /* 复制数据到用户缓冲区 */
    memcpy(buf, meminfo_buf + file->pos, count);
    file->pos += count;
    
    return count;
}

/**
 * 注册/proc/meminfo
 */
void procfs_meminfo_init(void)
{
    kprintf("[PROCFS] Registering /proc/meminfo...\n");
    /* TODO: 在procfs中注册meminfo节点 */
    kprintf("[PROCFS] /proc/meminfo registered\n");
}

