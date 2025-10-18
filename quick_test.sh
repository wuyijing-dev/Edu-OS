#!/bin/bash
# 快速编译和测试脚本

echo "=========================================="
echo "   EduOS Production Build & Test"
echo "=========================================="
echo ""

# 步骤1：编译用户程序
echo "[1/4] Compiling user program..."
make -C user clean >/dev/null 2>&1
if make -C user; then
    echo "      ✅ User program compiled"
else
    echo "      ❌ User program compilation failed"
    exit 1
fi
echo ""

# 步骤2：更新FAT32磁盘
echo "[2/4] Updating FAT32 disk..."
if ./tools/create_fat32_disk.sh 2>&1 | grep -q "FAT32"; then
    echo "      ✅ FAT32 disk updated"
else
    echo "      ❌ FAT32 disk creation failed"
    exit 1
fi
echo ""

# 步骤3：编译内核
echo "[3/4] Compiling kernel..."
if make -j4 2>&1 | tail -5; then
    echo "      ✅ Kernel compiled"
else
    echo "      ❌ Kernel compilation failed"
    exit 1
fi
echo ""

# 步骤4：运行
echo "[4/4] Starting EduOS..."
echo "      Press Ctrl+C to stop"
echo ""
echo "=========================================="
echo ""

make run
