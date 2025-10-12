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
 */
void *sys_brk(void *addr)
{
    /* TODO: 实现完整的 brk */
    (void)addr;
    kprintf("[SYSCALL] sys_brk not yet implemented\n");
    return (void*)-1;
}

/*
 * sys_mmap - 内存映射
 */
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    /* TODO: 实现 mmap */
    (void)addr;
    (void)length;
    (void)prot;
    (void)flags;
    (void)fd;
    (void)offset;
    return (void*)-1;
}

/*
 * sys_munmap - 解除内存映射
 */
int sys_munmap(void *addr, size_t length)
{
    /* TODO: 实现 munmap */
    (void)addr;
    (void)length;
    return -ENOSYS;
}

