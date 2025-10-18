/*
 * user_process.c - 用户进程创建和管理
 * 
 * 实现独立用户页表和进程地址空间管理
 */

#include <kernel.h>
#include <process/process.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <fs/vfs.h>
#include <string.h>
#include <elf.h>

/* 外部函数 */
extern void switch_to_user_mode(uint32_t entry, uint32_t stack);
extern uint32_t tss_get_address(void);
extern void tss_set_kernel_stack(uint32_t stack);

/*
 * 为用户进程创建独立页表
 * 
 * 布局：
 *   0x00000000 - 0xBFFFFFFF: 用户空间（3GB）
 *   0xC0000000 - 0xFFFFFFFF: 内核空间（1GB，共享）
 */
static struct page_directory *create_user_page_directory(void)
{
    /* 创建新页目录 */
    struct page_directory *pd = vmm_create_page_directory();
    if (!pd) {
        return NULL;
    }
    
    kprintf("[USER_PROC] Created user page directory at phys 0x%08x\n", pd->physical_addr);
    
    /* 内核空间映射已经由 vmm_create_page_directory() 复制 */
    /* 用户空间目前为空，会在加载ELF时按需映射 */
    
    return pd;
}

/*
 * 创建用户进程
 * 
 * @param name: 进程名
 * @param elf_path: ELF可执行文件路径
 * @return: 新进程的PID，失败返回负数
 */
pid_t create_user_process(const char *name, const char *elf_path)
{
    kprintf("[USER_PROC] Creating user process: %s (ELF: %s)\n", name, elf_path);
    
    /* 1. 打开ELF文件 */
    int fd = vfs_open(elf_path, O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[USER_PROC] Failed to open ELF file: %d\n", fd);
        return -1;
    }
    
    /* 2. 读取ELF头 */
    Elf32_Ehdr ehdr;
    if (vfs_read(fd, (char*)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        vfs_close(fd);
        return -1;
    }
    
    /* 3. 验证ELF */
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        kprintf("[USER_PROC] Not an ELF file\n");
        vfs_close(fd);
        return -1;
    }
    
    /* 4. 创建进程控制块 */
    struct process *proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) {
        vfs_close(fd);
        return -1;
    }
    
    memset(proc, 0, sizeof(struct process));
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->pid = process_allocate_pid();
    proc->state = PROCESS_STATE_NEW;
    proc->priority = 120;  // 普通优先级
    proc->vma_list = NULL;  // 初始化VMA链表
    
    /* 5. 创建独立页表 */
    proc->page_dir = create_user_page_directory();
    if (!proc->page_dir) {
        kfree(proc);
        vfs_close(fd);
        return -1;
    }
    
    /* 6. 分配内核栈 */
    proc->kernel_stack_size = 8192;  // 8KB
    uint32_t stack_phys = pmm_alloc_frame();
    if (!stack_phys) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(proc);
        vfs_close(fd);
        return -1;
    }
    
    /* 映射内核栈到内核空间 */
    uint32_t stack_virt = 0xC0000000 + stack_phys;  // 临时映射
    proc->kernel_stack = stack_virt + proc->kernel_stack_size;
    
    kprintf("[USER_PROC] Process %s created:\n", name);
    kprintf("            PID: %u\n", proc->pid);
    kprintf("            Page Dir: 0x%08x\n", proc->page_dir->physical_addr);
    kprintf("            Kernel Stack: 0x%08x\n", proc->kernel_stack);
    
    /* 7. 读取程序头表 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(proc);
        vfs_close(fd);
        return -1;
    }
    
    vfs_read(fd, (char*)phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    
    /* Linux方式：不切换页表，在内核态完成所有设置 */
    kprintf("[USER_PROC] Loading ELF in kernel context (Linux style)\n");
    
    /* 8. 加载ELF段到用户空间 */
    /* Linux方式：为新进程的页表手动设置映射，而不切换CR3 */
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        kprintf("[USER_PROC] Preparing segment %d: vaddr=0x%08x, memsz=%u, filesz=%u\n",
                i, ph->p_vaddr, ph->p_memsz, ph->p_filesz);
        
        /* 计算段的范围 */
        uint32_t vaddr_start = ph->p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;
        uint32_t filesz_end = (ph->p_vaddr + ph->p_filesz + 0xFFF) & ~0xFFF;
        
        /* 如果有BSS段（memsz > filesz），为BSS部分创建VMA */
        if (ph->p_memsz > ph->p_filesz) {
            uint32_t bss_start = filesz_end;
            uint32_t bss_end = vaddr_end;
            
            if (bss_end > bss_start) {
                extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
                extern void vma_add(struct vma **list, struct vma *vma);
                
                /* 创建VMA：可读、可写、按需分配零页 */
                uint32_t vma_flags = 0x01 | 0x02 | 0x08;  /* VMA_READ | VMA_WRITE | VMA_ZERO */
                struct vma *vma = vma_create(bss_start, bss_end, vma_flags);
                if (vma) {
                    vma_add(&proc->vma_list, vma);
                    kprintf("[USER_PROC] Created VMA for BSS: 0x%08x-0x%08x (%u KB, lazy)\n",
                            bss_start, bss_end, (bss_end - bss_start) / 1024);
                }
            }
            
            /* 只为有数据的部分预分配 */
            vaddr_end = filesz_end;
        }
        
        for (uint32_t vaddr = vaddr_start; vaddr < vaddr_end; vaddr += 4096) {
            uint32_t paddr = pmm_alloc_frame();
            if (!paddr) {
                kfree(phdrs);
                vmm_destroy_page_directory(proc->page_dir);
                kfree(proc);
                vfs_close(fd);
                return -1;
            }
            
            /* 通过内核直接映射清零页 */
            if (paddr < 0x400000) {
                memset((void*)(paddr + 0xC0000000), 0, 4096);
            }
            
            /* 手动设置新进程页表的映射（通过直接访问页表结构）*/
            uint32_t flags = 0x01 | 0x04;  /* PRESENT | USER */
            if (ph->p_flags & PF_W) flags |= 0x02;  /* WRITABLE */
            
            /* 在新进程的页表中创建映射 */
            vmm_map_page_in_directory(proc->page_dir, vaddr, paddr, flags);
        }
        
        /* 读取段数据并写入到物理页 */
        if (ph->p_filesz > 0) {
            vfs_lseek(fd, ph->p_offset, SEEK_SET);
            
            char *buffer = kmalloc(ph->p_filesz);
            if (buffer) {
                vfs_read(fd, buffer, ph->p_filesz);
                
                /* 复制数据到物理页（通过物理地址） */
                uint32_t page_offset = ph->p_vaddr & 0xFFF;
                uint32_t vaddr = ph->p_vaddr & ~0xFFF;
                uint32_t bytes_copied = 0;
                
                while (bytes_copied < ph->p_filesz) {
                    /* 获取此虚拟页对应的物理地址 */
                    uint32_t paddr = vmm_virt_to_phys_in_directory(proc->page_dir, vaddr);
                    if (paddr) {
                        uint32_t copy_size = 4096 - page_offset;
                        if (copy_size > ph->p_filesz - bytes_copied) {
                            copy_size = ph->p_filesz - bytes_copied;
                        }
                        
                        /* 通过直接映射写入 */
                        memcpy((void*)(paddr + 0xC0000000 + page_offset), 
                               buffer + bytes_copied, copy_size);
                        
                        bytes_copied += copy_size;
                        vaddr += 4096;
                        page_offset = 0;
                    } else {
                        break;
                    }
                }
                
                kfree(buffer);
            }
        }
    }
    
    kfree(phdrs);
    vfs_close(fd);
    
    /* 9. 设置用户栈（0x08100000）*/
    uint32_t user_stack_top = 0x08100000;
    for (int i = 0; i < 2; i++) {
        uint32_t paddr = pmm_alloc_frame();
        if (paddr && paddr < 0x400000) {
            memset((void*)(paddr + 0xC0000000), 0, 4096);
        }
        /* 在新进程的页表中映射栈 */
        vmm_map_page_in_directory(proc->page_dir, user_stack_top - (i+1)*4096, 
                                  paddr, 0x07);  /* USER|WRITE|PRESENT */
    }
    
    /* 10. 初始化进程上下文（用户态）*/
    memset(&proc->context, 0, sizeof(struct cpu_context));
    proc->context.eip = ehdr.e_entry;
    proc->context.esp = user_stack_top;
    proc->context.eflags = 0x202;  /* IF=1 */
    proc->context.cs = 0x1B;       /* 用户代码段 (RPL=3) */
    
    /* 11. 添加到调度器（调度器会在真正运行时切换CR3） */
    extern void scheduler_add_process(struct process *proc);
    scheduler_add_process(proc);
    
    kprintf("[USER_PROC] Process created successfully\n");
    kprintf("            Ready to run at entry: 0x%08x\n", ehdr.e_entry);
    
    return proc->pid;
}

/*
 * 从当前进程执行ELF（类似execve）
 * 
 * 会替换当前进程的地址空间
 */
int do_exec(const char *path, char *argv[], char *envp[])
{
    (void)argv;
    (void)envp;
    
    kprintf("[USER_PROC] Executing: %s\n", path);
    
    struct process *proc = process_get_current();
    if (!proc) {
        return -ESRCH;
    }
    
    /* 1. 打开ELF文件 */
    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    
    /* 2. 读取并验证ELF头 */
    Elf32_Ehdr ehdr;
    if (vfs_read(fd, (char*)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        vfs_close(fd);
        return -EIO;
    }
    
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        vfs_close(fd);
        return -EINVAL;
    }
    
    /* 3. 清空当前用户空间（保留内核映射） */
    if (proc->page_dir) {
        /* 只清空用户空间页（0-3GB） */
        /* 现在简化：创建新页表 */
        struct page_directory *old_pd = proc->page_dir;
        proc->page_dir = vmm_create_page_directory();
        if (!proc->page_dir) {
            proc->page_dir = old_pd;
            vfs_close(fd);
            return -ENOMEM;
        }
        vmm_destroy_page_directory(old_pd);
    }
    
    /* 4. 加载新ELF（类似create_user_process） */
    /* 这里简化：直接调用elf_exec */
    extern int elf_exec(const char *path);
    
    vfs_close(fd);
    
    /* 注意：elf_exec不会返回 */
    return elf_exec(path);
}
