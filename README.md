# EduOS - 第2章：系统启动与引导

本目录包含 EduOS 第2章对应的代码实现。这是一个精简版本，只包含启动和引导相关的核心代码。

## 📚 章节内容

**第2章：系统启动与引导**

本章学习计算机从按下电源键到操作系统内核运行的完整过程：

1. BIOS启动流程
2. MBR引导扇区（Stage 1）
3. 多阶段引导加载器（Stage 2）
4. 实模式到保护模式切换
5. GDT（全局描述符表）配置
6. A20地址线启用
7. 内核加载与执行

## 📁 目录结构

```
eduos_test/
├── boot/                      # 引导程序
│   ├── stage1/               # Stage 1: MBR引导扇区（512字节）
│   │   └── boot_stage1.asm   # 主引导记录
│   └── stage2/               # Stage 2: 加载器
│       └── loader_complete.asm  # 完整加载器（切换保护模式）
│
├── kernel/                    # 内核代码
│   ├── arch/i386/            # 架构相关代码
│   │   └── entry.asm         # 内核入口（汇编）
│   ├── main.c                # 内核主函数
│   ├── vga.c                 # VGA文本模式显示
│   ├── serial.c              # 串口调试输出
│   └── kernel.c              # 内核基础功能
│
├── include/                   # 头文件
│   ├── kernel.h              # 内核基础定义
│   ├── vga.h                 # VGA接口
│   ├── serial.h              # 串口接口
│   ├── string.h              # 字符串函数
│   ├── types.h               # 类型定义
│   └── io.h                  # 端口操作
│
├── lib/                       # 库函数
│   └── string.c              # 字符串处理函数
│
├── linker.ld                  # 链接脚本
├── Makefile                   # 构建脚本
└── README.md                  # 本文件
```

## 🚀 快速开始

### 1. 编译

```bash
make
```

### 2. 运行

```bash
make run
```

### 3. 调试

```bash
# 终端1：启动QEMU等待GDB连接
make debug

# 终端2：启动GDB
gdb build/kernel.elf
(gdb) target remote localhost:1234
(gdb) break kernel_main
(gdb) continue
```

### 4. 清理

```bash
make clean
```

## 📖 代码说明

### 1. Stage 1 引导扇区 (`boot/stage1/boot_stage1.asm`)

- 大小：512字节（BIOS强制要求）
- 功能：
  - 被BIOS加载到 `0x7C00`
  - 从磁盘读取 Stage 2 到内存
  - 跳转到 Stage 2 执行
- 关键点：
  - 最后两字节必须是 `0x55AA`（引导签名）
  - 使用BIOS `INT 0x13` 读取磁盘

### 2. Stage 2 加载器 (`boot/stage2/loader_complete.asm`)

- 功能：
  - 配置GDT（全局描述符表）
  - 启用A20地址线
  - 切换到32位保护模式
  - 加载内核到 `0x100000`（1MB）
  - 跳转到内核入口
- 关键技术：
  - GDT设置（代码段和数据段）
  - A20启用（通过键盘控制器）
  - 设置CR0寄存器PE位
  - 长跳转刷新CS寄存器

### 3. 内核入口 (`kernel/arch/i386/entry.asm`)

- 功能：
  - 清空BSS段（未初始化数据）
  - 设置栈指针（ESP）
  - 调用C语言的 `kernel_main()`
- 桥梁作用：汇编 → C语言

### 4. 内核主函数 (`kernel/main.c`)

- 功能：
  - 初始化VGA文本显示
  - 初始化串口调试输出
  - 显示启动信息
  - 进入内核主循环
- 特点：简洁明了，只包含第2章必需功能

## 🎯 学习要点

### 1. 启动流程

```
BIOS → MBR (0x7C00) → Loader → 保护模式 → 内核 (0x100000)
```

### 2. 实模式 vs 保护模式

| 特性 | 实模式 | 保护模式 |
|------|--------|----------|
| CPU位数 | 16位 | 32位 |
| 最大内存 | 1MB | 4GB |
| 内存保护 | 无 | 有 |
| 特权级 | 无 | Ring 0-3 |

### 3. GDT结构

```
段描述符（8字节）：
- 段基址（32位）
- 段界限（20位）
- 访问权限（DPL）
- 段属性（粒度、32/16位）
```

### 4. A20地址线

- 历史遗留问题：兼容8086
- 默认关闭：内存回卷到1MB以下
- 必须启用：才能访问1MB以上内存

## 🔍 调试技巧

### 1. 查看编译产物

```bash
# 查看内核ELF信息
objdump -x build/kernel.elf

# 反汇编内核
objdump -d build/kernel.elf

# 查看符号表
nm build/kernel.elf

# 查看段信息
readelf -S build/kernel.elf
```

### 2. QEMU调试

```bash
# 查看CPU寄存器
info registers

# 查看内存
x/16xb 0x7c00  # 查看MBR
x/16xb 0x100000  # 查看内核

# 单步执行
si  # 单步（进入函数）
ni  # 单步（跳过函数）
```

### 3. 串口输出

程序的串口输出会显示在终端，用于调试：

```
=== EduOS Kernel v0.1.0 (Chapter 2) ===
Serial port initialized successfully.
Kernel main loop started. System halted.
```

## 🌟 本章成就

完成本章后，你将实现：

- [x] MBR引导扇区（512字节）
- [x] 多阶段引导加载器
- [x] 实模式到保护模式切换
- [x] GDT配置
- [x] A20地址线启用
- [x] 内核加载到1MB
- [x] VGA文本显示
- [x] 串口调试输出

## 📚 相关文档

- 课件：`/root/EduOS_folder/eduos/第2章-系统启动与引导.md`
- 参考资料：OSDev Wiki, Intel Software Developer Manual

## 🚧 下一章预告

**第3章：中断与异常处理**

- 中断描述符表（IDT）
- 异常处理（除零、缺页等）
- 硬件中断（IRQ）
- 8259A PIC编程
- 定时器中断

## ⚠️ 注意事项

1. **本版本是精简版**：只包含第2章相关代码
2. **没有中断系统**：内核运行后会一直HLT（正常现象）
3. **没有内存管理**：下一章才会实现
4. **适合学习**：代码简洁，注释详细

## 🤝 贡献

本项目是教学项目，欢迎：
- 报告bug
- 提出改进建议
- 完善文档
- 分享学习心得

## 📄 许可

Educational use only.

---

**EduOS - 从零开始写操作系统**
