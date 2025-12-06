; ===========================================================================
; entry.asm - 内核入口点（GRUB2简化版）
; ===========================================================================
; 说明：
;   - 内核运行在物理地址0x100000（1MB）
;   - 虚拟地址 = 物理地址（无高半核映射）
;   - GRUB2直接加载到0x100000
; ===========================================================================

[BITS 32]

extern kernel_main
extern kernel_load_end
extern kernel_end
global _start

section .text
align 4

; ===========================================================================
; Multiboot1 Header (for GRUB2) - Using AOUT_KLUDGE
; ===========================================================================
; 必须在内核的前8KB内，且对齐到4字节
; 采用Linux的方法：使用MULTIBOOT_AOUT_KLUDGE标志明确指定加载地址
multiboot_header:
    ; Multiboot magic number
    dd 0x1BADB002                   ; Multiboot magic
    
    ; Multiboot flags
    ; Bit 0: all boot modules loaded on page boundaries
    ; Bit 1: memory size parameters valid
    ; Bit 16: load address fields valid (MULTIBOOT_AOUT_KLUDGE)
    dd 0x00010003                   ; Flags: AOUT_KLUDGE + page align + memory info
    
    ; Checksum (magic + flags + checksum = 0)
    dd -(0x1BADB002 + 0x00010003)   ; Checksum
    
    ; These fields are valid because MULTIBOOT_AOUT_KLUDGE is set
    dd multiboot_header             ; header_addr (address of this header)
    dd 0x00100000                   ; load_addr (where to load the kernel)
    dd kernel_load_end              ; load_end_addr (end of loaded sections, excluding BSS)
    dd kernel_end                   ; bss_end_addr (end of BSS)
    dd _start                       ; entry_addr (entry point)

_start:
    ; ========== 第1步：保存GRUB参数（最优先！） ==========
    ; 在修改任何寄存器之前保存GRUB传入的参数
    ; EAX = magic (0x2BADB002)
    ; EBX = multiboot info pointer
    mov ecx, eax                      ; 保存magic到ECX
    mov edx, ebx                      ; 保存MBI指针到EDX
    
    ; ========== 第2步：禁用分页 ==========
    mov eax, cr0
    and eax, ~0x80000000              ; 清除PG位（bit 31）
    mov cr0, eax
    
    ; 刷新TLB
    xor eax, eax
    mov cr3, eax
    
    ; ========== 第3步：初始化串口用于调试 ==========
    ; 初始化COM1（端口0x3F8）
    mov al, 0x80                      ; DLAB = 1
    mov dx, 0x3FB                     ; Line Control Register
    out dx, al
    
    mov al, 1                         ; 波特率分频器低字节（115200 baud）
    mov dx, 0x3F8
    out dx, al
    
    mov al, 0                         ; 波特率分频器高字节
    mov dx, 0x3F9
    out dx, al
    
    mov al, 0x03                      ; 8 bits, no parity, 1 stop bit
    mov dx, 0x3FB
    out dx, al
    
    ; ========== 第4步：通过串口输出启动信息 ==========
    ; 发送'K'
    mov al, 'K'
    mov dx, 0x3F8
    out dx, al
    
    ; 发送'K'
    mov al, 'K'
    
    ; 发送'\n'
    mov al, 0x0A
    out dx, al
    
    ; ========== 第5步：不修改段寄存器 ==========
    ; 保持GRUB2设置的段寄存器
    ; 修改段寄存器可能导致异常（如果GRUB2的GDT无效）
    ; 注释掉段寄存器设置
    
    ; ========== 第6步：设置栈 ==========
    ; 使用更高的地址避免与内核代码冲突
    mov esp, 0x00400000              ; 4MB处的栈
    mov ebp, esp
    
    ; ========== 第7步：调用kernel_main ==========
    ; 发送'C'表示即将调用kernel_main
    mov al, 'C'
    out dx, al
    
    mov al, 0x0A
    out dx, al
    
    ; 按照C调用约定，参数从右到左压栈
    push edx                          ; 推送multiboot info指针
    push ecx                          ; 推送multiboot magic
    
    ; 调用kernel_main
    call kernel_main
    
    ; ========== 第8步：如果返回，显示错误 ==========
    ; 发送'R'表示从kernel_main返回了
    mov al, 'R'
    mov dx, 0x3F8
    out dx, al
    
    mov al, 0x0A
    out dx, al
    
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
