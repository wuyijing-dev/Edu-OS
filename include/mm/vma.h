/*
 * vma.h - Virtual Memory Area（虚拟内存区域）
 * 
 * 用于实现按需分配（Lazy Allocation）
 */

#ifndef _MM_VMA_H
#define _MM_VMA_H

#include <types.h>

/* VMA 标志 */
#define VMA_READ      0x01  /* 可读 */
#define VMA_WRITE     0x02  /* 可写 */
#define VMA_EXEC      0x04  /* 可执行 */
#define VMA_ZERO      0x08  /* 按需分配零页（BSS段） */
#define VMA_FILE      0x10  /* 文件映射 */
#define VMA_SHARED    0x20  /* 共享映射 */
#define VMA_PRIVATE   0x40  /* 私有映射（COW） */
#define VMA_ANONYMOUS 0x80  /* 匿名映射 */
#define VMA_LOCKED    0x100 /* 内存锁定（不可换出） */

/* 虚拟内存区域 */
struct vma {
    uint32_t start;         /* 起始虚拟地址（页对齐） */
    uint32_t end;           /* 结束虚拟地址（页对齐） */
    uint32_t flags;         /* VMA_* 标志 */
    uint32_t file_offset;   /* 如果是文件映射，文件偏移 */
    int fd;                 /* 文件描述符（文件映射） */
    void *private_data;     /* 私有数据 */
    struct vma *next;       /* 链表指针 */
};

/* 前向声明 */
struct page_directory;

/* VMA 操作函数 */
struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
void vma_destroy(struct vma *vma);
struct vma *vma_find(struct vma *list, uint32_t addr);
void vma_add(struct vma **list, struct vma *vma);
void vma_remove(struct vma **list, struct vma *vma);
void vma_destroy_all(struct vma *list);
int vma_handle_page_fault(struct vma *vma, uint32_t fault_addr, 
                          struct page_directory *pd);

#endif /* _MM_VMA_H */
