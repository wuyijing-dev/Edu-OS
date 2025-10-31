/* 
 * user.ld - Linux风格的用户程序链接脚本
 * 
 * 参考Linux内核的用户程序链接脚本
 */

OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
    /* 代码段从0x08000000开始（Linux标准用户空间起始地址） */
    . = 0x08000000;
    
    /* 代码段 */
    .text : {
        *(.text._start)     /* _start必须在最前面 */
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    }
    
    /* 只读数据段（页对齐） */
    . = ALIGN(4096);
    .rodata : {
        *(.rodata .rodata.*)
        *(.gnu.linkonce.r.*)
    }
    
    /* 数据段（页对齐） */
    . = ALIGN(4096);
    .data : {
        *(.data .data.*)
        *(.gnu.linkonce.d.*)
    }
    
    /* GOT和PLT（即使是静态链接也需要） */
    .got : {
        *(.got)
    }
    .got.plt : {
        *(.got.plt)
    }
    
    /* BSS段（页对齐） */
    . = ALIGN(4096);
    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        __bss_end = .;
    }
    
    /* 调试信息（不加载到内存） */
    .debug_info     0 : { *(.debug_info) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_str      0 : { *(.debug_str) }
    
    /* 丢弃不需要的段 */
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu.lto_*)
        *(.comment)
    }
}
