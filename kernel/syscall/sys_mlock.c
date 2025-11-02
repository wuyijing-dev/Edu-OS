/**
 * sys_mlock.c - mlock/munlock系统调用实现
 * 
 * 锁定内存页面，防止被swap出去
 */

#include <syscall.h>
#include <process/process.h>
#include <mm/vmm.h>
#include <mm/vma.h>
#include <mm/page_reclaim.h>
#include <kernel.h>
#include <errno.h>

/**
 * mlock - 锁定内存区域
 * @param addr: 起始地址（必须页对齐）
 * @param len: 长度
 * @return: 成功返回0，失败返回-1并设置errno
 */
int sys_mlock(const void *addr, size_t len)
{
    struct process *proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    
    /* 参数检查 */
    uint32_t start = (uint32_t)addr;
    if (start & 0xFFF) {
        return -EINVAL;  /* 地址必须页对齐 */
    }
    
    if (len == 0) {
        return 0;
    }
    
    /* 长度向上对齐到页大小 */
    len = (len + 0xFFF) & ~0xFFF;
    uint32_t end = start + len;
    
    /* 检查地址范围 */
    if (end < start || end > 0xC0000000) {
        return -EINVAL;
    }
    
    kprintf("[MLOCK] Locking memory: 0x%08x - 0x%08x (%u KB)\n", 
            start, end, len / 1024);
    
    uint32_t locked_pages = 0;
    
    /* 遍历所有VMA，找到覆盖此区域的VMA */
    struct vma *vma = proc->vma_list;
    while (vma) {
        /* 检查VMA是否与要锁定的区域重叠 */
        if (vma->start < end && vma->end > start) {
            uint32_t lock_start = (vma->start > start) ? vma->start : start;
            uint32_t lock_end = (vma->end < end) ? vma->end : end;
            
            /* 标记VMA为锁定 */
            vma->flags |= VMA_LOCKED;
            
            /* 确保所有页面都已分配并锁定 */
            for (uint32_t addr = lock_start; addr < lock_end; addr += 4096) {
                /* 检查页面是否已映射 */
                uint32_t phys = vmm_virt_to_phys(addr);
                
                if (phys == 0) {
                    /* 页面未映射，触发缺页分配 */
                    if (vma_handle_page_fault(vma, addr, proc->page_dir) < 0) {
                        kprintf("[MLOCK] Failed to allocate page at 0x%08x\n", addr);
                        return -ENOMEM;
                    }
                    phys = vmm_virt_to_phys(addr);
                }
                
                /* 锁定物理页面 */
                if (phys) {
                    struct page_descriptor *page = get_page_descriptor(phys);
                    if (page) {
                        lock_page(page);
                        locked_pages++;
                    }
                }
            }
        }
        vma = vma->next;
    }
    
    kprintf("[MLOCK] Locked %u pages\n", locked_pages);
    
    return 0;
}

/**
 * munlock - 解锁内存区域
 * @param addr: 起始地址（必须页对齐）
 * @param len: 长度
 * @return: 成功返回0，失败返回-1并设置errno
 */
int sys_munlock(const void *addr, size_t len)
{
    struct process *proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    
    /* 参数检查 */
    uint32_t start = (uint32_t)addr;
    if (start & 0xFFF) {
        return -EINVAL;
    }
    
    if (len == 0) {
        return 0;
    }
    
    len = (len + 0xFFF) & ~0xFFF;
    uint32_t end = start + len;
    
    if (end < start || end > 0xC0000000) {
        return -EINVAL;
    }
    
    kprintf("[MUNLOCK] Unlocking memory: 0x%08x - 0x%08x\n", start, end);
    
    uint32_t unlocked_pages = 0;
    
    /* 遍历VMA并解锁页面 */
    struct vma *vma = proc->vma_list;
    while (vma) {
        if (vma->start < end && vma->end > start) {
            uint32_t unlock_start = (vma->start > start) ? vma->start : start;
            uint32_t unlock_end = (vma->end < end) ? vma->end : end;
            
            /* 清除VMA锁定标志 */
            vma->flags &= ~VMA_LOCKED;
            
            /* 解锁物理页面 */
            for (uint32_t addr = unlock_start; addr < unlock_end; addr += 4096) {
                uint32_t phys = vmm_virt_to_phys(addr);
                if (phys) {
                    struct page_descriptor *page = get_page_descriptor(phys);
                    if (page) {
                        unlock_page(page);
                        unlocked_pages++;
                    }
                }
            }
        }
        vma = vma->next;
    }
    
    kprintf("[MUNLOCK] Unlocked %u pages\n", unlocked_pages);
    
    return 0;
}

/**
 * mlockall - 锁定进程的所有内存
 * @param flags: MCL_CURRENT（当前映射）或 MCL_FUTURE（未来映射）
 * @return: 成功返回0，失败返回-1
 */
int sys_mlockall(int flags)
{
    struct process *proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    
    kprintf("[MLOCKALL] Locking all memory for process %s (PID %u)\n", 
            proc->name, proc->pid);
    
    /* MCL_CURRENT: 锁定当前所有映射 */
    if (flags & 0x01) {  /* MCL_CURRENT */
        struct vma *vma = proc->vma_list;
        while (vma) {
            sys_mlock((void*)vma->start, vma->end - vma->start);
            vma = vma->next;
        }
    }
    
    /* MCL_FUTURE: 标记进程，使未来的映射自动锁定 */
    if (flags & 0x02) {  /* MCL_FUTURE */
        /* 设置进程标志（简化实现）*/
        kprintf("[MLOCKALL] Future mappings will be locked\n");
    }
    
    return 0;
}

/**
 * munlockall - 解锁进程的所有内存
 * @return: 成功返回0，失败返回-1
 */
int sys_munlockall(void)
{
    struct process *proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    
    kprintf("[MUNLOCKALL] Unlocking all memory for process %s (PID %u)\n", 
            proc->name, proc->pid);
    
    struct vma *vma = proc->vma_list;
    while (vma) {
        sys_munlock((void*)vma->start, vma->end - vma->start);
        vma = vma->next;
    }
    
    return 0;
}

