; ===========================================================================
; context_switch.asm - CPU上下文切换（Linux风格实现）
; ===========================================================================
; 参考Linux内核的switch_to和ret_from_fork机制
;
; 原型：void context_switch_asm(struct cpu_context *prev_ctx,
;                                struct cpu_context *next_ctx)
; 
; struct cpu_context {
;     uint32_t edi;       // +0
;     uint32_t esi;       // +4
;     uint32_t ebp;       // +8
;     uint32_t esp;       // +12  (内核栈指针)
;     uint32_t ebx;       // +16
;     uint32_t edx;       // +20
;     uint32_t ecx;       // +24
;     uint32_t eax;       // +28
;     uint32_t eip;       // +32  (恢复点)
;     uint32_t cs;        // +36
;     uint32_t eflags;    // +40
;     uint32_t user_esp;  // +44  (用户栈指针)
;     uint32_t ss;        // +48
; };
;
; Linux风格设计：
; 1. 新进程首次运行时，eip指向ret_from_fork
; 2. ret_from_fork负责设置段寄存器并执行iret
; 3. 所有上下文切换都通过统一的路径
; ===========================================================================

[BITS 32]

global context_switch_asm

section .data
temp_cr3: dd 0              ; 临时存储CR3
debug_eip: dd 0             ; 调试：保存即将跳转的EIP
debug_esp: dd 0             ; 调试：保存即将使用的ESP
debug_cs: dd 0              ; 调试：保存CS

section .text

; ---------------------------------------------------------------------------
; context_switch_asm - 执行上下文切换（Linux风格）
; 参数：
;   [esp+4]  = prev_ctx (旧进程上下文指针，可能为NULL)
;   [esp+8]  = next_ctx (新进程上下文指针，必须非NULL)
;
; Linux风格实现：
; 1. 保存当前上下文到prev_ctx（如果非NULL）
; 2. 从next_ctx恢复寄存器
; 3. 跳转到next_ctx->eip（可能是正常返回地址或ret_from_fork）
; ---------------------------------------------------------------------------
context_switch_asm:
    ; 保存调用者的寄存器
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    
    mov eax, [ebp+8]     ; eax = prev_ctx
    mov edx, [ebp+12]    ; edx = next_ctx
    
    ; 检查next_ctx是否为NULL
    test edx, edx
    jz .error
    
    ; === 保存旧进程上下文（如果prev_ctx不为NULL） ===
    test eax, eax
    jz .load_next
    
    ; 保存通用寄存器到prev_ctx
    mov [eax+0],  edi
    mov [eax+4],  esi
    mov [eax+8],  ebp
    mov ecx, esp
    add ecx, 20          ; 调整ESP（考虑push的5个值）
    mov [eax+12], ecx    ; 保存ESP
    mov [eax+16], ebx
    mov [eax+20], edx
    mov [eax+24], ecx
    mov [eax+28], eax
    
    ; 保存返回地址作为EIP
    mov ecx, [ebp+4]     ; 获取返回地址
    mov [eax+32], ecx    ; context.eip
    
    ; 保存EFLAGS
    pushfd
    pop ecx
    mov [eax+40], ecx    ; context.eflags

.load_next:
    ; === Linux风格：先读取所有数据到栈，再切换CR3 ===
    
    ; 1. 从next_ctx读取所有需要的值到当前栈（切换CR3前）
    ;    这样即使切换CR3后next_ctx不可访问也没关系
    push dword [edx+32]     ; EIP
    push dword [edx+12]     ; ESP
    push dword [edx+8]      ; EBP
    push dword [edx+0]      ; EDI
    push dword [edx+4]      ; ESI
    push dword [edx+16]     ; EBX
    
    ; 2. 切换CR3（Linux关键步骤）
    extern g_next_cr3
    mov eax, [g_next_cr3]
    test eax, eax
    jz .skip_cr3
    
    mov cr3, eax            ; 切换页表
    
.skip_cr3:
    ; 3. 从当前栈恢复寄存器（栈在所有页表中都可访问）
    ; 栈上布局（从栈顶到栈底）：EBX, ESI, EDI, EBP, ESP, EIP
    ; 策略：先读取最底部的EIP和ESP到寄存器，然后pop其他的
    
    ; 方法：通过[esp+offset]直接读取，不改变esp
    mov eax, [esp+20]       ; eax = EIP (esp+20 = 跳过5个dword)
    mov ecx, [esp+16]       ; ecx = 新ESP (esp+16 = 跳过4个dword)
    
    ; 现在恢复其他寄存器
    pop ebx                 ; 恢复EBX
    pop esi                 ; 恢复ESI  
    pop edi                 ; 恢复EDI
    pop ebp                 ; 恢复EBP
    add esp, 4              ; 跳过已经读取的ESP值
    add esp, 4              ; 跳过已经读取的EIP值
    
    ; 切换到新进程的内核栈
    ; 注意：切换ESP后不能再使用栈！
    mov esp, ecx
    
    ; 4. 跳转到新进程的恢复点（Linux风格）
    ;    对于新创建的进程，eax=ret_from_fork
    ;    对于被切换出去的进程，eax=返回地址
    jmp eax                 ; 跳转到context.eip

.error:
    ; next_ctx为NULL，系统错误
    int 0xFF            ; 触发异常
    hlt
    jmp .error

; ---------------------------------------------------------------------------
; ret_from_fork - 新进程首次运行的入口点（Linux风格）
; 
; 这个函数在新进程首次被调度时执行
; 它负责：
; 1. 设置段寄存器为用户段
; 2. 准备iret栈帧
; 3. 执行iret切换到用户态
;
; 调用约定：
;   此函数通过jmp调用，不会返回
;   新进程的context.eip应该指向这里
; ---------------------------------------------------------------------------
global ret_from_fork
ret_from_fork:
    ; Linux风格：新进程首次运行的入口点
    ; 栈上已准备iret所需的5个值: EIP, CS, EFLAGS, ESP, SS
    
    ; 调试：直接检查栈上第一个值（应该是EIP=0x08000000）
    cmp dword [esp], 0x08000000
    je .eip_ok
    
    ; EIP不对，输出'B'并停止
    push eax
    mov al, 'B'
    mov dx, 0x3F8
    out dx, al
    pop eax
    jmp $
    
.eip_ok:
    ; EIP正确，输出'R'
    push eax
    mov al, 'R'
    mov dx, 0x3F8
    out dx, al
    pop eax
    
    ; 设置用户态数据段寄存器
    mov ax, 0x23        ; 用户数据段 (GDT[4], DPL=3)
    
    ; 调试：输出'D'表示即将设置DS
    push eax
    mov al, 'D'
    mov dx, 0x3F8
    out dx, al
    pop eax
    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; 调试：输出'I'表示即将执行iret
    push eax
    mov al, 'I'
    mov dx, 0x3F8
    out dx, al
    pop eax
    
    ; 临时测试：在iret前验证栈帧
    ; 检查栈上的5个值是否正确
    mov eax, [esp+0]    ; EIP
    cmp eax, 0x08000000
    jne .bad_stack
    
    mov eax, [esp+4]    ; CS
    cmp eax, 0x1B
    jne .bad_stack
    
    mov eax, [esp+12]   ; ESP
    cmp eax, 0x080ffffc
    jne .bad_stack
    
    mov eax, [esp+16]   ; SS
    cmp eax, 0x23
    jne .bad_stack
    
    ; 栈帧正确，输出'OK'
    push eax
    mov al, 'O'
    mov dx, 0x3F8
    out dx, al
    mov al, 'K'
    out dx, al
    pop eax
    
    ; 执行iret切换到用户态
    ; 栈上：[ESP+0]=EIP, [ESP+4]=CS, [ESP+8]=EFLAGS, [ESP+12]=ESP, [ESP+16]=SS
    
    ; 最后的调试输出
    push eax
    mov al, '>'
    mov dx, 0x3F8
    out dx, al
    pop eax
    
    ; ===== 临时测试：插入一个HLT指令到用户入口点 =====
    ; 这样可以验证是用户代码的问题还是异常处理的问题
    ; 如果系统停止且不重启，说明用户代码成功执行了HLT
    ; 如果重启或无限循环，说明iret或异常处理有问题
    ;mov eax, [esp+0]        ; 获取 EIP (0x08000000)
    ;mov byte [eax], 0xF4    ; 插入 HLT 指令（停机）
    ; =============================================================
    
    iret
    
    ; 如果能执行到这里，说明iret返回了（不应该发生）
    push eax
    mov al, '<'
    mov dx, 0x3F8
    out dx, al
    pop eax
    jmp $
    
.bad_stack:
    push eax
    mov al, '!'
    mov dx, 0x3F8
    out dx, al
    pop eax
    jmp $
    
    ; 如果iret成功，不会执行到这里
    push eax
    mov al, 'X'
    mov dx, 0x3F8
    out dx, al
    pop eax
    jmp $
    
.bad_eip:
    mov al, 'E'
    mov dx, 0x3F8
    out dx, al
    jmp $
    
.bad_cs:
    mov al, 'C'
    mov dx, 0x3F8
    out dx, al
    jmp $
    
    ; === 不会执行到这里 ===
    int 0xFF            ; 如果iret失败
    hlt
