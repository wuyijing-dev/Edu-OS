/**
 * page_reclaim.h - 页面回收机制（Linux风格）
 * 
 * 实现LRU（Least Recently Used）页面替换算法
 */

#ifndef _MM_PAGE_RECLAIM_H
#define _MM_PAGE_RECLAIM_H

#include <types.h>
#include <list.h>

/* 页面状态标志 */
#define PAGE_FLAG_LOCKED    0x0001  /* 页面被锁定，不可回收 */
#define PAGE_FLAG_DIRTY     0x0002  /* 页面已修改 */
#define PAGE_FLAG_ACCESSED  0x0004  /* 页面最近被访问 */
#define PAGE_FLAG_ACTIVE    0x0008  /* 页面在活跃列表中 */
#define PAGE_FLAG_SLAB      0x0010  /* 页面属于slab分配器 */
#define PAGE_FLAG_RESERVED  0x0020  /* 页面保留，不可回收 */
#define PAGE_FLAG_SWAPBACKED 0x0040 /* 页面可以swap */

/**
 * 页面描述符（Linux风格的struct page简化版）
 */
struct page_descriptor {
    uint32_t flags;              /* 页面标志 */
    uint32_t ref_count;          /* 引用计数 */
    uint32_t phys_addr;          /* 物理地址 */
    struct list_head lru;        /* LRU链表节点 */
    void *private;               /* 私有数据（如VMA指针）*/
    uint32_t index;              /* 在文件中的页索引 */
};

/**
 * LRU列表管理器
 */
struct lru_manager {
    struct list_head active_list;    /* 活跃页面列表 */
    struct list_head inactive_list;  /* 非活跃页面列表 */
    uint32_t active_count;           /* 活跃页面数量 */
    uint32_t inactive_count;         /* 非活跃页面数量 */
    uint32_t lock;                   /* 保护锁 */
};

/**
 * 页面回收统计信息
 */
struct reclaim_stats {
    uint64_t pages_scanned;      /* 扫描的页面数 */
    uint64_t pages_reclaimed;    /* 回收的页面数 */
    uint64_t pages_activated;    /* 激活的页面数 */
    uint64_t pages_deactivated;  /* 去激活的页面数 */
};

/* 全局LRU管理器 */
extern struct lru_manager g_lru_manager;
extern struct reclaim_stats g_reclaim_stats;

/**
 * 初始化页面回收系统
 */
void page_reclaim_init(void);

/**
 * 将页面添加到LRU列表
 */
void lru_add_page(struct page_descriptor *page);

/**
 * 从LRU列表移除页面
 */
void lru_remove_page(struct page_descriptor *page);

/**
 * 标记页面为已访问（更新LRU）
 */
void mark_page_accessed(struct page_descriptor *page);

/**
 * 尝试回收指定数量的页面
 * @param nr_pages: 需要回收的页面数
 * @return: 实际回收的页面数
 */
uint32_t try_to_free_pages(uint32_t nr_pages);

/**
 * 收缩活跃列表（将不活跃的页面移到非活跃列表）
 */
uint32_t shrink_active_list(uint32_t nr_to_scan);

/**
 * 收缩非活跃列表（回收页面）
 */
uint32_t shrink_inactive_list(uint32_t nr_to_scan);

/**
 * 获取页面描述符
 */
struct page_descriptor *get_page_descriptor(uint32_t phys_addr);

/**
 * 锁定页面（防止被回收）
 */
void lock_page(struct page_descriptor *page);

/**
 * 解锁页面
 */
void unlock_page(struct page_descriptor *page);

/**
 * 获取回收统计信息
 */
void get_reclaim_stats(struct reclaim_stats *stats);

#endif /* _MM_PAGE_RECLAIM_H */

