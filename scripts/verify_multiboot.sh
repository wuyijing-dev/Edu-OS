#!/bin/bash
# Verify Multiboot header in kernel ELF

KERNEL_ELF="${1:-build/kernel.elf}"

if [ ! -f "$KERNEL_ELF" ]; then
    echo "Error: Kernel ELF not found: $KERNEL_ELF"
    exit 1
fi

echo "Checking Multiboot header in $KERNEL_ELF..."
echo ""

# Check for .multiboot section
echo "1. Checking for .multiboot section:"
objdump -h "$KERNEL_ELF" | grep -E "\.multiboot|\.text"
echo ""

# Extract first 32 bytes of .text section (should contain Multiboot header)
echo "2. Checking Multiboot magic number:"
objdump -s -j .text "$KERNEL_ELF" | head -20
echo ""

# Look for Multiboot1 magic (0x1BADB002)
echo "3. Searching for Multiboot1 magic (0x1BADB002):"
hexdump -C "$KERNEL_ELF" | grep -i "02 b0 ad 1b" | head -5
echo ""

# Check with readelf
echo "4. ELF sections:"
readelf -S "$KERNEL_ELF" | grep -E "\.multiboot|\.text|\.data|\.bss"
echo ""

echo "Done!"
