#ifndef _ARCH_I386_GDT_H
#define _ARCH_I386_GDT_H

#include <stdint.h>

/* GDT段选择子定义 */
#define GDT_KERNEL_CODE  0x08  /* 内核代码段选择子 */
#define GDT_KERNEL_DATA  0x10  /* 内核数据段选择子 */
#define GDT_USER_CODE    0x18  /* 用户代码段基址 (加上RPL=3后为0x1B) */
#define GDT_USER_DATA    0x20  /* 用户数据段基址 (加上RPL=3后为0x23) */
#define GDT_TSS          0x28  /* TSS段选择子 */

/* 段描述符结构 */
struct gdt_entry {
    uint16_t limit_low;      /* 段界限 0-15位 */
    uint16_t base_low;       /* 段基址 0-15位 */
    uint8_t  base_middle;    /* 段基址 16-23位 */
    uint8_t  access;         /* 访问权限字节 */
    uint8_t  granularity;    /* 粒度字节 */
    uint8_t  base_high;      /* 段基址 24-31位 */
} __attribute__((packed));

/* GDT指针结构 */
struct gdt_ptr {
    uint16_t limit;          /* GDT大小-1 */
    uint32_t base;           /* GDT基址 */
} __attribute__((packed));

/* GDT初始化 */
void gdt_init(void);

/* 设置GDT表项 */
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

/* 更新TSS段描述符 */
void gdt_update_tss(uint32_t tss_base, uint32_t tss_limit);

#endif /* _ARCH_I386_GDT_H */
