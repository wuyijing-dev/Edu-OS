/*
 * sys_mem.c - 内存管理相关系统调用
 */

#include <syscall.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <mm/vmm.h>
#include <mm/kmalloc.h>
#include <mm/vma.h>
#include <process/process.h>

/*
 * sys_brk - 改变数据段大小（Linux风格实现）
 * 
 * Linux 用于实现 malloc
 * 使用VMA管理堆空间，支持按需分配
 * 
 * @param addr: 新的brk地址（页对齐）
 * @return: 成功返回新brk，失败返回旧brk
 */
void *sys_brk(void *addr)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc) {
        return (void*)-1;
    }
    
    /* 堆起始地址（通常在BSS段之后）*/
    static void *heap_start = NULL;
    static void *current_brk = NULL;
    
    /* 首次调用：初始化堆 */
    if (heap_start == NULL) {
        heap_start = (void*)0x10000000;  /* 256MB处开始 */
        current_brk = heap_start;
        
        kprintf("[BRK] Heap initialized at 0x%08x\n", (uint32_t)heap_start);
    }
    
    /* 查询当前brk */
    if (addr == NULL || addr == (void*)0) {
        return current_brk;
    }
    
    /* 验证地址范围 */
    if (addr < heap_start) {
        /* 不能缩小到堆起始之前 */
        kprintf("[BRK] Error: addr < heap_start\n");
        return current_brk;
    }
    
    if ((uint32_t)addr >= 0xC0000000) {
        /* 不能进入内核空间 */
        kprintf("[BRK] Error: addr in kernel space\n");
        return current_brk;
    }
    
    /* 页对齐 */
    uint32_t new_brk = ((uint32_t)addr + 0xFFF) & ~0xFFF;
    uint32_t old_brk = ((uint32_t)current_brk + 0xFFF) & ~0xFFF;
    
    if (new_brk > old_brk) {
        /* 扩展堆：创建新的VMA */
        extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
        extern void vma_add(struct vma **list, struct vma *vma);
        
        /* 检查是否已有堆VMA */
        extern struct vma *vma_find(struct vma *list, uint32_t addr);
        struct vma *existing = vma_find(proc->vma_list, old_brk);
        
        if (existing && existing->start == (uint32_t)heap_start) {
            /* 已有堆VMA，扩展它 */
            existing->end = new_brk;
            kprintf("[BRK] Extended heap VMA: 0x%08x-0x%08x\n", 
                    existing->start, existing->end);
        } else {
            /* 创建新的堆VMA */
            struct vma *heap_vma = vma_create(old_brk, new_brk,
                                              0x01 | 0x02 | 0x80);  /* READ|WRITE|ANONYMOUS */
            if (heap_vma) {
                heap_vma->fd = -1;
                vma_add(&proc->vma_list, heap_vma);
                
                kprintf("[BRK] Created heap VMA: 0x%08x-0x%08x (%u KB)\n",
                        old_brk, new_brk, (new_brk - old_brk) / 1024);
            } else {
                return current_brk;  /* 失败 */
            }
        }
    } else if (new_brk < old_brk) {
        /* 缩小堆：释放VMA和页面 */
        extern struct vma *vma_find(struct vma *list, uint32_t addr);
        struct vma *vma = vma_find(proc->vma_list, old_brk - 4096);
        
        if (vma && vma->start == (uint32_t)heap_start) {
            /* 缩小堆VMA */
            vma->end = new_brk;
            
            /* 释放多余的页面 */
            extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
            extern void pmm_free_frame(uint32_t addr);
            extern void vmm_unmap_page(uint32_t virt);
            
            for (uint32_t va = new_brk; va < old_brk; va += 4096) {
                uint32_t paddr = vmm_virt_to_phys_in_directory(proc->page_dir, va);
                if (paddr) {
                    pmm_free_frame(paddr);
                    vmm_unmap_page(va);
                }
            }
            
            kprintf("[BRK] Heap shrunk: 0x%08x-0x%08x\n", vma->start, vma->end);
        }
    }
    
    /* 更新brk */
    current_brk = addr;
    return current_brk;
}

/* 外部实现（在kernel/mm/mmap.c中） */
extern void *mmap_impl(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_impl(void *addr, size_t length);

/*
 * sys_mmap - 内存映射
 * 
 * 我们的实现：用户态通过结构体传参（ebx指向结构体）
 * 因为x86系统调用最多5个寄存器参数，mmap需要6个
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    /* Linux风格：实现copy_from_user来安全读取用户空间数据 */
    uint32_t args_ptr = (uint32_t)addr;
    
    kprintf("[SYS_MMAP] Called with addr=0x%08x\n", args_ptr);
    
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    kprintf("[SYS_MMAP] Current process: %s (PID %u)\n", 
            proc ? proc->name : "NULL", proc ? proc->pid : 0);
    kprintf("[SYS_MMAP] page_dir: 0x%08x\n", proc && proc->page_dir ? proc->page_dir->physical_addr : 0);
    
    /* 本地数组，从用户空间复制 */
    unsigned long args_copy[6];
    
    /* Linux风格：通过copy_from_user安全访问用户空间 */
    if (proc && proc->page_dir && args_ptr >= 0x08000000 && args_ptr < 0xC0000000) {
        extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
        uint32_t args_phys = vmm_virt_to_phys_in_directory(proc->page_dir, args_ptr);
        
        kprintf("[SYS_MMAP] Args phys: 0x%08x\n", args_phys);
        
        if (args_phys) {
            /* 使用kmap（支持低端和高端内存统一接口）*/
            extern void *kmap(uint32_t paddr);
            extern void kunmap(void *vaddr);
            
            void *mapped_ptr = kmap(args_phys);
            
            if (mapped_ptr) {
                /* 直接读取：mapped_ptr已经指向正确的物理页 */
                unsigned long *args_array = (unsigned long*)mapped_ptr;
                
                /* 复制参数 */
                for (int i = 0; i < 6; i++) {
                    args_copy[i] = args_array[i];
                }
                
                kunmap(mapped_ptr);
                
                kprintf("[SYS_MMAP] Decoded: addr=%p, len=%u, prot=0x%x, flags=0x%x, fd=%d, off=%u\n",
                        (void*)args_copy[0], args_copy[1], args_copy[2], args_copy[3], 
                        (int)args_copy[4], args_copy[5]);
                
                return mmap_impl((void*)args_copy[0], args_copy[1], args_copy[2], 
                                args_copy[3], (int)args_copy[4], args_copy[5]);
            } else {
                kprintf("[SYS_MMAP] ERROR: kmap failed for 0x%08x\n", args_phys);
                return (void*)-EFAULT;
            }
        } else {
            kprintf("[SYS_MMAP] ERROR: Args not mapped at 0x%08x\n", args_ptr);
            return (void*)-EFAULT;
        }
    }
    
    kprintf("[SYS_MMAP] ERROR: Invalid context or address\n");
    return (void*)-EINVAL;
}

/*
 * sys_munmap - 解除内存映射
 */
int sys_munmap(void *addr, size_t length)
{
    return munmap_impl(addr, length);
}

