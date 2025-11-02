/**
 * swap.c - SWAP（交换空间）实现
 * 
 * Linux风格的SWAP系统，支持页面换入/换出
 */

#include <mm/swap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel.h>
#include <string.h>

/* 全局SWAP管理器 */
struct swap_manager g_swap_manager;

/* SWAP模拟存储（简化实现：使用内存模拟磁盘）*/
static uint8_t *swap_storage = NULL;

/**
 * 初始化SWAP系统
 */
int swap_init(void)
{
    kprintf("[SWAP] Initializing swap system...\n");
    
    /* 分配SWAP存储空间（简化：使用物理内存模拟）*/
    /* 在实际系统中，这应该是磁盘分区 */
    swap_storage = (uint8_t *)kmalloc(SWAP_MAX_PAGES * 4096);
    if (!swap_storage) {
        kprintf("[SWAP] ERROR: Failed to allocate swap storage\n");
        return -1;
    }
    
    /* 初始化位图 */
    bitmap_zero(g_swap_manager.slot_bitmap, SWAP_MAX_PAGES);
    
    /* 初始化槽位描述符 */
    memset(g_swap_manager.slots, 0, sizeof(g_swap_manager.slots));
    
    /* 设置SWAP信息 */
    g_swap_manager.swap_device = 0;  /* 简化：设备0 */
    g_swap_manager.swap_base = (uint32_t)swap_storage;
    g_swap_manager.swap_size = SWAP_MAX_PAGES;
    
    /* 初始化统计 */
    g_swap_manager.total_slots = SWAP_MAX_PAGES;
    g_swap_manager.used_slots = 0;
    g_swap_manager.free_slots = SWAP_MAX_PAGES;
    g_swap_manager.swap_in_count = 0;
    g_swap_manager.swap_out_count = 0;
    g_swap_manager.swap_in_pages = 0;
    g_swap_manager.swap_out_pages = 0;
    
    kprintf("[SWAP] Swap system initialized:\n");
    kprintf("[SWAP]   Total slots: %u (%u MB)\n", 
            SWAP_MAX_PAGES, SWAP_MAX_PAGES * 4 / 1024);
    kprintf("[SWAP]   Storage at: 0x%08x\n", g_swap_manager.swap_base);
    
    return 0;
}

/**
 * 分配SWAP槽位
 */
int swap_alloc_slot(void)
{
    /* 查找第一个空闲槽位 */
    uint32_t slot = find_first_zero_bit(g_swap_manager.slot_bitmap, SWAP_MAX_PAGES);
    
    if (slot >= SWAP_MAX_PAGES) {
        kprintf("[SWAP] ERROR: No free swap slots\n");
        return -1;
    }
    
    /* 标记为已使用 */
    set_bit(slot, g_swap_manager.slot_bitmap);
    
    /* 初始化槽位描述符 */
    g_swap_manager.slots[slot].flags = SWAP_ENTRY_VALID;
    g_swap_manager.slots[slot].refcount = 1;
    g_swap_manager.slots[slot].pid = 0;
    g_swap_manager.slots[slot].vaddr = 0;
    
    /* 更新统计 */
    g_swap_manager.used_slots++;
    g_swap_manager.free_slots--;
    
    return slot;
}

/**
 * 释放SWAP槽位
 */
void swap_free_slot(int slot)
{
    if (slot < 0 || slot >= SWAP_MAX_PAGES) {
        return;
    }
    
    if (!test_bit(slot, g_swap_manager.slot_bitmap)) {
        return;  /* 槽位未使用 */
    }
    
    /* 减少引用计数 */
    if (g_swap_manager.slots[slot].refcount > 0) {
        g_swap_manager.slots[slot].refcount--;
    }
    
    /* 如果引用计数为0，释放槽位 */
    if (g_swap_manager.slots[slot].refcount == 0) {
        clear_bit(slot, g_swap_manager.slot_bitmap);
        g_swap_manager.slots[slot].flags = 0;
        
        /* 更新统计 */
        g_swap_manager.used_slots--;
        g_swap_manager.free_slots++;
    }
}

/**
 * 换出页面到SWAP
 */
int swap_out_page(uint32_t vaddr, uint32_t phys_addr, uint32_t pid)
{
    /* 分配SWAP槽位 */
    int slot = swap_alloc_slot();
    if (slot < 0) {
        return -1;
    }
    
    /* 计算SWAP存储地址 */
    uint8_t *swap_addr = swap_storage + (slot * 4096);
    
    /* 将页面内容复制到SWAP空间 */
    /* 注意：phys_addr需要临时映射到虚拟地址才能访问 */
    extern void *kmap(uint32_t paddr);
    extern void kunmap(void *vaddr);
    
    void *page_vaddr = kmap(phys_addr);
    if (!page_vaddr) {
        swap_free_slot(slot);
        return -1;
    }
    
    memcpy(swap_addr, page_vaddr, 4096);
    kunmap(page_vaddr);
    
    /* 更新槽位信息 */
    g_swap_manager.slots[slot].pid = pid;
    g_swap_manager.slots[slot].vaddr = vaddr;
    g_swap_manager.slots[slot].flags |= SWAP_ENTRY_DIRTY;
    
    /* 更新统计 */
    g_swap_manager.swap_out_count++;
    g_swap_manager.swap_out_pages++;
    
    /* 释放物理页面 */
    pmm_free_frame(phys_addr);
    
    /* 返回SWAP条目 */
    struct swap_entry entry;
    entry.present = 0;
    entry.type = 0;
    entry.offset = slot;
    
    return swap_entry_to_pte(entry);
}

/**
 * 从SWAP换入页面
 */
int swap_in_page(uint32_t swap_pte, uint32_t *phys_addr)
{
    /* 提取SWAP条目 */
    struct swap_entry entry = pte_to_swap_entry(swap_pte);
    int slot = entry.offset;
    
    if (slot < 0 || slot >= SWAP_MAX_PAGES) {
        return -1;
    }
    
    if (!test_bit(slot, g_swap_manager.slot_bitmap)) {
        kprintf("[SWAP] ERROR: Invalid swap slot %d\n", slot);
        return -1;
    }
    
    /* 分配物理页面 */
    uint32_t new_phys = pmm_alloc_frame();
    if (new_phys == 0) {
        kprintf("[SWAP] ERROR: Failed to allocate physical page for swap-in\n");
        return -1;
    }
    
    /* 计算SWAP存储地址 */
    uint8_t *swap_addr = swap_storage + (slot * 4096);
    
    /* 从SWAP空间复制页面内容到物理页面 */
    extern void *kmap(uint32_t paddr);
    extern void kunmap(void *vaddr);
    
    void *page_vaddr = kmap(new_phys);
    if (!page_vaddr) {
        pmm_free_frame(new_phys);
        return -1;
    }
    
    memcpy(page_vaddr, swap_addr, 4096);
    kunmap(page_vaddr);
    
    /* 更新统计 */
    g_swap_manager.swap_in_count++;
    g_swap_manager.swap_in_pages++;
    
    /* 释放SWAP槽位 */
    swap_free_slot(slot);
    
    /* 返回物理地址 */
    *phys_addr = new_phys;
    
    return 0;
}

/**
 * 批量换出页面（用于内存回收）
 */
int swap_out_pages(uint32_t nr_pages)
{
    uint32_t swapped = 0;
    
    /* TODO: 实现页面选择策略
     * 1. 从LRU链表中选择最久未使用的页面
     * 2. 跳过锁定的页面
     * 3. 优先换出干净页面（不需要写回）
     */
    
    /* 简化实现：这里需要与页面回收机制集成 */
    /* 暂时返回0，等待完整的页面回收集成 */
    kprintf("[SWAP] swap_out_pages: requested %u pages (not fully implemented)\n", nr_pages);
    swapped = 0;
    
    return swapped;
}

/**
 * 获取SWAP统计信息
 */
void swap_get_stats(struct swap_manager *stats)
{
    if (stats) {
        memcpy(stats, &g_swap_manager, sizeof(struct swap_manager));
    }
}

/**
 * 打印SWAP信息（调试用）
 */
void swap_print_info(void)
{
    kprintf("\n=== SWAP Statistics ===\n");
    kprintf("Total slots:   %u\n", g_swap_manager.total_slots);
    kprintf("Used slots:    %u\n", g_swap_manager.used_slots);
    kprintf("Free slots:    %u\n", g_swap_manager.free_slots);
    kprintf("Usage:         %u%%\n", SWAP_USAGE_PERCENT(&g_swap_manager));
    kprintf("Swap out:      %llu times (%llu pages)\n", 
            g_swap_manager.swap_out_count, g_swap_manager.swap_out_pages);
    kprintf("Swap in:       %llu times (%llu pages)\n", 
            g_swap_manager.swap_in_count, g_swap_manager.swap_in_pages);
    kprintf("=======================\n\n");
}

