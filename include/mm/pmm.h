#ifndef _MM_PMM_H
#define _MM_PMM_H

/*
 * 物理内存管理器（Physical Memory Manager）
 * 
 * 使用位图算法管理物理页帧
 */

#include <types.h>

/* 页大小定义 */
#define PAGE_SIZE       4096        // 4KB页
#define PAGE_SHIFT      12          // log2(4096)
#define PAGE_MASK       0xFFFFF000  // 页对齐掩码

/* 页帧对齐宏 */
#define PAGE_ALIGN_DOWN(addr)   ((addr) & PAGE_MASK)
#define PAGE_ALIGN_UP(addr)     (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define IS_PAGE_ALIGNED(addr)   (((addr) & (PAGE_SIZE - 1)) == 0)

/* 地址/页帧转换 */
#define ADDR_TO_FRAME(addr)     ((addr) >> PAGE_SHIFT)
#define FRAME_TO_ADDR(frame)    ((frame) << PAGE_SHIFT)

/*
 * 内存区域类型
 */
enum memory_type {
    MEMORY_TYPE_RESERVED  = 0,  // 保留（BIOS/硬件）
    MEMORY_TYPE_AVAILABLE = 1,  // 可用
    MEMORY_TYPE_KERNEL    = 2,  // 内核占用
    MEMORY_TYPE_ACPI      = 3,  // ACPI
};

/*
 * 内存区域描述符
 */
struct memory_region {
    uint32_t base;              // 起始物理地址
    uint32_t length;            // 长度（字节）
    enum memory_type type;      // 类型
};

/*
 * PMM统计信息
 */
struct pmm_stats {
    uint32_t total_frames;      // 总页帧数
    uint32_t used_frames;       // 已用页帧数
    uint32_t free_frames;       // 空闲页帧数
    uint32_t reserved_frames;   // 保留页帧数
    uint32_t kernel_frames;     // 内核占用页帧数
};

/*
 * 初始化物理内存管理器
 * 
 * @param mem_size: 物理内存总大小（字节）
 * @param kernel_start: 内核起始物理地址
 * @param kernel_end: 内核结束物理地址
 */
void pmm_init(uint32_t mem_size, uint32_t kernel_start, uint32_t kernel_end);

/*
 * 分配一个物理页帧
 * 
 * @return: 物理页帧地址，失败返回0
 */
uint32_t pmm_alloc_frame(void);

/*
 * 分配连续的物理页帧
 * 
 * @param count: 页帧数量
 * @return: 起始物理地址，失败返回0
 */
uint32_t pmm_alloc_frames(uint32_t count);

/*
 * 释放一个物理页帧
 * 
 * @param frame_addr: 物理页帧地址
 */
void pmm_free_frame(uint32_t frame_addr);

/*
 * 释放连续的物理页帧
 * 
 * @param frame_addr: 起始物理地址
 * @param count: 页帧数量
 */
void pmm_free_frames(uint32_t frame_addr, uint32_t count);

/*
 * 标记页帧为已用
 * 
 * @param frame_addr: 物理页帧地址
 */
void pmm_mark_frame_used(uint32_t frame_addr);

/*
 * 标记页帧为空闲
 * 
 * @param frame_addr: 物理页帧地址
 */
void pmm_mark_frame_free(uint32_t frame_addr);

/*
 * 检查页帧是否被使用
 * 
 * @param frame_addr: 物理页帧地址
 * @return: true=已用，false=空闲
 */
bool pmm_is_frame_used(uint32_t frame_addr);

/*
 * 获取PMM统计信息
 * 
 * @param stats: 输出统计信息结构体指针
 */
void pmm_get_stats(struct pmm_stats *stats);

/*
 * 打印内存映射信息
 */
void pmm_print_memory_map(void);

/*
 * 打印PMM统计信息
 */
void pmm_print_stats(void);

#endif /* _MM_PMM_H */

