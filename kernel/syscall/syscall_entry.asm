; ===========================================================================
; syscall_entry.asm - 系统调用汇编入口（INT 0x80）
; ===========================================================================

[BITS 32]

[GLOBAL syscall_handler]
[EXTERN syscall_dispatch]

; 系统调用处理程序
syscall_handler:
    ; 保存所有通用寄存器
    pushad
    
    ; 保存段寄存器
    push ds
    push es
    push fs
    push gs
    
    ; 切换到内核数据段
    mov ax, 0x10        ; 内核数据段选择子
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 调用 C 分发器（传递栈帧指针）
    push esp            ; 传递 struct syscall_frame 指针
    call syscall_dispatch
    add esp, 4          ; 清理参数
    
    ; 恢复段寄存器
    pop gs
    pop fs
    pop es
    pop ds
    
    ; 恢复通用寄存器（eax 包含返回值，会被 popad 恢复）
    popad
    
    ; 返回用户态
    iret

