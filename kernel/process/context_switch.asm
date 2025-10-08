; ===========================================================================
; context_switch.asm - CPU上下文切换（汇编实现）
; ===========================================================================
; 原型：void context_switch_asm(struct cpu_context *prev_ctx,
;                                struct cpu_context *next_ctx)
; 
; struct cpu_context {
;     uint32_t edi;      // +0
;     uint32_t esi;      // +4
;     uint32_t ebp;      // +8
;     uint32_t esp;      // +12
;     uint32_t ebx;      // +16
;     uint32_t edx;      // +20
;     uint32_t ecx;      // +24
;     uint32_t eax;      // +28
;     uint32_t eip;      // +32
;     uint32_t cs;       // +36
;     uint32_t eflags;   // +40
; };
; ===========================================================================

[BITS 32]

global context_switch_asm

section .data
temp_eip: dd 0              ; 临时存储EIP

section .text

; ---------------------------------------------------------------------------
; context_switch_asm - 执行上下文切换
; 参数：
;   [esp+4]  = prev_ctx (旧进程上下文指针，可能为NULL)
;   [esp+8]  = next_ctx (新进程上下文指针，必须非NULL)
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
    ; === 加载新进程上下文 ===
    ; edx指向next_ctx
    
    ; 保存EIP到临时变量
    mov eax, [edx+32]
    mov [temp_eip], eax
    
    ; 恢复EFLAGS
    mov eax, [edx+40]
    push eax
    popfd
    
    ; 恢复ESP
    mov esp, [edx+12]
    
    ; 恢复通用寄存器
    mov edi, [edx+0]
    mov esi, [edx+4]
    mov ebp, [edx+8]
    mov ebx, [edx+16]
    mov ecx, [edx+24]
    mov eax, [edx+28]
    mov edx, [edx+20]
    
    ; 跳转到新进程的EIP
    jmp [temp_eip]

.error:
    ; next_ctx为NULL，触发panic
    int 0xFF
    hlt
    jmp .error
