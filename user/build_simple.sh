#!/bin/bash
#
# build_simple.sh - 编译简单的用户程序（不使用libc）
#

set -e

echo "=== Building Simple User Program (No libc) ==="

PROGRAM=$1
if [ -z "$PROGRAM" ]; then
    echo "Usage: $0 <program.c>"
    echo "Example: $0 test_loop.c"
    exit 1
fi

BASENAME=$(basename "$PROGRAM" .c)
OUTPUT="${BASENAME}.elf"

echo "Program: $PROGRAM"
echo "Output:  $OUTPUT"
echo ""

# 编译选项
CC="gcc"
CFLAGS="-m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -fno-pic -fno-pie -O0 -g"

# 链接选项
LD="ld"
LDFLAGS="-m elf_i386 -nostdlib -Ttext=0x08000000 -e _start"

# 1. 编译C文件到目标文件
echo "[1/3] Compiling $PROGRAM..."
$CC $CFLAGS -c "$PROGRAM" -o "${BASENAME}.o"

# 2. 链接生成ELF文件
echo "[2/3] Linking to $OUTPUT..."
$LD $LDFLAGS "${BASENAME}.o" -o "$OUTPUT"

# 3. 显示ELF信息
echo "[3/3] ELF information:"
echo ""
echo "=== ELF Header ==="
readelf -h "$OUTPUT" | grep -E "Entry point|Class|Type"
echo ""
echo "=== Program Headers ==="
readelf -l "$OUTPUT" | grep -A 5 "LOAD"
echo ""

# 计算文件大小
SIZE=$(stat -c%s "$OUTPUT")
echo "✅ Build successful!"
echo "   Output: $OUTPUT ($SIZE bytes)"
echo ""
echo "To test in EduOS:"
echo "  1. Copy to disk image: cp $OUTPUT ../build/fat32_test.img"
echo "  2. Run EduOS: make run"
