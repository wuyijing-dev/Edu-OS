/**
 * page_cache.h - 页面缓存（Page Cache）
 * 
 * Linux风格的页面缓存，用于缓存文件内容，减少磁盘I/O
 */

#ifndef _MM_PAGE_CACHE_H
#define _MM_PAGE_CACHE_H

#include <types.h>
#include <list.h>
#include <rbtree.h>

/**
 * 页面缓存标志
 */
#define PAGE_CACHE_DIRTY    0x01  /* 脏页（需要写回）*/
#define PAGE_CACHE_LOCKED   0x02  /* 页面被锁定 */
#define PAGE_CACHE_UPTODATE 0x04  /* 页面数据是最新的 */
#define PAGE_CACHE_WRITEBACK 0x08 /* 正在写回 */

/**
 * 缓存页面描述符
 */
struct cached_page {
    /* 基本信息 */
    uint32_t physical_addr;     /* 物理页面地址 */
    uint32_t flags;             /* PAGE_CACHE_* 标志 */
    
    /* 文件映射信息 */
    uint32_t inode;             /* 文件inode号 */
    uint32_t offset;            /* 文件内的偏移量（页对齐）*/
    
    /* LRU链表节点 */
    struct list_head lru;       /* LRU链表 */
    
    /* 红黑树节点（用于快速查找）*/
    struct rb_node rb_node;     /* 红黑树节点 */
    
    /* 引用计数 */
    uint32_t refcount;          /* 引用计数 */
    
    /* 时间戳 */
    uint64_t access_time;       /* 最后访问时间 */
    uint64_t dirty_time;        /* 变脏时间 */
};

/**
 * 页面缓存管理器
 */
struct page_cache_manager {
    /* 缓存页面红黑树（按inode+offset索引）*/
    struct rb_root cache_tree;
    
    /* LRU链表（用于页面回收）*/
    struct list_head lru_list;
    
    /* 脏页链表（用于周期性写回）*/
    struct list_head dirty_list;
    
    /* 统计信息 */
    uint32_t total_pages;       /* 总缓存页数 */
    uint32_t dirty_pages;       /* 脏页数 */
    uint32_t locked_pages;      /* 锁定页数 */
    
    /* 性能统计 */
    uint64_t hits;              /* 缓存命中次数 */
    uint64_t misses;            /* 缓存未命中次数 */
    uint64_t evictions;         /* 页面驱逐次数 */
    uint64_t writebacks;        /* 写回次数 */
};

/**
 * 全局页面缓存管理器
 */
extern struct page_cache_manager g_page_cache;

/**
 * 页面缓存操作函数
 */

/* 初始化页面缓存 */
void page_cache_init(void);

/* 查找缓存页面 */
struct cached_page *page_cache_lookup(uint32_t inode, uint32_t offset);

/* 添加页面到缓存 */
struct cached_page *page_cache_add(uint32_t inode, uint32_t offset, uint32_t phys_addr);

/* 从缓存中移除页面 */
void page_cache_remove(struct cached_page *page);

/* 标记页面为脏 */
void page_cache_mark_dirty(struct cached_page *page);

/* 锁定/解锁页面 */
void page_cache_lock(struct cached_page *page);
void page_cache_unlock(struct cached_page *page);

/* 增加/减少引用计数 */
void page_cache_get(struct cached_page *page);
void page_cache_put(struct cached_page *page);

/* 写回脏页 */
int page_cache_writeback(struct cached_page *page);
int page_cache_writeback_all(void);

/* 回收页面（用于内存不足时）*/
int page_cache_reclaim(uint32_t nr_pages);

/* 同步特定文件的所有页面 */
int page_cache_sync_inode(uint32_t inode);

/* 使特定文件的缓存失效 */
void page_cache_invalidate_inode(uint32_t inode);

/* 获取统计信息 */
void page_cache_get_stats(struct page_cache_manager *stats);

/* 打印缓存信息（调试用）*/
void page_cache_print_info(void);

/**
 * 辅助宏：计算缓存命中率
 */
#define PAGE_CACHE_HIT_RATE(cache) \
    ((cache)->hits + (cache)->misses > 0 ? \
     ((cache)->hits * 100) / ((cache)->hits + (cache)->misses) : 0)

#endif /* _MM_PAGE_CACHE_H */

