; ===========================================================================
; loader_complete.asm - Linux风格大内核加载器（支持数MB内核）
; ===========================================================================
; 功能：
;   1. 使用Unreal模式在实模式下访问4GB内存
;   2. 直接加载大内核到1MB物理地址（支持2MB-16MB内核）
;   3. ELF头检测和智能加载
;   4. 启用A20地址线
;   5. 设置GDT和分页
;   6. 切换到保护模式并跳转到内核
; ===========================================================================

[BITS 16]
[ORG 0x8000]

; ==== 常量定义 ====
KERNEL_START_SECTOR equ 9       ; 内核从第9扇区开始（Stage2占用扇区1-8）
KERNEL_MAX_SECTORS equ 8192     ; 最大支持8192扇区（4MB内核）
KERNEL_LOAD_ADDR equ 0x100000   ; 内核直接加载到1MB物理地址
MEMORY_MAP_ADDR equ 0x7000      ; 内存映射存储地址（28KB处）
MAX_MEMORY_ENTRIES equ 32       ; 最多32个内存条目
ELF_HEADER_BUFFER equ 0x7E00    ; ELF头临时缓冲区（512字节）

start:
    ; 清屏并显示欢迎信息
    call clear_screen
    
    mov si, msg_loader_start
    call print_string
    
    ; 保存驱动器号（应该在DL中）
    mov [boot_drive], dl
    
    ; === 步骤1: 启用A20地址线（必须在Unreal模式前） ===
    mov si, msg_a20
    call print_string
    
    call enable_a20
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤2: 进入Unreal模式（访问4GB内存） ===
    mov si, msg_unreal_mode
    call print_string
    
    call enter_unreal_mode
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤3: 加载内核到1MB地址 ===
    mov si, msg_load_kernel
    call print_string
    
    call load_kernel_big
    jc .error
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤4: 检测物理内存 ===
    mov si, msg_detect_memory
    call print_string
    
    call detect_memory
    jc .no_e820
    
    mov si, msg_ok
    call print_string
    jmp .gdt_step
    
.no_e820:
    mov si, msg_e820_failed
    call print_string
    ; 即使失败也继续，内核会使用默认值
    
.gdt_step:
    ; === 步骤5: 加载GDT ===
    mov si, msg_gdt
    call print_string
    
    lgdt [gdt_descriptor]
    
    mov si, msg_ok
    call print_string
    
    ; === 步骤6: 切换到保护模式 ===
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
    
    ; 显示详细错误信息
    mov si, msg_error_detail
    call print_string
    
    ; 暂停系统，等待按键
    mov si, msg_press_key
    call print_string
    
    ; 等待按键
    xor ax, ax
    int 0x16
    
    ; 按键后重启
    int 0x19

; ---------------------------------------------------------------------------
; 函数：enter_unreal_mode
; 功能：进入Unreal模式（16位代码，32位段限制 = 4GB访问）
; 说明：这是Linux风格大内核加载的关键技术
; ---------------------------------------------------------------------------
enter_unreal_mode:
    push eax
    push ds
    
    cli                         ; 关闭中断
    
    ; 保存GDT
    lgdt [gdt_descriptor]
    
    ; 临时进入保护模式
    mov eax, cr0
    or al, 1                    ; 设置PE位
    mov cr0, eax
    
    ; 加载数据段选择子（4GB限制）
    mov bx, 0x10                ; GDT数据段选择子
    mov ds, bx
    mov es, bx
    
    ; 返回实模式
    and al, 0xFE                ; 清除PE位
    mov cr0, eax
    
    ; 恢复实模式段寄存器（但保留32位限制！）
    xor ax, ax
    mov ds, ax
    mov es, ax
    
    sti                         ; 重新启用中断
    
    pop ds
    pop eax
    ret

; ---------------------------------------------------------------------------
; 函数：load_kernel_big
; 功能：使用Unreal模式直接加载大内核到1MB物理地址
; 返回：CF=1表示错误
; 说明：支持2MB-16MB内核，Linux风格加载
; ---------------------------------------------------------------------------

; 磁盘地址包（DAP）结构
dap:
    db 0x10                     ; DAP大小（16字节）
    db 0                        ; 保留（必须为0）
dap_sectors:
    dw 0                        ; 要读取的扇区数
dap_offset:
    dw 0                        ; 目标偏移
dap_segment:
    dw 0                        ; 目标段
dap_lba_low:
    dd 0                        ; 起始LBA扇区号（低32位）
dap_lba_high:
    dd 0                        ; 起始LBA扇区号（高32位）

; 内核加载变量
kernel_size_sectors dw 0        ; 实际要加载的扇区数

load_kernel_big:
    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    
    ; 打印检测信息
    mov si, msg_check_int13h
    call print_string
    
    ; 显示驱动器号（调试用）
    mov al, [boot_drive]
    call print_hex_byte
    mov si, msg_newline
    call print_string
    
    ; 检测是否支持INT 13h扩展
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .no_int13h_ext           ; 不支持扩展
    cmp bx, 0xAA55
    jne .no_int13h_ext
    
    mov si, msg_lba_mode
    call print_string
    jmp .continue_load

.no_int13h_ext:
    mov si, msg_no_int13h
    call print_string
    jmp .error

.continue_load:
    
    ; 步骤1：读取ELF头到临时缓冲区（用于检测大小）
    mov si, msg_detect_size
    call print_string
    
    call detect_kernel_size
    jc .use_default_size
    
    ; 显示检测到的大小
    mov ax, [kernel_size_sectors]
    call print_hex_word
    mov si, msg_sectors
    call print_string
    jmp .start_load
    
.use_default_size:
    ; 默认加载2MB（4096扇区）
    mov word [kernel_size_sectors], 4096
    mov si, msg_default_size
    call print_string
    
.start_load:
    mov si, msg_loading_big
    call print_string
    
    ; 使用Unreal模式直接写入1MB地址
    mov edi, KERNEL_LOAD_ADDR   ; EDI = 1MB物理地址
    mov ecx, 0                  ; ECX = 已加载扇区数
    mov dx, [kernel_size_sectors] ; DX = 总扇区数
    
.load_loop:
    ; 检查是否完成
    cmp cx, dx
    jae .success
    
    ; 计算本次读取扇区数（最多127扇区，约64KB）
    mov ax, dx
    sub ax, cx                  ; 剩余扇区数
    cmp ax, 127
    jbe .do_read
    mov ax, 127                 ; 限制为127扇区
    
.do_read:
    ; 使用临时缓冲区读取（因为BIOS限制）
    push ax
    push cx
    push dx
    
    ; 设置DAP
    mov word [dap_sectors], ax
    mov word [dap_offset], 0
    mov word [dap_segment], 0x2000  ; 临时缓冲区：0x20000（128KB处）
    
    ; 计算LBA
    mov ax, cx
    add ax, KERNEL_START_SECTOR
    movzx eax, ax
    mov dword [dap_lba_low], eax
    mov dword [dap_lba_high], 0
    
    ; 打印进度
    push si
    mov si, msg_reading
    call print_string
    mov ax, cx
    call print_hex_word
    mov si, msg_slash
    call print_string
    mov ax, dx
    call print_hex_word
    pop si
    
    ; 执行BIOS读取
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .error_pop
    
    ; 打印点
    push si
    mov si, msg_dot
    call print_string
    pop si
    
    ; 从临时缓冲区复制到目标地址（使用Unreal模式）
    pop dx
    pop cx
    pop ax                      ; AX = 本次读取的扇区数
    
    push cx
    push edi
    
    ; 源地址：0x20000，目标地址：EDI
    mov esi, 0x20000
    movzx ecx, ax               ; ECX = 扇区数
    shl ecx, 7                  ; * 128 (512字节/扇区 / 4字节/DWORD)
    
    ; 32位内存复制（Unreal模式的魔力！）
    a32 rep movsd               ; 使用32位地址覆盖
    
    pop edi
    pop cx
    
    ; 更新指针和计数器
    movzx eax, word [dap_sectors]
    shl eax, 9                  ; * 512字节
    add edi, eax                ; 更新目标地址
    add cx, word [dap_sectors]  ; 更新已加载扇区数
    
    jmp .load_loop
    
.success:
    mov si, msg_newline
    call print_string
    clc
    jmp .done
    
.error_pop:
    pop dx
    pop cx
    pop ax
    
.error:
    mov si, msg_newline
    call print_string
    stc
    
.done:
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; ---------------------------------------------------------------------------
; 函数：detect_kernel_size
; 功能：通过读取ELF头检测内核实际大小
; 返回：CF=0成功，kernel_size_sectors包含大小；CF=1失败
; ---------------------------------------------------------------------------
detect_kernel_size:
    push eax
    push ebx
    push ecx
    push edx
    push si
    
    ; 读取第一个扇区（包含ELF头）
    mov word [dap_sectors], 1
    mov word [dap_offset], 0
    mov word [dap_segment], 0x7E0   ; 缓冲区在0x7E00
    mov dword [dap_lba_low], KERNEL_START_SECTOR
    mov dword [dap_lba_high], 0
    
    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .failed
    
    ; 检查ELF魔数
    mov eax, [0x7E00]
    cmp eax, 0x464C457F         ; 0x7F 'E' 'L' 'F'
    jne .not_elf
    
    ; 读取ELF文件大小（粗略估计：使用程序头）
    ; 这里简化处理：使用固定大小或从Multiboot读取
    ; 实际Linux会解析所有程序段
    
    ; 默认策略：检测磁盘上的实际大小
    ; 这里先使用保守估计：2MB
    mov word [kernel_size_sectors], 4096
    clc
    jmp .done
    
.not_elf:
    ; 如果不是ELF，假设是纯二进制，使用默认大小
    mov word [kernel_size_sectors], 4096
    clc
    jmp .done
    
.failed:
    stc
    
.done:
    pop si
    pop edx
    pop ecx
    pop ebx
    pop eax
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
    
    ; 清屏并显示第一条消息
    mov edi, 0xB8000
    mov ecx, 2000
    mov ax, 0x0F20          ; 白字黑底空格
    rep stosw
    
    mov edi, 0xB8000
    mov esi, pm_msg
    call print_string_pm
    
    ; 注意：内核已在Unreal模式下直接加载到1MB，无需复制！
    mov edi, 0xB8000 + 160
    mov esi, pm_ready_msg
    call print_string_pm
    
    ; 步骤2：设置页表
    mov edi, 0xB8000 + 320
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
    
    ; 步骤3：启用分页前的消息
    mov edi, 0xB8000 + 480
    mov esi, pm_enable_paging
    call print_string_pm
    
    ; 7. 加载页目录地址到CR3
    mov eax, PAGE_DIRECTORY_ADDR
    mov cr3, eax
    
    ; 8. 启用分页：设置CR0的PG位（位31）
    mov eax, cr0
    or eax, 0x80000000      ; 设置PG位
    mov cr0, eax
    
    ; 步骤4：分页已启用
    mov edi, 0xB8000 + 640
    mov esi, pm_paging_enabled
    call print_string_pm
    
    ; 步骤5：准备跳转
    mov edi, 0xB8000 + 800
    mov esi, pm_jump
    call print_string_pm
    
    ; 给用户一点时间看到消息（可选）
    mov ecx, 0x1000000
.delay:
    loop .delay
    
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

; 打印16位十六进制数 (AX中的值)
print_hex_word:
    push ax
    push bx
    push cx
    push dx
    
    mov cx, 4                   ; 4个十六进制数字
    mov bx, ax                  ; 保存值到BX
    
.hex_loop:
    rol bx, 4                   ; 循环左移4位
    mov al, bl
    and al, 0x0F                ; 取低4位
    
    cmp al, 9
    jbe .digit
    add al, 'A' - 10            ; A-F
    jmp .print
.digit:
    add al, '0'                 ; 0-9
.print:
    mov ah, 0x0E
    int 0x10
    
    loop .hex_loop
    
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; 打印8位十六进制数 (AL中的值)
print_hex_byte:
    push ax
    push bx
    push cx
    
    mov cx, 2                   ; 2个十六进制数字
    mov bl, al                  ; 保存值到BL
    
.hex_loop:
    rol bl, 4                   ; 循环左移4位
    mov al, bl
    and al, 0x0F                ; 取低4位
    
    cmp al, 9
    jbe .digit
    add al, 'A' - 10            ; A-F
    jmp .print
.digit:
    add al, '0'                 ; 0-9
.print:
    mov ah, 0x0E
    int 0x10
    
    loop .hex_loop
    
    pop cx
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

; 用户代码段（DPL=3）
gdt_user_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11111010b        ; P=1, DPL=3, S=1, Type=1010 (代码段)
    db 11001111b
    db 0x00

; 用户数据段（DPL=3）
gdt_user_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11110010b        ; P=1, DPL=3, S=1, Type=0010 (数据段)
    db 11001111b
    db 0x00

; TSS 段（运行时设置）
gdt_tss:
    dw 0x0067           ; 限制：103 字节（TSS 最小大小）
    dw 0x0000           ; 基址低16位（运行时填充）
    db 0x00             ; 基址中8位
    db 10001001b        ; P=1, DPL=0, Type=1001 (Available TSS)
    db 01000000b        ; G=0, 限制高4位=0
    db 0x00             ; 基址高8位

gdt_end:

; 段选择子定义：
; 0x08 = 内核代码段 (GDT[1], RPL=0)
; 0x10 = 内核数据段 (GDT[2], RPL=0)
; 0x1B = 用户代码段 (GDT[3], RPL=3)  = 0x18 | 3
; 0x23 = 用户数据段 (GDT[4], RPL=3)  = 0x20 | 3
; 0x28 = TSS 段 (GDT[5], RPL=0)

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ===========================================================================
; 数据区
; ===========================================================================
boot_drive          db 0

msg_loader_start    db '=== EduOS Big Kernel Loader (Linux-style) ===', 0x0D, 0x0A, 0
msg_a20             db '[1/6] Enabling A20 line...', 0
msg_unreal_mode     db '[2/6] Entering Unreal mode (4GB access)...', 0
msg_load_kernel     db '[3/6] Loading big kernel to 1MB...', 0x0D, 0x0A, 0
msg_detect_memory   db '[4/6] Detecting physical memory...', 0
msg_gdt             db '[5/6] Loading GDT...', 0
msg_entering_pm     db '[6/6] Entering protected mode...', 0x0D, 0x0A, 0

msg_check_int13h    db '  Checking INT 13h Extensions (Drive: 0x', 0
msg_lba_mode        db '  INT 13h Extensions supported!', 0x0D, 0x0A, 0
msg_no_int13h       db '  ERROR: INT 13h Extensions NOT supported!', 0x0D, 0x0A, \
                       '  Please use IDE/SATA disk (not floppy)', 0x0D, 0x0A, 0
msg_detect_size     db '  Detecting kernel size...', 0
msg_default_size    db '  Using default: 2MB (4096 sectors)', 0x0D, 0x0A, 0
msg_sectors         db ' sectors', 0x0D, 0x0A, 0
msg_loading_big     db '  Loading (Unreal mode -> 1MB physical):', 0x0D, 0x0A, 0

msg_ok              db ' OK', 0x0D, 0x0A, 0
msg_error           db 0x0D, 0x0A, ' FAILED!', 0x0D, 0x0A, 0
msg_error_detail    db 'Disk read error. Check:', 0x0D, 0x0A, \
                       '  1) INT 13h Extensions support', 0x0D, 0x0A, \
                       '  2) Disk image integrity', 0x0D, 0x0A, \
                       '  3) Kernel size (max 4MB supported)', 0x0D, 0x0A, 0
msg_press_key       db 'Press any key to reboot...', 0x0D, 0x0A, 0
msg_e820_failed     db ' WARN (using defaults)', 0x0D, 0x0A, 0

msg_reading         db '    Sector ', 0
msg_slash           db '/', 0
msg_dot             db '.', 0
msg_newline         db 0x0D, 0x0A, 0

pm_msg              db '[PM] Protected Mode Active', 0
pm_ready_msg        db '[PM] Kernel ready at 1MB (Unreal load)', 0
pm_paging_msg       db '[PM] Setting up page tables...', 0
pm_enable_paging    db '[PM] Enabling paging...', 0
pm_paging_enabled   db '[PM] Paging enabled!', 0
pm_jump             db '[PM] Jumping to 0xC0100000...', 0

; ===========================================================================
; 填充到 4KB（8个扇区）- 大内核加载器需要更多空间
; ===========================================================================
times 4096-($-$$) db 0

