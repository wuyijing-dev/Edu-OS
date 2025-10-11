/*
 * proc_meminfo.c - /proc/meminfo 实现
 * 
 * 显示内存信息（Linux风格）
 */

#include <fs/procfs.h>
#include <kernel.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <types.h>

/*
 * 读取内存信息
 */
int proc_meminfo_read(char *buf, size_t size, off_t *offset, void *data)
{
    (void)data;
    
    /* 获取物理内存统计 */
    uint32_t total_mem = pmm_get_total_memory();
    uint32_t free_mem = pmm_get_free_memory();
    uint32_t used_mem = total_mem - free_mem;
    
    /* 获取堆统计 */
    struct heap_stats heap_stats;
    kmalloc_get_stats(&heap_stats);
    uint32_t heap_total = heap_stats.total_size;
    uint32_t heap_used = heap_stats.used_size;
    uint32_t heap_free = heap_stats.free_size;
    
    /* 生成内存信息 */
    char content[2048];
    int len = snprintf(content, sizeof(content),
        "MemTotal:       %8u kB\n"
        "MemFree:        %8u kB\n"
        "MemAvailable:   %8u kB\n"
        "Buffers:        %8u kB\n"
        "Cached:         %8u kB\n"
        "SwapCached:     %8u kB\n"
        "Active:         %8u kB\n"
        "Inactive:       %8u kB\n"
        "Active(anon):   %8u kB\n"
        "Inactive(anon): %8u kB\n"
        "Active(file):   %8u kB\n"
        "Inactive(file): %8u kB\n"
        "Unevictable:    %8u kB\n"
        "Mlocked:        %8u kB\n"
        "SwapTotal:      %8u kB\n"
        "SwapFree:       %8u kB\n"
        "Dirty:          %8u kB\n"
        "Writeback:      %8u kB\n"
        "AnonPages:      %8u kB\n"
        "Mapped:         %8u kB\n"
        "Shmem:          %8u kB\n"
        "Slab:           %8u kB\n"
        "SReclaimable:   %8u kB\n"
        "SUnreclaim:     %8u kB\n"
        "KernelStack:    %8u kB\n"
        "PageTables:     %8u kB\n"
        "CommitLimit:    %8u kB\n"
        "Committed_AS:   %8u kB\n"
        "VmallocTotal:   %8u kB\n"
        "VmallocUsed:    %8u kB\n"
        "VmallocChunk:   %8u kB\n"
        "HeapTotal:      %8u kB\n"
        "HeapUsed:       %8u kB\n"
        "HeapFree:       %8u kB\n",
        total_mem / 1024,
        free_mem / 1024,
        free_mem / 1024,
        0,  // Buffers
        0,  // Cached
        0,  // SwapCached
        used_mem / 1024,  // Active
        0,  // Inactive
        0,  // Active(anon)
        0,  // Inactive(anon)
        0,  // Active(file)
        0,  // Inactive(file)
        0,  // Unevictable
        0,  // Mlocked
        0,  // SwapTotal
        0,  // SwapFree
        0,  // Dirty
        0,  // Writeback
        used_mem / 1024,  // AnonPages
        0,  // Mapped
        0,  // Shmem
        heap_used / 1024,  // Slab
        0,  // SReclaimable
        heap_used / 1024,  // SUnreclaim
        0,  // KernelStack
        0,  // PageTables
        total_mem / 1024,  // CommitLimit
        used_mem / 1024,  // Committed_AS
        1024 * 1024,  // VmallocTotal (1GB)
        0,  // VmallocUsed
        1024 * 1024,  // VmallocChunk
        heap_total / 1024,
        heap_used / 1024,
        heap_free / 1024
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

