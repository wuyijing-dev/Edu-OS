# 第2讲：中断系统完全指南 - IDT, PIC 与 IRQ

> 中断是操作系统的心跳

## 🎯 课程目标

学完本课，你将完整理解：

1. **什么是中断？为什么需要中断？**
2. **IDT（中断描述符表）的结构和作用**
3. **PIC（中断控制器）的工作原理**
4. **IRQ（硬件中断）的处理流程**
5. **如何实现定时器、键盘等实际中断**
6. **中断系统的初始化和管理**

**前置知识：**
- ✅ I/O 端口操作（第0讲）
- ✅ VGA 显示（第1讲）
- ✅ 基础 C 语言和汇编
- ❌ 不需要任何中断知识

**涉及文件：**
- `kernel/arch/i386/idt.c` - IDT 初始化
- `kernel/arch/i386/pic.c` - PIC 控制器
- `kernel/arch/i386/irq.c` - IRQ 处理
- `kernel/arch/i386/interrupts.asm` - 汇编入口

---

## 📖 第一课：什么是中断

### 1.1 从生活例子理解中断

**场景1：你在读书**
```
你正在专心读书...
  ↓
突然！门铃响了
  ↓
你停下来
  ↓
去开门
  ↓
处理完毕
  ↓
回来继续读书
```

**这就是中断！**

```
CPU 正在执行程序...
  ↓
突然！键盘按键
  ↓
CPU 停下当前程序
  ↓
执行键盘中断处理程序
  ↓
处理完毕
  ↓
回来继续执行原程序
```

### 1.2 没有中断会怎样？

**轮询方式（Polling）：**

```c
// 不断检查键盘
while (1) {
    if (key_pressed()) {
        char ch = read_key();
        process_key(ch);
    }
    
    // 继续做其他事情...
}
```

**问题：**
- ❌ CPU 一直忙于检查（浪费）
- ❌ 可能错过按键（检查间隔太长）
- ❌ 响应慢（延迟）
- ❌ 无法同时处理多个设备

**中断方式：**

```c
// 注册中断处理程序
register_interrupt(KEYBOARD_IRQ, keyboard_handler);

// 然后就不用管了！
// CPU 做自己的事情
// 当键盘有输入时，CPU 自动调用 keyboard_handler

void keyboard_handler(void)
{
    char ch = read_key();
    process_key(ch);
    // 处理完自动返回
}
```

**优势：**
- ✅ CPU 高效利用
- ✅ 不会错过事件
- ✅ 响应快
- ✅ 可以处理多个设备

---

### 1.3 中断的类型

**在 x86 中，有 3 种"中断"：**

**1. 硬件中断（Hardware Interrupt / IRQ）**
```
来源：外部硬件设备

示例：
  - 键盘按键    → IRQ 1
  - 定时器滴答  → IRQ 0
  - 鼠标移动    → IRQ 12
  - 硬盘完成    → IRQ 14
  
特点：
  - 异步（随时可能发生）
  - 可屏蔽（可以禁用）
  - 通过中断控制器（PIC）
```

**2. 软件中断（Software Interrupt）**
```
来源：程序执行 INT 指令

示例：
  - INT 0x80  → 系统调用（Linux）
  - INT 0x10  → BIOS 视频服务
  - INT 0x13  → BIOS 磁盘服务
  
特点：
  - 同步（程序主动调用）
  - 不可屏蔽
  - 用于请求服务
```

**3. 异常（Exception）**
```
来源：CPU 检测到错误

示例：
  - 除零错误    → #DE (Divide Error)
  - 缺页异常    → #PF (Page Fault)
  - 非法指令    → #UD (Undefined Opcode)
  - 保护故障    → #GP (General Protection)
  
特点：
  - 同步（由错误指令触发）
  - 不可屏蔽
  - 必须处理，否则崩溃
```

---

## 📖 第二课：IDT 的结构

### 2.1 什么是 IDT？

**IDT = Interrupt Descriptor Table（中断描述符表）**

```
作用：告诉 CPU 当中断发生时，去哪里执行代码

类比：
  电话簿：
    110 → 报警电话号码
    120 → 急救电话号码
    
  IDT：
    中断 0  → 除零异常处理程序地址
    中断 13 → 保护故障处理程序地址
    中断 33 → 键盘中断处理程序地址
```

**IDT 在内存中的样子：**
```
地址          内容
━━━━━━━━━━━━━━━━━━━━━━━━━━━━
IDT_BASE + 0    → 中断 0 的描述符（8字节）
IDT_BASE + 8    → 中断 1 的描述符（8字节）
IDT_BASE + 16   → 中断 2 的描述符（8字节）
...
IDT_BASE + 2040 → 中断 255 的描述符（8字节）

总大小：256 × 8 = 2048 字节
```

### 2.2 中断描述符的结构

**每个描述符 8 字节，包含什么？**

```
中断描述符（64 位 = 8 字节）：

Bit 0-15:   offset_low   (处理程序地址的低 16 位)
Bit 16-31:  selector     (代码段选择子，通常 0x08)
Bit 32-39:  reserved     (保留，必须为 0)
Bit 40-47:  flags        (类型、DPL 等属性)
Bit 48-63:  offset_high  (处理程序地址的高 16 位)

完整地址 = (offset_high << 16) | offset_low
```

**示例：**
```
假设键盘中断处理程序地址是 0xC0101234

中断描述符：
  offset_low  = 0x1234
  selector    = 0x0008  (内核代码段)
  reserved    = 0x00
  flags       = 0x8E    (P=1, DPL=0, Type=E)
  offset_high = 0xC010
  
组合：0xC010 1234 = 完整地址
```

### 2.3 flags 字段详解

**Flags 字节（8 位）：**

```
Bit 7:    P (Present)        - 描述符有效位
Bit 6-5:  DPL (特权级)       - 0=内核, 3=用户
Bit 4:    S (Storage)        - 必须为 0
Bit 3-0:  Gate Type (门类型) - 中断门/陷阱门

常用值：
  0x8E = 1000 1110
       = P=1, DPL=0, S=0, Type=E
       = 内核级中断门（不允许中断嵌套）
  
  0x8F = 1000 1111  
       = 内核级陷阱门（允许中断嵌套）
  
  0xEE = 1110 1110
       = P=1, DPL=3, S=0, Type=E
       = 用户级中断门（用于系统调用）
```

**门类型的区别：**
```
中断门（Type=E）：
  CPU 自动清除 IF 标志
  → 禁止中断嵌套
  → 用于硬件中断
  
陷阱门（Type=F）：
  CPU 不清除 IF 标志
  → 允许中断嵌套
  → 用于异常处理
```

---

## 📖 第三课：中断处理流程

### 3.1 中断发生时 CPU 做什么？

**完整流程（硬件自动）：**

```
1. 保存当前状态
   → PUSH EFLAGS
   → PUSH CS
   → PUSH EIP
   
2. 清除 IF 标志（如果是中断门）
   → 禁止中断嵌套
   
3. 从 IDT 读取描述符
   → 获取处理程序地址
   → 获取代码段选择子
   
4. 跳转到处理程序
   → CS = selector
   → EIP = offset
   
5. 执行中断处理代码
   → 你的 handler 函数
   
6. IRET 返回
   → POP EIP
   → POP CS
   → POP EFLAGS
   → 恢复原来的执行
```

**栈的变化：**
```
中断前：
┌──────────┐
│   ...    │
│          │
└──────────┘ ← ESP

中断后（CPU 自动）：
┌──────────┐
│   ...    │
├──────────┤
│ EFLAGS   │ ← CPU 压入
├──────────┤
│ CS       │ ← CPU 压入
├──────────┤
│ EIP      │ ← CPU 压入
└──────────┘ ← ESP

处理程序可能再压入：
┌──────────┐
│ EFLAGS   │
│ CS       │
│ EIP      │
├──────────┤
│ EAX      │ ← 你压入
│ EBX      │ ← 你压入
│ ...      │
└──────────┘ ← ESP
```

### 3.2 中断处理程序的结构

**基本框架：**

```asm
; 汇编入口（保存寄存器）
keyboard_interrupt_asm:
    pusha              ; 保存所有通用寄存器
    push ds
    push es
    push fs
    push gs
    
    ; 设置内核数据段
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    ; 调用 C 处理函数
    call keyboard_handler
    
    ; 恢复寄存器
    pop gs
    pop fs
    pop es
    pop ds
    popa
    
    iret               ; 中断返回
```

```c
// C 处理函数
void keyboard_handler(void)
{
    // 读取键盘扫描码
    unsigned char scancode = inb(0x60);
    
    // 处理按键
    process_keypress(scancode);
    
    // 发送 EOI（End Of Interrupt）给中断控制器
    outb(0x20, 0x20);
}
```

---

## 📖 第四课：实现 IDT

### 4.1 定义 IDT 数据结构

```c
/* IDT 描述符结构 */
struct idt_entry {
    uint16_t offset_low;   // 处理程序地址低 16 位
    uint16_t selector;     // 代码段选择子
    uint8_t  zero;         // 保留，必须为 0
    uint8_t  flags;        // 类型和属性
    uint16_t offset_high;  // 处理程序地址高 16 位
} __attribute__((packed));  // 不要填充，紧凑排列

/* IDT 指针（给 LIDT 指令用）*/
struct idt_ptr {
    uint16_t limit;        // IDT 大小 - 1
    uint32_t base;         // IDT 起始地址
} __attribute__((packed));

/* 全局 IDT 表（256 个描述符）*/
struct idt_entry idt[256];
struct idt_ptr idtp;
```

### 4.2 设置 IDT 表项

```c
/*
 * 设置一个 IDT 表项
 */
void idt_set_gate(
    uint8_t num,           // 中断号 (0-255)
    uint32_t handler,      // 处理程序地址
    uint16_t selector,     // 代码段选择子
    uint8_t flags          // 属性标志
)
{
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

// 使用示例
idt_set_gate(33, (uint32_t)keyboard_interrupt_asm, 0x08, 0x8E);
//           ↑    ↑                                 ↑     ↑
//          IRQ1  处理程序地址                    内核段  中断门
```

### 4.3 加载 IDT

```c
void idt_init(void)
{
    // 1. 设置 IDT 指针
    idtp.limit = sizeof(idt) - 1;  // 256×8-1 = 2047
    idtp.base = (uint32_t)&idt;
    
    // 2. 清空 IDT
    memset(&idt, 0, sizeof(idt));
    
    // 3. 设置所有中断处理程序
    // （后面会讲）
    
    // 4. 加载 IDT 到 CPU
    idt_load(&idtp);
}
```

**idt_load 汇编实现：**

```asm
[GLOBAL idt_load]

idt_load:
    mov eax, [esp + 4]     ; 获取参数（IDT 指针地址）
    lidt [eax]             ; 加载 IDT
    ret

; LIDT 指令：
;   加载 IDT 寄存器
;   CPU 从此知道 IDT 在哪里
```

---

## 📖 第五课：硬件中断（IRQ）和 PIC 控制器

### 5.1 什么是 IRQ？

**IRQ = Interrupt Request（中断请求）**

当硬件需要 CPU 注意时，它会发送一个 IRQ：

```
键盘被按下：
  键盘控制器 → 发送 IRQ 1 → PIC → CPU
  
定时器滴答：
  定时器芯片 → 发送 IRQ 0 → PIC → CPU
  
硬盘完成：
  硬盘控制器 → 发送 IRQ 14 → PIC → CPU
```

**问题来了：CPU 只有一个中断输入引脚！**

```
键盘、鼠标、硬盘、网卡...全都想要中断 CPU
但 CPU 的 INTR 引脚只有一个！

怎么办？
```

### 5.2 PIC 芯片的解决方案

**PIC = Programmable Interrupt Controller（可编程中断控制器）**

PIC 充当"总机"的角色：

```
硬件设备                PIC             CPU
━━━━━━━━              ━━━━━           ━━━━
定时器  ──IRQ 0──┐
键盘    ──IRQ 1──┤
       ──IRQ 2──┤
串口1   ──IRQ 3──┤
串口2   ──IRQ 4──┼──→  主 PIC  ──INTR──→ CPU
并口2   ──IRQ 5──┤     (8259A)
软盘    ──IRQ 6──┤
并口1   ──IRQ 7──┘

RTC     ──IRQ 8──┐
       ──IRQ 9──┤
       ──IRQ 10─┤
       ──IRQ 11─┼──→  从 PIC  ──连接到──→ 主PIC IRQ2
鼠标    ──IRQ 12─┤     (8259A)
FPU     ──IRQ 13─┤
主IDE   ──IRQ 14─┤
从IDE   ──IRQ 15─┘

总共：16 个 IRQ（8+8）
```

**PIC 的工作：**

1. 接收来自各个设备的 IRQ
2. 确定优先级（IRQ 0 最高）
3. 通过 INTR 引脚通知 CPU
4. 告诉 CPU 是哪个 IRQ（通过数据总线）

### 5.3 IRQ 设备分配表

**PC 标准的 IRQ 分配：**

| IRQ | 设备 | 中断号 | 说明 |
|-----|------|--------|------|
| 0 | 定时器（PIT） | 32 | 系统时钟，100Hz |
| 1 | 键盘 | 33 | PS/2 键盘 |
| 2 | 级联 | - | 连接从 PIC |
| 3 | COM2 | 35 | 串口2 |
| 4 | COM1 | 36 | 串口1 |
| 5 | LPT2/声卡 | 37 | 并口2或声卡 |
| 6 | 软盘 | 38 | 软盘控制器 |
| 7 | LPT1 | 39 | 并口1 |
| 8 | RTC | 40 | 实时时钟 |
| 9 | ACPI | 41 | 电源管理 |
| 10 | 可用 | 42 | 网卡等 |
| 11 | 可用 | 43 | 网卡等 |
| 12 | PS/2 鼠标 | 44 | PS/2 鼠标 |
| 13 | 协处理器 | 45 | FPU |
| 14 | 主 IDE | 46 | 主硬盘 |
| 15 | 从 IDE | 47 | 从硬盘 |

**中断号是如何确定的？**

```
CPU 中断向量分配：
  0-31   → CPU 异常（保留）
  32-255 → 可用于硬件中断

IRQ 到中断号的映射（重映射后）：
  IRQ 0 → 中断 32
  IRQ 1 → 中断 33
  ...
  IRQ 15 → 中断 47

公式：中断号 = IRQ号 + 32
```

**为什么要加 32？**

因为 PIC 重映射！让我们详细了解...

---

### 5.4 PIC 重映射 - 解决冲突问题

#### 默认映射的问题

**PIC 出厂默认设置：**

```
主 PIC（IRQ 0-7）  → 中断 8-15
从 PIC（IRQ 8-15） → 中断 0x70-0x77
```

**致命冲突：**

```
IRQ 0（定时器）→ 中断 8   但！中断 8 = Double Fault（CPU 异常）
IRQ 1（键盘）  → 中断 9   但！中断 9 = Coprocessor Segment Overrun
...

CPU 无法区分：
  是定时器中断？
  还是 Double Fault 异常？
  
结果：系统崩溃！
```

**解决方案：重映射**

把 IRQ 移到 32-47，避开 CPU 保留的 0-31：

```
重映射后：
  IRQ 0-7   → 中断 32-39  ✅ 不冲突！
  IRQ 8-15  → 中断 40-47  ✅ 不冲突！
  
现在：
  中断 8  → Double Fault（异常）
  中断 32 → IRQ 0（定时器）
  
清晰分离！
```

### 5.5 PIC 重映射的实现

基于 `kernel/arch/i386/pic.c` 的实现。

#### PIC 的端口地址

```c
// 主 PIC（Master PIC）
#define PIC1_COMMAND  0x20    // 命令端口
#define PIC1_DATA     0x21    // 数据端口

// 从 PIC（Slave PIC）
#define PIC2_COMMAND  0xA0    // 命令端口
#define PIC2_DATA     0xA1    // 数据端口
```

#### 重映射代码

```c
void pic_init(uint8_t offset1, uint8_t offset2)
{
    // offset1 = 32（主 PIC 偏移）
    // offset2 = 40（从 PIC 偏移）
    
    // 保存当前中断屏蔽
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // ICW1：开始初始化
    outb(PIC1_COMMAND, 0x11);  // 初始化 + 需要ICW4
    outb(PIC2_COMMAND, 0x11);
    
    // ICW2：设置中断向量偏移
    outb(PIC1_DATA, offset1);  // 主 PIC：IRQ 0-7 → INT 32-39
    outb(PIC2_DATA, offset2);  // 从 PIC：IRQ 8-15 → INT 40-47
    
    // ICW3：设置级联
    outb(PIC1_DATA, 0x04);     // 主 PIC：IRQ 2 连接从 PIC
    outb(PIC2_DATA, 0x02);     // 从 PIC：连接到主 PIC IRQ 2
    
    // ICW4：设置模式
    outb(PIC1_DATA, 0x01);     // 8086 模式
    outb(PIC2_DATA, 0x01);
    
    // 恢复屏蔽
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
```

**ICW 命令详解：**

```
ICW = Initialization Command Word（初始化命令字）

ICW1 (0x11 = 0001 0001)：
  Bit 4 = 1 → 需要 ICW4
  Bit 0 = 1 → 级联模式（有从片）
  
ICW2 (偏移值)：
  主 PIC：32 → IRQ 0-7 映射到 INT 32-39
  从 PIC：40 → IRQ 8-15 映射到 INT 40-47
  
ICW3 (级联设置)：
  主 PIC：0x04 = 0000 0100 → Bit 2 = IRQ 2 有从片
  从 PIC：0x02 = 0000 0010 → 连接到主片 IRQ 2
  
ICW4 (0x01)：
  Bit 0 = 1 → 8086/8088 模式（不是 8080）
```

---

### 5.6 EOI - End of Interrupt

**什么是 EOI？**

中断处理完成后，必须告诉 PIC "我处理完了"，否则 PIC 不会发送下一个中断。

```
没有 EOI：
  定时器触发 IRQ 0
  → CPU 处理
  → 忘记发送 EOI
  → PIC 认为还在处理
  → 不再发送 IRQ 0
  → 定时器停止！
```

**发送 EOI 的代码：**

```c
void pic_send_eoi(uint8_t irq)
{
    // 如果是从 PIC 的 IRQ（8-15）
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);  // 给从 PIC 发 EOI
    }
    
    // 总是给主 PIC 发 EOI
    outb(PIC1_COMMAND, 0x20);
}
```

**为什么从 PIC 要发两次 EOI？**

```
IRQ 14（硬盘中断）的路径：
  硬盘 → 从 PIC IRQ 6 → 主 PIC IRQ 2 → CPU
  
处理完成后：
  1. 先告诉从 PIC："IRQ 6 处理完了"
  2. 再告诉主 PIC："IRQ 2 处理完了"
  
如果只发给主 PIC：
  → 主 PIC 继续
  → 但从 PIC 阻塞
  → IRQ 8-15 全部失效！
```

---

## 📖 第六课：IRQ 处理系统

### 6.1 IRQ 处理器架构

**三层结构：**

```
1. 汇编入口（interrupts.asm）
   → 保存寄存器
   → 调用 C 函数
   ↓
2. IRQ 分发器（irq.c）
   → 查找处理函数
   → 调用具体处理器
   ↓
3. 设备处理器（timer.c, keyboard.c 等）
   → 处理具体设备
   → 返回
```

### 6.2 注册 IRQ 处理函数

基于 `kernel/arch/i386/irq.c`：

```c
// IRQ 处理函数类型
typedef void (*irq_handler_t)(struct interrupt_frame *frame);

// 处理函数表（16个IRQ）
static irq_handler_t irq_handlers[16] = {NULL};

// 注册处理函数
int irq_install_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq >= 16) {
        return -1;
    }
    
    irq_handlers[irq] = handler;
    kprintf("[IRQ] Installed handler for IRQ%d\n", irq);
    
    return 0;
}

// 使用示例
irq_install_handler(0, timer_handler);     // 定时器
irq_install_handler(1, keyboard_handler);  // 键盘
```

### 6.3 IRQ 处理流程（实际实现）

基于 `kernel/arch/i386/irq.c`:

```c
void irq_handler(uint8_t irq, struct interrupt_frame *frame)
{
    /* 1. 验证IRQ号 */
    if (irq >= 16) {
        kprintf("[IRQ] Error: Invalid IRQ number %d\n", irq);
        return;
    }
    
    /* 2. 增加统计计数 */
    irq_counts[irq]++;
    
    /* 3. 检查伪中断（spurious interrupt）*/
    if (pic_is_spurious(irq)) {
        return;  // 伪中断：不处理，不发EOI
    }
    
    /* 4. 调用注册的处理函数 */
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](frame);
    } else {
        /* 未注册处理函数 - 静默处理避免刷屏 */
        static uint32_t unhandled_count[16] = {0};
        unhandled_count[irq]++;
        
        /* 仅首次报告 */
        if (unhandled_count[irq] == 1) {
            kprintf("[IRQ] Unhandled IRQ%d (will not report again)\n", irq);
        }
    }
    
    /* 5. 发送EOI到PIC */
    pic_send_eoi(irq);
}
```

**实际代码的改进：**
- ✅ IRQ号验证（防止数组越界）
- ✅ 统计计数器（性能分析）
- ✅ 未注册处理函数只报告一次（避免日志刷屏）
- ✅ 伪中断检测（硬件可靠性）

**关键点：伪中断检测（实际实现）**

基于 `kernel/arch/i386/pic.c`:

```c
bool pic_is_spurious(uint8_t irq)
{
    uint16_t isr;
    
    /* 只有IRQ7和IRQ15可能是伪中断 */
    if (irq == 7) {
        isr = pic_get_isr();
        /* 检查主PIC的bit 7 */
        if (!(isr & 0x80)) {
            kprintf("[PIC] Spurious IRQ7 detected\n");
            return true;  // 是伪中断
        }
    } else if (irq == 15) {
        isr = pic_get_isr();
        /* 检查从PIC的bit 7 */
        if (!(isr & 0x8000)) {
            kprintf("[PIC] Spurious IRQ15 detected\n");
            /* 特殊：IRQ15伪中断仍需向主PIC发EOI */
            outb(PIC_MASTER_CMD, 0x20);
            return true;
        }
    }
    
    return false;  // 不是伪中断
}

/* 读取ISR寄存器 */
uint16_t pic_get_isr(void)
{
    outb(PIC_MASTER_CMD, 0x0B);  // 读取ISR命令
    outb(PIC_SLAVE_CMD, 0x0B);
    return ((uint16_t)inb(PIC_SLAVE_CMD) << 8) | inb(PIC_MASTER_CMD);
}
```

**为什么伪中断很重要？**

```
不检测伪中断的后果：
  1. 伪IRQ7触发
  2. 调用处理函数（没有实际硬件事件）
  3. 发送EOI
  4. PIC混乱
  5. 可能导致其他IRQ失效

实际系统必须检测伪中断！
```

### 6.4 启用和禁用 IRQ

```c
// 启用指定 IRQ
void irq_enable(uint8_t irq)
{
    pic_unmask_irq(irq);
}

// 禁用指定 IRQ
void irq_disable(uint8_t irq)
{
    pic_mask_irq(irq);
}

// 屏蔽/取消屏蔽的实现
void pic_unmask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port) & ~(1 << (irq % 8));
    outb(port, value);
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port) | (1 << (irq % 8));
    outb(port, value);
}
```

**IRQ 屏蔽寄存器：**

```
主 PIC（端口 0x21）：
  Bit 0 = IRQ 0 屏蔽位（1=禁用，0=启用）
  Bit 1 = IRQ 1 屏蔽位
  ...
  Bit 7 = IRQ 7 屏蔽位

示例：
  0xFF = 1111 1111 → 全部禁用
  0x00 = 0000 0000 → 全部启用
  0xFC = 1111 1100 → 只启用 IRQ 0 和 1
```

---

## 📖 第七课：实现定时器中断（详细版）

基于 `kernel/drivers/timer.c`。

### 7.1 定时器的作用

**系统定时器（PIT）做什么？**

```
每隔固定时间触发一次中断：
  → 更新系统时间
  → 进程调度（时间片）
  → 延时函数
  → 性能统计
```

### 7.2 PIT 8253/8254 芯片详解

**什么是 PIT？**

PIT = Programmable Interval Timer（可编程间隔定时器）

**芯片型号：**
- Intel 8253（早期）
- Intel 8254（现代PC）

**PIT 有 3 个独立的计数器（通道）：**

```
通道 0：
  - 连接到 IRQ 0
  - 用于系统时钟
  - 我们主要使用这个
  
通道 1：
  - DRAM 刷新
  - 系统自动使用
  - 不要修改！
  
通道 2：
  - PC 扬声器
  - 可以用来发声（蜂鸣器）
  - 可选功能
```

### 7.3 PIT 寄存器和端口

**I/O 端口地址：**

```c
#define PIT_CHANNEL0    0x40    // 通道0数据端口
#define PIT_CHANNEL1    0x41    // 通道1数据端口
#define PIT_CHANNEL2    0x42    // 通道2数据端口
#define PIT_COMMAND     0x43    // 命令寄存器
```

**命令寄存器（端口 0x43）格式：**

```
Bit 7-6: 选择通道（SC - Select Counter）
  00 = 通道 0
  01 = 通道 1
  10 = 通道 2
  11 = 回读命令（8254专用）

Bit 5-4: 读写模式（RW - Read/Write）
  00 = 锁存计数值
  01 = 只读/写低字节
  10 = 只读/写高字节
  11 = 先低字节后高字节（16位）

Bit 3-1: 工作模式（Mode）
  000 = 模式0：中断结束时计数
  001 = 模式1：硬件可重触发单稳态
  010 = 模式2：分频器（速率发生器）
  011 = 模式3：方波发生器 ← 我们用这个！
  100 = 模式4：软件触发选通
  101 = 模式5：硬件触发选通

Bit 0: 计数格式（BCD）
  0 = 二进制（0-65535）
  1 = BCD（0-9999）
```

**常用命令字示例：**

```c
// 通道0，方波模式，16位，二进制
0x36 = 0011 0110
       ││││ ││││
       ││││ │││└─ BCD=0（二进制）
       ││││ └┴─── Mode=011（方波）
       ││└┴────── RW=11（16位）
       └┴──────── SC=00（通道0）

// 通道2，方波模式，16位，二进制（扬声器）
0xB6 = 1011 0110
       ││││ ││││
       ││││ │││└─ BCD=0
       ││││ └┴─── Mode=011
       ││└┴────── RW=11
       └┴──────── SC=10（通道2）
```

### 7.4 PIT 频率计算详解

**核心概念：**

```
PIT 输入时钟频率（固定）：
  1.193182 MHz = 1,193,182 Hz
  
这个频率来自哪里？
  主板晶振频率 / 分频器
  = 14.31818 MHz / 12
  = 1.193182 MHz
  
为什么是这个奇怪的数字？
  历史原因：IBM PC 兼容性
  14.31818 MHz 是 NTSC 彩色副载波频率的 3 倍
```

**分频器计算公式：**

```
输出频率 = 输入频率 / 分频值

分频值 = 输入频率 / 输出频率
       = 1193182 / 目标频率

示例：
  目标频率 = 100 Hz（每秒100次中断）
  分频值 = 1193182 / 100 = 11931.82 ≈ 11932
  
  实际频率 = 1193182 / 11932 = 100.01 Hz
  误差 = 0.01 Hz（可以忽略）
```

**频率范围限制：**

```
最小分频值：1
  → 最高频率 = 1193182 Hz（约1.2 MHz）
  → 但实际上太快，CPU处理不过来
  
最大分频值：65535（16位最大值）
  → 最低频率 = 1193182 / 65535 ≈ 18.2 Hz
  
推荐范围：
  18 Hz ~ 1000 Hz
  
常用频率：
  - 18.2 Hz：DOS 默认（65536分频）
  - 100 Hz：Linux 早期默认
  - 1000 Hz：现代 Linux（CONFIG_HZ=1000）
```

**精度计算示例：**

```c
/* 计算不同目标频率的实际值 */

目标 100 Hz：
  分频值 = 1193182 / 100 = 11931.82 → 11932
  实际频率 = 1193182 / 11932 = 100.01 Hz
  误差 = 0.01%（优秀）
  
目标 1000 Hz：
  分频值 = 1193182 / 1000 = 1193.182 → 1193
  实际频率 = 1193182 / 1193 = 1000.15 Hz
  误差 = 0.015%（优秀）
  
目标 60 Hz（视频同步）：
  分频值 = 1193182 / 60 = 19886.37 → 19886
  实际频率 = 1193182 / 19886 = 60.00 Hz
  误差 = 0.001%（完美）
```

### 7.5 PIT 工作模式详解

**模式 3：方波发生器（我们使用的）**

```
时序图：
  
  初始值 = 4
  
  ┌───┐   ┌───┐   ┌───┐
  │   │   │   │   │   │
  │   └───┘   └───┘   └───
  
  计数：4 3 2 1 4 3 2 1 4 3...
  输出：高→低→高→低→高...
  
特点：
  - 输出 50% 占空比的方波
  - 自动重载计数值
  - 适合做时钟信号
  - IRQ 0 在下降沿触发
```

**其他模式对比：**

```
模式 0：中断结束时计数
  - 计数到0后输出高电平
  - 不自动重载
  - 用于单次定时
  
模式 2：分频器
  - 计数到1时输出脉冲
  - 自动重载
  - 输出不对称波形
  
模式 4：软件触发选通
  - 软件触发后开始计数
  - 计数到0输出脉冲
  - 不自动重载
```

### 7.6 PIT 编程实现（详细注释版）

基于 `kernel/drivers/timer.c`:

```c
/* ============================================
 * PIT 8253/8254 定时器驱动
 * ============================================ */

/* PIT 常量定义 */
#define PIT_FREQUENCY   1193182  // PIT输入频率（Hz）
#define PIT_COMMAND     0x43     // 命令寄存器端口
#define PIT_CHANNEL0    0x40     // 通道0数据端口
#define PIT_CHANNEL1    0x41     // 通道1数据端口
#define PIT_CHANNEL2    0x42     // 通道2数据端口

/* 命令字节位定义 */
#define PIT_CMD_CHANNEL0  0x00  // 选择通道0 (Bit 7-6 = 00)
#define PIT_CMD_CHANNEL1  0x40  // 选择通道1 (Bit 7-6 = 01)
#define PIT_CMD_CHANNEL2  0x80  // 选择通道2 (Bit 7-6 = 10)

#define PIT_CMD_LATCH     0x00  // 锁存计数值 (Bit 5-4 = 00)
#define PIT_CMD_LOW       0x10  // 只读/写低字节 (Bit 5-4 = 01)
#define PIT_CMD_HIGH      0x20  // 只读/写高字节 (Bit 5-4 = 10)
#define PIT_CMD_BOTH      0x30  // 先低后高16位 (Bit 5-4 = 11)

#define PIT_CMD_MODE0     0x00  // 模式0：中断结束时计数
#define PIT_CMD_MODE1     0x02  // 模式1：硬件可重触发单稳态
#define PIT_CMD_MODE2     0x04  // 模式2：分频器
#define PIT_CMD_MODE3     0x06  // 模式3：方波发生器 ← 推荐
#define PIT_CMD_MODE4     0x08  // 模式4：软件触发选通
#define PIT_CMD_MODE5     0x0A  // 模式5：硬件触发选通

#define PIT_CMD_BINARY    0x00  // 二进制计数（0-65535）
#define PIT_CMD_BCD       0x01  // BCD计数（0-9999）

/* 全局变量 */
static uint32_t timer_frequency = 0;  // 实际定时器频率
static volatile uint64_t system_ticks = 0;  // 系统tick计数

/**
 * 初始化PIT定时器
 * 
 * @param frequency 目标频率（Hz）
 * 
 * 频率范围：
 *   最小：18 Hz（分频值65535）
 *   最大：1193182 Hz（分频值1）
 *   推荐：100-1000 Hz
 */
void timer_init(uint32_t frequency)
{
    /* ===== 步骤1：验证频率范围 ===== */
    if (frequency < 18 || frequency > 1193182) {
        kprintf("[TIMER] Error: Invalid frequency %d Hz\n", frequency);
        kprintf("[TIMER] Valid range: 18 - 1193182 Hz\n");
        frequency = 100;  // 使用默认值
    }
    
    /* ===== 步骤2：计算分频值 ===== */
    /*
     * 公式：divisor = PIT_FREQUENCY / target_frequency
     * 
     * 示例：
     *   100 Hz → 1193182 / 100 = 11931.82 → 11932
     *   1000 Hz → 1193182 / 1000 = 1193.182 → 1193
     */
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    /* 限制在16位范围内（0-65535） */
    if (divisor > 65535) {
        divisor = 65535;
        kprintf("[TIMER] Warning: Divisor clamped to 65535\n");
    }
    if (divisor == 0) {
        divisor = 1;
        kprintf("[TIMER] Warning: Divisor set to minimum (1)\n");
    }
    
    /* ===== 步骤3：构造命令字 ===== */
    /*
     * 命令字 = 通道选择 | 读写模式 | 工作模式 | 计数格式
     * 
     * 我们使用：
     *   通道0（系统时钟）
     *   16位读写（先低后高）
     *   模式3（方波）
     *   二进制计数
     */
    uint8_t command = PIT_CMD_CHANNEL0 |  // 通道0
                      PIT_CMD_BOTH |      // 16位
                      PIT_CMD_MODE3 |     // 方波模式
                      PIT_CMD_BINARY;     // 二进制
    
    /* ===== 步骤4：发送命令到PIT ===== */
    outb(PIT_COMMAND, command);  // 0x43 ← 0x36
    
    /* ===== 步骤5：发送分频值 ===== */
    /*
     * 必须先发送低字节，再发送高字节
     * 这是由命令字中的 PIT_CMD_BOTH 决定的
     */
    outb(PIT_CHANNEL0, divisor & 0xFF);         // 低8位
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);  // 高8位
    
    /* ===== 步骤6：注册中断处理程序 ===== */
    irq_install_handler(0, timer_handler);
    irq_enable(0);  // 启用IRQ 0
    
    /* ===== 步骤7：计算并显示实际频率 ===== */
    timer_frequency = PIT_FREQUENCY / divisor;
    
    kprintf("[TIMER] PIT Timer initialized\n");
    kprintf("[TIMER] Requested: %d Hz, Actual: %d Hz (divisor: %d)\n", 
            frequency, timer_frequency, divisor);
    kprintf("[TIMER] Tick interval: %d.%03d ms\n", 
            1000 / timer_frequency,
            (1000 % timer_frequency) * 1000 / timer_frequency);
    
    /* 计算误差 */
    int32_t error = (int32_t)timer_frequency - (int32_t)frequency;
    if (error != 0) {
        kprintf("[TIMER] Frequency error: %s%d Hz (%.3f%%)\n",
                error > 0 ? "+" : "", error,
                (float)error * 100.0f / frequency);
    }
}
```

**实际代码的改进：**
- ✅ 详细的步骤注释（便于理解）
- ✅ 频率范围验证（防止无效值）
- ✅ 分频值边界检查（防止溢出）
- ✅ 使用宏定义（代码清晰）
- ✅ 输出实际频率和误差（调试友好）
- ✅ 毫秒级精度显示（更准确）

### 7.3 定时器中断处理（实际实现）

基于 `kernel/drivers/timer.c`:

```c
static volatile uint64_t system_ticks = 0;

static void timer_handler(struct interrupt_frame *frame)
{
    (void)frame;  // 未使用
    
    system_ticks++;  // 增加tick计数
    
    /* 调用调度器tick（如果调度器已初始化） */
    extern void scheduler_tick(void);
    scheduler_tick();  // ← 进程调度的心跳！
    
    // 注意：不需要手动发送 EOI
    // irq_handler() 会自动发送
}

/* 获取系统运行tick数 */
uint64_t timer_get_ticks(void)
{
    return system_ticks;
}

/* 获取系统运行秒数 */
uint32_t timer_get_seconds(void)
{
    return system_ticks / timer_frequency;
}

/* 获取系统运行毫秒数 */
uint64_t timer_get_milliseconds(void)
{
    return (system_ticks * 1000) / timer_frequency;
}

/* 延时函数（忙等待） */
void timer_wait(uint32_t ticks)
{
    uint64_t start = system_ticks;
    while (system_ticks < start + ticks) {
        asm volatile("hlt");  // 等待中断
    }
}
```

**实际代码的改进：**
- ✅ 集成调度器（多任务支持）
- ✅ 提供毫秒级时间（更精确）
- ✅ 延时函数（实用工具）
- ✅ 使用HLT节能（不是空转）

---

## 📖 第八课：实现键盘中断

基于 `kernel/drivers/keyboard.c`。

### 8.1 键盘中断处理

```c
void keyboard_init(void)
{
    // 注册 IRQ 1 处理函数
    irq_install_handler(1, keyboard_interrupt_handler);
    
    // 启用 IRQ 1
    irq_enable(1);
}

void keyboard_interrupt_handler(struct interrupt_frame *frame)
{
    // 读取扫描码
    uint8_t scancode = inb(0x60);
    
    // 处理扫描码
    if (!(scancode & 0x80)) {
        // 按键按下
        char ch = scancode_to_ascii[scancode];
        if (ch) {
            kprintf("%c", ch);
        }
    }
    
    // EOI 由 irq_handler() 自动发送
}
```

---

## 🎓 完整的初始化流程

### 从零开始搭建中断系统

**步骤1：初始化 IDT**

```c
void idt_init(void)
{
    // 清空 IDT
    memset(idt, 0, sizeof(idt));
    
    // 设置 IDT 指针
    idtp.limit = sizeof(idt) - 1;
    idtp.base = (uint32_t)&idt;
    
    // 安装所有 ISR（异常 0-31）
    idt_install_isrs();
    
    // 安装所有 IRQ（中断 32-47）
    idt_install_irqs();
    
    // 加载 IDT 到 CPU
    asm volatile("lidt %0" : : "m"(idtp));
}
```

**步骤2：初始化 IRQ 子系统（实际实现）**

基于 `kernel/arch/i386/irq.c`:

```c
void irq_init(void)
{
    /* 初始化PIC，重映射IRQ到INT 32-47 */
    pic_init(32, 40);
    
    /* 默认屏蔽所有IRQ（除了级联IRQ2）
     * 0xFFFF = 1111 1111 1111 1111（全部屏蔽）
     * ~(1 << 2) = 清除bit 2（启用IRQ2级联）
     */
    pic_set_mask(0xFFFF & ~(1 << IRQ_CASCADE));
    
    kprintf("[IRQ] IRQ subsystem initialized\n");
    kprintf("[IRQ] All IRQs masked except cascade (IRQ2)\n");
}
```

**为什么要屏蔽所有IRQ？**

```
默认屏蔽的原因：
  1. 避免未初始化的设备产生中断
  2. 防止未注册处理函数的IRQ触发
  3. 只有明确需要的IRQ才启用
  
启用流程：
  1. irq_install_handler() - 注册处理函数
  2. irq_enable() - 启用IRQ
  
这样更安全！
```

**步骤3：初始化具体设备（实际实现）**

基于 `kernel/main.c`:

```c
void kernel_main(void)
{
    /* 第1步：初始化GDT（全局描述符表） */
    gdt_init();
    
    /* 第2步：初始化IDT（中断描述符表） */
    idt_init();
    kprintf("[IDT] Interrupt Descriptor Table initialized\n");
    
    /* 第3步：初始化PIC（中断控制器） */
    // pic_init()已经在irq_init()中调用
    
    /* 第4步：初始化IRQ子系统 */
    irq_init();
    
    /* 第5步：初始化定时器（IRQ 0） */
    timer_init(100);  // 100 Hz = 每秒100次中断
    
    /* 第6步：初始化键盘（IRQ 1） */
    keyboard_init();
    
    /* 第7步：启用中断 */
    asm volatile("sti");  // Set Interrupt Flag
    
    kprintf("[INIT] Interrupt system ready!\n");
    kprintf("[INIT] Timer: 100 Hz, Keyboard: Enabled\n");
    
    /* 现在中断系统完全工作了！ */
    /* 每10ms会触发一次定时器中断 */
    /* 按键会立即响应（IRQ 1） */
}
```

**关键顺序：**

```
1. GDT 先初始化
   ↓
2. IDT 初始化（注册所有中断入口）
   ↓
3. IRQ 初始化（重映射PIC，屏蔽所有）
   ↓
4. 设备初始化（注册处理函数，启用对应IRQ）
   ↓
5. STI 启用中断
   
如果顺序错误：
  - IDT未初始化就STI → Triple Fault
  - 设备未初始化就启用IRQ → Unhandled中断
```

---

## 📊 中断系统总结

### 完整架构图

```
硬件事件（按键、定时器等）
    ↓
  IRQ 信号
    ↓
  PIC 芯片（8259A）
    ↓
  INTR 引脚 → CPU
    ↓
  查 IDT 表
    ↓
  汇编入口（保存寄存器）
    ↓
  IRQ 分发器（irq_handler）
    ↓
  设备处理函数（timer_handler, keyboard_handler）
    ↓
  发送 EOI
    ↓
  恢复寄存器
    ↓
  IRET 返回
```

### 核心组件

✅ **IDT** - 中断描述符表，256 个入口  
✅ **PIC** - 8259A 中断控制器，管理 16 个 IRQ  
✅ **IRQ** - 硬件中断请求，0-15  
✅ **EOI** - 中断结束信号，必须发送  
✅ **屏蔽位** - 启用/禁用特定 IRQ  

### 下一步

学习 **异常处理（第3讲）**，理解 CPU 异常与硬件中断的区别！

---

**中断系统 - 操作系统响应世界的方式！** ⚡


```
PIC 默认映射：
  IRQ 0-7  → 中断 8-15
  IRQ 8-15 → 中断 70-77

问题：
  中断 8-15 是 CPU 异常！
  IRQ 会和异常冲突！
  
解决：
  重映射到 32-47
  → 避开 CPU 保留的 0-31
```

**PIC 端口：**
```
Master PIC：
  0x20 → 命令端口
  0x21 → 数据端口
  
Slave PIC：
  0xA0 → 命令端口
  0xA1 → 数据端口
```

**重映射代码（实际实现）：**

```c
/* I/O等待宏（用于PIC操作之间的短暂延迟） */
#define IO_WAIT() outb(0x80, 0)

void pic_init(uint8_t offset1, uint8_t offset2)
{
    uint8_t mask1, mask2;
    
    /* 保存当前屏蔽位 */
    mask1 = inb(PIC_MASTER_DATA);  // 0x21
    mask2 = inb(PIC_SLAVE_DATA);   // 0xA1
    
    /* ICW1: 初始化命令
     * 0x11 = 0001 0001b
     * - bit 0: ICW4需要
     * - bit 4: 1=初始化
     */
    outb(PIC_MASTER_CMD, 0x11);
    IO_WAIT();  // ← 关键！PIC需要时间处理
    outb(PIC_SLAVE_CMD, 0x11);
    IO_WAIT();
    
    /* ICW2: 中断向量偏移 */
    outb(PIC_MASTER_DATA, offset1);  // 主PIC: 32
    IO_WAIT();
    outb(PIC_SLAVE_DATA, offset2);   // 从PIC: 40
    IO_WAIT();
    
    /* ICW3: 级联设置 */
    outb(PIC_MASTER_DATA, 0x04);  // 主: IRQ2连接从PIC
    IO_WAIT();
    outb(PIC_SLAVE_DATA, 0x02);   // 从: 连到主的IRQ2
    IO_WAIT();
    
    /* ICW4: 附加信息 */
    outb(PIC_MASTER_DATA, 0x01);  // 8086模式
    IO_WAIT();
    outb(PIC_SLAVE_DATA, 0x01);
    IO_WAIT();
    
    /* 恢复屏蔽位 */
    outb(PIC_MASTER_DATA, mask1);
    outb(PIC_SLAVE_DATA, mask2);
    
    kprintf("[PIC] 8259A PIC initialized\n");
}
```

**为什么需要 IO_WAIT()？**

```
PIC是老硬件，处理命令需要时间
如果连续发送太快，PIC来不及处理
  → 初始化失败
  → 中断不工作

IO_WAIT() = outb(0x80, 0)
  → 端口0x80是诊断端口，写入无副作用
  → 但执行需要几个CPU周期
  → 给PIC足够的反应时间
```

**每个命令的含义：**

```
ICW1 (0x11 = 0001 0001):
  Bit 4: 1  → 需要 ICW4
  Bit 0: 1  → 级联模式（有从片）
  
ICW2 (偏移)：
  32 → 基础中断号
  
ICW3 (级联设置)：
  Master: 0x04 = 0000 0100 → IRQ 2 有从片
  Slave:  0x02 → 连接到主片 IRQ 2
  
ICW4 (模式)：
  0x01 → 8086/8088 模式（不是 8080）
```

---

## 📖 第六课：实现第一个中断 - 定时器

### 6.1 什么是 PIT（定时器）？

**PIT = Programmable Interval Timer（可编程间隔定时器）**

**作用：**
- 每隔一定时间触发一次中断
- 用于：
  - 系统时钟（统计时间）
  - 进程调度（时间片）
  - 延时函数

**PIT 8253/8254 芯片：**
```
通道 0：连接到 IRQ 0（我们用这个）
通道 1：DRAM 刷新（系统用）
通道 2：扬声器（可以发声）
```

### 6.2 设置定时器频率

**PIT 输入频率：** 1.193182 MHz（固定）

**计算分频器：**
```
想要的频率 = 100 Hz（每秒 100 次中断）

分频器 = 1193182 / 100 = 11931.82 ≈ 11932

设置：
  outb(0x40, 11932 & 0xFF);         // 低字节
  outb(0x40, (11932 >> 8) & 0xFF);  // 高字节
```

**初始化代码：**

```c
void timer_init(uint32_t frequency)
{
    // 计算分频器
    uint32_t divisor = 1193182 / frequency;
    
    // 设置PIT
    outb(0x43, 0x36);              // 命令：通道0, 方波, 16位
    outb(0x40, divisor & 0xFF);    // 低字节
    outb(0x40, (divisor >> 8) & 0xFF); // 高字节
    
    // 注册中断处理程序
    idt_set_gate(32, (uint32_t)timer_interrupt_asm, 0x08, 0x8E);
}
```

**命令字节 0x36 的含义：**
```
0x36 = 00 11 011 0
       ││ ││ │││ │
       ││ ││ │││ └─ BCD 模式（0=二进制）
       ││ ││ └┴─── 模式（011=方波）
       ││ └┴────── 读写方式（11=先低后高）
       └┴───────── 通道（00=通道0）
```

### 6.3 定时器中断处理

```asm
; timer_interrupt_asm
[GLOBAL timer_interrupt_asm]
[EXTERN timer_handler]

timer_interrupt_asm:
    pusha
    
    call timer_handler    ; 调用 C 函数
    
    ; 发送 EOI
    mov al, 0x20
    out 0x20, al
    
    popa
    iret
```

```c
// C 处理函数
static volatile uint64_t ticks = 0;

void timer_handler(void)
{
    ticks++;  // 增加计数器
    
    // 每秒打印一次
    if (ticks % 100 == 0) {
        kprintf("Timer tick: %llu\n", ticks);
    }
}
```

**关键：EOI（End Of Interrupt）**

```c
outb(0x20, 0x20);  // 发送 EOI 给 PIC

为什么需要？
  PIC 收到 EOI 后才会发送下一个中断
  
  忘记发送 EOI：
    → 只触发一次中断
    → 后续中断被阻塞
    → 定时器停止工作！
```

---

## 📖 第七课：实现键盘中断

### 7.1 键盘工作原理

**按键过程：**
```
1. 你按下 'A' 键
   ↓
2. 键盘控制器生成扫描码
   ↓
3. 扫描码放入键盘缓冲区（端口 0x60）
   ↓
4. 键盘控制器触发 IRQ 1
   ↓
5. CPU 跳转到键盘中断处理程序
   ↓
6. 程序读取端口 0x60
   ↓
7. 处理扫描码
```

**扫描码是什么？**

```
扫描码 ≠ ASCII 码

示例：
  按下 'A' → 扫描码 0x1E
  松开 'A' → 扫描码 0x9E (0x1E + 0x80)
  
  按下 'Enter' → 扫描码 0x1C
  按下 'Shift' → 扫描码 0x2A
```

**扫描码到 ASCII 的转换表：**

```c
static char scancode_to_ascii[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  // Ctrl
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,  // Left Shift
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0,  // Right Shift
    '*',
    0,  // Alt
    ' ',  // Space
    // ... 更多
};
```

### 7.2 键盘中断处理

```c
void keyboard_handler(void)
{
    // 读取扫描码
    unsigned char scancode = inb(0x60);
    
    // 检查是按下还是松开
    if (scancode & 0x80) {
        // 松开（高位为1）
        scancode &= 0x7F;  // 去掉松开标志
        // 处理按键松开...
    } else {
        // 按下
        char ascii = scancode_to_ascii[scancode];
        
        if (ascii) {
            // 显示字符
            vga_putchar(ascii);
        }
    }
    
    // 发送 EOI
    outb(0x20, 0x20);
}
```

---

## 🧪 实战练习

### 练习1：计时器

```c
uint32_t get_seconds(void)
{
    return ticks / 100;  // 100 Hz → 秒
}

void show_uptime(void)
{
    uint32_t seconds = get_seconds();
    uint32_t hours = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs = seconds % 60;
    
    vga_printf("Uptime: %02d:%02d:%02d\n", hours, minutes, secs);
}
```

### 练习2：简单的输入

```c
char input_buffer[256];
int input_pos = 0;

void keyboard_handler(void)
{
    unsigned char scancode = inb(0x60);
    
    if (!(scancode & 0x80)) {
        char ch = scancode_to_ascii[scancode];
        
        if (ch == '\n') {
            // 回车：处理输入
            input_buffer[input_pos] = '\0';
            process_command(input_buffer);
            input_pos = 0;
        } else if (ch == '\b') {
            // 退格
            if (input_pos > 0) {
                input_pos--;
                vga_putchar('\b');
            }
        } else if (ch) {
            // 普通字符
            input_buffer[input_pos++] = ch;
            vga_putchar(ch);
        }
    }
    
    outb(0x20, 0x20);
}
```

---

## 📊 总结

### 核心概念回顾

✅ **中断** - CPU 暂停当前工作，处理紧急事件  
✅ **IDT** - 中断描述符表，256个处理程序地址  
✅ **PIC** - 中断控制器，管理硬件中断  
✅ **IRQ** - 硬件中断请求  
✅ **EOI** - 中断结束信号  

### 中断处理流程

```
1. 硬件事件（按键、定时器等）
   ↓
2. PIC 发送中断信号给 CPU
   ↓
3. CPU 查 IDT，找到处理程序
   ↓
4. CPU 保存状态（EFLAGS, CS, EIP）
   ↓
5. 跳转到处理程序
   ↓
6. 处理程序执行
   ↓
7. 发送 EOI 给 PIC
   ↓
8. IRET 返回原程序
```

### 实际运行效果

**启动时的输出（实际系统）：**

```
[GDT] Initializing Global Descriptor Table...
[GDT] GDT loaded successfully
[IDT] Initializing Interrupt Descriptor Table...
[IDT] IDT base: 0xc0124058, limit: 2048 bytes (256 entries)
[PIC] 8259A PIC initialized
[PIC] Master PIC: IRQ0-7  -> INT 32-39
[PIC] Slave PIC:  IRQ8-15 -> INT 40-47
[IRQ] IRQ subsystem initialized
[IRQ] All IRQs masked except cascade (IRQ2)
[IRQ] Installed handler for IRQ0: Timer (PIT)
[IRQ] Enabled IRQ0: Timer (PIT)
[TIMER] PIT Timer initialized
[TIMER] Requested: 100 Hz, Actual: 100 Hz (divisor: 11931)
[TIMER] Tick interval: 10.000 ms
[IRQ] Installed handler for IRQ1: Keyboard
[IRQ] Enabled IRQ1: Keyboard
[KEYBOARD] PS/2 Keyboard initialized

✓ 中断系统已就绪！
  - 定时器每10ms触发一次
  - 键盘随时响应输入
```

**查看IRQ统计：**

```c
irq_print_stats();

输出：
=== IRQ Statistics ===
IRQ  Name                        Count         Handler
─────────────────────────────────────────────────────────
 0   Timer (PIT)                   12547       Installed
 1   Keyboard                        342       Installed
 2   Cascade (PIC2)                    0       None
14   Primary ATA                      18       Installed
```

### 调试技巧

**技巧1：验证中断是否工作**

```c
void test_timer(void)
{
    uint64_t start = timer_get_ticks();
    kprintf("Waiting for 100 ticks...\n");
    
    while (timer_get_ticks() < start + 100);
    
    kprintf("Done! Timer works!\n");
}
```

**技巧2：临时禁用中断**

```c
void critical_section(void)
{
    cli();  // 关中断
    
    // 临界区代码（不会被中断打断）
    modify_global_data();
    
    sti();  // 开中断
}
```

**技巧3：保存和恢复中断状态**

```c
uint32_t flags;
asm volatile("pushf; pop %0; cli" : "=r"(flags));

// 临界区...

asm volatile("push %0; popf" :: "r"(flags));
```

---

### 下一步

学习 **第3讲：异常处理**，理解Page Fault、COW等高级内存管理！

---

**IDT 中断系统 - 操作系统的神经系统！** ⚡

