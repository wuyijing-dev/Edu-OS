/**
 * swap.h - SWAP（交换空间）支持
 * 
 * Linux风格的SWAP机制，用于将不活跃的页面换出到磁盘
 */

#ifndef _MM_SWAP_H
#define _MM_SWAP_H

#include <types.h>
#include <list.h>
#include <bitmap.h>

/**
 * SWAP配置
 */
#define SWAP_MAX_PAGES      4096    /* 最大SWAP页面数（16MB）*/
#define SWAP_CLUSTER_SIZE   32      /* 一次换入/换出的页面数 */

/**
 * SWAP条目标志
 */
#define SWAP_ENTRY_VALID    0x01    /* 条目有效 */
#define SWAP_ENTRY_DIRTY    0x02    /* 页面已修改 */
#define SWAP_ENTRY_LOCKED   0x04    /* 正在进行I/O */

/**
 * SWAP条目（存储在页表项中）
 * 
 * 当页面被换出时，PTE的格式：
 * Bit 0: Present = 0（页面不在内存中）
 * Bit 1-7: SWAP类型（保留，用于多个SWAP设备）
 * Bit 8-31: SWAP偏移量（页面在SWAP空间中的索引）
 */
struct swap_entry {
    uint32_t type:7;        /* SWAP设备类型 */
    uint32_t offset:24;     /* SWAP空间偏移 */
    uint32_t present:1;     /* 必须为0（表示不在内存中）*/
};

/**
 * SWAP槽位描述符
 */
struct swap_slot {
    uint32_t flags;         /* SWAP_ENTRY_* 标志 */
    uint32_t refcount;      /* 引用计数（共享页面）*/
    uint32_t pid;           /* 拥有该页面的进程ID */
    uint32_t vaddr;         /* 原虚拟地址 */
};

/**
 * SWAP管理器
 */
struct swap_manager {
    /* SWAP空间位图（跟踪哪些槽位已使用）*/
    DECLARE_BITMAP(slot_bitmap, SWAP_MAX_PAGES);
    
    /* SWAP槽位描述符数组 */
    struct swap_slot slots[SWAP_MAX_PAGES];
    
    /* SWAP设备信息 */
    uint32_t swap_device;   /* SWAP设备号（简化：使用内存模拟）*/
    uint32_t swap_base;     /* SWAP区域基地址（物理地址）*/
    uint32_t swap_size;     /* SWAP区域大小（页数）*/
    
    /* 统计信息 */
    uint32_t total_slots;   /* 总槽位数 */
    uint32_t used_slots;    /* 已使用槽位数 */
    uint32_t free_slots;    /* 空闲槽位数 */
    
    /* 性能统计 */
    uint64_t swap_in_count;     /* 换入次数 */
    uint64_t swap_out_count;    /* 换出次数 */
    uint64_t swap_in_pages;     /* 换入页面数 */
    uint64_t swap_out_pages;    /* 换出页面数 */
};

/**
 * 全局SWAP管理器
 */
extern struct swap_manager g_swap_manager;

/**
 * SWAP操作函数
 */

/* 初始化SWAP系统 */
int swap_init(void);

/* 分配SWAP槽位 */
int swap_alloc_slot(void);

/* 释放SWAP槽位 */
void swap_free_slot(int slot);

/* 换出页面到SWAP */
int swap_out_page(uint32_t vaddr, uint32_t phys_addr, uint32_t pid);

/* 从SWAP换入页面 */
int swap_in_page(uint32_t swap_entry, uint32_t *phys_addr);

/* 批量换出页面（用于内存回收）*/
int swap_out_pages(uint32_t nr_pages);

/* 检查SWAP条目是否有效 */
static inline int is_swap_entry(uint32_t pte)
{
    /* Present位为0，且不是全0（表示是SWAP条目）*/
    return (pte != 0) && !(pte & 0x01);
}

/* 从PTE提取SWAP条目 */
static inline struct swap_entry pte_to_swap_entry(uint32_t pte)
{
    struct swap_entry entry;
    entry.present = pte & 0x01;
    entry.type = (pte >> 1) & 0x7F;
    entry.offset = (pte >> 8) & 0xFFFFFF;
    return entry;
}

/* 将SWAP条目转换为PTE */
static inline uint32_t swap_entry_to_pte(struct swap_entry entry)
{
    return (entry.offset << 8) | (entry.type << 1) | 0;
}

/* 获取SWAP统计信息 */
void swap_get_stats(struct swap_manager *stats);

/* 打印SWAP信息（调试用）*/
void swap_print_info(void);

/**
 * 辅助宏：计算SWAP使用率
 */
#define SWAP_USAGE_PERCENT(swap) \
    ((swap)->total_slots > 0 ? \
     ((swap)->used_slots * 100) / (swap)->total_slots : 0)

#endif /* _MM_SWAP_H */

