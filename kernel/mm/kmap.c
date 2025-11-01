/*
 * kmap.c - 临时内核映射（Linux风格）
 * 
 * 用于临时映射高端物理内存到内核虚拟地址空间
 * 解决直接映射只支持低4MB的限制
 */

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <kernel.h>
#include <sync/spinlock.h>
#include <string.h>

/* 临时映射区域：0xF0000000 - 0xF0400000 (4MB, 1024个页)
 * Linux使用fixmap，我们简化为临时映射池
 */
#define KMAP_BASE       0xF0000000
#define KMAP_PAGES      1024
#define KMAP_SIZE       (KMAP_PAGES * 4096)

/* 临时映射槽位 */
static struct {
    uint32_t phys_addr;    /* 映射的物理地址，0表示空闲 */
    uint32_t ref_count;    /* 引用计数 */
} kmap_slots[KMAP_PAGES];

static struct spinlock kmap_lock;
static bool kmap_initialized = false;

/*
 * 初始化kmap系统
 */
void kmap_init(void)
{
    if (kmap_initialized) {
        return;
    }
    
    memset(kmap_slots, 0, sizeof(kmap_slots));
    spin_lock_init(&kmap_lock, "kmap");
    
    kprintf("[KMAP] Initialized: base=0x%08x, %u pages\n", KMAP_BASE, KMAP_PAGES);
    kmap_initialized = true;
}

/*
 * 临时映射物理页到内核空间
 * 
 * @param paddr: 物理地址（页对齐）
 * @return: 虚拟地址，失败返回NULL
 */
void *kmap(uint32_t paddr)
{
    if (!kmap_initialized) {
        kmap_init();
    }
    
    uint32_t page_addr = paddr & ~0xFFF;  /* 页对齐 */
    uint32_t offset = paddr & 0xFFF;       /* 页内偏移 */
    
    /* 低端内存直接映射（保留偏移）*/
    if (page_addr < 0x400000) {
        return (void*)(paddr + 0xC0000000);  /* 保留偏移！ */
    }
    
    paddr = page_addr;  /* 后续使用页对齐地址 */
    
    spin_lock(&kmap_lock);
    
    /* 查找已有映射 */
    for (uint32_t i = 0; i < KMAP_PAGES; i++) {
        if (kmap_slots[i].phys_addr == paddr && kmap_slots[i].ref_count > 0) {
            kmap_slots[i].ref_count++;
            spin_unlock(&kmap_lock);
            return (void*)(KMAP_BASE + i * 4096);
        }
    }
    
    /* 分配新槽位 */
    for (uint32_t i = 0; i < KMAP_PAGES; i++) {
        if (kmap_slots[i].ref_count == 0) {
            kmap_slots[i].phys_addr = paddr;
            kmap_slots[i].ref_count = 1;
            
            uint32_t vaddr = KMAP_BASE + i * 4096;
            
            /* 建立映射 */
            vmm_map_page(vaddr, paddr, 0x03);  /* PRESENT | WRITABLE */
            
            spin_unlock(&kmap_lock);
            return (void*)vaddr;
        }
    }
    
    spin_unlock(&kmap_lock);
    kprintf("[KMAP] ERROR: No free slots\n");
    return NULL;
}

/*
 * 解除临时映射
 */
void kunmap(void *vaddr)
{
    if (!kmap_initialized) {
        return;
    }
    
    uint32_t addr = (uint32_t)vaddr;
    
    /* 检查是否在kmap范围内 */
    if (addr < KMAP_BASE || addr >= KMAP_BASE + KMAP_SIZE) {
        return;  /* 不是kmap地址，忽略 */
    }
    
    uint32_t index = (addr - KMAP_BASE) / 4096;
    
    spin_lock(&kmap_lock);
    
    if (index < KMAP_PAGES && kmap_slots[index].ref_count > 0) {
        kmap_slots[index].ref_count--;
        
        if (kmap_slots[index].ref_count == 0) {
            /* 清除映射 */
            kmap_slots[index].phys_addr = 0;
            vmm_unmap_page(KMAP_BASE + index * 4096);
        }
    }
    
    spin_unlock(&kmap_lock);
}

