# 第0讲：I/O 端口操作详解 - 硬件通信的基础

> 在学习 VGA 和中断之前，你必须理解如何与硬件对话

## 🎯 课程目标

学完本课，你将理解：

1. **什么是 I/O 端口？**
2. **内存映射 I/O vs 端口映射 I/O**
3. **IN 和 OUT 指令的使用**
4. **如何用 C 语言操作 I/O 端口**
5. **常见硬件设备的端口地址**

**前置知识：**
- ✅ 基础 C 语言
- ✅ 理解内存地址
- ❌ 不需要任何硬件知识

---

## 📖 第一课：从按钮说起

### 1.1 现实世界的类比

想象你要控制一台自动售货机：

```
你（CPU）        售货机（硬件）
   ↓                  ↓
按下按钮 A     →   掉出可乐
投入硬币      →   显示金额
选择商品      →   送出商品
```

**关键问题：你怎么"按下按钮"？**

在普通程序中，你可能会：
```c
vending_machine.press_button('A');  // ❌ 这样不行！
```

**在操作系统中，你需要：**
```c
outb(0x60, 0x1E);  // ✅ 直接操作硬件！
```

这就是 I/O 端口操作！

---

### 1.2 什么是 I/O 端口？

**I/O 端口 = 硬件设备的"地址"**

想象计算机内部是一条街道：

```
地址编号        门牌（设备）
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
0x60           键盘控制器
0x64           键盘命令端口
0x3D4          VGA 控制器
0x3D5          VGA 数据
0x1F0-0x1F7    硬盘控制器
...
```

**敲门（读取）：**
```c
unsigned char data = inb(0x60);  // 从键盘读取数据
```

**送信（写入）：**
```c
outb(0x3D4, 0x0E);  // 向 VGA 控制器写入命令
```

---

### 1.3 两种 I/O 方式

**方式1：端口映射 I/O（Port-Mapped I/O, PMIO）**

x86 CPU 使用这种方式：

```
内存地址空间：
  0x00000000 - 0xFFFFFFFF  （4GB，用于访问内存）
  
I/O 端口空间：
  0x0000 - 0xFFFF  （64K 个端口，单独的地址空间）
  
使用特殊指令访问：
  IN  - 从端口读取
  OUT - 向端口写入
```

**方式2：内存映射 I/O（Memory-Mapped I/O, MMIO）**

某些设备（如 VGA 显存）使用这种方式：

```
VGA 显存：
  地址 0xB8000 - 0xB8FA0
  
  就像普通内存一样访问：
    char *video = (char*)0xB8000;
    video[0] = 'A';  // 直接内存访问
```

**对比：**

| 特性 | PMIO | MMIO |
|------|------|------|
| 地址空间 | 独立（64K端口） | 共享（4GB内存） |
| 访问指令 | IN/OUT | MOV（普通内存） |
| 使用场景 | 控制寄存器 | 大块数据区 |
| 示例 | 键盘、硬盘控制器 | VGA显存、网卡缓冲区 |

---

## 📖 第二课：IN 和 OUT 指令

### 2.1 OUT 指令 - 向硬件写入

**格式：**
```asm
OUT 端口, 数据

示例：
  OUT 0x60, AL    ; 向端口 0x60 写入 AL 寄存器的值
  OUT 0x3D4, AX   ; 向端口 0x3D4 写入 AX 寄存器的值（16位）
```

**三种大小：**
```asm
OUTB - 输出字节（8位）
  OUT 0x60, AL

OUTW - 输出字（16位）
  OUT 0x3D4, AX

OUTD - 输出双字（32位）
  OUT 0x1F0, EAX
```

**生活类比：**
```
OUT 就像发送短信：
  端口号 = 电话号码
  数据   = 短信内容
  
  OUT 0x60, 0x1E  
  → 给 0x60 号设备（键盘）发送 0x1E
```

### 2.2 IN 指令 - 从硬件读取

**格式：**
```asm
IN 目标, 端口

示例：
  IN AL, 0x60     ; 从端口 0x60 读取到 AL
  IN AX, 0x3D4    ; 从端口 0x3D4 读取到 AX（16位）
```

**生活类比：**
```
IN 就像查看邮箱：
  端口号 = 邮箱号码
  数据   = 邮件内容
  
  IN AL, 0x60
  → 打开 0x60 号邮箱，看看有什么
  → 结果放到 AL 寄存器
```

---

### 2.3 为什么需要特殊指令？

**问题：为什么不能用 MOV？**

```asm
; ❌ 这样不行：
MOV AL, [0x60]      ; 这是访问内存地址 0x60，不是端口！

; ✅ 必须用：
IN AL, 0x60         ; 这才是访问 I/O 端口 0x60
```

**原因：CPU 的两个地址空间**

```
当 CPU 执行 MOV [addr], data：
  → 地址总线输出 addr
  → 控制信号：MEMR/MEMW（内存读写）
  → 内存芯片响应
  
当 CPU 执行 OUT port, data：
  → 地址总线输出 port
  → 控制信号：IOR/IOW（I/O 读写）← 不同的信号！
  → I/O 设备响应
```

---

## 📖 第三课：用 C 语言操作 I/O 端口

### 3.1 内联汇编包装

**在 C 中没有 IN/OUT 指令，必须用汇编：**

```c
/* include/io.h - I/O 端口操作 */

// 输出字节到端口
static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

// 从端口读取字节
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

**逐行解析：**

```c
asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));

分解：
  asm volatile   - 内联汇编，禁止优化
  "outb %0, %1"  - 汇编指令模板
  :              - 输出操作数（无）
  :              - 输入操作数
  "a"(value)     - %0 = AL 寄存器 = value
  "Nd"(port)     - %1 = 立即数或 DX = port

生成的汇编：
  mov al, value
  mov dx, port   (如果 port 不是常数)
  out dx, al
  
或者：
  mov al, value
  out 0x60, al   (如果 port 是常数)
```

### 3.2 16 位和 32 位版本

```c
// 输出/输入 16 位（字）
static inline void outw(uint16_t port, uint16_t value)
{
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 输出/输入 32 位（双字）
static inline void outl(uint16_t port, uint32_t value)
{
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
```

**使用场景：**

```c
// 8 位操作（最常见）
outb(0x3D4, 0x0E);              // 写入命令
uint8_t scancode = inb(0x60);   // 读取键盘

// 16 位操作
uint16_t data = inw(0x1F0);     // 读取硬盘数据

// 32 位操作（PCI 配置空间）
outl(0xCF8, 0x80000000);        // PCI 地址
uint32_t vendor = inl(0xCFC);   // PCI 数据
```

---

### 3.3 为什么要 volatile？

```c
static inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
    //  ^^^^^^^^ 这个关键字
}
```

**volatile 的作用：**

```c
// 假设没有 volatile：
for (int i = 0; i < 10; i++) {
    outb(0x60, 0xFF);
}

编译器可能优化为：
  outb(0x60, 0xFF);  // 只执行一次！
  
因为编译器看到：
  "你在做同样的事情 10 次，我帮你优化掉"
```

**加上 volatile：**

```c
asm volatile(...)

告诉编译器：
  "这个操作有副作用！"
  "每次都必须真正执行！"
  "不要优化掉！"
  
对于硬件操作：
  每次 OUT 都可能改变硬件状态
  必须真正执行
```

---

## 📖 第四课：常见硬件端口

### 4.1 VGA 控制器

```c
#define VGA_CTRL_REGISTER   0x3D4    // CRT 控制器索引
#define VGA_DATA_REGISTER   0x3D5    // CRT 控制器数据

// 使用示例：设置光标位置
void set_cursor(int x, int y)
{
    uint16_t pos = y * 80 + x;
    
    // 选择光标高字节寄存器
    outb(0x3D4, 0x0E);
    // 写入光标高字节
    outb(0x3D5, pos >> 8);
    
    // 选择光标低字节寄存器
    outb(0x3D4, 0x0F);
    // 写入光标低字节
    outb(0x3D5, pos & 0xFF);
}
```

**工作原理：**
```
第1步：outb(0x3D4, 0x0E)
  → 告诉 VGA："我要操作寄存器 0x0E"
  
第2步：outb(0x3D5, value)
  → 写入数据到寄存器 0x0E
  
第3步：outb(0x3D4, 0x0F)
  → 切换到寄存器 0x0F
  
第4步：outb(0x3D5, value)
  → 写入数据到寄存器 0x0F
```

### 4.2 键盘控制器

```c
#define KEYBOARD_DATA_PORT    0x60    // 数据端口
#define KEYBOARD_STATUS_PORT  0x64    // 状态/命令端口

// 读取按键
uint8_t read_key(void)
{
    // 等待数据就绪
    while (!(inb(0x64) & 0x01)) {
        // Bit 0 = 数据就绪标志
    }
    
    // 读取扫描码
    return inb(0x60);
}

// 发送命令到键盘
void keyboard_command(uint8_t cmd)
{
    // 等待输入缓冲区空
    while (inb(0x64) & 0x02) {
        // Bit 1 = 输入缓冲区满标志
    }
    
    // 写入命令
    outb(0x64, cmd);
}
```

**端口功能：**
```
0x60 (读取)：
  → 从键盘读取数据（扫描码）
  
0x60 (写入)：
  → 向键盘发送数据
  
0x64 (读取)：
  → 读取状态寄存器
  → Bit 0：输出缓冲区满（有数据可读）
  → Bit 1：输入缓冲区满（不能写入）
  
0x64 (写入)：
  → 向键盘控制器发送命令
```

### 4.3 定时器（PIT 8253）

```c
#define PIT_CHANNEL0  0x40    // 通道 0 数据
#define PIT_CHANNEL1  0x41    // 通道 1 数据
#define PIT_CHANNEL2  0x42    // 通道 2 数据
#define PIT_COMMAND   0x43    // 命令寄存器

// 设置定时器频率
void timer_set_frequency(uint32_t hz)
{
    uint32_t divisor = 1193182 / hz;  // PIT 基础频率
    
    // 发送命令：通道0，模式3（方波），16位
    outb(PIT_COMMAND, 0x36);
    
    // 写入分频器（先低后高）
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}
```

### 4.4 中断控制器（PIC 8259）

```c
#define PIC1_COMMAND  0x20    // 主 PIC 命令
#define PIC1_DATA     0x21    // 主 PIC 数据
#define PIC2_COMMAND  0xA0    // 从 PIC 命令
#define PIC2_DATA     0xA1    // 从 PIC 数据

// 发送 EOI（中断结束）
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        // IRQ 8-15：也要给从片发送 EOI
        outb(PIC2_COMMAND, 0x20);
    }
    
    // 总是给主片发送 EOI
    outb(PIC1_COMMAND, 0x20);
}

// 禁用 IRQ
void pic_disable_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port) | (1 << (irq % 8));
    outb(port, value);
}
```

### 4.5 串口（COM 端口）

```c
#define COM1_BASE  0x3F8    // COM1 基址
#define COM2_BASE  0x2F8    // COM2 基址

// COM1 寄存器（基址 + 偏移）
#define COM1_DATA  (COM1_BASE + 0)    // 数据寄存器
#define COM1_IER   (COM1_BASE + 1)    // 中断使能
#define COM1_LSR   (COM1_BASE + 5)    // 线状态

// 发送字符到串口
void serial_putc(char c)
{
    // 等待发送缓冲区空
    while (!(inb(COM1_LSR) & 0x20)) {
        // Bit 5 = 发送缓冲区空标志
    }
    
    // 写入数据
    outb(COM1_DATA, c);
}

// 从串口读取字符
char serial_getc(void)
{
    // 等待数据就绪
    while (!(inb(COM1_LSR) & 0x01)) {
        // Bit 0 = 数据就绪标志
    }
    
    // 读取数据
    return inb(COM1_DATA);
}
```

---

## 📖 第五课：I/O 操作的实际应用

### 5.1 延时函数

**为什么需要延时？**

```
某些硬件很慢：
  你发送命令后，设备需要时间处理
  
  如果立即发送下一个命令：
    → 设备还没准备好
    → 命令丢失或错误
```

**使用 I/O 端口实现延时：**

```c
// I/O 延时（约 1 微秒）
static inline void io_wait(void)
{
    // 端口 0x80 通常未使用
    // 访问它只是为了浪费时间
    outb(0x80, 0);
}
```

**为什么 outb(0x80, 0) 能延时？**

```
I/O 操作比内存操作慢得多：
  内存访问：~1 纳秒
  I/O 访问：~1 微秒（1000 倍慢！）
  
outb 到端口 0x80：
  → CPU 等待 I/O 总线完成
  → 产生约 1 微秒的延迟
```

**实际使用：**

```c
// 重启计算机
void reboot(void)
{
    // 通过键盘控制器重启
    outb(0x64, 0xFE);  // 发送重启命令
    
    // 如果失败，用三重故障重启
    asm volatile("int $0x00");  // 触发异常
}

// 设置 PIT 时需要延时
void timer_init(void)
{
    outb(0x43, 0x36);
    io_wait();  // 等待命令被处理
    
    outb(0x40, divisor & 0xFF);
    io_wait();
    
    outb(0x40, divisor >> 8);
}
```

---

### 5.2 轮询 vs 中断

**轮询方式（使用 I/O 端口）：**

```c
// 不停检查键盘
while (1) {
    if (inb(0x64) & 0x01) {  // 数据就绪？
        uint8_t key = inb(0x60);  // 读取按键
        process_key(key);
    }
    
    // 做其他事情...
}
```

**缺点：**
- CPU 一直忙于检查
- 浪费资源
- 可能错过事件

**中断方式（更好）：**

```c
// 注册中断处理程序
register_irq_handler(1, keyboard_interrupt);

// 然后忘掉它！
// 有按键时自动调用 keyboard_interrupt

void keyboard_interrupt(void)
{
    uint8_t key = inb(0x60);  // 读取按键
    process_key(key);
    
    outb(0x20, 0x20);  // 发送 EOI
}
```

---

## 📖 第六课：常见陷阱和调试

### 6.1 端口号错误

```c
// ❌ 错误：端口号写错
outb(0x34D, 0x0E);  // 应该是 0x3D4

// ✅ 正确：
outb(0x3D4, 0x0E);

// 后果：
//   写到了错误的设备
//   可能导致系统崩溃
```

**调试技巧：**
```c
// 添加调试输出
kprintf("[DEBUG] Writing 0x%02X to port 0x%04X\n", value, port);
outb(port, value);
```

### 6.2 读写顺序错误

```c
// ❌ 错误：先写数据，后写索引
outb(0x3D5, value);   // 写数据
outb(0x3D4, 0x0E);    // 写索引

// ✅ 正确：先写索引，后写数据
outb(0x3D4, 0x0E);    // 写索引（选择寄存器）
outb(0x3D5, value);   // 写数据（写入该寄存器）
```

### 6.3 忘记等待设备就绪

```c
// ❌ 危险：不等待
outb(0x60, data1);
outb(0x60, data2);  // 设备还没处理完 data1！

// ✅ 安全：等待就绪
void keyboard_send(uint8_t data)
{
    // 等待输入缓冲区空
    while (inb(0x64) & 0x02);
    
    // 现在可以安全写入
    outb(0x60, data);
}
```

---

## 🧪 实战练习

### 练习1：系统扬声器（蜂鸣器）

```c
// 使 PC 扬声器发声
void beep(uint32_t frequency)
{
    uint32_t div = 1193182 / frequency;
    
    // 设置 PIT 通道 2（扬声器）
    outb(0x43, 0xB6);
    outb(0x42, div & 0xFF);
    outb(0x42, div >> 8);
    
    // 启用扬声器
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp | 0x03);
}

// 关闭扬声器
void no_beep(void)
{
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & 0xFC);
}

// 使用
beep(1000);  // 1000 Hz
delay(500);  // 延时 500ms
no_beep();
```

### 练习2：读取 CMOS 时间

```c
#define CMOS_ADDRESS  0x70
#define CMOS_DATA     0x71

// 从 CMOS 读取
uint8_t read_cmos(uint8_t reg)
{
    outb(CMOS_ADDRESS, reg);   // 选择寄存器
    return inb(CMOS_DATA);     // 读取数据
}

// 获取当前时间
void get_rtc_time(void)
{
    uint8_t second = read_cmos(0x00);
    uint8_t minute = read_cmos(0x02);
    uint8_t hour   = read_cmos(0x04);
    
    kprintf("Time: %02d:%02d:%02d\n", hour, minute, second);
}
```

### 练习3：控制 LED（键盘指示灯）

```c
void set_keyboard_leds(bool caps, bool num, bool scroll)
{
    uint8_t led_state = 0;
    
    if (scroll) led_state |= 1;
    if (num)    led_state |= 2;
    if (caps)   led_state |= 4;
    
    // 发送命令：设置 LED
    keyboard_send(0xED);
    // 发送数据：LED 状态
    keyboard_send(led_state);
}

// 使用
set_keyboard_leds(true, false, false);  // Caps Lock 亮
```

---

## 📊 I/O 端口速查表

### 常用端口地址

| 端口 | 设备 | 功能 |
|------|------|------|
| 0x20, 0x21 | PIC 主片 | 中断控制器 |
| 0xA0, 0xA1 | PIC 从片 | 中断控制器 |
| 0x40-0x43 | PIT 8253 | 定时器 |
| 0x60, 0x64 | 键盘控制器 | 键盘输入 |
| 0x70, 0x71 | CMOS/RTC | 实时时钟 |
| 0x80 | 诊断端口 | 延时用 |
| 0x92 | PS/2 控制器 | A20, 复位 |
| 0x3D4, 0x3D5 | VGA CRT | 显示控制 |
| 0x3F8-0x3FF | COM1 | 串口1 |
| 0x2F8-0x2FF | COM2 | 串口2 |
| 0x1F0-0x1F7 | IDE 主通道 | 硬盘 |
| 0x170-0x177 | IDE 从通道 | 硬盘 |
| 0xCF8, 0xCFC | PCI | PCI 配置 |

### 端口访问规则

```c
// 规则1：先索引，后数据
outb(INDEX_PORT, register_number);  // 选择寄存器
outb(DATA_PORT, value);             // 读写数据

// 规则2：检查状态再操作
while (inb(STATUS_PORT) & BUSY_BIT);  // 等待不忙
outb(DATA_PORT, value);                // 安全写入

// 规则3：读取后处理
uint8_t status = inb(STATUS_PORT);    // 读取状态
if (status & ERROR_BIT) {             // 检查错误
    handle_error();
}
```

---

## 🎓 总结

### 核心概念

✅ **I/O 端口** - 硬件设备的地址（独立的 64K 地址空间）  
✅ **IN/OUT 指令** - 与硬件通信的唯一方式  
✅ **端口映射** - 每个端口对应特定硬件  
✅ **volatile** - 防止编译器优化掉硬件操作  
✅ **等待就绪** - 操作前检查设备状态  

### 记忆要点

```
outb(port, value)  → 写入（发送命令/数据）
value = inb(port)  → 读取（获取状态/数据）

常用端口：
  0x60/0x64 → 键盘
  0x3D4/0x3D5 → VGA
  0x20/0xA0 → 中断控制器
  0x40/0x43 → 定时器
```

### 与 VGA 和中断的联系

```
VGA 显示：
  需要 I/O 端口控制光标 (0x3D4, 0x3D5)
  ↓
中断系统：
  需要 I/O 端口配置 PIC (0x20, 0xA0)
  需要 I/O 端口读取键盘 (0x60)
  ↓
所有硬件驱动：
  都基于 I/O 端口操作！
```

### 下一步

现在你理解了 I/O 端口，可以学习：
- **VGA 驱动** - 使用端口控制光标
- **中断系统** - 使用端口配置 PIC
- **设备驱动** - 使用端口与硬件通信

---

**I/O 端口操作 - 硬件编程的基石！** ⚙️

