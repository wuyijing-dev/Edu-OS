/* ===========================================================================
 * linker.ld - EduOS内核链接脚本（GRUB2兼容）
 * ===========================================================================
 * 虚拟地址 = 物理地址（无高半核映射）
 *   - 内核加载地址：0x00100000 (1MB)
 *   - 导出kernel_load_end和kernel_end供Multiboot头使用
 * ===========================================================================
 */

ENTRY(_start)

SECTIONS
{
    /* 内核从1MB开始 */
    . = 0x00100000;
    
    /* 保存内核起始地址 */
    kernel_start = .;
    kernel_physical_start = .;
    
    /* 代码段 */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text*)
    }
    
    /* 只读数据段 */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata*)
    }
    
    /* 已初始化数据段 */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data*)
    }
    
    /* 保存加载结束地址（不包括BSS） - 用于Multiboot头 */
    kernel_load_end = .;
    
    /* 未初始化数据段（BSS） */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss*)
    }
    
    /* 保存内核结束地址（包括BSS） - 用于Multiboot头 */
    kernel_end = .;
    kernel_physical_end = .;
    
    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
