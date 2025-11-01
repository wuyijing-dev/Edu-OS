/*
 * vma.c - Virtual Memory Area 管理实现
 */

#include <mm/vma.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel.h>
#include <string.h>
#include <serial.h>
#include <process/process.h>
#include <fs/vfs.h>

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
    vma->fd = -1;
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
 * 处理 VMA 缺页（Linux风格实现）
 * 
 * 当访问一个 VMA 区域但页不存在时调用
 * 支持：文件映射、匿名映射、零页、COW私有映射
 */
int vma_handle_page_fault(struct vma *vma, uint32_t fault_addr, 
                          struct page_directory *pd)
{
    if (!vma || !pd) {
        return -1;
    }
    
    uint32_t vaddr = fault_addr & ~0xFFF;
    
    /* 调试：输出单个字符避免触发新缺页 */
    serial_write('V');
    
    /* 检查地址是否在 VMA 范围内 */
    if (vaddr < vma->start || vaddr >= vma->end) {
        return -1;
    }
    
    /* 分配物理页 */
    uint32_t paddr = pmm_alloc_frame();
    if (!paddr) {
        serial_write('!');
        return -1;
    }
    serial_write('A');
    
    /* 初始化页面内容（根据VMA类型）*/
    void *page_ptr = NULL;
    bool need_kunmap = false;
    
    if (paddr < 0x400000) {
        /* 低端内存：直接映射 */
        page_ptr = (void*)(paddr + 0xC0000000);
    } else {
        /* 高端内存：使用kmap临时映射 */
        extern void *kmap(uint32_t paddr);
        page_ptr = kmap(paddr);
        need_kunmap = true;
        serial_write('H');  /* H = High memory */
    }
    
    if (page_ptr) {
        /* Linux风格：先清零整页 */
        memset(page_ptr, 0, 4096);
        
        /* 根据VMA类型填充内容 */
        if (vma->flags & VMA_FILE) {
            serial_write('F');
            /* 文件映射：从文件读取数据 */
            if (vma->fd >= 0) {
                /* 获取当前进程的fd_table */
                extern struct process *process_get_current(void);
                struct process *proc = process_get_current();
                
                if (proc && proc->fd_table && vma->fd < MAX_FILES_PER_PROCESS) {
                    struct vfs_file *file = proc->fd_table->files[vma->fd];
                    
                    if (file) {
                        /* 计算文件偏移 */
                        uint32_t offset_in_vma = vaddr - vma->start;
                        uint32_t file_offset = vma->file_offset + offset_in_vma;
                        
                        /* 关闭中断，避免在VFS操作中被打断 */
                        asm volatile("cli");
                        
                        serial_write('S');
                        
                        /* 保存当前文件位置 */
                        uint32_t old_pos = file->pos;
                        
                        /* 设置文件位置 */
                        file->pos = file_offset;
                        
                        /* 使用vfs_file_read读取 */
                        extern ssize_t vfs_file_read(struct vfs_file *file, void *buf, size_t count);
                        serial_write('R');
                        int bytes_read = vfs_file_read(file, (char*)page_ptr, 4096);
                        
                        /* 恢复文件位置 */
                        file->pos = old_pos;
                        
                        if (bytes_read < 0) {
                            serial_write('E');
                        } else {
                            serial_write('K');
                        }
                        
                        /* 恢复中断（会在iret时自动恢复） */
                    } else {
                        serial_write('N');  /* No file */
                    }
                } else {
                    serial_write('P');  /* No process or fd_table */
                }
            }
        } else if (vma->private_data) {
            /* 设备映射：直接使用物理地址（如BGA framebuffer）
             * private_data存储设备的物理基地址
             */
            serial_write('D');
            serial_write('E');
            serial_write('V');
            
            /* 计算本页对应的设备物理地址 */
            uint32_t offset_in_vma = vaddr - vma->start;
            uint32_t device_phys = (uint32_t)vma->private_data + offset_in_vma;
            
            /* 使用设备物理地址替代paddr */
            pmm_free_frame(paddr);  /* 释放刚分配的物理页 */
            paddr = device_phys;     /* 使用设备物理地址 */
            page_ptr = NULL;         /* 设备内存不需要初始化 */
        }
        /* VMA_ZERO 和 VMA_ANONYMOUS 已经通过 memset 处理了 */
    }
    
    /* 计算页表标志（Linux风格）*/
    uint32_t page_flags = 0x01 | 0x04;  /* PRESENT | USER */
    
    /* 写权限 */
    if (vma->flags & VMA_WRITE) {
        /* 如果是私有映射，先设为只读（COW）*/
        if (vma->flags & VMA_PRIVATE) {
            /* COW: 首次映射为只读，写入时触发缺页复制 */
            page_flags &= ~0x02;
        } else {
            page_flags |= 0x02;  /* WRITABLE */
        }
    }
    
    /* 执行权限（x86不支持NX位，除非启用PAE）*/
    /* 在标准32位模式下，所有页面都可执行 */
    
    /* 建立映射 */
    serial_write('M');
    extern void vmm_map_page_in_directory(struct page_directory *pd,
                                          uint32_t virt, uint32_t phys,
                                          uint32_t flags);
    vmm_map_page_in_directory(pd, vaddr, paddr, page_flags);
    
    /* 刷新TLB（关键！）*/
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    
    serial_write('D');
    
    /* 如果使用了kmap，需要解除映射 */
    if (need_kunmap && page_ptr) {
        extern void kunmap(void *vaddr);
        kunmap(page_ptr);
    }
    
    /* 调试输出（可选）*/
    #if 0
    kprintf("[VMA] Page fault handled: vaddr=0x%08x -> paddr=0x%08x "
            "(flags=0x%02x, vma_flags=0x%02x)\n",
            vaddr, paddr, page_flags, vma->flags);
    #endif
    
    return 0;
}
