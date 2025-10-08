; ===========================================================================
; boot_stage1.asm - Stage 1 引导扇区（MBR）
; ===========================================================================
; 功能：
;   1. 初始化环境
;   2. 显示启动信息
;   3. 从磁盘加载Stage 2到内存
;   4. 跳转到Stage 2执行
; ===========================================================================

[BITS 16]
[ORG 0x7C00]

; ==== 常量定义 ====
LOADER_SEG      equ 0x0800      ; Stage 2加载到0x8000
LOADER_OFFSET   equ 0x0000
LOADER_SECTOR   equ 1           ; Stage 2起始扇区（紧跟MBR）
LOADER_COUNT    equ 4           ; Stage 2占用4个扇区

start:
    ; 初始化段寄存器
    cli                         ; 关闭中断（修改SS时需要）
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00              ; 栈向下增长，不会覆盖代码
    sti                         ; 重新启用中断
    
    ; 保存BIOS传递的驱动器号
    mov [boot_drive], dl
    
    ; 显示启动信息
    mov si, msg_boot
    call print_string
    
    ; 显示驱动器信息
    mov si, msg_drive
    call print_string
    mov al, [boot_drive]
    call print_hex_byte
    call print_newline
    
    ; 加载Stage 2
    mov si, msg_loading
    call print_string
    
    ; 设置读取参数
    mov ah, 0x02                ; BIOS读取扇区功能
    mov al, LOADER_COUNT        ; 读取扇区数
    mov ch, 0                   ; 柱面0
    mov cl, LOADER_SECTOR + 1   ; 扇区号（从1开始，所以+1）
    mov dh, 0                   ; 磁头0
    mov dl, [boot_drive]        ; 驱动器号
    mov bx, LOADER_SEG          ; ES:BX = 目标地址
    mov es, bx
    xor bx, bx
    
    int 0x13                    ; 调用BIOS
    
    jc disk_error               ; CF=1表示错误
    
    ; 检查实际读取的扇区数
    cmp al, LOADER_COUNT
    jne disk_error
    
    ; 成功
    mov si, msg_success
    call print_string
    
    ; 跳转到Stage 2
    mov si, msg_jump
    call print_string
    
    ; 设置DL为驱动器号（Stage 2可能需要）
    mov dl, [boot_drive]
    
    ; 跳转！
    jmp LOADER_SEG:LOADER_OFFSET

; ---------------------------------------------------------------------------
; 错误处理
; ---------------------------------------------------------------------------
disk_error:
    mov si, msg_disk_error
    call print_string
    
    ; 显示错误码
    mov si, msg_error_code
    call print_string
    mov al, ah                  ; AH包含错误码
    call print_hex_byte
    call print_newline
    
    jmp hang

hang:
    mov si, msg_hang
    call print_string
    cli
    hlt
    jmp hang

; ---------------------------------------------------------------------------
; 函数：print_string
; 功能：打印以0结尾的字符串
; 参数：SI = 字符串地址
; ---------------------------------------------------------------------------
print_string:
    push ax
    push bx
    mov ah, 0x0E                ; BIOS teletype功能
    mov bh, 0                   ; 页号
    mov bl, 0x07                ; 颜色（灰色）
.loop:
    lodsb                       ; AL = [DS:SI], SI++
    test al, al                 ; 检查是否为0
    jz .done
    int 0x10                    ; 调用BIOS
    jmp .loop
.done:
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; 函数：print_newline
; 功能：打印换行符
; ---------------------------------------------------------------------------
print_newline:
    push ax
    mov ah, 0x0E
    mov al, 0x0D                ; CR
    int 0x10
    mov al, 0x0A                ; LF
    int 0x10
    pop ax
    ret

; ---------------------------------------------------------------------------
; 函数：print_hex_byte
; 功能：以十六进制打印AL寄存器的值
; 参数：AL = 要打印的字节
; ---------------------------------------------------------------------------
print_hex_byte:
    push ax
    push bx
    push cx
    
    mov cx, ax                  ; 保存原值
    
    ; 打印高4位
    shr al, 4
    call print_hex_digit
    
    ; 打印低4位
    mov ax, cx
    and al, 0x0F
    call print_hex_digit
    
    pop cx
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; 函数：print_hex_digit
; 功能：打印单个十六进制数字（0-F）
; 参数：AL = 数字（0-15）
; ---------------------------------------------------------------------------
print_hex_digit:
    push ax
    push bx
    
    cmp al, 9
    jle .digit
    add al, 7                   ; 'A'-'9'-1 = 7
.digit:
    add al, '0'
    
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    
    pop bx
    pop ax
    ret

; ===========================================================================
; 数据区
; ===========================================================================
boot_drive      db 0            ; 启动驱动器号

msg_boot        db 0x0D, 0x0A
                db '======================', 0x0D, 0x0A
                db ' EduOS Stage 1 Boot  ', 0x0D, 0x0A
                db '======================', 0x0D, 0x0A, 0
msg_drive       db 'Boot Drive: 0x', 0
msg_loading     db 'Loading Stage 2...', 0
msg_success     db ' OK', 0x0D, 0x0A, 0
msg_jump        db 'Jumping to Stage 2...', 0x0D, 0x0A, 0
msg_disk_error  db ' FAILED!', 0x0D, 0x0A, 'Disk Error!', 0x0D, 0x0A, 0
msg_error_code  db 'Error Code: 0x', 0
msg_hang        db 'System Halted.', 0x0D, 0x0A, 0

; ===========================================================================
; 填充和引导签名
; ===========================================================================
; 填充到510字节
times 510-($-$$) db 0

; MBR引导签名（必须在最后两字节）
dw 0xAA55

; ===========================================================================
; 引导扇区结束（正好512字节）
; ===========================================================================
