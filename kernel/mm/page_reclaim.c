/**
 * page_reclaim.c - 页面回收机制实现
 * 
 * 实现Linux风格的LRU页面回收算法
 */

#include <mm/page_reclaim.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel.h>
#include <string.h>
#include <sync/spinlock.h>

/* 全局LRU管理器 */
struct lru_manager g_lru_manager;
struct reclaim_stats g_reclaim_stats;

/* 页面描述符数组（简化实现：假设最大128MB内存 = 32768页）*/
#define MAX_PAGES 32768
static struct page_descriptor page_descriptors[MAX_PAGES];
static bool page_reclaim_initialized = false;

/**
 * 初始化页面回收系统
 */
void page_reclaim_init(void)
{
    if (page_reclaim_initialized) {
        return;
    }
    
    kprintf("[PAGE_RECLAIM] Initializing page reclaim system...\n");
    
    /* 初始化LRU列表 */
    INIT_LIST_HEAD(&g_lru_manager.active_list);
    INIT_LIST_HEAD(&g_lru_manager.inactive_list);
    g_lru_manager.active_count = 0;
    g_lru_manager.inactive_count = 0;
    spin_lock_init(&g_lru_manager.lock, "lru");
    
    /* 初始化页面描述符 */
    memset(page_descriptors, 0, sizeof(page_descriptors));
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        page_descriptors[i].phys_addr = i * 4096;
        INIT_LIST_HEAD(&page_descriptors[i].lru);
    }
    
    /* 初始化统计信息 */
    memset(&g_reclaim_stats, 0, sizeof(g_reclaim_stats));
    
    page_reclaim_initialized = true;
    kprintf("[PAGE_RECLAIM] Initialized: %u page descriptors\n", MAX_PAGES);
}

/**
 * 获取页面描述符
 */
struct page_descriptor *get_page_descriptor(uint32_t phys_addr)
{
    uint32_t page_index = phys_addr / 4096;
    if (page_index >= MAX_PAGES) {
        return NULL;
    }
    return &page_descriptors[page_index];
}

/**
 * 将页面添加到LRU列表（添加到非活跃列表）
 */
void lru_add_page(struct page_descriptor *page)
{
    if (!page || !page_reclaim_initialized) {
        return;
    }
    
    spin_lock(&g_lru_manager.lock);
    
    /* 如果已经在列表中，先移除 */
    if (!list_empty(&page->lru)) {
        list_del(&page->lru);
        if (page->flags & PAGE_FLAG_ACTIVE) {
            g_lru_manager.active_count--;
        } else {
            g_lru_manager.inactive_count--;
        }
    }
    
    /* 添加到非活跃列表头部（最近使用）*/
    list_add(&page->lru, &g_lru_manager.inactive_list);
    page->flags &= ~PAGE_FLAG_ACTIVE;
    g_lru_manager.inactive_count++;
    
    spin_unlock(&g_lru_manager.lock);
}

/**
 * 从LRU列表移除页面
 */
void lru_remove_page(struct page_descriptor *page)
{
    if (!page || !page_reclaim_initialized) {
        return;
    }
    
    spin_lock(&g_lru_manager.lock);
    
    if (!list_empty(&page->lru)) {
        list_del(&page->lru);
        INIT_LIST_HEAD(&page->lru);
        
        if (page->flags & PAGE_FLAG_ACTIVE) {
            g_lru_manager.active_count--;
        } else {
            g_lru_manager.inactive_count--;
        }
    }
    
    spin_unlock(&g_lru_manager.lock);
}

/**
 * 标记页面为已访问（LRU更新）
 */
void mark_page_accessed(struct page_descriptor *page)
{
    if (!page || !page_reclaim_initialized) {
        return;
    }
    
    spin_lock(&g_lru_manager.lock);
    
    /* 设置访问标志 */
    page->flags |= PAGE_FLAG_ACCESSED;
    
    /* 如果页面在非活跃列表且被访问，提升到活跃列表 */
    if (!(page->flags & PAGE_FLAG_ACTIVE) && !list_empty(&page->lru)) {
        list_del(&page->lru);
        g_lru_manager.inactive_count--;
        
        list_add(&page->lru, &g_lru_manager.active_list);
        page->flags |= PAGE_FLAG_ACTIVE;
        g_lru_manager.active_count++;
        
        g_reclaim_stats.pages_activated++;
    }
    
    spin_unlock(&g_lru_manager.lock);
}

/**
 * 收缩活跃列表（将不活跃的页面移到非活跃列表）
 */
uint32_t shrink_active_list(uint32_t nr_to_scan)
{
    uint32_t nr_moved = 0;
    struct list_head *pos, *n;
    
    if (!page_reclaim_initialized) {
        return 0;
    }
    
    spin_lock(&g_lru_manager.lock);
    
    /* 从活跃列表尾部开始扫描（最久未使用）*/
    list_for_each_safe_reverse(pos, n, &g_lru_manager.active_list) {
        if (nr_moved >= nr_to_scan) {
            break;
        }
        
        struct page_descriptor *page = list_entry(pos, struct page_descriptor, lru);
        
        g_reclaim_stats.pages_scanned++;
        
        /* 如果页面被锁定，跳过 */
        if (page->flags & PAGE_FLAG_LOCKED) {
            continue;
        }
        
        /* 如果页面最近被访问，清除访问标志并保留在活跃列表 */
        if (page->flags & PAGE_FLAG_ACCESSED) {
            page->flags &= ~PAGE_FLAG_ACCESSED;
            /* 移到活跃列表头部（给它第二次机会）*/
            list_del(&page->lru);
            list_add(&page->lru, &g_lru_manager.active_list);
            continue;
        }
        
        /* 页面不活跃，移到非活跃列表 */
        list_del(&page->lru);
        list_add(&page->lru, &g_lru_manager.inactive_list);
        page->flags &= ~PAGE_FLAG_ACTIVE;
        
        g_lru_manager.active_count--;
        g_lru_manager.inactive_count++;
        g_reclaim_stats.pages_deactivated++;
        nr_moved++;
    }
    
    spin_unlock(&g_lru_manager.lock);
    
    return nr_moved;
}

/**
 * 收缩非活跃列表（回收页面）
 */
uint32_t shrink_inactive_list(uint32_t nr_to_scan)
{
    uint32_t nr_reclaimed = 0;
    struct list_head *pos, *n;
    
    if (!page_reclaim_initialized) {
        return 0;
    }
    
    spin_lock(&g_lru_manager.lock);
    
    /* 从非活跃列表尾部开始扫描（最久未使用）*/
    list_for_each_safe_reverse(pos, n, &g_lru_manager.inactive_list) {
        if (nr_reclaimed >= nr_to_scan) {
            break;
        }
        
        struct page_descriptor *page = list_entry(pos, struct page_descriptor, lru);
        
        g_reclaim_stats.pages_scanned++;
        
        /* 如果页面被锁定或保留，跳过 */
        if ((page->flags & PAGE_FLAG_LOCKED) || 
            (page->flags & PAGE_FLAG_RESERVED)) {
            continue;
        }
        
        /* 如果页面最近被访问，提升到活跃列表 */
        if (page->flags & PAGE_FLAG_ACCESSED) {
            page->flags &= ~PAGE_FLAG_ACCESSED;
            list_del(&page->lru);
            list_add(&page->lru, &g_lru_manager.active_list);
            page->flags |= PAGE_FLAG_ACTIVE;
            
            g_lru_manager.inactive_count--;
            g_lru_manager.active_count++;
            g_reclaim_stats.pages_activated++;
            continue;
        }
        
        /* 如果页面是脏页，需要写回（这里简化处理）*/
        if (page->flags & PAGE_FLAG_DIRTY) {
            /* TODO: 写回脏页到磁盘或swap */
            page->flags &= ~PAGE_FLAG_DIRTY;
        }
        
        /* 回收页面 */
        if (page->ref_count == 0) {
            list_del(&page->lru);
            INIT_LIST_HEAD(&page->lru);
            g_lru_manager.inactive_count--;
            
            /* 释放物理页面 */
            pmm_free_frame(page->phys_addr);
            
            g_reclaim_stats.pages_reclaimed++;
            nr_reclaimed++;
        }
    }
    
    spin_unlock(&g_lru_manager.lock);
    
    return nr_reclaimed;
}

/**
 * 尝试回收指定数量的页面
 */
uint32_t try_to_free_pages(uint32_t nr_pages)
{
    uint32_t total_freed = 0;
    
    if (!page_reclaim_initialized) {
        return 0;
    }
    
    kprintf("[PAGE_RECLAIM] Attempting to free %u pages...\n", nr_pages);
    
    /* 第一步：收缩活跃列表（将不活跃的页面移到非活跃列表）*/
    uint32_t nr_moved = shrink_active_list(nr_pages * 2);
    kprintf("[PAGE_RECLAIM] Moved %u pages from active to inactive list\n", nr_moved);
    
    /* 第二步：收缩非活跃列表（回收页面）*/
    uint32_t nr_freed = shrink_inactive_list(nr_pages);
    total_freed += nr_freed;
    kprintf("[PAGE_RECLAIM] Freed %u pages from inactive list\n", nr_freed);
    
    /* 如果还不够，再次尝试 */
    if (total_freed < nr_pages) {
        nr_freed = shrink_inactive_list(nr_pages - total_freed);
        total_freed += nr_freed;
    }
    
    kprintf("[PAGE_RECLAIM] Total freed: %u pages\n", total_freed);
    
    return total_freed;
}

/**
 * 锁定页面（防止被回收）
 */
void lock_page(struct page_descriptor *page)
{
    if (page) {
        page->flags |= PAGE_FLAG_LOCKED;
    }
}

/**
 * 解锁页面
 */
void unlock_page(struct page_descriptor *page)
{
    if (page) {
        page->flags &= ~PAGE_FLAG_LOCKED;
    }
}

/**
 * 获取回收统计信息
 */
void get_reclaim_stats(struct reclaim_stats *stats)
{
    if (stats) {
        memcpy(stats, &g_reclaim_stats, sizeof(struct reclaim_stats));
    }
}

