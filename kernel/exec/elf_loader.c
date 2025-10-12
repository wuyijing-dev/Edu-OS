/*
 * elf_loader.c - ELF 程序加载器
 */

#include <elf.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <process/process.h>
#include <string.h>

/*
 * 验证 ELF 头
 */
static bool elf_validate(Elf32_Ehdr *ehdr)
{
    /* 检查魔数 */
    if (ehdr->e_ident[EI_MAG0] != 0x7F ||
        ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' ||
        ehdr->e_ident[EI_MAG3] != 'F') {
        kprintf("[ELF] Invalid magic number\n");
        return false;
    }
    
    /* 检查 32 位 */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        kprintf("[ELF] Not 32-bit ELF\n");
        return false;
    }
    
    /* 检查小端 */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        kprintf("[ELF] Not little-endian\n");
        return false;
    }
    
    /* 检查可执行文件 */
    if (ehdr->e_type != ET_EXEC) {
        kprintf("[ELF] Not executable (type=%d)\n", ehdr->e_type);
        return false;
    }
    
    /* 检查 i386 */
    if (ehdr->e_machine != EM_386) {
        kprintf("[ELF] Not i386 (machine=%d)\n", ehdr->e_machine);
        return false;
    }
    
    return true;
}

/*
 * 加载 ELF 程序
 * 
 * @param path: ELF 文件路径
 * @param entry: 输出入口地址
 * @return: 0 成功，<0 失败
 */
int elf_load(const char *path, uint32_t *entry)
{
    kprintf("[ELF] Loading: %s\n", path);
    
    /* 打开文件 */
    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[ELF] Failed to open file: %d\n", fd);
        return fd;
    }
    
    /* 读取 ELF 头 */
    Elf32_Ehdr ehdr;
    int ret = vfs_read(fd, (char*)&ehdr, sizeof(ehdr));
    if (ret != sizeof(ehdr)) {
        kprintf("[ELF] Failed to read ELF header\n");
        vfs_close(fd);
        return -EIO;
    }
    
    /* 验证 ELF 头 */
    if (!elf_validate(&ehdr)) {
        vfs_close(fd);
        return -EINVAL;
    }
    
    kprintf("[ELF] Entry point: 0x%08x\n", ehdr.e_entry);
    kprintf("[ELF] Program headers: %d at offset %u\n", 
            ehdr.e_phnum, ehdr.e_phoff);
    
    /* 读取程序头表 */
    if (ehdr.e_phnum == 0) {
        kprintf("[ELF] No program headers\n");
        vfs_close(fd);
        return -EINVAL;
    }
    
    /* 分配临时缓冲区存储程序头 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        vfs_close(fd);
        return -ENOMEM;
    }
    
    /* TODO: 实现 lseek 后使用，现在假设程序头紧跟ELF头 */
    /* vfs_lseek(fd, ehdr.e_phoff, SEEK_SET); */
    ret = vfs_read(fd, (char*)phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (ret != (int)(sizeof(Elf32_Phdr) * ehdr.e_phnum)) {
        kprintf("[ELF] Failed to read program headers\n");
        kfree(phdrs);
        vfs_close(fd);
        return -EIO;
    }
    
    /* 加载所有 PT_LOAD 段 */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        kprintf("[ELF] Loading segment %d:\n", i);
        kprintf("      VAddr: 0x%08x, Offset: %u\n", ph->p_vaddr, ph->p_offset);
        kprintf("      FileSize: %u, MemSize: %u\n", ph->p_filesz, ph->p_memsz);
        kprintf("      Flags: %s%s%s\n",
                ph->p_flags & PF_R ? "R" : "-",
                ph->p_flags & PF_W ? "W" : "-",
                ph->p_flags & PF_X ? "X" : "-");
        
        /* 计算需要的页数 */
        uint32_t vaddr_start = ph->p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;
        uint32_t num_pages = (vaddr_end - vaddr_start) / 4096;
        
        kprintf("[ELF] Allocating %u pages\n", num_pages);
        
        /* 为每一页分配物理内存 */
        for (uint32_t j = 0; j < num_pages; j++) {
            uint32_t vaddr = vaddr_start + j * 4096;
            uint32_t paddr = pmm_alloc_frame();
            
            if (!paddr) {
                kprintf("[ELF] Out of memory\n");
                kfree(phdrs);
                vfs_close(fd);
                return -ENOMEM;
            }
            
            /* 清零页 */
            memset((void*)(paddr + 0xC0000000), 0, 4096);
            
            /* 计算权限标志 */
            uint32_t flags = 0x01 | 0x04;  // PRESENT | USER
            if (ph->p_flags & PF_W) {
                flags |= 0x02;  // WRITABLE
            }
            
            /* 映射到当前地址空间（临时用于加载数据） */
            /* 简化：直接使用物理地址 + 0xC0000000 */
        }
        
        /* 读取段数据 */
        /* TODO: 实现 lseek */
        /* 现在简化：假设段按顺序排列 */
        if (ph->p_filesz > 0) {
            /* 读取到临时缓冲区 */
            char *buffer = kmalloc(ph->p_filesz);
            if (buffer) {
                ret = vfs_read(fd, buffer, ph->p_filesz);
                if (ret > 0) {
                    /* 复制到目标地址（通过物理地址） */
                    uint32_t dest_phys = vaddr_start & ~0xFFF;
                    memcpy((void*)(dest_phys + 0xC0000000 + (ph->p_vaddr & 0xFFF)),
                           buffer, ph->p_filesz);
                    kprintf("[ELF] Loaded %d bytes\n", ret);
                }
                kfree(buffer);
            }
        }
        
        /* BSS 段（p_memsz > p_filesz）已经在分配时清零 */
    }
    
    kfree(phdrs);
    vfs_close(fd);
    
    *entry = ehdr.e_entry;
    kprintf("[ELF] Loaded successfully\n");
    
    return 0;
}

