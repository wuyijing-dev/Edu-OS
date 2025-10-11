# EduOS 大内核加载器技术文档

## 概述

EduOS 现在支持 **Linux 风格的大内核加载**，可以加载 2MB-4MB 甚至更大的内核映像。这个实现使用了先进的 **Unreal 模式** 技术，在 16 位实模式代码中访问完整的 4GB 物理地址空间。

## 🚀 核心技术特性

### 1. Unreal 模式（Big Real Mode）

**什么是 Unreal 模式？**
- 在 16 位实模式下运行的特殊模式
- 可以访问完整的 4GB 物理内存空间
- 使用 32 位段寄存器但保持 16 位代码执行
- Linux、GRUB 等引导加载器的标准技术

**工作原理：**
```
1. 启用 A20 地址线
2. 加载 GDT（全局描述符表）
3. 临时切换到保护模式
4. 加载 32 位数据段（4GB 限制）到 DS/ES
5. 切换回实模式
6. 现在可以使用 32 位地址！
```

**代码实现：**
```asm
enter_unreal_mode:
    cli                         ; 关闭中断
    lgdt [gdt_descriptor]       ; 加载 GDT
    
    ; 临时进入保护模式
    mov eax, cr0
    or al, 1                    ; 设置 PE 位
    mov cr0, eax
    
    ; 加载 4GB 数据段
    mov bx, 0x10
    mov ds, bx
    mov es, bx
    
    ; 返回实模式
    and al, 0xFE
    mov cr0, eax
    
    ; 段寄存器值为 0，但段限制仍为 4GB！
    xor ax, ax
    mov ds, ax
    mov es, ax
    
    sti
    ret
```

### 2. 直接加载到 1MB 物理地址

**传统方法的问题：**
- 实模式最多访问 1MB 内存（640KB 可用）
- 大内核需要先加载到低端内存，再复制到 1MB
- 双重复制降低性能，浪费内存

**Unreal 模式解决方案：**
```
┌─────────────────────────────────────────────────┐
│ BIOS 读取 → 临时缓冲区（128KB）                │
│        ↓                                        │
│ Unreal 模式 32 位内存复制                       │
│        ↓                                        │
│ 直接写入 1MB+ 物理地址                          │
└─────────────────────────────────────────────────┘
```

**关键代码：**
```asm
; 使用 32 位地址复制
mov esi, 0x20000            ; 源：临时缓冲区
mov edi, 0x100000           ; 目标：1MB 物理地址
mov ecx, sectors * 128      ; 大小（DWORD）
a32 rep movsd               ; 32 位地址复制！
```

### 3. 智能大小检测

**ELF 头检测：**
```asm
detect_kernel_size:
    ; 读取第一个扇区
    mov word [dap_segment], 0x7E0
    int 0x13
    
    ; 检查 ELF 魔数
    mov eax, [0x7E00]
    cmp eax, 0x464C457F         ; 0x7F 'E' 'L' 'F'
    jne .not_elf
    
    ; 解析 ELF 头获取大小
    ; ...
```

**支持的内核大小：**
- 最小：512 字节（1 扇区）
- 默认：2MB（4096 扇区）
- 最大：4MB（8192 扇区）
- 理论上限：8MB+（受磁盘镜像大小限制）

## 📊 性能对比

| 特性 | 旧版加载器 | 新版大内核加载器 |
|------|-----------|------------------|
| 最大内核 | 100KB | 4MB+ |
| 加载模式 | CHS/LBA | LBA（必需） |
| 内存访问 | 实模式（1MB） | Unreal 模式（4GB） |
| 复制次数 | 2 次 | 1 次 |
| 加载速度 | 慢（双重复制） | 快（直接加载） |
| Linux 兼容性 | 否 | 是 |

## 🛠️ 使用方法

### 编译和运行

```bash
# 清理并重新编译
make clean
make

# 运行（内核会自动加载到 1MB）
make run

# 调试模式
make debug
```

### 查看加载过程

启动时会看到：
```
=== EduOS Big Kernel Loader (Linux-style) ===
[1/6] Enabling A20 line... OK
[2/6] Entering Unreal mode (4GB access)... OK
[3/6] Loading big kernel to 1MB...
  Using INT 13h Extensions (LBA mode)
  Detecting kernel size...
  Using default: 2MB (4096 sectors)
  Loading (Unreal mode -> 1MB physical):
    Sector 0000/1000....
    Sector 007F/1000....
    ... (继续加载)
[4/6] Detecting physical memory... OK
[5/6] Loading GDT... OK
[6/6] Entering protected mode...
```

## 🔧 配置选项

### loader_complete.asm 中的常量

```asm
; 内核起始扇区
KERNEL_START_SECTOR equ 5       ; 默认第 5 扇区

; 最大支持扇区数（可调整）
KERNEL_MAX_SECTORS equ 8192     ; 4MB（8192 * 512 字节）

; 加载目标地址
KERNEL_LOAD_ADDR equ 0x100000   ; 1MB 物理地址

; 临时缓冲区（用于 BIOS 读取）
TEMP_BUFFER equ 0x20000         ; 128KB 处
```

### 调整最大内核大小

如果需要支持更大的内核：

1. **修改 loader_complete.asm：**
```asm
KERNEL_MAX_SECTORS equ 16384    ; 支持 8MB
```

2. **修改 Makefile 增加磁盘镜像大小：**
```makefile
# 创建 16MB 磁盘镜像
dd if=/dev/zero of=$@ bs=1M count=16 2>/dev/null
```

3. **重新编译：**
```bash
make clean && make
```

## 🧪 技术细节

### 为什么使用 Unreal 模式而不是直接进入保护模式？

1. **BIOS 兼容性：** BIOS 中断（INT 13h）只在实模式下工作
2. **简化设计：** 不需要在保护模式下实现磁盘驱动
3. **行业标准：** Linux、GRUB 等都使用这种方法
4. **最佳性能：** 既能用 BIOS，又能访问高端内存

### 32 位地址前缀（a32）

```asm
a32 rep movsd               ; 使用 32 位地址
```

**作用：**
- 告诉 CPU 使用 ESI/EDI 而不是 SI/DI
- 在 16 位代码模式下启用 32 位寻址
- 这是 Unreal 模式的关键技术

### LBA 模式的优势

**CHS（柱面-磁头-扇区）：**
- 老旧的寻址方式
- 最大支持 8GB
- 计算复杂

**LBA（逻辑块地址）：**
- 现代寻址方式
- 支持 TB 级磁盘
- 简单线性地址
- 使用 INT 13h 扩展（AH=0x42）

## 🐛 故障排除

### 错误：加载失败

**可能原因：**
1. BIOS 不支持 INT 13h 扩展
2. 磁盘镜像损坏
3. 内核超过 4MB

**解决方法：**
```bash
# 检查磁盘镜像
ls -lh build/eduos.img

# 检查内核大小
ls -lh build/kernel.bin

# 在 QEMU 中添加调试
qemu-system-i386 -drive format=raw,file=build/eduos.img,if=floppy \
    -serial stdio -m 128M -d int,cpu_reset
```

### 错误：内核不启动

**检查项：**
1. 确保 A20 启用成功
2. 验证内核在 1MB 处正确加载
3. 检查页表映射（虚拟地址 0xC0100000 → 物理地址 0x100000）

## 📚 参考资料

### 相关技术文档

- **Unreal Mode:** OSDev Wiki - Unreal Mode
- **INT 13h Extensions:** BIOS Enhanced Disk Drive Specification
- **Linux Boot Protocol:** Documentation/x86/boot.txt
- **GRUB:** GNU GRUB Manual

### 代码来源

这个实现参考了：
- Linux 2.6 早期引导代码
- GRUB Legacy 的 Stage 1.5
- OSDev 社区的最佳实践

## 🎓 学习要点

通过这个大内核加载器，你将学到：

1. **实模式的限制和突破方法**
2. **Unreal 模式的工作原理**
3. **BIOS 磁盘服务的高级用法**
4. **32 位寻址技术**
5. **ELF 格式基础**
6. **Linux 引导协议**
7. **现代操作系统的引导流程**

## 🚀 未来改进

可以进一步增强：

1. **完整的 ELF 解析器** - 读取程序头表，只加载需要的段
2. **压缩内核支持** - 加载 gzip 压缩的内核，在内存中解压
3. **Multiboot 支持** - 兼容 GRUB 的 Multiboot 规范
4. **UEFI 支持** - 使用 UEFI 引导服务加载内核
5. **错误恢复** - 更强大的错误处理和重试机制

## 总结

这个大内核加载器展示了真实操作系统引导加载器的核心技术。通过 Unreal 模式，我们在保持实模式 BIOS 兼容性的同时，获得了 32 位地址空间的全部能力。这正是 Linux、Windows 等操作系统在引导早期阶段使用的技术！

现在你的 EduOS 已经具备了加载现代大内核的能力！🎉

