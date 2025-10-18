/*
 * elf_exec.c - ELF 程序执行（完整版，Linux 风格）
 * 
 * 完整实现从加载到执行的全过程
 */

#include <elf.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <process/process.h>
#include <string.h>

/*
 * 临时映射物理页到内核空间
 */
static void *temp_map_page(uint32_t paddr)
{
    /* 简化：假设物理地址在低 4MB，可以通过 0xC0000000 访问 */
    if (paddr < 0x400000) {
        return (void*)(paddr + 0xC0000000);
    }
    
    /* TODO: 对于高端内存，需要动态映射 */
    return NULL;
}

/*
 * 加载并执行 ELF 程序（完整版）
 * 
 * @param path: ELF 文件路径
 * @return: 不返回（切换到用户态），失败返回错误码
 */
int elf_exec(const char *path)
{
    kprintf("[EXEC] Loading and executing: %s\n", path);
    
    /* 打开文件 */
    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[EXEC] Failed to open: %d\n", fd);
        return fd;
    }
    
    /* 读取 ELF 头 */
    Elf32_Ehdr ehdr;
    if (vfs_read(fd, (char*)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        vfs_close(fd);
        return -EIO;
    }
    
    /* 验证 ELF */
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        kprintf("[EXEC] Not an ELF file\n");
        vfs_close(fd);
        return -EINVAL;
    }
    
    if (ehdr.e_type != ET_EXEC || ehdr.e_machine != EM_386) {
        kprintf("[EXEC] Not i386 executable\n");
        vfs_close(fd);
        return -EINVAL;
    }
    
    kprintf("[EXEC] Entry: 0x%08x, %d program headers\n", 
            ehdr.e_entry, ehdr.e_phnum);
    
    /* 读取程序头 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        vfs_close(fd);
        return -ENOMEM;
    }
    
    vfs_read(fd, (char*)phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    
    /* Linux 风格：在当前页表中映射用户空间（不创建新页表） */
    kprintf("[EXEC] Using current kernel page directory\n");
    
    /* 加载所有 PT_LOAD 段 */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        kprintf("[EXEC] Loading segment %d: 0x%08x (%u bytes, flags:%s%s%s)\n",
                i, ph->p_vaddr, ph->p_memsz,
                ph->p_flags & PF_R ? "R" : "-",
                ph->p_flags & PF_W ? "W" : "-",
                ph->p_flags & PF_X ? "X" : "-");
        
        /* 读取段数据 */
        char *segment_data = NULL;
        if (ph->p_filesz > 0) {
            segment_data = kmalloc(ph->p_memsz);  // 分配足够大的空间
            if (!segment_data) {
                kfree(phdrs);
                vfs_close(fd);
                return -ENOMEM;
            }
            
            memset(segment_data, 0, ph->p_memsz);  // 清零（BSS部分）
            
            /* 关键：使用lseek定位到段的文件偏移 */
            if (vfs_lseek(fd, ph->p_offset, SEEK_SET) < 0) {
                kprintf("[EXEC] Failed to seek to offset %u\n", ph->p_offset);
                kfree(segment_data);
                kfree(phdrs);
                vfs_close(fd);
                return -EIO;
            }
            
            int n = vfs_read(fd, segment_data, ph->p_filesz);
            kprintf("[EXEC] Seeked to offset %u, read %d bytes (expected %u)\n", 
                    ph->p_offset, n, ph->p_filesz);
            
            if (n != (int)ph->p_filesz) {
                kprintf("[EXEC] Warning: incomplete read\n");
            }
        }
        
        /* 分配物理页，复制数据，映射 */
        uint32_t vaddr_start = ph->p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;
        uint32_t bytes_copied = 0;
        
        for (uint32_t vaddr = vaddr_start; vaddr < vaddr_end; vaddr += 4096) {
            uint32_t paddr = pmm_alloc_frame();
            if (!paddr) {
                if (segment_data) kfree(segment_data);
                kfree(phdrs);
                vfs_close(fd);
                return -ENOMEM;
            }
            
            /* 清零并复制数据 */
            void *page_ptr = temp_map_page(paddr);
            if (page_ptr) {
                memset(page_ptr, 0, 4096);
                
                /* 计算本页要复制的数据 */
                if (segment_data && bytes_copied < ph->p_memsz) {
                    uint32_t offset_in_page = (vaddr == vaddr_start) ? (ph->p_vaddr & 0xFFF) : 0;
                    uint32_t copy_size = 4096 - offset_in_page;
                    if (bytes_copied + copy_size > ph->p_memsz) {
                        copy_size = ph->p_memsz - bytes_copied;
                    }
                    
                    memcpy(page_ptr + offset_in_page, segment_data + bytes_copied, copy_size);
                    bytes_copied += copy_size;
                }
            }
            
            /* 计算权限 */
            uint32_t flags = 0x01 | 0x04;  // PRESENT | USER
            if (ph->p_flags & PF_W) flags |= 0x02;  // WRITABLE
            
            /* 映射 */
            vmm_map_page(vaddr, paddr, flags);
        }
        
        if (segment_data) {
            kprintf("[EXEC] Copied %u bytes to user space\n", bytes_copied);
            kfree(segment_data);
        }
    }
    
    kfree(phdrs);
    vfs_close(fd);
    
    /* 为用户进程打开标准文件描述符（0=stdin, 1=stdout, 2=stderr） */
    kprintf("[EXEC] Setting up standard file descriptors...\n");
    
    /* 打开 /dev/console 作为 stdin/stdout/stderr */
    int stdin_fd = vfs_open("/console", O_RDWR, 0);
    int stdout_fd = vfs_open("/console", O_RDWR, 0);
    int stderr_fd = vfs_open("/console", O_RDWR, 0);
    
    kprintf("[EXEC] stdin=%d, stdout=%d, stderr=%d\n", stdin_fd, stdout_fd, stderr_fd);
    
    if (stdin_fd != 0 || stdout_fd != 1 || stderr_fd != 2) {
        kprintf("[EXEC] Warning: Standard fds not in expected order\n");
    }
    
    /* 分配用户栈（Linux 风格：在当前页表中映射） */
    uint32_t stack_top = 0x08100000;
    for (int i = 0; i < 2; i++) {
        uint32_t paddr = pmm_alloc_frame();
        void *page = temp_map_page(paddr);
        if (page) memset(page, 0, 4096);
        
        vmm_map_page(stack_top - (i+1)*4096, paddr, 0x07);  // USER | WRITABLE | PRESENT
    }
    
    kprintf("[EXEC] User stack ready at 0x%08x\n", stack_top);
    kprintf("[EXEC] All segments mapped in current page table\n");
    kprintf("[EXEC] Jumping to Ring 3 at 0x%08x...\n\n", ehdr.e_entry);
    
    /* 切换到用户态 */
    asm volatile(
        "cli\n"
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        "pushl $0x23\n"          // SS
        "pushl %1\n"             // ESP
        "pushf\n"
        "popl %%eax\n"
        "orl $0x200, %%eax\n"
        "pushl %%eax\n"          // EFLAGS
        "pushl $0x1B\n"          // CS
        "pushl %0\n"             // EIP
        "iret\n"
        :
        : "r"(ehdr.e_entry), "r"(stack_top)
        : "eax"
    );
    
    /* 不会执行到这里 */
    return 0;
}

