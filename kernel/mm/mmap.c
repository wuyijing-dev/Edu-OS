/*
 * mmap.c - 内存映射实现（mmap/munmap）
 * 
 * 支持：
 * - 匿名映射（MAP_ANONYMOUS）
 * - 文件映射（MAP_FILE）
 * - 共享映射（MAP_SHARED）
 * - 私有映射（MAP_PRIVATE）
 */

#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <process/process.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>

/* mmap标志 */
#define MAP_SHARED      0x01    /* 共享映射 */
#define MAP_PRIVATE     0x02    /* 私有映射（Copy-On-Write） */
#define MAP_FIXED       0x10    /* 固定地址 */
#define MAP_ANONYMOUS   0x20    /* 匿名映射（不关联文件） */

/* mmap保护标志 */
#define PROT_READ       0x1     /* 可读 */
#define PROT_WRITE      0x2     /* 可写 */
#define PROT_EXEC       0x4     /* 可执行 */
#define PROT_NONE       0x0     /* 不可访问 */

/* 内存映射区域结构 */
struct vm_area {
    uint32_t start;           /* 起始虚拟地址 */
    uint32_t end;             /* 结束虚拟地址 */
    uint32_t flags;           /* MAP_* 标志 */
    uint32_t prot;            /* PROT_* 保护标志 */
    int fd;                   /* 文件描述符（如果是文件映射） */
    uint32_t offset;          /* 文件偏移 */
    struct vm_area *next;     /* 链表指针 */
};

/* 进程的VMA链表（应该在PCB中，这里简化） */
static struct vm_area *vma_list_head = NULL;

/*
 * 分配虚拟地址空间（简化版：从0x40000000开始）
 */
static uint32_t find_free_vaddr(size_t size)
{
    uint32_t vaddr_start = 0x40000000;  /* 用户动态映射区起始 */
    uint32_t vaddr_end = 0x80000000;    /* 2GB限制 */
    uint32_t current = vaddr_start;
    
    /* 遍历已有映射，找空闲区域 */
    struct vm_area *vma = vma_list_head;
    while (vma) {
        if (current + size <= vma->start) {
            return current;  /* 找到足够的空间 */
        }
        current = (vma->end + 0xFFF) & ~0xFFF;  /* 对齐到页 */
        vma = vma->next;
    }
    
    if (current + size <= vaddr_end) {
        return current;
    }
    
    return 0;  /* 没有足够的空间 */
}

/*
 * mmap_impl - 内存映射实现
 * 
 * @param addr: 建议的起始地址（NULL表示由系统选择）
 * @param length: 映射长度
 * @param prot: 保护标志（PROT_READ|PROT_WRITE|PROT_EXEC）
 * @param flags: 映射标志（MAP_SHARED|MAP_PRIVATE|MAP_ANONYMOUS等）
 * @param fd: 文件描述符（MAP_ANONYMOUS时忽略）
 * @param offset: 文件偏移（MAP_ANONYMOUS时忽略）
 * @return: 映射的起始地址，失败返回MAP_FAILED
 */
void *mmap_impl(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    kprintf("[MMAP] Request: addr=%p, len=%u, prot=0x%x, flags=0x%x, fd=%d, off=%u\n",
            addr, length, prot, flags, fd, offset);
    
    /* 参数验证 */
    if (length == 0) {
        return (void*)-EINVAL;
    }
    
    /* 长度向上对齐到页大小 */
    length = (length + 0xFFF) & ~0xFFF;
    
    /* 确定映射地址 */
    uint32_t vaddr;
    if (flags & MAP_FIXED) {
        /* 固定地址 */
        vaddr = (uint32_t)addr;
        if (vaddr == 0 || vaddr & 0xFFF) {
            return (void*)-EINVAL;  /* 地址必须页对齐 */
        }
    } else {
        /* 系统分配地址 */
        vaddr = find_free_vaddr(length);
        if (vaddr == 0) {
            return (void*)-ENOMEM;
        }
    }
    
    kprintf("[MMAP] Mapping at 0x%08x, length=%u bytes\n", vaddr, length);
    
    /* 创建VMA结构 */
    struct vm_area *vma = kmalloc(sizeof(struct vm_area));
    if (!vma) {
        return (void*)-ENOMEM;
    }
    
    vma->start = vaddr;
    vma->end = vaddr + length;
    vma->flags = flags;
    vma->prot = prot;
    vma->fd = fd;
    vma->offset = offset;
    vma->next = vma_list_head;
    vma_list_head = vma;
    
    /* 计算页标志 */
    uint32_t page_flags = 0x01 | 0x04;  /* PRESENT | USER */
    if (prot & PROT_WRITE) {
        page_flags |= 0x02;  /* WRITABLE */
    }
    
    /* 获取当前进程（如果是内核线程或初始化阶段，使用内核页目录）*/
    struct process *proc = process_get_current();
    struct page_directory *target_pd = NULL;
    
    if (proc && proc->page_dir) {
        /* 用户进程：使用进程的页目录 */
        target_pd = proc->page_dir;
    } else {
        /* 内核线程或初始化：使用内核页目录 */
        extern struct page_directory *vmm_get_current_page_directory(void);
        target_pd = vmm_get_current_page_directory();
        
        if (!target_pd) {
            kfree(vma);
            return (void*)-EINVAL;
        }
    }
    
    /* 分配物理页并建立映射 */
    for (uint32_t va = vaddr; va < vaddr + length; va += 4096) {
        uint32_t paddr;
        
        if (flags & MAP_ANONYMOUS) {
            /* 匿名映射：分配新物理页 */
            paddr = pmm_alloc_frame();
            if (!paddr) {
                /* TODO: 清理已分配的页 */
                kfree(vma);
                return (void*)-ENOMEM;
            }
            
            /* 清零页 */
            if (paddr < 0x400000) {
                memset((void*)(paddr + 0xC0000000), 0, 4096);
            }
        } else {
            /* 文件映射：TODO: 从文件读取数据 */
            paddr = pmm_alloc_frame();
            if (!paddr) {
                kfree(vma);
                return (void*)-ENOMEM;
            }
            
            /* TODO: 读取文件内容到物理页 */
            if (paddr < 0x400000) {
                memset((void*)(paddr + 0xC0000000), 0, 4096);
            }
        }
        
        /* 在目标页表中建立映射 */
        extern void vmm_map_page_in_directory(struct page_directory *pd, 
                                              uint32_t virt, uint32_t phys, uint32_t flags);
        vmm_map_page_in_directory(target_pd, va, paddr, page_flags);
    }
    
    kprintf("[MMAP] Successfully mapped at 0x%08x\n", vaddr);
    
    return (void*)vaddr;
}

/*
 * munmap_impl - 取消内存映射实现
 * 
 * @param addr: 映射起始地址
 * @param length: 映射长度
 * @return: 成功返回0，失败返回-1
 */
int munmap_impl(void *addr, size_t length)
{
    uint32_t vaddr = (uint32_t)addr;
    
    kprintf("[MUNMAP] Unmapping: addr=0x%08x, len=%u\n", vaddr, length);
    
    /* 参数验证 */
    if (vaddr == 0 || (vaddr & 0xFFF) || length == 0) {
        return -EINVAL;
    }
    
    /* 长度向上对齐到页大小 */
    length = (length + 0xFFF) & ~0xFFF;
    
    /* 查找对应的VMA */
    struct vm_area *vma = vma_list_head;
    struct vm_area *prev = NULL;
    
    while (vma) {
        if (vma->start == vaddr && vma->end == vaddr + length) {
            break;
        }
        prev = vma;
        vma = vma->next;
    }
    
    if (!vma) {
        kprintf("[MUNMAP] VMA not found\n");
        return -EINVAL;
    }
    
    /* 获取目标页目录 */
    struct process *proc = process_get_current();
    struct page_directory *target_pd = NULL;
    
    if (proc && proc->page_dir) {
        target_pd = proc->page_dir;
    } else {
        extern struct page_directory *vmm_get_current_page_directory(void);
        target_pd = vmm_get_current_page_directory();
        if (!target_pd) {
            return -EINVAL;
        }
    }
    
    /* 取消页表映射并释放物理页 */
    for (uint32_t va = vaddr; va < vaddr + length; va += 4096) {
        /* 获取物理地址 */
        extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
        uint32_t paddr = vmm_virt_to_phys_in_directory(target_pd, va);
        
        if (paddr) {
            /* 如果是私有映射或最后一个引用，释放物理页 */
            if (vma->flags & MAP_PRIVATE) {
                pmm_free_frame(paddr);
            }
            /* TODO: 对于共享映射，需要引用计数 */
            
            /* 取消映射 */
            extern void vmm_unmap_page(uint32_t virt);
            vmm_unmap_page(va);
        }
    }
    
    /* 从VMA链表中移除 */
    if (prev) {
        prev->next = vma->next;
    } else {
        vma_list_head = vma->next;
    }
    
    kfree(vma);
    
    kprintf("[MUNMAP] Successfully unmapped\n");
    
    return 0;
}

/*
 * sys_mprotect - 修改内存保护属性
 * 
 * @param addr: 起始地址
 * @param length: 长度
 * @param prot: 新的保护标志
 * @return: 成功返回0，失败返回-1
 */
int sys_mprotect(void *addr, size_t length, int prot)
{
    uint32_t vaddr = (uint32_t)addr;
    
    kprintf("[MPROTECT] addr=0x%08x, len=%u, prot=0x%x\n", vaddr, length, prot);
    
    /* TODO: 实现mprotect */
    /* 需要修改页表项的权限位 */
    
    return -ENOSYS;
}
