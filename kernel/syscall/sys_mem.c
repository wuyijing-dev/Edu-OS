/*
 * sys_mem.c - 内存管理相关系统调用
 */

#include <syscall.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <mm/vmm.h>
#include <mm/kmalloc.h>

/*
 * sys_brk - 改变数据段大小
 * 
 * Linux 用于实现 malloc
 * 简化实现：返回当前 brk 值，实际内存分配由 mmap 完成
 */
void *sys_brk(void *addr)
{
    /* 简化实现：维护一个简单的brk指针 */
    static void *current_brk = (void*)0x10000000;  /* 256MB起始 */
    
    if (addr == NULL) {
        /* 查询当前 brk */
        return current_brk;
    }
    
    /* TODO: 验证地址范围合法性 */
    /* TODO: 分配/释放内存页 */
    
    /* 简化：直接更新 brk */
    current_brk = addr;
    return current_brk;
}

/* 外部实现（在kernel/mm/mmap.c中） */
extern void *mmap_impl(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_impl(void *addr, size_t length);

/*
 * sys_mmap - 内存映射
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return mmap_impl(addr, length, prot, flags, fd, offset);
}

/*
 * sys_munmap - 解除内存映射
 */
int sys_munmap(void *addr, size_t length)
{
    return munmap_impl(addr, length);
}

