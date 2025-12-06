/*
 * multiboot.h - Multiboot2 Boot Information Header
 * 
 * This file defines the structures used by GRUB2 to pass boot information
 * to the kernel via the Multiboot2 protocol.
 */

#ifndef _MULTIBOOT_H
#define _MULTIBOOT_H

#include <types.h>

/* Multiboot2 magic number passed in EAX by bootloader */
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

/* Multiboot2 tag types */
#define MULTIBOOT_TAG_TYPE_END                  0
#define MULTIBOOT_TAG_TYPE_CMDLINE              1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME     2
#define MULTIBOOT_TAG_TYPE_MODULE               3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO        4
#define MULTIBOOT_TAG_TYPE_BOOTDEV              5
#define MULTIBOOT_TAG_TYPE_MMAP                 6
#define MULTIBOOT_TAG_TYPE_VBE                  7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER          8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS         9
#define MULTIBOOT_TAG_TYPE_APM                  10
#define MULTIBOOT_TAG_TYPE_EFI32                11
#define MULTIBOOT_TAG_TYPE_EFI64                12
#define MULTIBOOT_TAG_TYPE_SMBIOS               13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD             14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW             15
#define MULTIBOOT_TAG_TYPE_NETWORK              16
#define MULTIBOOT_TAG_TYPE_RSDP_V1              17
#define MULTIBOOT_TAG_TYPE_RSDP_V2              18
#define MULTIBOOT_TAG_TYPE_RELOCATABLE          19

/* Multiboot2 header structure */
struct multiboot_header {
    uint32_t magic;
    uint32_t architecture;
    uint32_t header_length;
    uint32_t checksum;
};

/* Multiboot2 tag header */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

/* Multiboot2 boot information structure */
struct multiboot_info {
    uint32_t total_size;
    uint32_t reserved;
    /* Tags follow immediately after this structure */
};

/* Memory map entry */
struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

/* Memory map tag */
struct multiboot_tag_mmap {
    struct multiboot_tag tag;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
};

/* Basic memory info tag */
struct multiboot_tag_basic_meminfo {
    struct multiboot_tag tag;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

/* Boot loader name tag */
struct multiboot_tag_string {
    struct multiboot_tag tag;
    char string[0];
};

/* Framebuffer info tag */
struct multiboot_tag_framebuffer {
    struct multiboot_tag tag;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t reserved;
};

/* Function to iterate through Multiboot2 tags */
static inline struct multiboot_tag *multiboot_tag_first(struct multiboot_info *mbi)
{
    return (struct multiboot_tag *)(mbi + 1);
}

static inline struct multiboot_tag *multiboot_tag_next(struct multiboot_tag *tag)
{
    return (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
}

#endif /* _MULTIBOOT_H */
