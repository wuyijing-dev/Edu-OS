/*
 * 物理内存管理器（Physical Memory Manager）
 * 
 * 使用位图算法管理物理页帧
 */

#include <mm/pmm.h>
#include <string.h>
#include <kernel.h>

/* 位图数据 */
static uint32_t *pmm_bitmap = NULL;
static uint32_t pmm_bitmap_size = 0;  // 位图大小（字节）

/* 统计信息 */
static struct pmm_stats pmm_stats = {0};

/* 内存区域信息 */
static uint32_t memory_size = 0;
static uint32_t kernel_start_phys = 0;
static uint32_t kernel_end_phys = 0;

/*
 * 设置位图中的某一位
 */
static inline void pmm_set_bit(uint32_t frame)
{
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;
    pmm_bitmap[index] |= (1 << bit);
}

/*
 * 清除位图中的某一位
 */
static inline void pmm_clear_bit(uint32_t frame)
{
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;
    pmm_bitmap[index] &= ~(1 << bit);
}

/*
 * 测试位图中的某一位
 */
static inline bool pmm_test_bit(uint32_t frame)
{
    uint32_t index = frame / 32;
    uint32_t bit = frame % 32;
    return (pmm_bitmap[index] & (1 << bit)) != 0;
}

/*
 * 初始化物理内存管理器
 */
void pmm_init(uint32_t mem_size, uint32_t kernel_start, uint32_t kernel_end)
{
    memory_size = mem_size;
    kernel_start_phys = kernel_start;
    kernel_end_phys = kernel_end;
    
    /* 计算总页帧数 */
    pmm_stats.total_frames = mem_size / PAGE_SIZE;
    
    /* 计算位图大小（字节） */
    pmm_bitmap_size = (pmm_stats.total_frames + 7) / 8;
    pmm_bitmap_size = PAGE_ALIGN_UP(pmm_bitmap_size);
    
    /* 位图放在内核结束后 */
    pmm_bitmap = (uint32_t*)kernel_end_phys;
    
    kprintf("[PMM] Initializing Physical Memory Manager...\n");
    kprintf("[PMM] Total Memory: %u MB (%u frames)\n", 
            mem_size / 1024 / 1024, pmm_stats.total_frames);
    kprintf("[PMM] Kernel: 0x%08x - 0x%08x (%u KB)\n",
            kernel_start, kernel_end, (kernel_end - kernel_start) / 1024);
    kprintf("[PMM] Bitmap at: 0x%08x, size: %u bytes\n",
            (uint32_t)pmm_bitmap, pmm_bitmap_size);
    
    /* 初始化位图：全部标记为已用 */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    pmm_stats.used_frames = pmm_stats.total_frames;
    pmm_stats.free_frames = 0;
    
    /* 标记保留区域（0-1MB） */
    uint32_t reserved_frames = 0x100000 / PAGE_SIZE;  // 256 frames (1MB)
    pmm_stats.reserved_frames = reserved_frames;
    
    /* 标记内核占用区域 */
    uint32_t kernel_frames = (kernel_end - kernel_start) / PAGE_SIZE;
    if ((kernel_end - kernel_start) % PAGE_SIZE) {
        kernel_frames++;
    }
    pmm_stats.kernel_frames = kernel_frames;
    
    /* 标记位图占用的页帧 */
    uint32_t bitmap_frames = pmm_bitmap_size / PAGE_SIZE;
    
    /* 释放可用内存（位图之后到内存顶部） */
    uint32_t available_start = (uint32_t)pmm_bitmap + pmm_bitmap_size;
    available_start = PAGE_ALIGN_UP(available_start);
    
    uint32_t available_end = mem_size;
    
    for (uint32_t addr = available_start; addr < available_end; addr += PAGE_SIZE) {
        uint32_t frame = addr / PAGE_SIZE;
        if (frame < pmm_stats.total_frames) {
            pmm_clear_bit(frame);
            pmm_stats.used_frames--;
            pmm_stats.free_frames++;
        }
    }
    
    kprintf("[PMM] Available: %u MB (%u frames)\n",
            pmm_stats.free_frames * 4 / 1024, pmm_stats.free_frames);
    kprintf("[PMM] Initialization complete\n\n");
}

/*
 * 分配一个物理页帧
 */
uint32_t pmm_alloc_frame(void)
{
    /* 扫描位图找到第一个空闲帧 */
    for (uint32_t i = 0; i < pmm_stats.total_frames; i++) {
        if (!pmm_test_bit(i)) {
            /* 找到空闲帧 */
            pmm_set_bit(i);
            pmm_stats.used_frames++;
            pmm_stats.free_frames--;
            
            uint32_t addr = i * PAGE_SIZE;
            return addr;
        }
    }
    
    /* 内存不足 */
    kprintf("[PMM] ERROR: Out of memory!\n");
    return 0;
}

/*
 * 分配连续的物理页帧
 */
uint32_t pmm_alloc_frames(uint32_t count)
{
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_frame();
    
    /* 查找连续的空闲帧 */
    uint32_t found = 0;
    uint32_t start_frame = 0;
    
    for (uint32_t i = 0; i < pmm_stats.total_frames; i++) {
        if (!pmm_test_bit(i)) {
            if (found == 0) {
                start_frame = i;
            }
            found++;
            
            if (found == count) {
                /* 找到足够的连续帧 */
                for (uint32_t j = 0; j < count; j++) {
                    pmm_set_bit(start_frame + j);
                }
                
                pmm_stats.used_frames += count;
                pmm_stats.free_frames -= count;
                
                return start_frame * PAGE_SIZE;
            }
        } else {
            found = 0;
        }
    }
    
    /* 未找到足够的连续内存 */
    kprintf("[PMM] ERROR: Cannot allocate %u consecutive frames\n", count);
    return 0;
}

/*
 * 释放一个物理页帧
 */
void pmm_free_frame(uint32_t frame_addr)
{
    if (frame_addr == 0) return;
    
    uint32_t frame = frame_addr / PAGE_SIZE;
    
    if (frame >= pmm_stats.total_frames) {
        kprintf("[PMM] WARNING: Attempt to free invalid frame 0x%08x\n", frame_addr);
        return;
    }
    
    if (!pmm_test_bit(frame)) {
        kprintf("[PMM] WARNING: Double free detected at 0x%08x\n", frame_addr);
        return;
    }
    
    pmm_clear_bit(frame);
    pmm_stats.used_frames--;
    pmm_stats.free_frames++;
}

/*
 * 释放连续的物理页帧
 */
void pmm_free_frames(uint32_t frame_addr, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        pmm_free_frame(frame_addr + i * PAGE_SIZE);
    }
}

/*
 * 标记页帧为已用
 */
void pmm_mark_frame_used(uint32_t frame_addr)
{
    uint32_t frame = frame_addr / PAGE_SIZE;
    
    if (frame >= pmm_stats.total_frames) {
        return;
    }
    
    if (!pmm_test_bit(frame)) {
        pmm_set_bit(frame);
        pmm_stats.used_frames++;
        pmm_stats.free_frames--;
    }
}

/*
 * 标记页帧为空闲
 */
void pmm_mark_frame_free(uint32_t frame_addr)
{
    pmm_free_frame(frame_addr);
}

/*
 * 检查页帧是否被使用
 */
bool pmm_is_frame_used(uint32_t frame_addr)
{
    uint32_t frame = frame_addr / PAGE_SIZE;
    
    if (frame >= pmm_stats.total_frames) {
        return true;  // 超出范围视为已用
    }
    
    return pmm_test_bit(frame);
}

/*
 * 获取PMM统计信息
 */
void pmm_get_stats(struct pmm_stats *stats)
{
    if (stats) {
        *stats = pmm_stats;
    }
}

/*
 * 获取总内存大小（字节）
 */
uint32_t pmm_get_total_memory(void)
{
    return memory_size;
}

/*
 * 获取空闲内存大小（字节）
 */
uint32_t pmm_get_free_memory(void)
{
    return pmm_stats.free_frames * PAGE_SIZE;
}

/*
 * 打印内存映射信息
 */
void pmm_print_memory_map(void)
{
    kprintf("\n=== Physical Memory Map ===\n");
    kprintf("0x%08x - 0x%08x : Reserved (BIOS/Hardware)\n", 0, 0x100000);
    kprintf("0x%08x - 0x%08x : Kernel\n", kernel_start_phys, kernel_end_phys);
    kprintf("0x%08x - 0x%08x : PMM Bitmap\n", 
            (uint32_t)pmm_bitmap, (uint32_t)pmm_bitmap + pmm_bitmap_size);
    kprintf("0x%08x - 0x%08x : Available\n",
            PAGE_ALIGN_UP((uint32_t)pmm_bitmap + pmm_bitmap_size), memory_size);
    kprintf("\n");
}

/*
 * 打印PMM统计信息
 */
void pmm_print_stats(void)
{
    kprintf("\n=== Physical Memory Statistics ===\n");
    kprintf("Total Memory:    %u MB (%u frames)\n",
            pmm_stats.total_frames * 4 / 1024, pmm_stats.total_frames);
    kprintf("Used Memory:     %u MB (%u frames)\n",
            pmm_stats.used_frames * 4 / 1024, pmm_stats.used_frames);
    kprintf("Free Memory:     %u MB (%u frames)\n",
            pmm_stats.free_frames * 4 / 1024, pmm_stats.free_frames);
    kprintf("Reserved:        %u MB (%u frames)\n",
            pmm_stats.reserved_frames * 4 / 1024, pmm_stats.reserved_frames);
    kprintf("Kernel:          %u KB (%u frames)\n",
            pmm_stats.kernel_frames * 4, pmm_stats.kernel_frames);
    kprintf("Usage:           %u%%\n",
            pmm_stats.used_frames * 100 / pmm_stats.total_frames);
    kprintf("\n");
}

