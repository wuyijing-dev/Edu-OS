; gdt_flush.asm - 加载GDT并刷新段寄存器

[BITS 32]

global gdt_flush

; void gdt_flush(uint32_t gdt_ptr_addr)
; 参数: gdt_ptr_addr - GDT指针结构的地址
gdt_flush:
    mov eax, [esp+4]     ; 获取GDT指针地址
    lgdt [eax]           ; 加载GDT
    
    ; 刷新代码段：使用远跳转
    jmp 0x08:.flush      ; 0x08 = 内核代码段选择子
    
.flush:
    ; 刷新数据段寄存器
    mov ax, 0x10         ; 0x10 = 内核数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ret
