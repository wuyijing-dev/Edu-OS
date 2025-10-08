; =============================================================================
; 中断服务程序(ISR)和硬件中断(IRQ)处理程序 - 汇编实现
; =============================================================================
; 本文件实现所有中断的底层汇编入口点，负责：
; 1. 保存CPU寄存器状态
; 2. 调用C语言处理函数
; 3. 恢复CPU寄存器状态
; 4. 通过IRET返回

[BITS 32]

; 导出符号
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

global irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
global irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

; 导入C语言处理函数
extern interrupt_handler

; =============================================================================
; CPU异常处理程序（ISR 0-31）
; =============================================================================

; 无错误码的异常需要手动压入虚拟错误码（保持栈帧一致）

; ISR 0: 除零异常 (#DE)
isr0:
    push 0              ; 虚拟错误码
    push 0              ; 中断号
    jmp isr_common

; ISR 1: 调试异常 (#DB)
isr1:
    push 0
    push 1
    jmp isr_common

; ISR 2: 不可屏蔽中断 (NMI)
isr2:
    push 0
    push 2
    jmp isr_common

; ISR 3: 断点异常 (#BP)
isr3:
    push 0
    push 3
    jmp isr_common

; ISR 4: 溢出异常 (#OF)
isr4:
    push 0
    push 4
    jmp isr_common

; ISR 5: 边界检查异常 (#BR)
isr5:
    push 0
    push 5
    jmp isr_common

; ISR 6: 无效操作码 (#UD)
isr6:
    push 0
    push 6
    jmp isr_common

; ISR 7: 设备不可用 (#NM)
isr7:
    push 0
    push 7
    jmp isr_common

; ISR 8: 双重故障 (#DF) - CPU自动压入错误码
isr8:
    push 8
    jmp isr_common

; ISR 9: 协处理器段越界 (Coprocessor Segment Overrun)
isr9:
    push 0
    push 9
    jmp isr_common

; ISR 10: 无效TSS (#TS) - CPU自动压入错误码
isr10:
    push 10
    jmp isr_common

; ISR 11: 段不存在 (#NP) - CPU自动压入错误码
isr11:
    push 11
    jmp isr_common

; ISR 12: 栈段故障 (#SS) - CPU自动压入错误码
isr12:
    push 12
    jmp isr_common

; ISR 13: 通用保护故障 (#GP) - CPU自动压入错误码
isr13:
    push 13
    jmp isr_common

; ISR 14: 缺页异常 (#PF) - CPU自动压入错误码
isr14:
    push 14
    jmp isr_common

; ISR 15: Intel保留
isr15:
    push 0
    push 15
    jmp isr_common

; ISR 16: 浮点异常 (#MF)
isr16:
    push 0
    push 16
    jmp isr_common

; ISR 17: 对齐检查 (#AC) - CPU自动压入错误码
isr17:
    push 17
    jmp isr_common

; ISR 18: 机器检查 (#MC)
isr18:
    push 0
    push 18
    jmp isr_common

; ISR 19: SIMD浮点异常 (#XM)
isr19:
    push 0
    push 19
    jmp isr_common

; ISR 20: 虚拟化异常 (#VE)
isr20:
    push 0
    push 20
    jmp isr_common

; ISR 21: 控制保护异常 (#CP) - CPU自动压入错误码
isr21:
    push 21
    jmp isr_common

; ISR 22-31: Intel保留
isr22:
    push 0
    push 22
    jmp isr_common

isr23:
    push 0
    push 23
    jmp isr_common

isr24:
    push 0
    push 24
    jmp isr_common

isr25:
    push 0
    push 25
    jmp isr_common

isr26:
    push 0
    push 26
    jmp isr_common

isr27:
    push 0
    push 27
    jmp isr_common

isr28:
    push 0
    push 28
    jmp isr_common

isr29:
    push 0
    push 29
    jmp isr_common

isr30:
    push 0
    push 30
    jmp isr_common

isr31:
    push 0
    push 31
    jmp isr_common

; =============================================================================
; 硬件中断处理程序（IRQ 0-15，映射到中断32-47）
; =============================================================================

; IRQ 0: 定时器（PIT）
irq0:
    push 0              ; 虚拟错误码
    push 32             ; IRQ0 映射到中断32
    jmp irq_common

; IRQ 1: 键盘
irq1:
    push 0
    push 33
    jmp irq_common

; IRQ 2: 级联到从PIC
irq2:
    push 0
    push 34
    jmp irq_common

; IRQ 3: 串口2 (COM2)
irq3:
    push 0
    push 35
    jmp irq_common

; IRQ 4: 串口1 (COM1)
irq4:
    push 0
    push 36
    jmp irq_common

; IRQ 5: 并口2/声卡
irq5:
    push 0
    push 37
    jmp irq_common

; IRQ 6: 软盘控制器
irq6:
    push 0
    push 38
    jmp irq_common

; IRQ 7: 并口1 (LPT1)
irq7:
    push 0
    push 39
    jmp irq_common

; IRQ 8: 实时时钟 (RTC)
irq8:
    push 0
    push 40
    jmp irq_common

; IRQ 9: ACPI
irq9:
    push 0
    push 41
    jmp irq_common

; IRQ 10: 可用
irq10:
    push 0
    push 42
    jmp irq_common

; IRQ 11: 可用
irq11:
    push 0
    push 43
    jmp irq_common

; IRQ 12: PS/2鼠标
irq12:
    push 0
    push 44
    jmp irq_common

; IRQ 13: 数学协处理器
irq13:
    push 0
    push 45
    jmp irq_common

; IRQ 14: 主IDE控制器
irq14:
    push 0
    push 46
    jmp irq_common

; IRQ 15: 从IDE控制器
irq15:
    push 0
    push 47
    jmp irq_common

; =============================================================================
; ISR公共处理代码
; =============================================================================
; 此时栈布局：
; [ESP+0]  : 中断号
; [ESP+4]  : 错误码（CPU自动压入或手动压入的虚拟错误码）
; [ESP+8]  : EIP (CPU自动压入)
; [ESP+12] : CS  (CPU自动压入)
; [ESP+16] : EFLAGS (CPU自动压入)
; [ESP+20] : ESP (特权级改变时，CPU自动压入)
; [ESP+24] : SS  (特权级改变时，CPU自动压入)

isr_common:
    ; 保存所有通用寄存器（使用pusha）
    ; pusha压栈顺序：EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    pusha
    
    ; 保存段寄存器
    push ds
    push es
    push fs
    push gs
    
    ; 加载内核数据段选择子
    ; 确保中断处理程序可以访问内核数据
    mov ax, 0x10        ; 内核数据段选择子（GDT第2项）
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 对齐栈到16字节边界（提高性能，符合ABI）
    mov eax, esp
    and esp, 0xFFFFFFF0
    push eax            ; 保存原始ESP
    
    ; 调用C语言中断处理函数
    ; 参数：struct interrupt_frame *frame（栈指针）
    push eax            ; 传递中断帧指针
    call interrupt_handler
    add esp, 4          ; 清理参数
    
    ; 恢复原始ESP
    pop esp
    
    ; 恢复段寄存器
    pop gs
    pop fs
    pop es
    pop ds
    
    ; 恢复通用寄存器
    popa
    
    ; 清除中断号和错误码（8字节）
    add esp, 8
    
    ; 中断返回
    ; IRET自动恢复：EIP, CS, EFLAGS
    ; 如果特权级改变，还会恢复：ESP, SS
    iret

; =============================================================================
; IRQ公共处理代码
; =============================================================================
; IRQ处理与ISR类似，但需要额外发送EOI到PIC

irq_common:
    ; 保存所有通用寄存器
    pusha
    
    ; 保存段寄存器
    push ds
    push es
    push fs
    push gs
    
    ; 加载内核数据段
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 对齐栈到16字节
    mov eax, esp
    and esp, 0xFFFFFFF0
    push eax
    
    ; 调用C语言中断处理函数
    push eax
    call interrupt_handler
    add esp, 4
    
    ; 恢复原始ESP
    pop esp
    
    ; 恢复段寄存器
    pop gs
    pop fs
    pop es
    pop ds
    
    ; 恢复通用寄存器
    popa
    
    ; 清除中断号和错误码
    add esp, 8
    
    ; 中断返回
    iret

; =============================================================================
; 性能注意事项：
; 1. 使用pusha/popa简化代码，但稍慢于手动push/pop
; 2. 栈对齐到16字节提高缓存性能
; 3. 最小化中断处理时间，复杂逻辑应在C函数中实现
; =============================================================================

