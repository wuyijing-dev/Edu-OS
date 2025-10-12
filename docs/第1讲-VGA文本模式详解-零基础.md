# 第1讲：VGA 文本模式详解 - 从零开始

> 献给从未接触过硬件编程的你

## 🎯 课程目标

学完本课，你将理解：

1. **什么是显示器和显存？**
2. **如何在屏幕上显示字符？**
3. **VGA 文本模式的工作原理**
4. **如何用 C 语言控制屏幕**
5. **颜色、光标、滚动的实现**

**前置知识：**
- ✅ 基础 C 语言（变量、循环、函数）
- ✅ 基本的二进制和十六进制概念
- ❌ 不需要任何操作系统知识

---

## 📖 第一课：显示器是如何工作的

### 1.1 从最简单的问题开始

**问题：屏幕上的字符是怎么显示出来的？**

让我们从你熟悉的东西开始：

```
在 Windows 中打 printf("Hello");
  ↓
屏幕上出现 "Hello"

中间发生了什么？
```

**简化的流程：**
```
1. printf 调用 Windows API
2. Windows 调用显卡驱动
3. 显卡驱动写入显存
4. 显卡读取显存
5. 显示器显示字符
```

**在操作系统中：**
```
没有 Windows！
没有驱动！

你需要直接操作硬件！
  → 直接写入显存
  → 显卡自动显示
```

---

### 1.2 什么是显存（Video Memory）？

**显存 = 特殊的内存区域**

想象一下：

```
普通内存（RAM）：
  地址 0x1000: 0x42  ← 存储数据
  地址 0x1004: 0x5A  ← 存储数据
  ...
  
  CPU 读写这些地址，数据只是"存着"

显存（Video RAM）：
  地址 0xB8000: 'A'  ← 写入字符
  地址 0xB8001: 0x07 ← 写入颜色
  ...
  
  CPU 写入后，显卡会读取并显示到屏幕！
```

**关键区别：**
```
普通内存：
  CPU ←→ 内存
  
显存：
  CPU ←→ 显存 ←→ 显卡 ←→ 显示器
              ↑
           自动刷新
```

---

### 1.3 VGA 文本模式的魔法地址

**VGA 文本模式显存地址：** `0xB8000`

这是什么意思？

```
物理内存布局（PC 标准）：

0x00000000 ─────────┐
                    │  BIOS 数据区
0x00100000 ─────────┤
                    │  扩展内存
...                 │
                    │
0x000A0000 ─────────┤
                    │  显存区域 ← VGA 在这里！
0x000B8000 ─────────┤  文本模式显存开始
                    │  (32KB)
0x000C0000 ─────────┤
                    │  BIOS ROM
0x00100000 ─────────┘
```

**为什么是 0xB8000？**

这是 IBM 在 1981 年定的标准：
- 彩色文本模式：0xB8000
- 单色文本模式：0xB0000（MDA）

所有 PC 兼容机都遵循这个标准！

---

### 1.4 屏幕是一个字符数组

**标准 VGA 文本模式：**
```
屏幕尺寸：80 列 × 25 行 = 2000 个字符

每个字符占 2 字节：
  字节1：ASCII 字符
  字节2：颜色属性
  
总大小：2000 × 2 = 4000 字节 = 0xFA0 字节
```

**内存布局：**
```
显存地址          屏幕位置       内容
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0xB8000          (0, 0)        字符 + 颜色
0xB8002          (1, 0)        字符 + 颜色
0xB8004          (2, 0)        字符 + 颜色
...
0xB80A0          (80, 0)       第2行开始
...
0xB8F00          (0, 24)       最后一行开始
```

**坐标转地址公式：**
```c
屏幕坐标 (x, y) → 显存地址

地址 = 0xB8000 + (y × 80 + x) × 2

示例：
  (0, 0)   → 0xB8000 + (0×80 + 0)×2 = 0xB8000
  (10, 0)  → 0xB8000 + (0×80 + 10)×2 = 0xB8014
  (0, 1)   → 0xB8000 + (1×80 + 0)×2 = 0xB80A0
  (79, 24) → 0xB8000 + (24×80 + 79)×2 = 0xB8F9E (最后一个字符)
```

---

## 📖 第二课：显示你的第一个字符

### 2.1 最简单的例子

**目标：** 在屏幕左上角显示字母 'A'

#### 方法1：直接操作显存（最原始）

```c
void print_A_simple(void)
{
    unsigned char *video = (unsigned char*)0xB8000;
    
    video[0] = 'A';    // 字符
    video[1] = 0x07;   // 颜色
}
```

**工作原理：**
```
地址 0xB8000：写入 'A' (0x41)
地址 0xB8001：写入 0x07（白字黑底）

显卡读取这两个字节：
  → 在屏幕 (0,0) 位置
  → 显示白色的 'A'
```

#### 方法2：使用辅助函数（推荐，更清晰）

```c
void print_A(void)
{
    volatile uint16_t *vga_buffer = (volatile uint16_t*)0xB8000;
    
    // 生成 VGA 条目：字符 + 颜色
    uint16_t entry = ('A') | (0x07 << 8);
    
    vga_buffer[0] = entry;
}
```

**为什么这样写？**

```
一个 VGA 字符 = 16 位 = 2 字节

低字节（0-7位）  ：字符 ASCII
高字节（8-15位） ：颜色属性

组合方式：
  entry = 字符 | (颜色 << 8)
  
  'A' = 0x41
  0x07 << 8 = 0x0700
  
  结果：0x0741
  写入内存时：
    [0xB8000] = 0x41  (字符)
    [0xB8001] = 0x07  (颜色)
```

#### 辅助函数封装（更专业）

为了代码清晰，我们定义辅助函数：

```c
// 生成颜色字节
static inline uint8_t vga_make_color(uint8_t fg, uint8_t bg) 
{
    return fg | (bg << 4);
}

// 生成 VGA 条目
static inline uint16_t vga_make_entry(unsigned char c, uint8_t color) 
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

// 使用
uint8_t color = vga_make_color(7, 0);   // 白字黑底
uint16_t entry = vga_make_entry('A', color);
vga_buffer[0] = entry;
```

**运行效果：**
```
屏幕左上角出现一个白色的 'A'！
```

---

### 2.2 显示一个字符串

**目标：** 显示 "Hello"

```c
void print_hello(void)
{
    unsigned char *video = (unsigned char*)0xB8000;
    const char *message = "Hello";
    int i = 0;
    
    while (message[i] != '\0') {
        video[i * 2] = message[i];      // 字符
        video[i * 2 + 1] = 0x07;        // 颜色
        i++;
    }
}
```

**内存布局：**
```
地址        内容       说明
━━━━━━━━━━━━━━━━━━━━━━━━━
0xB8000    'H'        字符 H
0xB8001    0x07       白色
0xB8002    'e'        字符 e
0xB8003    0x07       白色
0xB8004    'l'        字符 l
0xB8005    0x07       白色
0xB8006    'l'        字符 l
0xB8007    0x07       白色
0xB8008    'o'        字符 o
0xB8009    0x07       白色
```

---

### 2.3 理解颜色属性字节

**颜色属性（1 字节 = 8 位）：**

```
Bit:  7  6  5  4  3  2  1  0
     │  └──┴──┘  └──┴──┴──┘
     │  背景色    前景色
     └─ 闪烁位

示例：0x07 = 0000 0111
  背景：000 (黑色)
  前景：111 (白色)
  闪烁：0   (不闪烁)
```

**颜色表（前景和背景通用）：**

| 值 | 颜色 | 值 | 颜色 |
|----|------|----|------|
| 0 | 黑色 | 8 | 深灰 |
| 1 | 蓝色 | 9 | 亮蓝 |
| 2 | 绿色 | 10 | 亮绿 |
| 3 | 青色 | 11 | 亮青 |
| 4 | 红色 | 12 | 亮红 |
| 5 | 品红 | 13 | 亮品红 |
| 6 | 棕色 | 14 | 黄色 |
| 7 | 浅灰 | 15 | 白色 |

**制作颜色字节：**
```c
// 颜色 = (背景 << 4) | 前景

// 白字黑底
unsigned char color = (0 << 4) | 7;    // 0x07

// 黄字蓝底
unsigned char color = (1 << 4) | 14;   // 0x1E

// 红字白底
unsigned char color = (15 << 4) | 4;   // 0xF4
```

**示例代码：**
```c
void print_colored(const char *text, unsigned char color)
{
    unsigned char *video = (unsigned char*)0xB8000;
    int i = 0;
    
    while (text[i]) {
        video[i * 2] = text[i];
        video[i * 2 + 1] = color;  // 使用指定颜色
        i++;
    }
}

// 使用
print_colored("WARNING", 0x4F);  // 白字红底
print_colored("SUCCESS", 0x2F);  // 白字绿底
```

---

## 📖 第三课：实现完整的 VGA 驱动

### 3.1 VGA 驱动的核心结构 

**文件位置：** `kernel/vga.c` 和 `include/vga.h`

**需要维护的状态：**

```c

static uint8_t cursor_x = 0;                           // 光标 X 坐标
static uint8_t cursor_y = 0;                           // 光标 Y 坐标
static uint8_t current_color = 0;                      // 当前颜色
static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_MEMORY;  // 显存指针

// 常量定义（来自 vga.h）
#define VGA_WIDTH   80           // 屏幕宽度
#define VGA_HEIGHT  25           // 屏幕高度
#define VGA_MEMORY  0xB8000      // 显存地址

// VGA CRT 控制器端口
#define VGA_CTRL_REGISTER   0x3D4    // 索引寄存器
#define VGA_DATA_REGISTER   0x3D5    // 数据寄存器

// CRT 寄存器索引
#define VGA_CURSOR_HIGH     0x0E     // 光标位置高字节
#define VGA_CURSOR_LOW      0x0F     // 光标位置低字节
```

**为什么用 `volatile`？**

```c
volatile uint16_t *vga_buffer

volatile 关键字的作用：
  告诉编译器：这个内存可能被外部改变
  → 不要优化掉对它的访问
  → 每次都真正读写内存
  
对于显存很重要：
  显卡硬件可能修改显存
  编译器不能假设显存内容不变
```

### 3.2 基本操作：putchar

**功能：** 在当前光标位置显示一个字符

**文件：** `kernel/vga.c` 第 67-93 行

```c
/* 来自你的 EduOS - vga.c */
void vga_putc(char c)
{
    // 特殊字符处理
    if (c == '\n') {
        // 换行
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        // 回车
        cursor_x = 0;
    } else if (c == '\t') {
        // Tab（跳到下一个 8 的倍数）
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        // 退格
        if (cursor_x > 0) cursor_x--;
    } else if (c >= ' ') {
        // 普通字符（ASCII >= 32，可打印字符）
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_make_entry(c, current_color);
        cursor_x++;
    }
    
    // 检查是否需要换行
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // 检查是否需要滚动
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    
    // 更新硬件光标
    update_cursor();
}
```

**关键技术点：**

```c
// 1. 使用 uint16_t 一次写入字符+颜色
vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_make_entry(c, current_color);

// vga_make_entry 的实现：
static inline uint16_t vga_make_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// 示例：
// c = 'A' (0x41), color = 0x07
// 结果：0x0741
//   低字节：0x41 = 'A'
//   高字节：0x07 = 颜色
```

```c
// 2. Tab 对齐技巧
cursor_x = (cursor_x + 8) & ~7;

解释：
  ~7 = 0xFFFFFFF8（二进制：...11111000）
  
  当前位置：13
  (13 + 8) & ~7 = 21 & 0xFFF8 = 16
  
  效果：跳到下一个 8 的倍数
```

**逐行解析：**

```c
int offset = (vga.cursor_y * 80 + vga.cursor_x) * 2;

为什么这样计算？
  1. vga.cursor_y * 80
     → 前面有 Y 行，每行 80 个字符
     
  2. + vga.cursor_x
     → 加上当前列
     
  3. × 2
     → 每个字符占 2 字节（字符+颜色）

示例：
  光标在 (10, 5)
  offset = (5 × 80 + 10) × 2
         = (400 + 10) × 2
         = 820
  
  地址 = 0xB8000 + 820 = 0xB8334
```

### 3.3 高级操作：滚动屏幕

**文件：** `kernel/vga.c` 第 50-65 行

**什么是滚动？**

```
屏幕满了：
┌────────────────┐
│ 第 1 行        │
│ 第 2 行        │
│ ...            │
│ 第 24 行       │
│ 第 25 行 (新的)│ ← 超出屏幕！
└────────────────┘

滚动后：
┌────────────────┐
│ 第 2 行        │ ← 原来的第1行消失
│ 第 3 行        │
│ ...            │
│ 第 25 行       │
│ (空行)         │ ← 准备接收新内容
└────────────────┘
```

**你的实际实现：**

```c
/* 来自 kernel/vga.c */
void vga_scroll(void)
{
    uint16_t blank = vga_make_entry(' ', current_color);
    
    // 把第 2-25 行复制到第 1-24 行
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // 清空最后一行
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    
    // 光标移到最后一行开头
    cursor_y = VGA_HEIGHT - 1;
}
```

**代码分析：**

```c
// 使用 uint16_t 访问（字符+颜色一起复制）
vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];

优势：
  一次复制 2 字节（字符+颜色）
  比分别复制快一倍
  
计算：
  源位置：(y+1) * 80 + x  （下一行）
  目标位置：y * 80 + x    （当前行）
```

**优化：使用 memcpy**

```c
void vga_scroll_fast(void)
{
    // 一次复制 24 行
    // 源：第 2 行开始（偏移 160 字节）
    // 目标：第 1 行开始
    // 大小：24 行 × 80 字符 × 2 字节 = 3840 字节
    
    unsigned char *src = vga.video_memory + 160;  // 第2行
    unsigned char *dst = vga.video_memory;        // 第1行
    int size = 24 * 80 * 2;                       // 3840 字节
    
    memcpy(dst, src, size);
    
    // 清空最后一行
    unsigned char *last_line = vga.video_memory + 24 * 80 * 2;
    for (int i = 0; i < 80; i++) {
        last_line[i * 2] = ' ';
        last_line[i * 2 + 1] = vga.color;
    }
}
```

---

## 📖 第四课：光标控制

### 4.1 什么是光标？

**光标 = 闪烁的下划线或方块**

```
屏幕上：
Hello_          ← 这个闪烁的 _ 就是光标
     ↑
   当前位置
```

**光标的作用：**
- 指示下一个字符将出现的位置
- 让用户知道当前在哪里

### 4.2 光标是如何实现的？

**关键：** 光标不是显存中的字符，而是 VGA 控制器的硬件功能！

**VGA 控制器端口（你的代码中的定义）：**
```c
#define VGA_CTRL_REGISTER   0x3D4    // 索引寄存器
#define VGA_DATA_REGISTER   0x3D5    // 数据寄存器

#define VGA_CURSOR_HIGH     0x0E     // 光标位置高字节寄存器
#define VGA_CURSOR_LOW      0x0F     // 光标位置低字节寄存器
```

**光标位置 = 字符偏移（不是字节偏移！）**

```
示例：
  光标在 (10, 5)
  位置 = 5 × 80 + 10 = 410
  
  写入：
    高字节 = 410 >> 8 = 1
    低字节 = 410 & 0xFF = 154
```

### 4.3 更新光标位置 

**文件：** `kernel/vga.c` 第 11-18 行

```c
/* 来自 kernel/vga.c */
static void update_cursor(void)
{
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    
    // 写入光标位置高字节
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);
    
    // 写入光标位置低字节
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, pos & 0xFF);
}
```

**公开的设置光标函数（vga.c 第 109-116 行）：**

```c
void vga_set_cursor(uint8_t x, uint8_t y)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
        update_cursor();
    }
}
```

### 4.4 启用/禁用光标 

**启用光标（vga.c 第 124-130 行）：**

```c
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end)
{
    outb(VGA_CTRL_REGISTER, 0x0A);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | cursor_start);
    outb(VGA_CTRL_REGISTER, 0x0B);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | cursor_end);
}
```

**参数说明：**
```
cursor_start：光标起始扫描线（0-15）
cursor_end：  光标结束扫描线（0-15）

示例：
  vga_enable_cursor(14, 15);
  → 光标从第 14 行到第 15 行
  → 显示为下划线 _
  
  vga_enable_cursor(0, 15);
  → 光标从第 0 行到第 15 行
  → 显示为方块 █
```

**禁用光标（vga.c 第 132-136 行）：**

```c
void vga_disable_cursor(void)
{
    outb(VGA_CTRL_REGISTER, 0x0A);
    outb(VGA_DATA_REGISTER, 0x20);  // 设置禁用位
}
```

**outb 函数是什么？**

```c
// outb = Output Byte（输出字节到 I/O 端口）

static inline void outb(unsigned short port, unsigned char value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

解释：
  port = 0x3D4  → VGA 索引寄存器
  value = 14    → 选择光标高字节寄存器
  
  CPU 执行 OUT 指令
    → 告诉 VGA 控制器：接下来要操作寄存器 14
```

---

## 📖 第五课：清屏和完整的 VGA 驱动（你的 EduOS 实际实现）

### 5.1 清屏（vga.c 第 27-38 行）

**目标：** 用空格填满整个屏幕

```c
/* 来自 kernel/vga.c - 你的实际代码 */
void vga_clear(void)
{
    uint16_t blank = vga_make_entry(' ', current_color);
    
    // 填充整个屏幕（2000 个字符）
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    
    // 重置光标到左上角
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}
```

**为什么这样更高效？**
```c
// 使用 uint16_t 一次写入字符+颜色
vga_buffer[i] = blank;  // blank = 0x0720

一次操作 vs 两次操作：
  你的实现：2000 次写入
  分开写：  4000 次写入
  
提升 50%！
```

**blank 的含义：**
```c
uint16_t blank = vga_make_entry(' ', current_color);

vga_make_entry(' ', 0x07) 返回：
  0x0720
  │ │└─ 0x20 = 空格字符
  └─── 0x07 = 颜色（白字黑底）
```

### 5.2 完整的 VGA 初始化（vga.c 第 20-25 行）

```c
/* 来自 kernel/vga.c - 你的实际代码 */
void vga_init(void)
{
    // 设置默认颜色
    current_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // 清屏
    vga_clear();
    
    // 启用光标（下划线样式）
    vga_enable_cursor(14, 15);
}
```

**在 main.c 中的使用：**

```c
void kernel_main(void)
{
    vga_init();  // 第一件事：初始化 VGA
    
    // 现在可以使用 VGA 函数了！
    vga_puts("EduOS starting...\n");
}
```

### 5.3 字符串输出（vga.c 第 95-100 行）

```c
/* 来自 kernel/vga.c */
void vga_puts(const char *str)
{
    while (*str) {
        vga_putc(*str++);  // 调用 putc 显示每个字符
    }
}

// 别名函数（为了兼容）
void vga_write_string(const char *str)
{
    vga_puts(str);
}
```

### 5.4 设置颜色（vga.c 第 40-48 行）

```c
/* 设置前景色和背景色 */
void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    current_color = vga_make_color(fg, bg);
}

/* 获取当前颜色 */
uint8_t vga_get_color(void)
{
    return current_color;
}
```

**使用示例：**

```c
// 改变颜色
vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_WHITE);
vga_puts("ERROR: Something went wrong!\n");

// 恢复正常颜色
vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
vga_puts("Back to normal.\n");
```

---

## 📖 第六课：高级功能（你的 EduOS 实际实现）

### 6.1 在指定位置打印字符（vga.c 第 102-107 行）

```c
/* 来自 kernel/vga.c - 你的实际代码 */
void vga_putc_at(char c, uint8_t color, uint8_t x, uint8_t y)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(c, color);
    }
}
```

**使用示例：**

```c
// 在右上角显示 'X'（红色）
vga_putc_at('X', vga_make_color(VGA_COLOR_RED, VGA_COLOR_BLACK), 79, 0);

// 在屏幕中央显示 '+'
vga_putc_at('+', 0x0F, 40, 12);

// 绘制边框
for (int x = 0; x < 80; x++) {
    vga_putc_at('=', 0x0E, x, 0);    // 顶部边框（黄色）
    vga_putc_at('=', 0x0E, x, 24);   // 底部边框
}
```

**与 vga_putc 的区别：**

```c
vga_putc(char c)：
  → 在当前光标位置显示
  → 自动移动光标
  → 会触发滚动
  
vga_putc_at(char c, color, x, y)：
  → 在指定位置显示
  → 不移动光标
  → 不触发滚动
  → 可以指定颜色
```

### 6.2 获取光标位置（vga.c 第 118-122 行）

```c
void vga_get_cursor(uint8_t *x, uint8_t *y)
{
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}
```

**使用场景：**

```c
// 保存当前光标位置
uint8_t saved_x, saved_y;
vga_get_cursor(&saved_x, &saved_y);

// 在别处打印
vga_set_cursor(0, 24);
vga_puts("[Status Bar]");

// 恢复光标
vga_set_cursor(saved_x, saved_y);
```

### 6.3 颜色枚举（vga.h 第 18-35 行）

```c
/* 来自 include/vga.h - 标准 VGA 颜色 */
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,  // 实际显示为黄色
    VGA_COLOR_WHITE         = 15,
};
```

**实际使用：**

```c
// 错误消息（红底白字）
vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
vga_puts(" ERROR \n");

// 成功消息（绿底黑字）
vga_set_color(VGA_COLOR_BLACK, VGA_COLOR_GREEN);
vga_puts(" OK \n");

// 警告消息（黄字黑底）
vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
vga_puts("Warning: ...\n");
```

---

## 📚 完整 API 参考（你的 EduOS）

### 你实现的所有 VGA 函数

**文件：** `include/vga.h` 和 `kernel/vga.c`

#### 初始化和清屏
```c
void vga_init(void);           // 初始化VGA（必须最先调用）
void vga_clear(void);          // 清空屏幕
```

#### 字符和字符串输出
```c
void vga_putc(char c);                              // 输出字符
void vga_putchar(char c);                           // 别名
void vga_puts(const char *str);                     // 输出字符串  
void vga_write_string(const char *str);             // 别名
void vga_putc_at(char c, uint8_t color, uint8_t x, uint8_t y);  // 指定位置输出
```

#### 颜色控制
```c
void vga_set_color(enum vga_color fg, enum vga_color bg);  // 设置颜色
uint8_t vga_get_color(void);                               // 获取当前颜色

// 辅助函数（内联）
static inline uint8_t vga_make_color(enum vga_color fg, enum vga_color bg);
static inline uint16_t vga_make_entry(unsigned char c, uint8_t color);
```

#### 光标控制
```c
void vga_set_cursor(uint8_t x, uint8_t y);         // 设置光标位置
void vga_get_cursor(uint8_t *x, uint8_t *y);       // 获取光标位置
void vga_enable_cursor(uint8_t start, uint8_t end);// 启用光标
void vga_disable_cursor(void);                     // 禁用光标
```

#### 屏幕控制
```c
void vga_scroll(void);                             // 滚动屏幕
```

### 实际使用示例（来自 kernel/main.c）

```c
void kernel_main(void)
{
    // 1. 初始化
    vga_init();
    
    // 2. 显示欢迎信息
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_puts("================================================================================\n");
    vga_puts("                         EduOS Kernel v0.9.0                                   \n");
    vga_puts("================================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // 3. 输出信息
    vga_puts("\n[INFO] Kernel boot sequence started...\n");
    
    // 4. 使用颜色
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("[OK] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_puts("VGA initialized\n");
}
```

---

## 📖 第七课：格式化输出（kprintf）

### 7.1 kprintf 不是 VGA 函数！

**重要区分：**

```c
VGA 函数（vga.c）：
  vga_putc()  - 只输出到 VGA 屏幕
  vga_puts()  - 只输出到 VGA 屏幕
  
kprintf 函数（kernel.c）：
  kprintf()   - 可以输出到多个地方
              - VGA 屏幕
              - 串口
              - 日志缓冲区
```

**kprintf 内部调用 VGA：**

```c
/* kernel.c */
void kprintf(const char *fmt, ...)
{
    // 格式化字符串
    char buffer[1024];
    format_string(buffer, fmt, args);
    
    // 输出到 VGA
    vga_puts(buffer);
    
    // 同时输出到串口（调试用）
    serial_puts(COM1, buffer);
}
```

**使用建议：**

```c
// 内核中统一用 kprintf
kprintf("Value: %d\n", value);

// VGA 函数主要给 kprintf 内部用
// 或者需要精确控制时用
```

---

### 7.2 格式化输出（kprintf 风格）

虽然这不是 VGA 的直接功能，但你可能想知道怎么实现 printf：

```c
void vga_printf(const char *fmt, ...)
{
    // 使用可变参数
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd': {  // 整数
                    int num = va_arg(args, int);
                    vga_print_int(num);
                    break;
                }
                case 's': {  // 字符串
                    const char *s = va_arg(args, const char*);
                    vga_puts(s);
                    break;
                }
                case 'x': {  // 十六进制
                    int num = va_arg(args, int);
                    vga_print_hex(num);
                    break;
                }
            }
        } else {
            vga_putchar(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}

// 使用
vga_printf("Value: %d, Hex: 0x%x\n", 42, 0x2A);
```

---

## 🎓 实战练习

### 练习1：显示彩色表格

**任务：** 显示所有颜色组合

```c
void show_color_table(void)
{
    for (int bg = 0; bg < 8; bg++) {
        for (int fg = 0; fg < 16; fg++) {
            unsigned char color = (bg << 4) | fg;
            vga_write_at("█", fg * 5, bg, color);
        }
    }
}
```

**效果：** 屏幕上出现 8×16 的彩色方块！

### 练习2：进度条

```c
void show_progress(int percent)
{
    vga_write_at("[", 0, 10, 0x07);
    
    int bars = percent / 5;  // 每 5% 一个方块
    for (int i = 0; i < 20; i++) {
        char ch = (i < bars) ? '=' : ' ';
        vga_write_at(&ch, i + 1, 10, 0x0A);  // 绿色
    }
    
    vga_write_at("]", 21, 10, 0x07);
}

// 使用
for (int i = 0; i <= 100; i += 5) {
    show_progress(i);
    delay(100);  // 延迟
}
```

### 练习3：简单的窗口

```c
void draw_box(int x, int y, int width, int height)
{
    // 顶部
    vga_write_at("┌", x, y, 0x0F);
    for (int i = 1; i < width - 1; i++) {
        vga_write_at("─", x + i, y, 0x0F);
    }
    vga_write_at("┐", x + width - 1, y, 0x0F);
    
    // 中间
    for (int row = 1; row < height - 1; row++) {
        vga_write_at("│", x, y + row, 0x0F);
        vga_write_at("│", x + width - 1, y + row, 0x0F);
    }
    
    // 底部
    vga_write_at("└", x, y + height - 1, 0x0F);
    for (int i = 1; i < width - 1; i++) {
        vga_write_at("─", x + i, y + height - 1, 0x0F);
    }
    vga_write_at("┘", x + width - 1, y + height - 1, 0x0F);
}
```

---

## 🐛 常见问题

### Q1: 为什么我的字符不显示？

**检查清单：**
```c
// 1. 地址是否正确？
unsigned char *video = (unsigned char*)0xB8000;  // 必须是 0xB8000

// 2. 是否写入了颜色？
video[0] = 'A';    // 字符
video[1] = 0x07;   // 颜色 ← 必须有！

// 3. 是否在屏幕范围内？
if (x >= 0 && x < 80 && y >= 0 && y < 25) {
    // OK
}
```

### Q2: 屏幕显示乱码

**可能原因：**
```c
// ❌ 错误：偏移计算错误
int offset = (y * 80 + x);  // 忘记 ×2！

// ✅ 正确：
int offset = (y * 80 + x) * 2;
```

### Q3: 颜色不对

**调试方法：**
```c
// 打印颜色值
unsigned char color = 0x1E;
vga_printf("Color byte: 0x%02X\n", color);
vga_printf("Foreground: %d, Background: %d\n", 
           color & 0x0F, color >> 4);
```

---

## 📊 性能对比

| 操作 | 简单实现 | 优化实现 | 提升 |
|------|---------|---------|------|
| 清屏 | 4000次写入 | 2000次写入 | 2倍 |
| 滚动 | 3840次复制 | 1次memcpy | 10倍+ |
| putchar | 每次检查 | 批量操作 | 5倍 |

---

## 🎓 总结

### 你学到了

✅ **显存的概念** - 特殊的内存区域  
✅ **VGA 文本模式** - 80×25 字符阵列  
✅ **颜色控制** - 前景色和背景色  
✅ **光标控制** - 通过 I/O 端口  
✅ **屏幕滚动** - 内存复制技巧  
✅ **完整的 VGA 驱动** - 工业级实现

### 下一步

学习 **中断系统（IDT）**，让你的操作系统能响应键盘、定时器等硬件事件！

---

**VGA 文本模式 - 操作系统显示的第一步！** 🖥️

