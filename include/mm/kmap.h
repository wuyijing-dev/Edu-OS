/*
 * kmap.h - 临时内核映射接口
 */

#ifndef KMAP_H
#define KMAP_H

#include <stdint.h>

/*
 * 初始化kmap系统
 */
void kmap_init(void);

/*
 * 临时映射物理页到内核空间
 * 
 * @param paddr: 物理地址
 * @return: 内核虚拟地址，失败返回NULL
 */
void *kmap(uint32_t paddr);

/*
 * 解除临时映射
 * 
 * @param vaddr: kmap返回的虚拟地址
 */
void kunmap(void *vaddr);

#endif /* KMAP_H */


