# GRUB2启动调试指南

## 问题描述
- GRUB2菜单正常显示
- 选择"EduOS"后显示"could not read boot disk"
- 然后返回GRUB2菜单

## 根本原因分析

### 已排除的问题：
1. ✓ Multiboot头正确（0x1BADB002在正确位置）
2. ✓ ELF文件格式正确
3. ✓ ISO创建成功（12MB+）
4. ✓ kernel.elf在ISO中

### 可能的问题：

#### 1. **ISO9660路径问题**
GRUB2在ISO中可能找不到`/boot/kernel.elf`

**调试方法：**
```bash
# 检查ISO中的文件结构
isoinfo -R -f -i build/eduos.iso | grep -i kernel

# 应该看到类似：
# /boot/kernel.elf
```

#### 2. **grub.cfg路径语法**
GRUB2的路径语法可能需要调整

**尝试的配置：**
```
# 方案1：相对路径
multiboot /boot/kernel.elf

# 方案2：CD设备
multiboot (cd)/boot/kernel.elf

# 方案3：完整路径
multiboot (cd0)/boot/kernel.elf

# 方案4：使用search命令
search --label GRUB --set root
multiboot /boot/kernel.elf
```

#### 3. **GRUB2模块问题**
可能需要加载额外的GRUB2模块

**尝试的配置：**
```
insmod part_msdos
insmod iso9660
insmod ext2
multiboot /boot/kernel.elf
```

## 调试步骤

### 步骤1：进入GRUB2命令行
1. 在GRUB2菜单中按`c`进入命令行
2. 输入以下命令探索：

```grub
# 列出可用的设备
ls

# 列出CD中的文件
ls (cd)/
ls (cd)/boot/

# 尝试加载内核
multiboot (cd)/boot/kernel.elf
boot
```

### 步骤2：检查ISO内容
```bash
# 使用isoinfo检查
isoinfo -R -f -i build/eduos.iso | head -50

# 使用7z检查
7z l build/eduos.iso | grep -i kernel

# 提取内核文件验证
isoinfo -R -x /boot/kernel.elf -i build/eduos.iso > /tmp/kernel_from_iso.elf
file /tmp/kernel_from_iso.elf
```

### 步骤3：验证Multiboot头
```bash
# 检查原始kernel.elf
hexdump -C build/kernel.elf | grep -A 2 "02 b0 ad 1b"

# 检查ISO中的kernel.elf
isoinfo -R -x /boot/kernel.elf -i build/eduos.iso | hexdump -C | grep -A 2 "02 b0 ad 1b"
```

## 可能的解决方案

### 方案A：使用GRUB2 Rescue模式
创建一个最小的grub.cfg，只包含基本的启动代码：

```
set default=0
set timeout=0

menuentry "EduOS Debug" {
    insmod part_msdos
    insmod iso9660
    set root=(cd)
    multiboot /boot/kernel.elf
    boot
}
```

### 方案B：使用multiboot2而不是multiboot1
修改entry.asm使用Multiboot2头，并在grub.cfg中使用`multiboot2`命令。

### 方案C：回到混合方案
保留原始的自定义bootloader，但让它支持从GRUB2启动的内核。

### 方案D：使用QEMU的-kernel选项
跳过GRUB2，直接用QEMU加载内核进行测试：

```bash
qemu-system-i386 -kernel build/kernel.elf -serial stdio -m 128M
```

## 快速测试命令

```bash
# 构建ISO
make run-iso &
sleep 2

# 在GRUB2菜单中，按'c'进入命令行，然后输入：
# ls (cd)/boot/
# multiboot (cd)/boot/kernel.elf
# boot

# 如果失败，按Ctrl+C退出QEMU
pkill -f qemu
```

## 参考资源

- [GRUB2手册 - Multiboot](https://www.gnu.org/software/grub/manual/grub/grub.html#Multiboot)
- [Multiboot规范](https://www.gnu.org/software/grub/manual/multiboot/)
- [GRUB2命令参考](https://www.gnu.org/software/grub/manual/grub/grub.html#Command-line-and-menu-entry-commands)
