; ===========================================================================
; boot.inc - 引导相关常量和宏定义
; ===========================================================================

; 内存布局
BOOT_SEG        equ 0x07C0      ; 引导扇区段地址
LOADER_SEG      equ 0x0800      ; Loader段地址（0x8000）
LOADER_OFFSET   equ 0x8000      ; Loader偏移地址
KERNEL_SEG      equ 0x1000      ; 内核段地址（临时）
KERNEL_OFFSET   equ 0x100000    ; 内核在保护模式下的加载地址（1MB）

; 磁盘位置
LOADER_START_SECTOR equ 1       ; Loader起始扇区（紧跟MBR）
LOADER_SECTORS      equ 4       ; Loader占用扇区数
KERNEL_START_SECTOR equ 5       ; 内核起始扇区
KERNEL_SECTORS      equ 20      ; 内核扇区数（暂定）

; GDT段选择子
CODE_SEG    equ 0x08            ; 代码段选择子
DATA_SEG    equ 0x10            ; 数据段选择子

; 屏幕常量
SCREEN_WIDTH    equ 80
SCREEN_HEIGHT   equ 25

; 颜色属性
COLOR_BLACK         equ 0x00
COLOR_BLUE          equ 0x01
COLOR_GREEN         equ 0x02
COLOR_CYAN          equ 0x03
COLOR_RED           equ 0x04
COLOR_MAGENTA       equ 0x05
COLOR_BROWN         equ 0x06
COLOR_LIGHT_GRAY    equ 0x07
COLOR_DARK_GRAY     equ 0x08
COLOR_LIGHT_BLUE    equ 0x09
COLOR_LIGHT_GREEN   equ 0x0A
COLOR_LIGHT_CYAN    equ 0x0B
COLOR_LIGHT_RED     equ 0x0C
COLOR_LIGHT_MAGENTA equ 0x0D
COLOR_YELLOW        equ 0x0E
COLOR_WHITE         equ 0x0F

; 常用颜色组合
ATTR_NORMAL     equ 0x07        ; 黑底白字
ATTR_REVERSE    equ 0x70        ; 白底黑字
ATTR_ERROR      equ 0x4F        ; 红底白字
ATTR_SUCCESS    equ 0x2F        ; 绿底白字

; 字符常量
CHAR_NULL       equ 0x00
CHAR_BACKSPACE  equ 0x08
CHAR_TAB        equ 0x09
CHAR_LF         equ 0x0A        ; Line Feed
CHAR_CR         equ 0x0D        ; Carriage Return
CHAR_SPACE      equ 0x20

; ===========================================================================
; 宏定义
; ===========================================================================

; 宏：打印字符串（实模式）
%macro PRINT_STRING 1
    mov si, %1
    call print_string
%endmacro

; 宏：打印换行
%macro PRINT_NEWLINE 0
    mov ah, 0x0E
    mov al, 0x0D
    int 0x10
    mov al, 0x0A
    int 0x10
%endmacro

; 宏：打印单个字符
%macro PRINT_CHAR 1
    mov ah, 0x0E
    mov al, %1
    int 0x10
%endmacro

; 宏：延时
%macro DELAY 1
    mov cx, %1
%%loop:
    nop
    loop %%loop
%endmacro

