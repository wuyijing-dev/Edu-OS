/*
 * vma.c - Virtual Memory Area 管理实现
 */

#include <mm/vma.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel.h>
#include <string.h>

/*
 * 创建新的 VMA
 */
struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags)
{
    struct vma *vma = kmalloc(sizeof(struct vma));
    if (!vma) {
        return NULL;
    }
    
    vma->start = start & ~0xFFF;  /* 页对齐 */
    vma->end = (end + 0xFFF) & ~0xFFF;
    vma->flags = flags;
    vma->file_offset = 0;
    vma->private_data = NULL;
    vma->next = NULL;
    
    return vma;
}

/*
 * 销毁 VMA
 */
void vma_destroy(struct vma *vma)
{
    if (vma) {
        kfree(vma);
    }
}

/*
 * 查找包含指定地址的 VMA
 */
struct vma *vma_find(struct vma *list, uint32_t addr)
{
    struct vma *vma = list;
    
    while (vma) {
        if (addr >= vma->start && addr < vma->end) {
            return vma;
        }
        vma = vma->next;
    }
    
    return NULL;
}

/*
 * 添加 VMA 到链表
 */
void vma_add(struct vma **list, struct vma *vma)
{
    if (!list || !vma) {
        return;
    }
    
    vma->next = *list;
    *list = vma;
}

/*
 * 从链表移除 VMA
 */
void vma_remove(struct vma **list, struct vma *vma)
{
    if (!list || !vma) {
        return;
    }
    
    struct vma *prev = NULL;
    struct vma *current = *list;
    
    while (current) {
        if (current == vma) {
            if (prev) {
                prev->next = current->next;
            } else {
                *list = current->next;
            }
            return;
        }
        prev = current;
        current = current->next;
    }
}

/*
 * 销毁所有 VMA
 */
void vma_destroy_all(struct vma *list)
{
    while (list) {
        struct vma *next = list->next;
        vma_destroy(list);
        list = next;
    }
}

/*
 * 处理 VMA 缺页
 * 
 * 当访问一个 VMA 区域但页不存在时调用
 */
int vma_handle_page_fault(struct vma *vma, uint32_t fault_addr, 
                          struct page_directory *pd)
{
    if (!vma || !pd) {
        return -1;
    }
    
    uint32_t vaddr = fault_addr & ~0xFFF;
    
    /* 检查地址是否在 VMA 范围内 */
    if (vaddr < vma->start || vaddr >= vma->end) {
        return -1;
    }
    
    /* 分配物理页 */
    uint32_t paddr = pmm_alloc_frame();
    if (!paddr) {
        kprintf("[VMA] Out of memory for page fault at 0x%08x\n", vaddr);
        return -1;
    }
    
    /* 如果是零页（BSS），清零 */
    if (vma->flags & VMA_ZERO) {
        if (paddr < 0x400000) {
            memset((void*)(paddr + 0xC0000000), 0, 4096);
        }
    }
    
    /* 计算页标志 */
    uint32_t page_flags = 0x01 | 0x04;  /* PRESENT | USER */
    if (vma->flags & VMA_WRITE) {
        page_flags |= 0x02;  /* WRITABLE */
    }
    
    /* 建立映射 */
    extern void vmm_map_page_in_directory(struct page_directory *pd,
                                          uint32_t virt, uint32_t phys,
                                          uint32_t flags);
    vmm_map_page_in_directory(pd, vaddr, paddr, page_flags);
    
    kprintf("[VMA] Page fault handled: vaddr=0x%08x -> paddr=0x%08x (flags=0x%x)\n",
            vaddr, paddr, vma->flags);
    
    return 0;
}
