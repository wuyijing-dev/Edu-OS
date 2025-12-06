#!/bin/bash
# ===========================================================================
# create_iso.sh - Create GRUB2 bootable ISO image for EduOS
# ===========================================================================

set -e

BUILD_DIR="${1:-.build}"
KERNEL_ELF="${2:-build/kernel.elf}"
ISO_OUTPUT="${3:-build/eduos.iso}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${YELLOW}=========================================${NC}"
echo -e "${YELLOW} Creating GRUB2 Bootable ISO Image${NC}"
echo -e "${YELLOW}=========================================${NC}"

# Check if kernel ELF exists
if [ ! -f "$KERNEL_ELF" ]; then
    echo -e "${RED}Error: Kernel ELF not found at $KERNEL_ELF${NC}"
    exit 1
fi

# Create temporary ISO directory structure
ISO_TEMP=$(mktemp -d)
trap "rm -rf $ISO_TEMP" EXIT

echo -e "${CYAN}Creating ISO directory structure...${NC}"
mkdir -p "$ISO_TEMP/boot/grub"
mkdir -p "$ISO_TEMP/boot/kernel"

# Copy kernel ELF
echo -e "${CYAN}Copying kernel ELF...${NC}"
cp "$KERNEL_ELF" "$ISO_TEMP/boot/kernel.elf"

# Copy GRUB2 configuration
echo -e "${CYAN}Copying GRUB2 configuration...${NC}"
if [ -f "boot/grub.cfg" ]; then
    cp boot/grub.cfg "$ISO_TEMP/boot/grub/grub.cfg"
else
    echo -e "${RED}Error: boot/grub.cfg not found${NC}"
    exit 1
fi

# Create ISO image using grub-mkrescue if available
if command -v grub-mkrescue &> /dev/null; then
    echo -e "${CYAN}Creating ISO with grub-mkrescue...${NC}"
    grub-mkrescue -o "$ISO_OUTPUT" "$ISO_TEMP" 2>/dev/null || {
        echo -e "${YELLOW}grub-mkrescue failed, trying xorriso...${NC}"
        
        if command -v xorriso &> /dev/null; then
            xorriso -as mkisofs -R -J -b boot/grub/i386-pc/eltorito.img \
                -no-emul-boot -boot-load-size 4 -boot-info-table \
                -o "$ISO_OUTPUT" "$ISO_TEMP" 2>/dev/null || {
                echo -e "${RED}Failed to create ISO with xorriso${NC}"
                exit 1
            }
        else
            echo -e "${RED}Neither grub-mkrescue nor xorriso found${NC}"
            exit 1
        fi
    }
else
    echo -e "${YELLOW}grub-mkrescue not found, trying mkisofs...${NC}"
    
    if command -v mkisofs &> /dev/null; then
        mkisofs -R -b boot/grub/stage2_eltorito -no-emul-boot \
            -boot-load-size 4 -boot-info-table -o "$ISO_OUTPUT" "$ISO_TEMP" 2>/dev/null || {
            echo -e "${RED}Failed to create ISO with mkisofs${NC}"
            exit 1
        }
    else
        echo -e "${RED}No ISO creation tool found (grub-mkrescue, xorriso, or mkisofs)${NC}"
        exit 1
    fi
fi

# Verify ISO was created
if [ -f "$ISO_OUTPUT" ]; then
    ISO_SIZE=$(stat -c%s "$ISO_OUTPUT")
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}✓ ISO image created successfully${NC}"
    echo -e "${CYAN}Output: $ISO_OUTPUT${NC}"
    echo -e "${CYAN}Size: $ISO_SIZE bytes${NC}"
    echo -e "${GREEN}=========================================${NC}"
else
    echo -e "${RED}Error: Failed to create ISO image${NC}"
    exit 1
fi
