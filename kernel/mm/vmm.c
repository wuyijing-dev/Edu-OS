/*
 * 虚拟内存管理器（Virtual Memory Manager）
 * 
 * 管理页表、页目录和地址空间映射
 */

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <string.h>
#include <kernel.h>

/* 当前页目录 */
static struct page_directory *current_directory = NULL;
static struct page_directory kernel_directory;

/* 外部符号：内核结束地址（由链接脚本定义） */
extern uint32_t kernel_end;

/*
 * 创建页目录项
 */
static inline uint32_t pde_create(uint32_t pt_phys, uint32_t flags)
{
    return (pt_phys & 0xFFFFF000) | (flags & 0xFFF);
}

/*
 * 创建页表项
 */
static inline uint32_t pte_create(uint32_t page_phys, uint32_t flags)
{
    return (page_phys & 0xFFFFF000) | (flags & 0xFFF);
}

/*
 * 获取页表项的物理地址
 */
static inline uint32_t pte_get_addr(uint32_t pte)
{
    return pte & 0xFFFFF000;
}

/*
 * 检查页表项是否存在
 */
static inline bool pte_is_present(uint32_t pte)
{
    return (pte & PAGE_PRESENT) != 0;
}

/*
 * VMM初始化
 * 
 * 说明：bootloader已经设置好基本的页表并启用了分页
 *       这里我们继承bootloader的页目录并进行扩展和优化
 */
void vmm_init(uint32_t kernel_end_phys)
{
    kprintf("[VMM] Initializing Virtual Memory Manager...\n");
    
    /* 从CR3获取bootloader创建的页目录地址 */
    uint32_t pd_phys = vmm_get_cr3();
    kprintf("[VMM] Inherited Page Directory from bootloader: 0x%08x\n", pd_phys);
    
    kernel_directory.physical_addr = pd_phys;
    
    /* bootloader创建的页目录在物理地址0x9000
     * 由于已经启用分页，我们需要通过虚拟地址访问它
     * 
     * 当前映射：
     * - 0x00000000-0x003FFFFF (0-4MB) -> 0x00000000-0x003FFFFF (恒等映射)
     * - 0xC0000000-0xC03FFFFF (3GB-3GB+4MB) -> 0x00000000-0x003FFFFF
     * 
     * 所以物理地址0x9000可以通过虚拟地址0x9000访问（恒等映射）
     */
    uint32_t *pd = (uint32_t*)pd_phys;
    
    kprintf("[VMM] Page Directory entries:\n");
    kprintf("      PD[0] (0-4MB identity map): 0x%08x\n", pd[0]);
    kprintf("      PD[768] (3GB+ kernel map): 0x%08x\n", pd[768]);
    
    /*
     * 设置递归映射：将页目录的最后一项指向自己
     * 这样可以通过 0xFFC00000 访问所有页表
     * 通过 0xFFFFF000 访问页目录本身
     */
    pd[1023] = pde_create(pd_phys, PAGE_PRESENT | PAGE_WRITE);
    kprintf("[VMM] Recursive mapping set at PD[1023]\n");
    
    /* 刷新TLB以应用递归映射 */
    vmm_set_cr3(pd_phys);
    
    /* 现在可以通过递归映射访问页目录 */
    kernel_directory.entries = GET_PAGE_DIRECTORY();
    current_directory = &kernel_directory;
    
    kprintf("[VMM] Current address space layout:\n");
    kprintf("      Identity mapping: 0x00000000 - 0x003FFFFF (0-4MB)\n");
    kprintf("      Kernel mapping:   0xC0000000 - 0xC03FFFFF (3GB-3GB+4MB)\n");
    kprintf("      Kernel physical:  0x%08x - 0x%08x\n", 
            KERNEL_PHYSICAL_BASE, kernel_end_phys);
    
    /* 
     * 检查是否需要扩展内核映射
     * bootloader只映射了4MB，如果内核超过4MB需要扩展
     */
    uint32_t kernel_size = kernel_end_phys - KERNEL_PHYSICAL_BASE;
    uint32_t mapped_size = 4 * 1024 * 1024;  // bootloader映射了4MB
    
    if (kernel_size > mapped_size) {
        kprintf("[VMM] Kernel size (%u KB) exceeds mapped size (%u KB), extending...\n",
                kernel_size / 1024, mapped_size / 1024);
        
        /* 扩展映射（暂时不实现，因为我们的内核远小于4MB） */
        panic("VMM: Kernel too large, need to extend mapping");
    }
    
    kprintf("[VMM] Virtual Memory Manager initialized\n\n");
}

/*
 * 创建新的页目录
 */
struct page_directory *vmm_create_page_directory(void)
{
    struct page_directory *pd = (struct page_directory*)kmalloc(sizeof(struct page_directory));
    if (!pd) {
        return NULL;
    }
    
    /* 分配物理页目录 */
    pd->physical_addr = pmm_alloc_frame();
    if (pd->physical_addr == 0) {
        kfree(pd);
        return NULL;
    }
    
    /* 临时映射并清空 */
    // TODO: 实现临时映射机制
    
    return pd;
}

/*
 * 销毁页目录
 */
void vmm_destroy_page_directory(struct page_directory *pd)
{
    if (!pd || pd == &kernel_directory) {
        return;
    }
    
    /* 释放所有页表 */
    // TODO: 遍历并释放所有用户空间的页表
    
    /* 释放页目录 */
    pmm_free_frame(pd->physical_addr);
    kfree(pd);
}

/*
 * 切换到指定页目录
 */
void vmm_switch_page_directory(struct page_directory *pd)
{
    if (!pd) return;
    
    current_directory = pd;
    vmm_set_cr3(pd->physical_addr);
}

/*
 * 获取当前页目录
 */
struct page_directory *vmm_get_current_page_directory(void)
{
    return current_directory;
}

/*
 * 映射单个页
 */
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    
    uint32_t *page_directory = GET_PAGE_DIRECTORY();
    
    /* 检查页表是否存在 */
    if (!pte_is_present(page_directory[pd_index])) {
        /* 分配新页表 */
        uint32_t pt_phys = pmm_alloc_frame();
        if (pt_phys == 0) {
            panic("VMM: Cannot allocate page table");
        }
        
        page_directory[pd_index] = pde_create(pt_phys, PAGE_PRESENT | PAGE_WRITE | flags);
        
        /* 清空新页表 */
        uint32_t *page_table = GET_PAGE_TABLE(pd_index);
        memset(page_table, 0, PAGE_SIZE);
    }
    
    /* 获取页表并设置映射 */
    uint32_t *page_table = GET_PAGE_TABLE(pd_index);
    page_table[pt_index] = pte_create(phys, PAGE_PRESENT | flags);
    
    /* 刷新TLB */
    vmm_invlpg(virt);
}

/*
 * 取消映射单个页
 */
void vmm_unmap_page(uint32_t virt)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    
    uint32_t *page_directory = GET_PAGE_DIRECTORY();
    
    if (!pte_is_present(page_directory[pd_index])) {
        return;  // 页表不存在
    }
    
    uint32_t *page_table = GET_PAGE_TABLE(pd_index);
    
    if (pte_is_present(page_table[pt_index])) {
        page_table[pt_index] = 0;
        vmm_invlpg(virt);
    }
}

/*
 * 映射连续的物理内存区域
 */
void vmm_map_region(uint32_t virt_start, uint32_t phys_start, 
                    uint32_t size, uint32_t flags)
{
    uint32_t virt_end = virt_start + size;
    
    for (uint32_t virt = virt_start, phys = phys_start; 
         virt < virt_end; 
         virt += PAGE_SIZE, phys += PAGE_SIZE) {
        vmm_map_page(virt, phys, flags);
    }
}

/*
 * 取消映射内存区域
 */
void vmm_unmap_region(uint32_t virt_start, uint32_t size)
{
    uint32_t virt_end = virt_start + size;
    
    for (uint32_t virt = virt_start; virt < virt_end; virt += PAGE_SIZE) {
        vmm_unmap_page(virt);
    }
}

/*
 * 虚拟地址转物理地址
 */
uint32_t vmm_virt_to_phys(uint32_t virt)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    uint32_t offset = PAGE_OFFSET(virt);
    
    uint32_t *page_directory = GET_PAGE_DIRECTORY();
    
    if (!pte_is_present(page_directory[pd_index])) {
        return 0;  // 页表不存在
    }
    
    uint32_t *page_table = GET_PAGE_TABLE(pd_index);
    
    if (!pte_is_present(page_table[pt_index])) {
        return 0;  // 页不存在
    }
    
    uint32_t phys_page = pte_get_addr(page_table[pt_index]);
    return phys_page + offset;
}

/*
 * 检查页是否已映射
 */
bool vmm_is_mapped(uint32_t virt)
{
    return vmm_virt_to_phys(virt) != 0;
}

/*
 * 获取页标志
 */
uint32_t vmm_get_page_flags(uint32_t virt)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    
    uint32_t *page_directory = GET_PAGE_DIRECTORY();
    
    if (!pte_is_present(page_directory[pd_index])) {
        return 0;
    }
    
    uint32_t *page_table = GET_PAGE_TABLE(pd_index);
    return page_table[pt_index] & 0xFFF;
}

/*
 * 设置页标志
 */
void vmm_set_page_flags(uint32_t virt, uint32_t flags)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    
    uint32_t *page_directory = GET_PAGE_DIRECTORY();
    
    if (!pte_is_present(page_directory[pd_index])) {
        return;
    }
    
    uint32_t *page_table = GET_PAGE_TABLE(pd_index);
    
    if (pte_is_present(page_table[pt_index])) {
        uint32_t phys = pte_get_addr(page_table[pt_index]);
        page_table[pt_index] = pte_create(phys, flags);
        vmm_invlpg(virt);
    }
}

/*
 * 打印页表信息
 */
void vmm_print_page_info(uint32_t virt)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    
    kprintf("\n=== Page Info for 0x%08x ===\n", virt);
    kprintf("PD Index: %u, PT Index: %u, Offset: 0x%x\n", 
            pd_index, pt_index, PAGE_OFFSET(virt));
    
    uint32_t *pd = GET_PAGE_DIRECTORY();
    kprintf("PDE[%u] = 0x%08x", pd_index, pd[pd_index]);
    
    if (pte_is_present(pd[pd_index])) {
        kprintf(" (Present");
        if (pd[pd_index] & PAGE_WRITE) kprintf(", Write");
        if (pd[pd_index] & PAGE_USER) kprintf(", User");
        kprintf(")\n");
        
        uint32_t *pt = GET_PAGE_TABLE(pd_index);
        kprintf("PTE[%u] = 0x%08x", pt_index, pt[pt_index]);
        
        if (pte_is_present(pt[pt_index])) {
            kprintf(" (Present");
            if (pt[pt_index] & PAGE_WRITE) kprintf(", Write");
            if (pt[pt_index] & PAGE_USER) kprintf(", User");
            if (pt[pt_index] & PAGE_ACCESSED) kprintf(", Accessed");
            if (pt[pt_index] & PAGE_DIRTY) kprintf(", Dirty");
            kprintf(")\n");
            
            uint32_t phys = pte_get_addr(pt[pt_index]);
            kprintf("Physical Address: 0x%08x\n", phys + PAGE_OFFSET(virt));
        } else {
            kprintf(" (Not Present)\n");
        }
    } else {
        kprintf(" (Not Present)\n");
    }
    
    kprintf("\n");
}

/*
 * 打印整个页目录
 */
void vmm_print_page_directory(void)
{
    kprintf("\n=== Page Directory ===\n");
    
    uint32_t *pd = GET_PAGE_DIRECTORY();
    int count = 0;
    
    for (int i = 0; i < 1024; i++) {
        if (pte_is_present(pd[i])) {
            kprintf("PD[%3d] = 0x%08x", i, pd[i]);
            
            if (i == 0) {
                kprintf(" (Identity-mapped)\n");
            } else if (i >= KERNEL_PAGE_DIR_INDEX) {
                kprintf(" (Kernel)\n");
            } else {
                kprintf(" (User)\n");
            }
            
            count++;
        }
    }
    
    kprintf("\nTotal entries: %d / 1024\n\n", count);
}

