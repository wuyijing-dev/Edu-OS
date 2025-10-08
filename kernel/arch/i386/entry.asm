; ===========================================================================
; entry.asm - 内核入口点（高半核版本）
; ===========================================================================
; 说明：
;   - 内核运行在虚拟地址0xC0100000（3GB+1MB）
;   - bootloader已经设置好分页并跳转到这里
;   - 我们需要移除低端恒等映射，只保留高地址映射
; ===========================================================================

[BITS 32]

extern kernel_main
global _start

section .text

_start:
    ; 设置段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 设置内核栈（使用内核BSS段中的栈空间）
    mov esp, kernel_stack_top
    mov ebp, esp
    
    ; 清除EFLAGS
    push 0
    popfd
    
    ; 显示内核横幅
    call display_kernel_banner
    
    ; 调用C语言内核主函数
    call kernel_main
    
    ; 如果kernel_main返回，进入无限循环
halt:
    cli
    hlt
    jmp halt

; ---------------------------------------------------------------------------
; 显示内核启动横幅
; ---------------------------------------------------------------------------
display_kernel_banner:
    push eax
    push ebx
    push ecx
    push edi
    
    ; 清屏（VGA缓冲区在0xB8000，通过恒等映射可访问）
    mov edi, 0xB8000
    mov ecx, 2000
    mov ax, 0x0720          ; 灰色空格
    rep stosw
    
    ; 在第2行显示横幅
    mov edi, 0xB8000 + 160 * 2
    mov esi, banner_text
    mov ah, 0x0E            ; 黄色
    
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
    
.done:
    pop edi
    pop ecx
    pop ebx
    pop eax
    ret

; ===========================================================================
; 数据段
; ===========================================================================
section .data
banner_text:
    db '=== EduOS Kernel v0.1.0 (Higher-Half) ===', 0

; ===========================================================================
; BSS段（未初始化数据）
; ===========================================================================
section .bss
align 16
kernel_stack_bottom:
    resb 16384              ; 16KB内核栈
kernel_stack_top:
