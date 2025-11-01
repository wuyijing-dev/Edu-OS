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
#include <mm/vma.h>
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
    
    /* 特殊处理：设备映射（如framebuffer）
     * Linux风格：通过检查文件inode来判断是否为设备映射
     * - 不从文件读取数据
     * - 直接映射设备物理内存
     */
    bool is_device_mapping = false;
    
    if (!(flags & MAP_ANONYMOUS) && fd >= 0) {
        /* 通过进程fd_table获取文件 */
        extern struct process *process_get_current(void);
        struct process *proc = process_get_current();
        
        kprintf("[MMAP_DEBUG] flags=0x%x, fd=%d, proc=%p\n", flags, fd, proc);
        
        if (proc && proc->fd_table && fd < MAX_FILES_PER_PROCESS) {
            struct vfs_file *file = proc->fd_table->files[fd];
            kprintf("[MMAP_DEBUG] fd_table=%p, file=%p\n", proc->fd_table, file);
            
            if (file && file->inode) {
                kprintf("[MMAP_DEBUG] inode=%p, rdev=0x%x\n", file->inode, file->inode->rdev);
                
                /* 检查是否为framebuffer设备（通过rdev判断）
                 * Linux风格：使用MAJOR宏提取major号（已通过vfs.h包含）
                 * framebuffer设备major=29
                 */
                uint32_t major = MAJOR(file->inode->rdev);
                kprintf("[MMAP_DEBUG] major=%d (checking if == 29)\n", major);
                
                if (major == 29) {
                    is_device_mapping = true;
                    kprintf("[MMAP] Framebuffer device detected (fd=%d, rdev=0x%x, major=%d)\n", 
                            fd, file->inode->rdev, major);
                }
            }
        }
    }
    
    if (is_device_mapping) {
        kprintf("[MMAP] Device mapping detected, will use lazy device VMA\n");
        /* 不直接返回内核地址，而是创建用户空间VMA
         * VMA的Page Fault处理器会映射设备内存
         */
    }
    
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
    
    kprintf("[MMAP] Mapping at 0x%08x, length=%u bytes (lazy)\n", vaddr, length);
    
    /* 使用新VMA系统：按需分配 */
    extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
    extern void vma_add(struct vma **list, struct vma *vma);
    extern struct process *process_get_current(void);
    
    struct process *proc = process_get_current();
    if (!proc) {
        kprintf("[MMAP] Warning: No current process, falling back to immediate allocation\n");
        /* 对于内核上下文，使用旧的立即分配方式 */
        goto immediate_alloc;
    }
    
    /* 创建VMA */
    uint32_t vma_flags = 0;
    if (prot & PROT_READ)  vma_flags |= VMA_READ;
    if (prot & PROT_WRITE) vma_flags |= VMA_WRITE;
    if (prot & PROT_EXEC)  vma_flags |= VMA_EXEC;
    
    if (flags & MAP_ANONYMOUS) {
        vma_flags |= VMA_ANONYMOUS;
    } else if (is_device_mapping) {
        /* 设备映射：标记为匿名+设备，不从文件读取 */
        vma_flags |= VMA_ANONYMOUS;  
        kprintf("[MMAP] Device VMA will map physical memory directly\n");
    } else {
        vma_flags |= VMA_FILE;
    }
    
    if (flags & MAP_SHARED) {
        vma_flags |= VMA_SHARED;
    } else {
        vma_flags |= VMA_PRIVATE;
    }
    
    struct vma *new_vma = vma_create(vaddr, vaddr + length, vma_flags);
    if (!new_vma) {
        return (void*)-ENOMEM;
    }
    
    /* 设置文件/设备信息 */
    if (!(flags & MAP_ANONYMOUS)) {
        new_vma->fd = fd;
        new_vma->file_offset = offset;
        if (is_device_mapping) {
            /* Linux风格：设备映射 - 通过ioctl或设备驱动获取物理地址 */
            extern uint32_t bga_get_framebuffer_physical(void);
            uint32_t device_phys = bga_get_framebuffer_physical();
            
            if (device_phys) {
                new_vma->private_data = (void*)device_phys;
                kprintf("[MMAP] Device VMA: fd=%d, phys=0x%08x, size=%u\n", 
                        fd, device_phys, length);
            } else {
                kprintf("[MMAP] Warning: Device physical address not available\n");
            }
        }
    }
    
    /* 添加到进程VMA列表 */
    vma_add(&proc->vma_list, new_vma);
    
    kprintf("[MMAP] VMA created successfully (lazy allocation enabled)\n");
    return (void*)vaddr;
    
immediate_alloc:
    /* 旧的立即分配方式（内核上下文） */
    kprintf("[MMAP] Using immediate allocation (kernel context)\n");
    
    /* 创建旧VMA结构 */
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
    
    /* 获取当前页目录（内核上下文的立即分配） */
    struct page_directory *target_pd = NULL;
    proc = process_get_current();  /* 重用前面定义的proc变量 */
    
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
    
    if (vaddr == 0 || (vaddr & 0xFFF) || length == 0) {
        return -EINVAL;
    }
    
    length = (length + 0xFFF) & ~0xFFF;
    
    struct process *proc = process_get_current();
    if (!proc || !proc->page_dir) {
        return -EINVAL;
    }
    
    /* 查找对应的VMA */
    extern struct vma *vma_find(struct vma *list, uint32_t addr);
    extern void vma_remove(struct vma **list, struct vma *vma);
    
    struct vma *vma = vma_find(proc->vma_list, vaddr);
    if (!vma) {
        return -EINVAL;
    }
    
    /* 释放页面 */
    extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
    extern void vmm_unmap_page(uint32_t virt);
    
    for (uint32_t va = vaddr; va < vaddr + length; va += 4096) {
        uint32_t paddr = vmm_virt_to_phys_in_directory(proc->page_dir, va);
        if (paddr) {
            if (vma->flags & VMA_PRIVATE) {
                pmm_free_frame(paddr);
            }
            vmm_unmap_page(va);
        }
    }
    
    /* 移除VMA */
    vma_remove(&proc->vma_list, vma);
    extern void vma_destroy(struct vma *vma);
    vma_destroy(vma);
    
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
    
    if (vaddr == 0 || (vaddr & 0xFFF) || length == 0) {
        return -EINVAL;
    }
    
    length = (length + 0xFFF) & ~0xFFF;
    
    struct process *proc = process_get_current();
    if (!proc || !proc->page_dir) {
        return -EINVAL;
    }
    
    /* 查找VMA */
    extern struct vma *vma_find(struct vma *list, uint32_t addr);
    struct vma *vma = vma_find(proc->vma_list, vaddr);
    
    if (!vma || vaddr + length > vma->end) {
        return -ENOMEM;
    }
    
    /* 更新VMA权限 */
    vma->flags &= ~(VMA_READ | VMA_WRITE | VMA_EXEC);
    if (prot & PROT_READ)  vma->flags |= VMA_READ;
    if (prot & PROT_WRITE) vma->flags |= VMA_WRITE;
    if (prot & PROT_EXEC)  vma->flags |= VMA_EXEC;
    
    /* 更新页表项 */
    uint32_t page_flags = 0x01 | 0x04;
    if (prot & PROT_WRITE) page_flags |= 0x02;
    
    extern void vmm_map_page_in_directory(struct page_directory *pd,
                                          uint32_t virt, uint32_t phys,
                                          uint32_t flags);
    extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
    
    for (uint32_t va = vaddr; va < vaddr + length; va += 4096) {
        uint32_t paddr = vmm_virt_to_phys_in_directory(proc->page_dir, va);
        if (paddr) {
            vmm_map_page_in_directory(proc->page_dir, va, paddr, page_flags);
        }
    }
    
    return 0;
}
