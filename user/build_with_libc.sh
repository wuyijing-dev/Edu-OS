#!/bin/bash
#
# build_with_libc.sh - 使用 libc.a 编译用户程序
#

set -e

echo "=== Building User Program with libc ==="

PROGRAM=$1
if [ -z "$PROGRAM" ]; then
    echo "Usage: $0 <program.c>"
    exit 1
fi

BASENAME=$(basename "$PROGRAM" .c)
LIBC_DIR="../userlib"
OUTPUT="${BASENAME}.elf"

echo "Program: $PROGRAM"
echo "Output:  $OUTPUT"
echo ""

# 1. 编译程序到目标文件
echo "[1/3] Compiling $PROGRAM..."
gcc -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
    -fno-stack-protector -O2 -Wall \
    -I${LIBC_DIR} \
    -c "$PROGRAM" -o "${BASENAME}.o"

# 2. 链接 libc.a
echo "[2/3] Linking with libc.a..."
ld -m elf_i386 -nostdlib \
    -Ttext=0x08000000 \
    "${BASENAME}.o" \
    "${LIBC_DIR}/libc.a" \
    -o "$OUTPUT"

# 3. 查看信息
echo "[3/3] Build complete!"
echo ""
echo "ELF file: $OUTPUT"
ls -lh "$OUTPUT"
echo ""
echo "Entry point:"
readelf -h "$OUTPUT" | grep "Entry point"
echo ""
echo "Sections:"
readelf -S "$OUTPUT" | grep -E "\.text|\.data|\.bss"

echo ""
echo "✓ Done! You can now run: $OUTPUT"
