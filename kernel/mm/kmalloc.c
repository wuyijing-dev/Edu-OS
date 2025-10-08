/*
 * 内核堆内存分配器
 * 
 * 实现动态内存分配（kmalloc/kfree）
 * 算法：首次适应（First Fit）+ 空闲块合并
 */

#include <mm/kmalloc.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <string.h>
#include <kernel.h>

/* 堆块头部结构 */
struct heap_block {
    uint32_t magic;             // 魔数
    uint32_t size;              // 块大小（包括头部）
    bool allocated;             // 是否已分配
    struct heap_block *next;    // 下一个块
    struct heap_block *prev;    // 上一个块
};

/* 堆信息 */
static uint32_t heap_start = 0;
static uint32_t heap_end = 0;
static uint32_t heap_max = 0;
static struct heap_block *heap_first = NULL;

/* 对齐宏 */
#define ALIGN_UP(addr, align) (((addr) + (align) - 1) & ~((align) - 1))

/*
 * 扩展堆
 */
static void *expand_heap(size_t size)
{
    /* 对齐到页大小 */
    size = ALIGN_UP(size + sizeof(struct heap_block), PAGE_SIZE);
    
    if (heap_end + size > heap_max) {
        kprintf("[KMALLOC] ERROR: Heap limit reached\n");
        return NULL;
    }
    
    /* 分配物理页 */
    uint32_t num_pages = size / PAGE_SIZE;
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t phys = pmm_alloc_frame();
        if (phys == 0) {
            kprintf("[KMALLOC] ERROR: Out of physical memory\n");
            return NULL;
        }
        
        vmm_map_page(heap_end + i * PAGE_SIZE, phys, PAGE_FLAGS_KERNEL);
    }
    
    /* 创建新块 */
    struct heap_block *block = (struct heap_block*)heap_end;
    block->magic = HEAP_MAGIC;
    block->size = size;
    block->allocated = false;
    block->next = NULL;
    block->prev = NULL;
    
    /* 添加到链表 */
    if (heap_first == NULL) {
        heap_first = block;
    } else {
        struct heap_block *last = heap_first;
        while (last->next) {
            last = last->next;
        }
        last->next = block;
        block->prev = last;
    }
    
    heap_end += size;
    
    return block;
}

/*
 * 分割块
 */
static void split_block(struct heap_block *block, size_t size)
{
    if (block->size < size + sizeof(struct heap_block) + HEAP_MIN_SIZE) {
        return;  // 剩余空间太小，不分割
    }
    
    /* 创建新块 */
    struct heap_block *new_block = (struct heap_block*)((uint32_t)block + size);
    new_block->magic = HEAP_MAGIC;
    new_block->size = block->size - size;
    new_block->allocated = false;
    new_block->next = block->next;
    new_block->prev = block;
    
    if (block->next) {
        block->next->prev = new_block;
    }
    
    block->next = new_block;
    block->size = size;
}

/*
 * 合并空闲块
 */
static void merge_blocks(struct heap_block *block)
{
    if (!block || block->allocated) {
        return;
    }
    
    /* 向后合并 */
    while (block->next && !block->next->allocated) {
        struct heap_block *next = block->next;
        
        /* 验证魔数 */
        if (next->magic != HEAP_MAGIC) {
            kprintf("[KMALLOC] WARNING: Corrupted heap block\n");
            break;
        }
        
        block->size += next->size;
        block->next = next->next;
        
        if (next->next) {
            next->next->prev = block;
        }
    }
    
    /* 向前合并 */
    if (block->prev && !block->prev->allocated) {
        merge_blocks(block->prev);
    }
}

/*
 * 初始化内核堆
 */
void kmalloc_init(uint32_t start, uint32_t size)
{
    heap_start = start;
    heap_end = start;
    heap_max = start + size;
    heap_first = NULL;
    
    kprintf("[KMALLOC] Initializing kernel heap...\n");
    kprintf("[KMALLOC] Heap range: 0x%08x - 0x%08x (%u MB)\n",
            heap_start, heap_max, size / 1024 / 1024);
    
    /* 预分配一些页 */
    expand_heap(PAGE_SIZE * 16);  // 64KB
    
    kprintf("[KMALLOC] Kernel heap initialized\n\n");
}

/*
 * 分配内存
 */
void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }
    
    /* 对齐到8字节 */
    size = ALIGN_UP(size, 8);
    size += sizeof(struct heap_block);
    
    if (size > HEAP_MAX_SIZE) {
        return NULL;
    }
    
    /* 搜索空闲块（首次适应） */
    struct heap_block *block = heap_first;
    while (block) {
        if (!block->allocated && block->size >= size) {
            /* 找到合适的块 */
            block->allocated = true;
            
            /* 如果块太大，分割它 */
            split_block(block, size);
            
            return (void*)((uint32_t)block + sizeof(struct heap_block));
        }
        block = block->next;
    }
    
    /* 没有找到，扩展堆 */
    block = (struct heap_block*)expand_heap(size);
    if (block) {
        block->allocated = true;
        return (void*)((uint32_t)block + sizeof(struct heap_block));
    }
    
    return NULL;
}

/*
 * 分配并清零内存
 */
void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * 分配对齐内存
 */
void *kmalloc_aligned(size_t size, size_t align)
{
    if (align == 0 || (align & (align - 1)) != 0) {
        return NULL;  // 对齐必须是2的幂
    }
    
    /* 分配额外空间用于对齐 */
    size_t total_size = size + align + sizeof(struct heap_block);
    void *ptr = kmalloc(total_size);
    
    if (!ptr) {
        return NULL;
    }
    
    /* 对齐地址 */
    uint32_t aligned = ALIGN_UP((uint32_t)ptr, align);
    
    return (void*)aligned;
}

/*
 * 释放内存
 */
void kfree(void *ptr)
{
    if (!ptr) {
        return;
    }
    
    /* 获取块头 */
    struct heap_block *block = 
        (struct heap_block*)((uint32_t)ptr - sizeof(struct heap_block));
    
    /* 验证魔数 */
    if (block->magic != HEAP_MAGIC) {
        kprintf("[KMALLOC] ERROR: Invalid free (bad magic at 0x%08x)\n", ptr);
        return;
    }
    
    /* 检查是否已分配 */
    if (!block->allocated) {
        kprintf("[KMALLOC] ERROR: Double free detected at 0x%08x\n", ptr);
        return;
    }
    
    /* 标记为空闲 */
    block->allocated = false;
    
    /* 合并相邻空闲块 */
    merge_blocks(block);
}

/*
 * 重新分配内存
 */
void *krealloc(void *ptr, size_t new_size)
{
    if (!ptr) {
        return kmalloc(new_size);
    }
    
    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    /* 获取旧块大小 */
    size_t old_size = kmalloc_get_size(ptr);
    
    if (new_size <= old_size) {
        return ptr;  // 新大小更小，直接返回
    }
    
    /* 分配新块 */
    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }
    
    /* 复制数据 */
    memcpy(new_ptr, ptr, old_size);
    
    /* 释放旧块 */
    kfree(ptr);
    
    return new_ptr;
}

/*
 * 获取已分配块的大小
 */
size_t kmalloc_get_size(void *ptr)
{
    if (!ptr) {
        return 0;
    }
    
    struct heap_block *block = 
        (struct heap_block*)((uint32_t)ptr - sizeof(struct heap_block));
    
    if (block->magic != HEAP_MAGIC) {
        return 0;
    }
    
    return block->size - sizeof(struct heap_block);
}

/*
 * 获取堆统计信息
 */
void kmalloc_get_stats(struct heap_stats *stats)
{
    if (!stats) {
        return;
    }
    
    memset(stats, 0, sizeof(struct heap_stats));
    
    stats->total_size = heap_end - heap_start;
    
    struct heap_block *block = heap_first;
    while (block) {
        stats->num_blocks++;
        
        if (block->allocated) {
            stats->used_size += block->size;
        } else {
            stats->free_size += block->size;
            stats->num_free_blocks++;
        }
        
        block = block->next;
    }
}

/*
 * 打印堆统计信息
 */
void kmalloc_print_stats(void)
{
    struct heap_stats stats;
    kmalloc_get_stats(&stats);
    
    kprintf("\n=== Kernel Heap Statistics ===\n");
    kprintf("Total Size:    %u KB\n", stats.total_size / 1024);
    kprintf("Used Size:     %u KB\n", stats.used_size / 1024);
    kprintf("Free Size:     %u KB\n", stats.free_size / 1024);
    kprintf("Total Blocks:  %u\n", stats.num_blocks);
    kprintf("Free Blocks:   %u\n", stats.num_free_blocks);
    kprintf("Usage:         %u%%\n",
            stats.total_size ? (stats.used_size * 100 / stats.total_size) : 0);
    kprintf("\n");
}

/*
 * 验证堆完整性
 */
bool kmalloc_verify_heap(void)
{
    struct heap_block *block = heap_first;
    int count = 0;
    
    while (block) {
        /* 检查魔数 */
        if (block->magic != HEAP_MAGIC) {
            kprintf("[KMALLOC] ERROR: Corrupted block at 0x%08x (bad magic)\n", 
                    (uint32_t)block);
            return false;
        }
        
        /* 检查大小 */
        if (block->size < sizeof(struct heap_block)) {
            kprintf("[KMALLOC] ERROR: Invalid block size at 0x%08x\n", 
                    (uint32_t)block);
            return false;
        }
        
        /* 检查链表一致性 */
        if (block->next && block->next->prev != block) {
            kprintf("[KMALLOC] ERROR: Inconsistent block links at 0x%08x\n", 
                    (uint32_t)block);
            return false;
        }
        
        count++;
        if (count > 10000) {
            kprintf("[KMALLOC] ERROR: Possible infinite loop in heap\n");
            return false;
        }
        
        block = block->next;
    }
    
    return true;
}

