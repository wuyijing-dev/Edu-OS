#!/bin/bash
#
# rebuild_and_test.sh - 完整重建并测试系统
#

set -e

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         EduOS Complete Rebuild and Test Script           ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# 1. 编译 libc
echo "[1/5] Building libc..."
cd userlib
make clean && make
echo "✓ libc.a built"
echo ""

# 2. 编译用户程序
echo "[2/5] Building user programs..."
cd ../user
./build_with_libc.sh test_libc.c
echo "✓ test_libc.elf built"
echo ""

# 3. 创建 FAT32 磁盘
echo "[3/5] Creating FAT32 disk..."
cd ../tools
echo "all" | ./create_fat32_disk.sh
echo "✓ FAT32 disk created"
echo ""

# 4. 编译内核
echo "[4/5] Building kernel..."
cd ..
make clean
make
echo "✓ Kernel built"
echo ""

# 5. 运行
echo "[5/5] Running EduOS..."
echo ""
echo "Press Ctrl+C to stop"
echo "═══════════════════════════════════════════════════════════"
echo ""
make run
