#ifndef _MM_KMALLOC_H
#define _MM_KMALLOC_H

/*
 * 内核堆内存分配器
 * 
 * 提供动态内存分配功能（类似用户空间的malloc/free）
 */

#include <types.h>

/* 堆魔数（用于检测损坏） */
#define HEAP_MAGIC      0x12345678

/* 最小/最大分配大小 */
#define HEAP_MIN_SIZE   16
#define HEAP_MAX_SIZE   (1024 * 1024)  // 1MB

/*
 * 初始化内核堆
 * 
 * @param start: 堆起始虚拟地址
 * @param size: 初始堆大小
 */
void kmalloc_init(uint32_t start, uint32_t size);

/*
 * 分配内存
 * 
 * @param size: 要分配的字节数
 * @return: 分配的内存指针，失败返回NULL
 */
void *kmalloc(size_t size);

/*
 * 分配并清零内存
 * 
 * @param size: 要分配的字节数
 * @return: 分配的内存指针，失败返回NULL
 */
void *kzalloc(size_t size);

/*
 * 分配对齐内存
 * 
 * @param size: 要分配的字节数
 * @param align: 对齐字节数（必须是2的幂）
 * @return: 分配的内存指针，失败返回NULL
 */
void *kmalloc_aligned(size_t size, size_t align);

/*
 * 释放内存
 * 
 * @param ptr: 要释放的内存指针
 */
void kfree(void *ptr);

/*
 * 重新分配内存
 * 
 * @param ptr: 原内存指针
 * @param new_size: 新大小
 * @return: 新内存指针，失败返回NULL
 */
void *krealloc(void *ptr, size_t new_size);

/*
 * 获取已分配块的大小
 * 
 * @param ptr: 内存指针
 * @return: 块大小（字节）
 */
size_t kmalloc_get_size(void *ptr);

/*
 * 获取堆统计信息
 */
struct heap_stats {
    uint32_t total_size;        // 总堆大小
    uint32_t used_size;         // 已用大小
    uint32_t free_size;         // 空闲大小
    uint32_t num_blocks;        // 总块数
    uint32_t num_free_blocks;   // 空闲块数
};

void kmalloc_get_stats(struct heap_stats *stats);

/*
 * 打印堆统计信息
 */
void kmalloc_print_stats(void);

/*
 * 验证堆完整性
 * 
 * @return: true=正常，false=损坏
 */
bool kmalloc_verify_heap(void);

#endif /* _MM_KMALLOC_H */

