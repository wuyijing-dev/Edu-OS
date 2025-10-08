/* ===========================================================================
 * linker.ld - EduOS内核链接脚本（高半核架构）
 * ===========================================================================
 * 虚拟地址空间布局：
 *   - 用户空间：0x00000000 - 0xBFFFFFFF (0-3GB)
 *   - 内核空间：0xC0000000 - 0xFFFFFFFF (3GB-4GB)
 * 
 * 内核物理地址：0x00100000 (1MB)
 * 内核虚拟地址：0xC0100000 (3GB + 1MB)
 * ===========================================================================
 */

ENTRY(_start)

/* 定义内核虚拟基址和物理基址 */
KERNEL_VIRTUAL_BASE = 0xC0000000;
KERNEL_PHYSICAL_BASE = 0x00100000;

SECTIONS
{
    /* 内核虚拟地址从3GB+1MB开始，物理地址在1MB */
    . = KERNEL_VIRTUAL_BASE + KERNEL_PHYSICAL_BASE;
    
    /* 保存内核起始地址（虚拟地址） */
    kernel_virtual_start = .;
    kernel_physical_start = . - KERNEL_VIRTUAL_BASE;
    
    /* 代码段 */
    .text : AT(ADDR(.text) - KERNEL_VIRTUAL_BASE) ALIGN(4K)
    {
        *(.multiboot)
        *(.text)
        *(.text*)
    }
    
    /* 只读数据段 */
    .rodata : AT(ADDR(.rodata) - KERNEL_VIRTUAL_BASE) ALIGN(4K)
    {
        *(.rodata)
        *(.rodata*)
    }
    
    /* 已初始化数据段 */
    .data : AT(ADDR(.data) - KERNEL_VIRTUAL_BASE) ALIGN(4K)
    {
        *(.data)
        *(.data*)
    }
    
    /* 未初始化数据段（BSS） */
    .bss : AT(ADDR(.bss) - KERNEL_VIRTUAL_BASE) ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss*)
    }
    
    /* 保存内核结束地址（虚拟地址） */
    kernel_virtual_end = .;
    kernel_physical_end = . - KERNEL_VIRTUAL_BASE;
    
    /* 为了兼容性，保留旧的符号名 */
    kernel_start = kernel_physical_start;
    kernel_end = kernel_physical_end;
    
    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
