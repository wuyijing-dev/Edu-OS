/*
 * elf.h - ELF (Executable and Linkable Format) 定义
 * 
 * 基于 ELF-32 规范
 */

#ifndef ELF_H
#define ELF_H

#include <types.h>

/* ELF 魔数 */
#define ELF_MAGIC  0x464C457F  /* 0x7F 'E' 'L' 'F' */

/* ELF 类型 */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;

/* ========== ELF 文件头 ========== */

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];  /* 魔数和其他信息 */
    Elf32_Half    e_type;              /* 文件类型 */
    Elf32_Half    e_machine;           /* 机器类型 */
    Elf32_Word    e_version;           /* 版本 */
    Elf32_Addr    e_entry;             /* 入口点地址 */
    Elf32_Off     e_phoff;             /* 程序头表偏移 */
    Elf32_Off     e_shoff;             /* 节头表偏移 */
    Elf32_Word    e_flags;             /* 处理器特定标志 */
    Elf32_Half    e_ehsize;            /* ELF 头大小 */
    Elf32_Half    e_phentsize;         /* 程序头大小 */
    Elf32_Half    e_phnum;             /* 程序头数量 */
    Elf32_Half    e_shentsize;         /* 节头大小 */
    Elf32_Half    e_shnum;             /* 节头数量 */
    Elf32_Half    e_shstrndx;          /* 节名字符串表索引 */
} Elf32_Ehdr;

/* e_ident 索引 */
#define EI_MAG0       0    /* 魔数字节 0 */
#define EI_MAG1       1    /* 魔数字节 1 */
#define EI_MAG2       2    /* 魔数字节 2 */
#define EI_MAG3       3    /* 魔数字节 3 */
#define EI_CLASS      4    /* 文件类 */
#define EI_DATA       5    /* 数据编码 */
#define EI_VERSION    6    /* 文件版本 */
#define EI_OSABI      7    /* OS/ABI 标识 */
#define EI_ABIVERSION 8    /* ABI 版本 */
#define EI_PAD        9    /* 填充开始 */

/* EI_CLASS */
#define ELFCLASSNONE  0    /* 无效 */
#define ELFCLASS32    1    /* 32 位 */
#define ELFCLASS64    2    /* 64 位 */

/* EI_DATA */
#define ELFDATANONE   0    /* 无效 */
#define ELFDATA2LSB   1    /* 小端 */
#define ELFDATA2MSB   2    /* 大端 */

/* e_type */
#define ET_NONE       0    /* 无类型 */
#define ET_REL        1    /* 可重定位文件 */
#define ET_EXEC       2    /* 可执行文件 */
#define ET_DYN        3    /* 共享目标文件 */
#define ET_CORE       4    /* 核心文件 */

/* e_machine */
#define EM_NONE       0    /* 无机器 */
#define EM_386        3    /* Intel 80386 */

/* ========== 程序头 ========== */

typedef struct {
    Elf32_Word p_type;      /* 段类型 */
    Elf32_Off  p_offset;    /* 段在文件中的偏移 */
    Elf32_Addr p_vaddr;     /* 段的虚拟地址 */
    Elf32_Addr p_paddr;     /* 段的物理地址 */
    Elf32_Word p_filesz;    /* 段在文件中的大小 */
    Elf32_Word p_memsz;     /* 段在内存中的大小 */
    Elf32_Word p_flags;     /* 段标志 */
    Elf32_Word p_align;     /* 对齐 */
} Elf32_Phdr;

/* p_type */
#define PT_NULL       0    /* 未使用 */
#define PT_LOAD       1    /* 可加载段 */
#define PT_DYNAMIC    2    /* 动态链接信息 */
#define PT_INTERP     3    /* 解释器路径 */
#define PT_NOTE       4    /* 辅助信息 */
#define PT_SHLIB      5    /* 保留 */
#define PT_PHDR       6    /* 程序头表 */

/* p_flags */
#define PF_X          0x1  /* 可执行 */
#define PF_W          0x2  /* 可写 */
#define PF_R          0x4  /* 可读 */

/* ========== 节头 ========== */

typedef struct {
    Elf32_Word sh_name;      /* 节名（字符串表索引） */
    Elf32_Word sh_type;      /* 节类型 */
    Elf32_Word sh_flags;     /* 节标志 */
    Elf32_Addr sh_addr;      /* 节的虚拟地址 */
    Elf32_Off  sh_offset;    /* 节在文件中的偏移 */
    Elf32_Word sh_size;      /* 节的大小 */
    Elf32_Word sh_link;      /* 链接到其他节 */
    Elf32_Word sh_info;      /* 附加信息 */
    Elf32_Word sh_addralign; /* 地址对齐 */
    Elf32_Word sh_entsize;   /* 固定大小条目的大小 */
} Elf32_Shdr;

/* sh_type */
#define SHT_NULL      0    /* 未使用 */
#define SHT_PROGBITS  1    /* 程序数据 */
#define SHT_SYMTAB    2    /* 符号表 */
#define SHT_STRTAB    3    /* 字符串表 */
#define SHT_RELA      4    /* 重定位表（带加数） */
#define SHT_HASH      5    /* 符号散列表 */
#define SHT_DYNAMIC   6    /* 动态链接信息 */
#define SHT_NOTE      7    /* 注释 */
#define SHT_NOBITS    8    /* BSS */
#define SHT_REL       9    /* 重定位表（不带加数） */

/* sh_flags */
#define SHF_WRITE     0x1  /* 可写 */
#define SHF_ALLOC     0x2  /* 占用内存 */
#define SHF_EXECINSTR 0x4  /* 可执行 */

#endif // ELF_H

