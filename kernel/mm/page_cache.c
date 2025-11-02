/**
 * page_cache.c - 页面缓存实现
 * 
 * Linux风格的页面缓存系统
 */

#include <mm/page_cache.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>
#include <drivers/timer.h>

/* 全局页面缓存管理器 */
struct page_cache_manager g_page_cache;

/* 最大缓存页面数（可配置）*/
#define MAX_CACHE_PAGES 1024

/**
 * 初始化页面缓存
 */
void page_cache_init(void)
{
    kprintf("[PageCache] Initializing page cache...\n");
    
    /* 初始化红黑树 */
    g_page_cache.cache_tree.rb_node = NULL;
    
    /* 初始化链表 */
    INIT_LIST_HEAD(&g_page_cache.lru_list);
    INIT_LIST_HEAD(&g_page_cache.dirty_list);
    
    /* 清空统计信息 */
    g_page_cache.total_pages = 0;
    g_page_cache.dirty_pages = 0;
    g_page_cache.locked_pages = 0;
    g_page_cache.hits = 0;
    g_page_cache.misses = 0;
    g_page_cache.evictions = 0;
    g_page_cache.writebacks = 0;
    
    kprintf("[PageCache] Page cache initialized (max: %u pages)\n", MAX_CACHE_PAGES);
}

/**
 * 比较两个缓存页面的键（inode + offset）
 */
static int compare_cache_key(uint32_t inode1, uint32_t offset1,
                             uint32_t inode2, uint32_t offset2)
{
    if (inode1 < inode2) return -1;
    if (inode1 > inode2) return 1;
    if (offset1 < offset2) return -1;
    if (offset1 > offset2) return 1;
    return 0;
}

/**
 * 查找缓存页面
 */
struct cached_page *page_cache_lookup(uint32_t inode, uint32_t offset)
{
    struct rb_node *node = g_page_cache.cache_tree.rb_node;
    
    while (node) {
        struct cached_page *page = rb_entry(node, struct cached_page, rb_node);
        int cmp = compare_cache_key(inode, offset, page->inode, page->offset);
        
        if (cmp < 0) {
            node = node->rb_left;
        } else if (cmp > 0) {
            node = node->rb_right;
        } else {
            /* 找到了，更新访问时间 */
            page->access_time = timer_get_ticks();
            
            /* 移动到LRU链表头部（最近使用）*/
            list_del(&page->lru);
            list_add(&page->lru, &g_page_cache.lru_list);
            
            g_page_cache.hits++;
            return page;
        }
    }
    
    g_page_cache.misses++;
    return NULL;
}

/**
 * 添加页面到缓存
 */
struct cached_page *page_cache_add(uint32_t inode, uint32_t offset, uint32_t phys_addr)
{
    /* 检查是否已存在 */
    struct cached_page *existing = page_cache_lookup(inode, offset);
    if (existing) {
        return existing;
    }
    
    /* 检查缓存是否已满 */
    if (g_page_cache.total_pages >= MAX_CACHE_PAGES) {
        /* 尝试回收一个页面 */
        if (page_cache_reclaim(1) < 0) {
            return NULL;
        }
    }
    
    /* 分配缓存页面描述符 */
    struct cached_page *page = (struct cached_page *)kmalloc(sizeof(struct cached_page));
    if (!page) {
        return NULL;
    }
    
    /* 初始化页面 */
    page->physical_addr = phys_addr;
    page->flags = PAGE_CACHE_UPTODATE;
    page->inode = inode;
    page->offset = offset;
    page->refcount = 1;
    page->access_time = timer_get_ticks();
    page->dirty_time = 0;
    
    INIT_LIST_HEAD(&page->lru);
    rb_init_node(&page->rb_node);
    
    /* 插入到红黑树 */
    struct rb_node **link = &g_page_cache.cache_tree.rb_node;
    struct rb_node *parent = NULL;
    
    while (*link) {
        parent = *link;
        struct cached_page *entry = rb_entry(parent, struct cached_page, rb_node);
        int cmp = compare_cache_key(inode, offset, entry->inode, entry->offset);
        
        if (cmp < 0) {
            link = &(*link)->rb_left;
        } else {
            link = &(*link)->rb_right;
        }
    }
    
    rb_link_node(&page->rb_node, parent, link);
    rb_insert_color(&page->rb_node, &g_page_cache.cache_tree);
    
    /* 添加到LRU链表头部 */
    list_add(&page->lru, &g_page_cache.lru_list);
    
    g_page_cache.total_pages++;
    
    return page;
}

/**
 * 从缓存中移除页面
 */
void page_cache_remove(struct cached_page *page)
{
    if (!page) return;
    
    /* 如果是脏页，先写回 */
    if (page->flags & PAGE_CACHE_DIRTY) {
        page_cache_writeback(page);
    }
    
    /* 从红黑树中删除 */
    rb_erase(&page->rb_node, &g_page_cache.cache_tree);
    
    /* 从LRU链表中删除 */
    list_del(&page->lru);
    
    /* 更新统计 */
    g_page_cache.total_pages--;
    if (page->flags & PAGE_CACHE_DIRTY) {
        g_page_cache.dirty_pages--;
    }
    if (page->flags & PAGE_CACHE_LOCKED) {
        g_page_cache.locked_pages--;
    }
    
    /* 释放物理页面 */
    pmm_free_frame(page->physical_addr);
    
    /* 释放描述符 */
    kfree(page);
}

/**
 * 标记页面为脏
 */
void page_cache_mark_dirty(struct cached_page *page)
{
    if (!page) return;
    
    if (!(page->flags & PAGE_CACHE_DIRTY)) {
        page->flags |= PAGE_CACHE_DIRTY;
        page->dirty_time = timer_get_ticks();
        
        /* 添加到脏页链表 */
        list_add_tail(&page->lru, &g_page_cache.dirty_list);
        
        g_page_cache.dirty_pages++;
    }
}

/**
 * 锁定页面
 */
void page_cache_lock(struct cached_page *page)
{
    if (!page) return;
    
    if (!(page->flags & PAGE_CACHE_LOCKED)) {
        page->flags |= PAGE_CACHE_LOCKED;
        g_page_cache.locked_pages++;
    }
}

/**
 * 解锁页面
 */
void page_cache_unlock(struct cached_page *page)
{
    if (!page) return;
    
    if (page->flags & PAGE_CACHE_LOCKED) {
        page->flags &= ~PAGE_CACHE_LOCKED;
        g_page_cache.locked_pages--;
    }
}

/**
 * 增加引用计数
 */
void page_cache_get(struct cached_page *page)
{
    if (page) {
        page->refcount++;
    }
}

/**
 * 减少引用计数
 */
void page_cache_put(struct cached_page *page)
{
    if (!page) return;
    
    if (page->refcount > 0) {
        page->refcount--;
    }
    
    /* 如果引用计数为0且未锁定，可以被回收 */
    if (page->refcount == 0 && !(page->flags & PAGE_CACHE_LOCKED)) {
        /* 页面可以被回收，但不立即删除，等待LRU回收 */
    }
}

/**
 * 写回脏页
 */
int page_cache_writeback(struct cached_page *page)
{
    if (!page || !(page->flags & PAGE_CACHE_DIRTY)) {
        return 0;
    }
    
    /* 标记为正在写回 */
    page->flags |= PAGE_CACHE_WRITEBACK;
    
    /* TODO: 实际的磁盘写入操作
     * 这里应该调用VFS层的写入函数
     * 例如：vfs_write_page(page->inode, page->offset, page->physical_addr);
     */
    
    /* 清除脏标志和写回标志 */
    page->flags &= ~(PAGE_CACHE_DIRTY | PAGE_CACHE_WRITEBACK);
    
    /* 从脏页链表中删除 */
    if (!list_empty(&page->lru)) {
        list_del(&page->lru);
        /* 重新添加到LRU链表 */
        list_add(&page->lru, &g_page_cache.lru_list);
    }
    
    g_page_cache.dirty_pages--;
    g_page_cache.writebacks++;
    
    return 0;
}

/**
 * 写回所有脏页
 */
int page_cache_writeback_all(void)
{
    struct cached_page *page, *tmp;
    int count = 0;
    
    list_for_each_entry_safe(page, tmp, &g_page_cache.dirty_list, lru) {
        if (page_cache_writeback(page) == 0) {
            count++;
        }
    }
    
    return count;
}

/**
 * 回收页面（用于内存不足时）
 */
int page_cache_reclaim(uint32_t nr_pages)
{
    uint32_t reclaimed = 0;
    struct cached_page *page, *tmp;
    
    /* 从LRU链表尾部开始回收（最久未使用）*/
    list_for_each_entry_safe_reverse(page, tmp, &g_page_cache.lru_list, lru) {
        if (reclaimed >= nr_pages) {
            break;
        }
        
        /* 跳过锁定的页面和引用计数不为0的页面 */
        if ((page->flags & PAGE_CACHE_LOCKED) || page->refcount > 0) {
            continue;
        }
        
        /* 移除页面 */
        page_cache_remove(page);
        reclaimed++;
        g_page_cache.evictions++;
    }
    
    return reclaimed;
}

/**
 * 同步特定文件的所有页面
 */
int page_cache_sync_inode(uint32_t inode)
{
    struct rb_node *node;
    int count = 0;
    
    for (node = rb_first(&g_page_cache.cache_tree); node; node = rb_next(node)) {
        struct cached_page *page = rb_entry(node, struct cached_page, rb_node);
        
        if (page->inode == inode && (page->flags & PAGE_CACHE_DIRTY)) {
            if (page_cache_writeback(page) == 0) {
                count++;
            }
        }
    }
    
    return count;
}

/**
 * 使特定文件的缓存失效
 */
void page_cache_invalidate_inode(uint32_t inode)
{
    struct rb_node *node, *next;
    
    for (node = rb_first(&g_page_cache.cache_tree); node; node = next) {
        next = rb_next(node);
        struct cached_page *page = rb_entry(node, struct cached_page, rb_node);
        
        if (page->inode == inode) {
            page_cache_remove(page);
        }
    }
}

/**
 * 获取统计信息
 */
void page_cache_get_stats(struct page_cache_manager *stats)
{
    if (stats) {
        memcpy(stats, &g_page_cache, sizeof(struct page_cache_manager));
    }
}

/**
 * 打印缓存信息（调试用）
 */
void page_cache_print_info(void)
{
    kprintf("\n=== Page Cache Statistics ===\n");
    kprintf("Total pages:   %u / %u\n", g_page_cache.total_pages, MAX_CACHE_PAGES);
    kprintf("Dirty pages:   %u\n", g_page_cache.dirty_pages);
    kprintf("Locked pages:  %u\n", g_page_cache.locked_pages);
    kprintf("Cache hits:    %llu\n", g_page_cache.hits);
    kprintf("Cache misses:  %llu\n", g_page_cache.misses);
    kprintf("Hit rate:      %llu%%\n", PAGE_CACHE_HIT_RATE(&g_page_cache));
    kprintf("Evictions:     %llu\n", g_page_cache.evictions);
    kprintf("Writebacks:    %llu\n", g_page_cache.writebacks);
    kprintf("=============================\n\n");
}

