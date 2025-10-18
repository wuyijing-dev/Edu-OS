#ifndef _MM_VMM_H
#define _MM_VMM_H

/*
 * 虚拟内存管理器（Virtual Memory Manager）
 * 
 * 管理页表、地址空间映射
 */

#include <types.h>

/* 虚拟内存布局常量 */
#define KERNEL_VIRTUAL_BASE     0xC0000000  // 内核虚拟地址起始（3GB）
#define KERNEL_PHYSICAL_BASE    0x00100000  // 内核物理地址起始（1MB）
#define KERNEL_PAGE_DIR_INDEX   (KERNEL_VIRTUAL_BASE >> 22)  // 768

/* 递归页表映射地址 */
#define PAGE_TABLE_VIRT_BASE    0xFFC00000  // 页表虚拟地址
#define PAGE_DIR_VIRT_ADDR      0xFFFFF000  // 页目录虚拟地址

/* 用户空间地址范围 */
#define USER_SPACE_START        0x00000000
#define USER_SPACE_END          0xC0000000

/* 页表标志位 */
#define PAGE_PRESENT            0x001   // P: 存在
#define PAGE_WRITE              0x002   // R/W: 可写
#define PAGE_USER               0x004   // U/S: 用户可访问
#define PAGE_PWT                0x008   // PWT: 写穿透
#define PAGE_PCD                0x010   // PCD: 禁用缓存
#define PAGE_ACCESSED           0x020   // A: 已访问
#define PAGE_DIRTY              0x040   // D: 已修改
#define PAGE_4MB                0x080   // PS: 4MB页（仅PDE）
#define PAGE_GLOBAL             0x100   // G: 全局页
#define PAGE_CUSTOM_0           0x200   // 自定义位0
#define PAGE_CUSTOM_1           0x400   // 自定义位1
#define PAGE_CUSTOM_2           0x800   // 自定义位2

/* 常用标志组合 */
#define PAGE_FLAGS_KERNEL       (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_FLAGS_USER_RO      (PAGE_PRESENT | PAGE_USER)
#define PAGE_FLAGS_USER_RW      (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

/* 页目录/页表索引提取 */
#define PD_INDEX(virt)          ((virt) >> 22)
#define PT_INDEX(virt)          (((virt) >> 12) & 0x3FF)
#define PAGE_OFFSET(virt)       ((virt) & 0xFFF)

/* 获取页表虚拟地址 */
#define GET_PAGE_TABLE(pd_idx)  ((uint32_t*)(PAGE_TABLE_VIRT_BASE + (pd_idx) * 0x1000))
#define GET_PAGE_DIRECTORY()    ((uint32_t*)PAGE_DIR_VIRT_ADDR)

/*
 * 页目录结构
 */
struct page_directory {
    uint32_t physical_addr;     // 页目录物理地址
    uint32_t *entries;          // 页目录项数组（虚拟地址）
};

/*
 * VMM初始化
 * 
 * @param kernel_end: 内核结束物理地址
 */
void vmm_init(uint32_t kernel_end);

/*
 * 创建新的页目录
 * 
 * @return: 新页目录指针
 */
struct page_directory *vmm_create_page_directory(void);

/*
 * 销毁页目录
 * 
 * @param pd: 要销毁的页目录
 */
void vmm_destroy_page_directory(struct page_directory *pd);

/*
 * 切换到指定页目录
 * 
 * @param pd: 要切换到的页目录
 */
void vmm_switch_page_directory(struct page_directory *pd);

/*
 * 获取当前页目录
 * 
 * @return: 当前页目录指针
 */
struct page_directory *vmm_get_current_page_directory(void);

/*
 * 映射单个页
 * 
 * @param virt: 虚拟地址
 * @param phys: 物理地址
 * @param flags: 页标志
 */
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

/*
 * 在指定页目录中映射页（Linux方式：不切换CR3）
 * 
 * @param pd: 目标页目录
 * @param virt: 虚拟地址
 * @param phys: 物理地址
 * @param flags: 页标志
 */
void vmm_map_page_in_directory(struct page_directory *pd, uint32_t virt, 
                                uint32_t phys, uint32_t flags);

/*
 * 在指定页目录中查询虚拟地址的物理地址
 * 
 * @param pd: 目标页目录
 * @param virt: 虚拟地址
 * @return: 物理地址，如果未映射返回0
 */
uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);

/*
 * 取消映射单个页
 * 
 * @param virt: 虚拟地址
 */
void vmm_unmap_page(uint32_t virt);

/*
 * 映射连续的物理内存区域
 * 
 * @param virt_start: 起始虚拟地址
 * @param phys_start: 起始物理地址
 * @param size: 大小（字节）
 * @param flags: 页标志
 */
void vmm_map_region(uint32_t virt_start, uint32_t phys_start, 
                    uint32_t size, uint32_t flags);

/*
 * 取消映射内存区域
 * 
 * @param virt_start: 起始虚拟地址
 * @param size: 大小（字节）
 */
void vmm_unmap_region(uint32_t virt_start, uint32_t size);

/*
 * 虚拟地址转物理地址
 * 
 * @param virt: 虚拟地址
 * @return: 物理地址，如果未映射返回0
 */
uint32_t vmm_virt_to_phys(uint32_t virt);

/*
 * 检查页是否已映射
 * 
 * @param virt: 虚拟地址
 * @return: true=已映射，false=未映射
 */
bool vmm_is_mapped(uint32_t virt);

/*
 * 获取页标志
 * 
 * @param virt: 虚拟地址
 * @return: 页标志，如果未映射返回0
 */
uint32_t vmm_get_page_flags(uint32_t virt);

/*
 * 设置页标志
 * 
 * @param virt: 虚拟地址
 * @param flags: 新的页标志
 */
void vmm_set_page_flags(uint32_t virt, uint32_t flags);

/*
 * 刷新TLB（单个页）
 * 
 * @param virt: 虚拟地址
 */
static inline void vmm_invlpg(uint32_t virt)
{
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/*
 * 刷新整个TLB
 */
static inline void vmm_flush_tlb(void)
{
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/*
 * 启用分页
 */
static inline void vmm_enable_paging(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // 设置PG位
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

/*
 * 禁用分页
 */
static inline void vmm_disable_paging(void)
{
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~0x80000000;  // 清除PG位
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

/*
 * 设置页目录基址（CR3）
 * 
 * @param phys_addr: 页目录物理地址
 */
static inline void vmm_set_cr3(uint32_t phys_addr)
{
    __asm__ volatile("mov %0, %%cr3" : : "r"(phys_addr) : "memory");
}

/*
 * 获取页目录基址（CR3）
 * 
 * @return: 页目录物理地址
 */
static inline uint32_t vmm_get_cr3(void)
{
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/*
 * 打印页表信息
 * 
 * @param virt: 虚拟地址
 */
void vmm_print_page_info(uint32_t virt);

/*
 * 打印整个页目录
 */
void vmm_print_page_directory(void);

#endif /* _MM_VMM_H */

