; ===========================================================================
; loader_complete.asm - 完整的Stage 2引导加载器（带内核加载）
; ===========================================================================
; 功能：
;   1. 加载内核到内存（在实模式下使用BIOS）
;   2. 启用A20地址线
;   3. 设置GDT
;   4. 切换到保护模式  
;   5. 跳转到内核执行
; ===========================================================================

[BITS 16]
[ORG 0x8000]

; ==== 常量定义 ====
KERNEL_SEGMENT equ 0x1000       ; 内核临时加载到0x10000（64KB处）
KERNEL_OFFSET equ 0x0000
KERNEL_START_SECTOR equ 5       ; 内核从第5扇区开始
KERNEL_SECTORS equ 100          ; 加载100个扇区（50KB，足够容纳内核）
MEMORY_MAP_ADDR equ 0x7000      ; 内存映射存储地址（28KB处）
MAX_MEMORY_ENTRIES equ 32       ; 最多32个内存条目

start:
    ; 清屏并显示欢迎信息
    call clear_screen
    
    mov si, msg_loader_start
    call print_string
    
    ; 保存驱动器号（应该在DL中）
    mov [boot_drive], dl
    
    ; === 步骤1: 加载内核到内存 ===
    mov si, msg_load_kernel
    call print_string
    
    call load_kernel
    jc .error
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤2: 检测物理内存 ===
    mov si, msg_detect_memory
    call print_string
    
    call detect_memory
    jc .no_e820
    
    mov si, msg_ok
    call print_string
    jmp .a20_step
    
.no_e820:
    mov si, msg_e820_failed
    call print_string
    ; 即使失败也继续，内核会使用默认值
    
.a20_step:
    ; === 步骤3: 启用A20地址线 ===
    mov si, msg_a20
    call print_string
    
    call enable_a20
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤4: 加载GDT ===
    mov si, msg_gdt
    call print_string
    
    lgdt [gdt_descriptor]
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤5: 切换到保护模式 ===
    mov si, msg_entering_pm
    call print_string
    
    cli                         ; 关闭中断
    
    ; 设置CR0的PE位
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    
    ; 远跳转刷新流水线
    jmp 0x08:protected_mode_start

.error:
    mov si, msg_error
    call print_string
    jmp $

; ---------------------------------------------------------------------------
; 函数：load_kernel
; 功能：从磁盘加载内核到内存
; 返回：CF=1表示错误
; ---------------------------------------------------------------------------
load_kernel:
    push ax
    push bx
    push cx
    push dx
    push es
    
    ; 设置目标地址 ES:BX
    mov ax, KERNEL_SEGMENT
    mov es, ax
    xor bx, bx                  ; ES:BX = 0x1000:0x0000 = 0x10000
    
    ; 读取参数
    mov ah, 0x02                ; BIOS读取扇区
    mov al, KERNEL_SECTORS      ; 扇区数
    mov ch, 0                   ; 柱面0
    mov cl, KERNEL_START_SECTOR + 1  ; 扇区号（从1开始）
    mov dh, 0                   ; 磁头0
    mov dl, [boot_drive]        ; 驱动器号
    
    int 0x13
    
    jc .done                    ; CF=1表示错误
    
    ; 检查实际读取的扇区数
    cmp al, KERNEL_SECTORS
    jne .error
    
    clc                         ; 清除CF（成功）
    jmp .done
    
.error:
    stc                         ; 设置CF（失败）
    
.done:
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; 函数：enable_a20
; 功能：启用A20地址线
; ---------------------------------------------------------------------------
enable_a20:
    push ax
    
    ; 方法1：通过BIOS
    mov ax, 0x2401
    int 0x15
    jnc .done
    
    ; 方法2：通过键盘控制器
    call wait_8042
    mov al, 0xAD
    out 0x64, al
    
    call wait_8042
    mov al, 0xD0
    out 0x64, al
    
    call wait_8042_out
    in al, 0x60
    push ax
    
    call wait_8042
    mov al, 0xD1
    out 0x64, al
    
    call wait_8042
    pop ax
    or al, 0x02
    out 0x60, al
    
    call wait_8042
    mov al, 0xAE
    out 0x64, al
    
    call wait_8042
    
.done:
    pop ax
    ret

wait_8042:
    in al, 0x64
    test al, 0x02
    jnz wait_8042
    ret

wait_8042_out:
    in al, 0x64
    test al, 0x01
    jz wait_8042_out
    ret

; ---------------------------------------------------------------------------
; 保护模式代码
; ---------------------------------------------------------------------------
[BITS 32]

; 页表常量
PAGE_DIRECTORY_ADDR equ 0x9000      ; 页目录地址（在36KB处）
PAGE_TABLE_0_ADDR   equ 0xA000      ; 页表0地址（在40KB处，映射0-4MB）
PAGE_TABLE_768_ADDR equ 0xB000      ; 页表768地址（在44KB处，映射3GB-3GB+4MB）

protected_mode_start:
    ; 设置段寄存器
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; 设置栈
    mov esp, 0x90000
    
    ; 清屏
    mov edi, 0xB8000
    mov ecx, 2000
    mov ax, 0x0F20          ; 白字黑底空格
    rep stosw
    
    ; 显示消息
    mov edi, 0xB8000
    mov esi, pm_msg
    call print_string_pm
    
    ; 复制内核到1MB处
    ; 源地址：0x10000，目标地址：0x100000
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, KERNEL_SECTORS * 512 / 4  ; 以双字为单位
    rep movsd
    
    ; === 设置页表以支持高半核 ===
    mov edi, 0xB8000 + 160
    mov esi, pm_paging_msg
    call print_string_pm
    
    ; 1. 清空页目录（1024个条目）
    mov edi, PAGE_DIRECTORY_ADDR
    mov ecx, 1024
    xor eax, eax
    rep stosd
    
    ; 2. 清空页表0（1024个条目，映射0-4MB）
    mov edi, PAGE_TABLE_0_ADDR
    mov ecx, 1024
    xor eax, eax
    rep stosd
    
    ; 3. 清空页表768（1024个条目，映射3GB-3GB+4MB）
    mov edi, PAGE_TABLE_768_ADDR
    mov ecx, 1024
    xor eax, eax
    rep stosd
    
    ; 4. 填充页表0：恒等映射0-4MB（物理地址 = 虚拟地址）
    ;    用于bootloader到内核的过渡阶段
    mov edi, PAGE_TABLE_0_ADDR
    mov eax, 0x00000003     ; 物理地址0x0，标志位：Present(1) + Writable(1)
    mov ecx, 1024           ; 1024个页面 = 4MB
.fill_pt0:
    stosd
    add eax, 0x1000         ; 下一个4KB页面
    loop .fill_pt0
    
    ; 5. 填充页表768：映射3GB-3GB+4MB到物理地址0-4MB
    ;    这样内核虚拟地址0xC0000000会映射到物理地址0x00000000
    mov edi, PAGE_TABLE_768_ADDR
    mov eax, 0x00000003     ; 物理地址0x0，标志位：Present(1) + Writable(1)
    mov ecx, 1024           ; 1024个页面 = 4MB
.fill_pt768:
    stosd
    add eax, 0x1000         ; 下一个4KB页面
    loop .fill_pt768
    
    ; 6. 设置页目录条目
    ;    页目录条目0：指向页表0（恒等映射）
    mov eax, PAGE_TABLE_0_ADDR
    or eax, 0x00000003      ; Present + Writable
    mov [PAGE_DIRECTORY_ADDR], eax
    
    ;    页目录条目768（索引768 = 3GB / 4MB）：指向页表768
    mov eax, PAGE_TABLE_768_ADDR
    or eax, 0x00000003      ; Present + Writable
    mov [PAGE_DIRECTORY_ADDR + 768 * 4], eax
    
    ; 7. 加载页目录地址到CR3
    mov eax, PAGE_DIRECTORY_ADDR
    mov cr3, eax
    
    ; 8. 启用分页：设置CR0的PG位（位31）
    mov eax, cr0
    or eax, 0x80000000      ; 设置PG位
    mov cr0, eax
    
    ; 现在分页已启用！
    ; 我们仍然在低地址执行（0x8000附近），因为恒等映射
    
    ; 显示跳转消息
    mov edi, 0xB8000 + 320
    mov esi, pm_jump
    call print_string_pm
    
    ; 跳转到内核的虚拟地址！
    ; 内核物理地址：0x100000，虚拟地址：0xC0100000
    jmp 0x08:0xC0100000

; 保护模式打印函数
print_string_pm:
    mov ah, 0x0F
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    ret

; ---------------------------------------------------------------------------
; 实模式辅助函数
; ---------------------------------------------------------------------------
[BITS 16]

; ===========================================================================
; detect_memory - 使用BIOS INT 0x15, EAX=0xE820检测物理内存
; ===========================================================================
; 输出：
;   CF=0 成功，CF=1 失败
;   内存映射存储在MEMORY_MAP_ADDR
; ===========================================================================
detect_memory:
    push es
    push di
    push bp
    
    ; 设置目标缓冲区
    mov ax, 0
    mov es, ax
    mov di, MEMORY_MAP_ADDR + 2    ; 前2字节保留给计数
    
    xor ebx, ebx                    ; EBX=0表示首次调用
    xor bp, bp                      ; BP用作条目计数器
    mov edx, 0x534D4150             ; 'SMAP'签名
    
.loop:
    mov eax, 0xE820                 ; 功能号
    mov ecx, 24                     ; 缓冲区大小
    int 0x15                        ; 调用BIOS
    
    jc .failed                      ; CF=1表示失败
    
    cmp eax, 0x534D4150             ; 验证签名
    jne .failed
    
    ; 成功获取一个条目
    add di, 24                      ; 移动到下一个条目
    inc bp                          ; 计数器+1
    
    test ebx, ebx                   ; EBX=0表示结束
    jz .done
    
    cmp bp, MAX_MEMORY_ENTRIES
    jge .done                       ; 达到最大条目数
    
    jmp .loop
    
.done:
    ; 保存条目数量到开头
    mov word [MEMORY_MAP_ADDR], bp
    clc                             ; 清除CF表示成功
    jmp .exit
    
.failed:
    ; 失败时至少设置一个默认条目
    mov word [MEMORY_MAP_ADDR], 1
    
    ; 创建默认条目（假设128MB内存）
    mov di, MEMORY_MAP_ADDR + 2
    
    ; Base = 0x100000 (1MB)
    mov dword [es:di], 0x00100000
    mov dword [es:di+4], 0
    
    ; Length = 127MB
    mov dword [es:di+8], 0x07F00000
    mov dword [es:di+12], 0
    
    ; Type = 1 (Available)
    mov dword [es:di+16], 1
    
    ; ACPI = 0
    mov dword [es:di+20], 0
    
    stc                             ; 设置CF表示失败（但有默认值）
    
.exit:
    pop bp
    pop di
    pop es
    ret

clear_screen:
    push ax
    push bx
    push cx
    push dx
    
    mov ah, 0x06
    mov al, 0
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184F
    int 0x10
    
    mov ah, 0x02
    xor bh, bh
    xor dx, dx
    int 0x10
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

print_string:
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; ===========================================================================
; GDT定义
; ===========================================================================
align 8
gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ===========================================================================
; 数据区
; ===========================================================================
boot_drive          db 0

msg_loader_start    db '=== EduOS Stage 2 Loader ===', 0x0D, 0x0A, 0
msg_load_kernel     db '[1/5] Loading kernel from disk...', 0
msg_detect_memory   db '[2/5] Detecting physical memory...', 0
msg_a20             db '[3/5] Enabling A20 line...', 0
msg_gdt             db '[4/5] Loading GDT...', 0
msg_entering_pm     db '[5/5] Entering protected mode...', 0x0D, 0x0A, 0
msg_ok              db ' OK', 0x0D, 0x0A, 0
msg_error           db ' FAILED!', 0x0D, 0x0A, 'System Halted.', 0
msg_e820_failed     db ' WARN (using defaults)', 0x0D, 0x0A, 0

pm_msg              db '32-bit Protected Mode Active', 0
pm_paging_msg       db 'Setting up paging (Higher-Half Kernel)...', 0
pm_jump             db 'Jumping to kernel at 0xC0100000...', 0

; ===========================================================================
; 填充
; ===========================================================================
times 2048-($-$$) db 0

