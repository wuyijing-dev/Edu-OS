#!/bin/bash
# EduOS 大内核加载器测试脚本

echo "=========================================="
echo " EduOS 大内核加载器测试"
echo "=========================================="
echo ""

# 清理旧文件
echo "[1/3] 清理旧构建文件..."
make clean

# 编译
echo ""
echo "[2/3] 编译 EduOS（带大内核加载器）..."
make

# 检查编译结果
if [ ! -f "build/eduos.img" ]; then
    echo "ERROR: 编译失败！"
    exit 1
fi

# 显示内核大小
echo ""
echo "[3/3] 检查内核大小..."
kernel_size=$(stat -c%s build/kernel.bin)
kernel_kb=$((kernel_size / 1024))
kernel_mb=$((kernel_size / 1048576))

echo "  内核大小: $kernel_size 字节 ($kernel_kb KB)"

if [ $kernel_size -gt 102400 ]; then
    echo "  ✓ 内核超过 100KB，需要大内核加载器"
else
    echo "  ℹ 内核小于 100KB，但仍使用大内核加载器"
fi

if [ $kernel_size -gt 4194304 ]; then
    echo "  ⚠ 警告：内核超过 4MB，可能需要调整加载器"
fi

echo ""
echo "=========================================="
echo " 编译完成！现在运行测试..."
echo "=========================================="
echo ""
echo "提示："
echo "  - 使用 IDE 硬盘模式（不是软盘）"
echo "  - Unreal 模式直接加载到 1MB"
echo "  - 支持最大 4MB 内核"
echo ""
echo "按 Ctrl+C 退出 QEMU"
echo ""
sleep 2

# 运行
make run

