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
#include <mm/vma.h>
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
    
    /* Linux风格：共享设备内存到用户空间 
     * 1. Framebuffer (0xE0000000) - BGA显存
     * 2. VGA Text Mode (0xB8000) - VGA文本显存
     */
    extern uint32_t vmm_virt_to_phys(uint32_t virt);
    uint32_t *user_pde = (uint32_t*)(pd->physical_addr + 0xC0000000);
    uint32_t *kernel_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(kernel_cr3));
    uint32_t *kernel_pde = (uint32_t*)((uint32_t)kernel_cr3 + 0xC0000000);
    
    /* 复制framebuffer页表（0xE0000000）*/
    uint32_t kernel_fb_page = vmm_virt_to_phys(0xE0000000);
    if (kernel_fb_page != 0) {
        /* 复制PDE 896 (0xE0000000 >> 22 = 896) */
        user_pde[896] = kernel_pde[896];
        kprintf("[USER_PROC] Framebuffer (0xE0000000) shared to user space\n");
    }
    
    /* 关键修复：共享VGA文本模式内存（0xB8000）到用户空间
     * 很多程序（包括printf）可能访问VGA内存
     */
    if (kernel_pde[0] & 0x01) {
        /* 复制PDE 0 (包含0xB8000的低端4MB) */
        user_pde[0] = kernel_pde[0];
        kprintf("[USER_PROC] VGA memory (0xB8000) shared to user space\n");
    }
    
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
        /* fd now managed by process fd_table */
        return -1;
    }
    
    /* 3. 验证ELF */
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        kprintf("[USER_PROC] Not an ELF file\n");
        /* fd now managed by process fd_table */
        return -1;
    }
    
    /* 4. 创建进程控制块 */
    struct process *proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) {
        /* fd now managed by process fd_table */
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
        /* fd now managed by process fd_table */
        return -1;
    }
    
    /* 6. 分配内核栈 
     * Linux方式：内核栈在内核堆中分配
     * 这样保证在所有页表中都可访问
     */
    proc->kernel_stack_size = 8192;  // 8KB
    void *kernel_stack_base = kmalloc(proc->kernel_stack_size);
    if (!kernel_stack_base) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(proc);
        /* fd now managed by process fd_table */
        return -1;
    }
    
    /* 栈从高地址向低地址增长，所以栈顶是基地址+大小-4 
     * 注意：kernel_stack 保存的是可用的栈顶地址，需要预留一些空间 */
    proc->kernel_stack = (uint32_t)kernel_stack_base + proc->kernel_stack_size - 4;
    
    kprintf("[USER_PROC] Process %s created:\n", name);
    kprintf("            PID: %u\n", proc->pid);
    kprintf("            Page Dir: 0x%08x\n", proc->page_dir->physical_addr);
    kprintf("            Kernel Stack: 0x%08x\n", proc->kernel_stack);
    
    /* 7. 读取程序头表 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(proc);
        /* fd now managed by process fd_table */
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
                /* fd now managed by process fd_table */
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
            if (!buffer) {
                kprintf("[USER_PROC] ERROR: Failed to allocate buffer for segment\n");
                continue;
            }
            
            int bytes_read = vfs_read(fd, buffer, ph->p_filesz);
            if (bytes_read != (int)ph->p_filesz) {
                kprintf("[USER_PROC] ERROR: Read %d bytes, expected %u\n", bytes_read, ph->p_filesz);
                kfree(buffer);
                continue;
            }
            
            kprintf("[USER_PROC] Read %u bytes from file, copying to memory...\n", ph->p_filesz);
            
            /* 复制数据到物理页（通过物理地址）
             * 关键：正确处理第一个页面的偏移 */
            uint32_t page_offset = ph->p_vaddr & 0xFFF;  /* 第一页内的偏移 */
            uint32_t vaddr = ph->p_vaddr & ~0xFFF;       /* 页面对齐的虚拟地址 */
            uint32_t bytes_copied = 0;
            
            while (bytes_copied < ph->p_filesz) {
                /* 获取此虚拟页对应的物理地址 */
                uint32_t paddr = vmm_virt_to_phys_in_directory(proc->page_dir, vaddr);
                if (!paddr) {
                    kprintf("[USER_PROC] ERROR: vaddr 0x%08x not mapped!\n", vaddr);
                    break;
                }
                
                /* 计算本次复制的大小 */
                uint32_t copy_size = 4096 - page_offset;
                if (copy_size > ph->p_filesz - bytes_copied) {
                    copy_size = ph->p_filesz - bytes_copied;
                }
                
                /* 通过直接映射写入（物理地址 + 0xC0000000） */
                void *dest = (void*)(paddr + 0xC0000000 + page_offset);
                memcpy(dest, buffer + bytes_copied, copy_size);
                
                kprintf("[USER_PROC]   Page 0x%08x (phys 0x%08x + %u): copied %u bytes\n",
                        vaddr, paddr, page_offset, copy_size);
                
                bytes_copied += copy_size;
                vaddr += 4096;
                page_offset = 0;  /* 后续页面从0开始 */
            }
            
            kfree(buffer);
            
            if (bytes_copied != ph->p_filesz) {
                kprintf("[USER_PROC] ERROR: Only copied %u/%u bytes!\n", bytes_copied, ph->p_filesz);
            } else {
                kprintf("[USER_PROC] Successfully copied %u bytes to segment\n", bytes_copied);
            }
        }
    }
    
    kfree(phdrs);
    vfs_close(fd);
    
    /* 9. 设置用户栈（0x08100000向下增长，映射8KB）*/
    uint32_t user_stack_top = 0x08100000;
    
    /* 映射包含栈顶的页面和下面的页面（共8KB）*/
    for (int i = 0; i < 2; i++) {
        uint32_t paddr = pmm_alloc_frame();
        if (!paddr) {
            kprintf("[USER_PROC] Failed to allocate stack page\n");
            kfree(phdrs);
            /* fd now managed by process fd_table */
            return -1;
        }
        
        if (paddr < 0x400000) {
            memset((void*)(paddr + 0xC0000000), 0, 4096);
        }
        
        /* 从栈顶向下映射：0x080FF000, 0x080FE000 */
        uint32_t stack_vaddr = user_stack_top - i*4096 - 4096;
        kprintf("[USER_PROC] Mapping stack page %d: vaddr=0x%08x, paddr=0x%08x\n", 
                i, stack_vaddr, paddr);
        vmm_map_page_in_directory(proc->page_dir, stack_vaddr, 
                                  paddr, 0x07);  /* USER|WRITE|PRESENT */
        
        /* 验证映射 */
        extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
        uint32_t check_phys = vmm_virt_to_phys_in_directory(proc->page_dir, stack_vaddr);
        kprintf("[USER_PROC] Verify mapping: vaddr=0x%08x -> paddr=0x%08x (expected 0x%08x) %s\n",
                stack_vaddr, check_phys, paddr, (check_phys == paddr) ? "✓" : "✗");
    }
    
    /* 10. 初始化进程上下文（Linux风格）*/
    memset(&proc->context, 0, sizeof(struct cpu_context));
    
    /* Linux风格：新进程的eip指向ret_from_fork
     * ret_from_fork会设置段寄存器并执行iret切换到用户态 */
    extern void ret_from_fork(void);
    proc->context.eip = (uint32_t)ret_from_fork;
    
    /* 在内核栈上准备iret栈帧
     * ret_from_fork会从这个栈帧执行iret 
     * 
     * 关键：用户栈ESP必须指向已映射的页面内！
     * 我们映射了 0x080FF000 和 0x080FE000
     * 所以ESP应该在这个范围内，从高地址向低地址使用
     */
    uint32_t user_esp = user_stack_top - 4;  /* 0x080FFFFC */
    
    /* 验证用户栈地址是否被映射 */
    uint32_t user_esp_phys = vmm_virt_to_phys_in_directory(proc->page_dir, user_esp);
    kprintf("[USER_PROC] User stack ESP: 0x%08x -> phys 0x%08x %s\n",
            user_esp, user_esp_phys, user_esp_phys ? "✓" : "✗ ERROR!");
    
    if (!user_esp_phys) {
        kprintf("[USER_PROC] ERROR: User stack ESP not mapped!\n");
        return -1;
    }
    
    uint32_t *kstack = (uint32_t*)proc->kernel_stack;
    *(--kstack) = 0x23;                    /* SS (用户数据段) */
    *(--kstack) = user_esp;                /* ESP (用户栈) */
    *(--kstack) = 0x202;                   /* EFLAGS (IF=1) */
    *(--kstack) = 0x1B;                    /* CS (用户代码段) */
    *(--kstack) = ehdr.e_entry;            /* EIP (程序入口) */
    
    /* 更新内核栈指针，指向准备好的iret栈帧 */
    proc->context.esp = (uint32_t)kstack;
    
    kprintf("[USER_PROC] iret stack frame prepared at kernel stack 0x%08x:\n", (uint32_t)kstack);
    kprintf("[USER_PROC]   [ESP+0] EIP  = 0x%08x\n", ehdr.e_entry);
    kprintf("[USER_PROC]   [ESP+4] CS   = 0x%08x\n", 0x1B);
    kprintf("[USER_PROC]   [ESP+8] EFLG = 0x%08x\n", 0x202);
    kprintf("[USER_PROC]   [ESP+12] ESP = 0x%08x\n", user_esp);
    kprintf("[USER_PROC]   [ESP+16] SS  = 0x%08x\n", 0x23);
    
    
    /* 保存用户栈信息（用于调试和信息显示） */
    proc->context.user_esp = user_stack_top - 4;
    proc->context.ss = 0x23;
    proc->context.cs = 0x1B;
    proc->context.eflags = 0x202;
    
    /* 验证关键地址映射 */
    kprintf("[USER_PROC] Verifying critical mappings:\n");
    extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
    uint32_t eip_phys = vmm_virt_to_phys_in_directory(proc->page_dir, ehdr.e_entry);
    uint32_t esp_phys = vmm_virt_to_phys_in_directory(proc->page_dir, proc->context.esp);
    kprintf("[USER_PROC]   EIP=0x%08x -> phys=0x%08x %s\n", 
            ehdr.e_entry, eip_phys, eip_phys ? "✓" : "✗ ERROR!");
    kprintf("[USER_PROC]   ESP=0x%08x -> phys=0x%08x %s\n",
            proc->context.esp, esp_phys, esp_phys ? "✓" : "✗ ERROR!");
    
    if (!eip_phys) {
        kprintf("[USER_PROC] ERROR: Entry point not mapped!\n");
        return -1;
    }
    
    /* 验证入口点的代码内容（前16字节） */
    if (eip_phys) {
        uint8_t *code = (uint8_t*)(eip_phys + 0xC0000000);
        kprintf("[USER_PROC] Code at entry (0x%08x, phys 0x%08x):\n", ehdr.e_entry, eip_phys);
        kprintf("[USER_PROC]   ");
        for (int i = 0; i < 16; i++) {
            kprintf("%02x ", code[i]);
        }
        kprintf("\n");
        
        /* 检查是否全是0（未加载） */
        bool all_zero = true;
        for (int i = 0; i < 16; i++) {
            if (code[i] != 0) {
                all_zero = false;
                break;
            }
        }
        if (all_zero) {
            kprintf("[USER_PROC] ERROR: Entry point contains all zeros - code not loaded!\n");
            return -1;
        }
    }
    
    /* 11. 添加到调度器（调度器会在真正运行时切换CR3） */
    extern void scheduler_add_process(struct process *proc);
    scheduler_add_process(proc);
    
    kprintf("[USER_PROC] Process created successfully\n");
    kprintf("            Ready to run at entry: 0x%08x\n", ehdr.e_entry);
    
    return proc->pid;
}

/*
 * 创建用户进程（Linux风格按需加载版本）
 * 
 * 只创建VMA，不预分配物理内存
 * 当访问时触发缺页，由Page Fault Handler按需加载
 * 
 * @param name: 进程名
 * @param elf_path: ELF可执行文件路径
 * @return: 新进程的PID，失败返回负数
 */
pid_t create_user_process_lazy(const char *name, const char *elf_path)
{
    kprintf("[USER_PROC_LAZY] Creating user process (Linux style): %s (ELF: %s)\n", name, elf_path);
    
    /* 1. 先创建进程控制块 */
    struct process *proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) {
        return -1;
    }
    
    memset(proc, 0, sizeof(struct process));
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->pid = process_allocate_pid();
    proc->state = PROCESS_STATE_NEW;
    proc->priority = 120;  // 普通优先级
    proc->vma_list = NULL;
    
    /* 2. 创建进程独立的文件描述符表 */
    extern struct file_descriptor_table *fd_table_create(void);
    proc->fd_table = fd_table_create();
    if (!proc->fd_table) {
        kprintf("[USER_PROC_LAZY] Failed to create fd_table!\n");
        kfree(proc);
        return -1;
    }
    
    /* 3. 在进程的fd_table中打开标准文件描述符 */
    kprintf("[USER_PROC_LAZY] Opening standard file descriptors in process fd_table...\n");
    
    extern struct vfs_file *vfs_open_file(const char *path, int flags, int mode);
    proc->fd_table->files[0] = vfs_open_file("/console", O_RDWR, 0);
    proc->fd_table->files[1] = vfs_open_file("/console", O_RDWR, 0);
    proc->fd_table->files[2] = vfs_open_file("/console", O_RDWR, 0);
    
    if (!proc->fd_table->files[0] || !proc->fd_table->files[1] || !proc->fd_table->files[2]) {
        kprintf("[USER_PROC_LAZY] Failed to open standard fds!\n");
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    kprintf("[USER_PROC_LAZY] Standard fds opened: stdin=0, stdout=1, stderr=2\n");
    
    /* 4. 打开ELF文件 */
    proc->fd_table->files[3] = vfs_open_file(elf_path, O_RDONLY, 0);
    if (!proc->fd_table->files[3]) {
        kprintf("[USER_PROC_LAZY] Failed to open ELF file: %s\n", elf_path);
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    proc->fd_table->count = 4;
    kprintf("[USER_PROC_LAZY] Opened ELF file: %s (process fd=3)\n", elf_path);
    
    /* 5. 读取ELF头 */
    Elf32_Ehdr ehdr;
    extern ssize_t vfs_file_read(struct vfs_file *file, void *buf, size_t count);
    if (vfs_file_read(proc->fd_table->files[3], (char*)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        kprintf("[USER_PROC_LAZY] Failed to read ELF header\n");
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    /* 6. 验证ELF */
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        kprintf("[USER_PROC_LAZY] Not an ELF file\n");
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    proc->preempt_count = 0;  /* Linux风格：可抢占 */
    
    /* 7. 创建独立页表 */
    proc->page_dir = create_user_page_directory();
    if (!proc->page_dir) {
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    /* 6. 分配内核栈 */
    proc->kernel_stack_size = 8192;  // 8KB
    void *kernel_stack_base = kmalloc(proc->kernel_stack_size);
    if (!kernel_stack_base) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(proc);
        /* fd now managed by process fd_table */
        return -1;
    }
    proc->kernel_stack = (uint32_t)kernel_stack_base + proc->kernel_stack_size - 4;
    
    kprintf("[USER_PROC_LAZY] Process %s created:\n", name);
    kprintf("                  PID: %u\n", proc->pid);
    kprintf("                  Page Dir: 0x%08x\n", proc->page_dir->physical_addr);
    
    /* 7. 读取程序头表 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        vmm_destroy_page_directory(proc->page_dir);
        kfree(kernel_stack_base);
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    kprintf("[USER_PROC_LAZY] Reading program headers: offset=%u, count=%u, size=%u bytes\n",
            ehdr.e_phoff, ehdr.e_phnum, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    
    /* 需要先seek到程序头表位置 */
    kprintf("[USER_PROC_LAZY] Before seek: file->pos=%u, inode=%p\n", 
            proc->fd_table->files[3]->pos, proc->fd_table->files[3]->inode);
    proc->fd_table->files[3]->pos = ehdr.e_phoff;
    kprintf("[USER_PROC_LAZY] After seek: file->pos=%u\n", proc->fd_table->files[3]->pos);
    
    ssize_t read_bytes = vfs_file_read(proc->fd_table->files[3], (char*)phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    kprintf("[USER_PROC_LAZY] After read: file->pos=%u, read_bytes=%d\n", 
            proc->fd_table->files[3]->pos, read_bytes);
    
    if (read_bytes != sizeof(Elf32_Phdr) * ehdr.e_phnum) {
        kprintf("[USER_PROC_LAZY] Failed to read program headers: expected=%u, got=%d\n",
                sizeof(Elf32_Phdr) * ehdr.e_phnum, read_bytes);
        kfree(phdrs);
        vmm_destroy_page_directory(proc->page_dir);
        kfree(kernel_stack_base);
        kfree(proc->fd_table);
        kfree(proc);
        return -1;
    }
    
    kprintf("[USER_PROC_LAZY] Creating VMAs for ELF segments (demand paging)...\n");
    
    /* 8. 为ELF段创建VMA（Linux风格：按需加载）*/
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        kprintf("[USER_PROC_LAZY] Segment %d: vaddr=0x%08x, memsz=%u, filesz=%u, flags=%s%s%s\n",
                i, ph->p_vaddr, ph->p_memsz, ph->p_filesz,
                ph->p_flags & PF_R ? "R" : "-",
                ph->p_flags & PF_W ? "W" : "-",
                ph->p_flags & PF_X ? "X" : "-");
        
        uint32_t vaddr_start = ph->p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;
        
        /* 计算VMA标志 */
        uint32_t vma_flags = 0;
        if (ph->p_flags & PF_R) vma_flags |= VMA_READ;
        if (ph->p_flags & PF_W) vma_flags |= VMA_WRITE;
        if (ph->p_flags & PF_X) vma_flags |= VMA_EXEC;
        
        if (ph->p_filesz > 0) {
            /* 有文件数据：创建文件映射VMA */
            uint32_t filesz_end = (ph->p_vaddr + ph->p_filesz + 0xFFF) & ~0xFFF;
            
            vma_flags |= VMA_FILE | VMA_PRIVATE;  /* 私有文件映射（COW）*/
            
            extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
            extern void vma_add(struct vma **list, struct vma *vma);
            
            struct vma *vma = vma_create(vaddr_start, filesz_end, vma_flags);
            if (vma) {
                vma->fd = 3;  /* ELF文件在进程fd_table中的索引 */
                
                /* Linux风格文件偏移计算：
                 * 公式：file_offset = p_offset - (p_vaddr % PAGE_SIZE)
                 * 这样当缺页时：
                 *   actual_offset = vma->file_offset + (fault_addr - vma->start)
                 *                 = (p_offset - page_offset) + (fault_addr - vaddr_start)
                 *                 = p_offset + (fault_addr - p_vaddr)
                 */
                uint32_t page_offset = ph->p_vaddr & 0xFFF;
                vma->file_offset = ph->p_offset - page_offset;
                vma_add(&proc->vma_list, vma);
                
                kprintf("[USER_PROC_LAZY]   Created FILE VMA: 0x%08x-0x%08x (%u KB, fd=%d, offset=%u)\n",
                        vaddr_start, filesz_end, (filesz_end - vaddr_start) / 1024,
                        vma->fd, vma->file_offset);
            }
            
            /* BSS段（如果有）*/
            if (filesz_end < vaddr_end) {
                struct vma *bss_vma = vma_create(filesz_end, vaddr_end, 
                                                 (vma_flags & ~VMA_FILE) | VMA_ZERO | VMA_ANONYMOUS);
                if (bss_vma) {
                    bss_vma->fd = -1;
                    vma_add(&proc->vma_list, bss_vma);
                    
                    kprintf("[USER_PROC_LAZY]   Created BSS VMA: 0x%08x-0x%08x (%u KB, zero-fill)\n",
                            filesz_end, vaddr_end, (vaddr_end - filesz_end) / 1024);
                }
            }
        } else {
            /* 纯BSS段：匿名零页映射 */
            vma_flags |= VMA_ZERO | VMA_ANONYMOUS;
            
            extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
            extern void vma_add(struct vma **list, struct vma *vma);
            
            struct vma *vma = vma_create(vaddr_start, vaddr_end, vma_flags);
            if (vma) {
                vma->fd = -1;
                vma_add(&proc->vma_list, vma);
                
                kprintf("[USER_PROC_LAZY]   Created BSS VMA: 0x%08x-0x%08x (%u KB, zero-fill)\n",
                        vaddr_start, vaddr_end, (vaddr_end - vaddr_start) / 1024);
            }
        }
    }
    
    kfree(phdrs);
    
    /* Linux风格：文件描述符管理
     * 注意：不能关闭fd，因为：
     * 1. 多个VMA可能共享同一个fd
     * 2. 按需加载时需要通过VMA->fd读取文件
     * 3. 进程退出时，VMA销毁会自动处理fd
     * 
     * TODO: 实现引用计数或在进程退出时统一关闭
     */
    /* vfs_close(fd); - 保持打开 */
    
    /* 9. 创建用户栈VMA（Linux风格：按需分配）*/
    uint32_t user_stack_top = 0x08100000;
    uint32_t user_stack_bottom = user_stack_top - (128 * 4096);  /* 512KB栈空间 */
    
    extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
    extern void vma_add(struct vma **list, struct vma *vma);
    
    struct vma *stack_vma = vma_create(user_stack_bottom, user_stack_top,
                                       VMA_READ | VMA_WRITE | VMA_ANONYMOUS);
    if (stack_vma) {
        stack_vma->fd = -1;
        vma_add(&proc->vma_list, stack_vma);
        
        kprintf("[USER_PROC_LAZY] Created STACK VMA: 0x%08x-0x%08x (512 KB, demand paging)\n",
                user_stack_bottom, user_stack_top);
        
        /* 预先映射栈顶页面，避免首次访问缺页 */
        uint32_t stack_top_page = user_stack_top - 4096;
        uint32_t paddr = pmm_alloc_frame();
        if (paddr) {
            if (paddr < 0x400000) {
                memset((void*)(paddr + 0xC0000000), 0, 4096);
            }
            vmm_map_page_in_directory(proc->page_dir, stack_top_page, paddr, 0x07);
            kprintf("[USER_PROC_LAZY] Pre-mapped stack top page: 0x%08x\n", stack_top_page);
        }
    }
    
    /* 10. 初始化进程上下文（Linux风格）*/
    memset(&proc->context, 0, sizeof(struct cpu_context));
    
    extern void ret_from_fork(void);
    proc->context.eip = (uint32_t)ret_from_fork;
    
    /* 用户栈指针：留出一些空间给启动代码使用 */
    uint32_t user_esp = user_stack_top - 64;  /* 0x080FFFC0 */
    
    /* 准备iret栈帧 */
    uint32_t *kstack = (uint32_t*)proc->kernel_stack;
    *(--kstack) = 0x23;                    /* SS */
    *(--kstack) = user_esp;                /* ESP */
    *(--kstack) = 0x202;                   /* EFLAGS */
    *(--kstack) = 0x1B;                    /* CS */
    *(--kstack) = ehdr.e_entry;            /* EIP */
    
    proc->context.esp = (uint32_t)kstack;
    
    /* 验证栈帧 */
    kprintf("[USER_PROC_LAZY] iret frame: EIP=0x%08x CS=0x%x EFLAGS=0x%x ESP=0x%08x SS=0x%x\n",
            ehdr.e_entry, 0x1B, 0x202, user_esp, 0x23);
    kprintf("[USER_PROC_LAZY] Stack frame verification:\n");
    kprintf("                  kstack=0x%08x\n", (uint32_t)kstack);
    kprintf("                  [kstack+0]=0x%08x (should be EIP=0x%08x)\n", kstack[0], ehdr.e_entry);
    kprintf("                  [kstack+1]=0x%08x (should be CS=0x%x)\n", kstack[1], 0x1B);
    kprintf("                  [kstack+2]=0x%08x (should be EFLAGS=0x%x)\n", kstack[2], 0x202);
    kprintf("                  [kstack+3]=0x%08x (should be ESP=0x%08x)\n", kstack[3], user_esp);
    kprintf("                  [kstack+4]=0x%08x (should be SS=0x%x)\n", kstack[4], 0x23);
    proc->context.user_esp = user_esp;
    proc->context.ss = 0x23;
    proc->context.cs = 0x1B;
    proc->context.eflags = 0x202;
    
    /* Linux风格：完全按需加载
     * 不预先加载任何代码，第一次访问时通过Page Fault加载
     * 
     * 注意：ELF文件保持打开状态（fd=3+），VMA需要用它来按需加载
     * 标准文件描述符 0,1,2 已在函数开始时打开
     */
    kprintf("[USER_PROC_LAZY] Entry point: 0x%08x (will load on first page fault)\n", ehdr.e_entry);
    kprintf("[USER_PROC_LAZY] ELF fd=3 will be used for demand paging\n");
    kprintf("[USER_PROC_LAZY] Standard I/O: stdin=0, stdout=1, stderr=2 (already opened)\n");
    
    /* 验证入口点页面是否未映射（应该通过Page Fault加载） */
    extern uint32_t vmm_virt_to_phys_in_directory(struct page_directory *pd, uint32_t virt);
    uint32_t entry_phys = vmm_virt_to_phys_in_directory(proc->page_dir, ehdr.e_entry);
    if (entry_phys != 0) {
        kprintf("[USER_PROC_LAZY] WARNING: Entry point already mapped! (phys=0x%08x)\n", entry_phys);
        kprintf("[USER_PROC_LAZY] This should not happen in demand paging mode!\n");
    } else {
        kprintf("[USER_PROC_LAZY] ✓ Entry point NOT mapped - will trigger Page Fault\n");
    }
    
    /* 12. 添加到调度器 */
    extern void scheduler_add_process(struct process *proc);
    scheduler_add_process(proc);
    
    kprintf("[USER_PROC_LAZY] ✅ Process created successfully (full demand paging)\n");
    kprintf("[USER_PROC_LAZY] All memory will be loaded on-demand via page faults\n\n");
    
    return proc->pid;
}

/*
 * 从当前进程执行ELF（Linux风格execve实现）
 * 
 * 清空当前进程的用户空间，加载新ELF程序
 * 类似 Linux 的 sys_execve()
 */
int do_exec(const char *path, char *argv[], char *envp[])
{
    (void)argv;  /* TODO: 参数和环境变量传递 */
    (void)envp;
    
    kprintf("[EXECVE] Executing: %s\n", path);
    
    struct process *proc = process_get_current();
    if (!proc) {
        kprintf("[EXECVE] ERROR: No current process\n");
        return -ESRCH;
    }
    
    /* 1. 打开新ELF文件 */
    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[EXECVE] ERROR: Failed to open %s: %d\n", path, fd);
        return fd;
    }
    
    /* 2. 读取并验证ELF头 */
    Elf32_Ehdr ehdr;
    if (vfs_read(fd, (char*)&ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        kprintf("[EXECVE] ERROR: Failed to read ELF header\n");
        /* fd now managed by process fd_table */
        return -EIO;
    }
    
    /* 验证ELF魔数 */
    if (ehdr.e_ident[EI_MAG0] != 0x7F || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {
        kprintf("[EXECVE] ERROR: Not a valid ELF file\n");
        /* fd now managed by process fd_table */
        return -EINVAL;
    }
    
    kprintf("[EXECVE] Valid ELF file, entry point: 0x%08x\n", ehdr.e_entry);
    
    /* 3. 清空当前进程的用户空间（Linux风格）*/
    kprintf("[EXECVE] Clearing user space (VMA list)...\n");
    
    /* 销毁所有VMA */
    if (proc->vma_list) {
        extern void vma_destroy_all(struct vma *list);
        vma_destroy_all(proc->vma_list);
        proc->vma_list = NULL;
        kprintf("[EXECVE]   All VMAs destroyed\n");
    }
    
    /* 清空用户页表（保留内核映射）*/
    if (proc->page_dir) {
        /* Linux方式：清空用户空间的PDE (0-767)，保留内核空间 (768-1023) */
        uint32_t *pd = (uint32_t*)((uint32_t)proc->page_dir->physical_addr + 0xC0000000);
        for (int i = 0; i < 768; i++) {
            if (pd[i] & 0x01) {  /* Present */
                /* 释放页表 */
                uint32_t pt_phys = pd[i] & ~0xFFF;
                extern void pmm_free_frame(uint32_t addr);
                pmm_free_frame(pt_phys);
                pd[i] = 0;  /* 清除PDE */
            }
        }
        kprintf("[EXECVE]   User space page tables cleared\n");
    }
    
    /* 4. 读取程序头表 */
    Elf32_Phdr *phdrs = kmalloc(sizeof(Elf32_Phdr) * ehdr.e_phnum);
    if (!phdrs) {
        /* fd now managed by process fd_table */
        return -ENOMEM;
    }
    
    vfs_read(fd, (char*)phdrs, sizeof(Elf32_Phdr) * ehdr.e_phnum);
    
    kprintf("[EXECVE] Creating new VMAs for %d segments...\n", ehdr.e_phnum);
    
    /* 5. 为新ELF段创建VMA（与create_user_process_lazy相同）*/
    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr *ph = &phdrs[i];
        
        if (ph->p_type != PT_LOAD) {
            continue;
        }
        
        uint32_t vaddr_start = ph->p_vaddr & ~0xFFF;
        uint32_t vaddr_end = (ph->p_vaddr + ph->p_memsz + 0xFFF) & ~0xFFF;
        
        uint32_t vma_flags = 0;
        if (ph->p_flags & PF_R) vma_flags |= VMA_READ;
        if (ph->p_flags & PF_W) vma_flags |= VMA_WRITE;
        if (ph->p_flags & PF_X) vma_flags |= VMA_EXEC;
        
        if (ph->p_filesz > 0) {
            /* 文件映射VMA */
            uint32_t filesz_end = (ph->p_vaddr + ph->p_filesz + 0xFFF) & ~0xFFF;
            
            vma_flags |= VMA_FILE | VMA_PRIVATE;
            
            extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
            extern void vma_add(struct vma **list, struct vma *vma);
            
            struct vma *vma = vma_create(vaddr_start, filesz_end, vma_flags);
            if (vma) {
                vma->fd = fd;
                uint32_t page_offset = ph->p_vaddr & 0xFFF;
                vma->file_offset = ph->p_offset - page_offset;
                vma_add(&proc->vma_list, vma);
            }
            
            /* BSS段 */
            if (filesz_end < vaddr_end) {
                struct vma *bss_vma = vma_create(filesz_end, vaddr_end, 
                                                 (vma_flags & ~VMA_FILE) | VMA_ZERO | VMA_ANONYMOUS);
                if (bss_vma) {
                    bss_vma->fd = -1;
                    vma_add(&proc->vma_list, bss_vma);
                }
            }
        } else {
            /* 纯BSS段 */
            vma_flags |= VMA_ZERO | VMA_ANONYMOUS;
            
            extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
            extern void vma_add(struct vma **list, struct vma *vma);
            
            struct vma *vma = vma_create(vaddr_start, vaddr_end, vma_flags);
            if (vma) {
                vma->fd = -1;
                vma_add(&proc->vma_list, vma);
            }
        }
    }
    
    kfree(phdrs);
    /* 文件保持打开状态，VMA需要用它 */
    
    /* 6. 创建新的用户栈VMA */
    uint32_t user_stack_top = 0x08100000;
    uint32_t user_stack_bottom = user_stack_top - (128 * 4096);
    
    extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
    extern void vma_add(struct vma **list, struct vma *vma);
    
    struct vma *stack_vma = vma_create(user_stack_bottom, user_stack_top,
                                       VMA_READ | VMA_WRITE | VMA_ANONYMOUS);
    if (stack_vma) {
        stack_vma->fd = -1;
        vma_add(&proc->vma_list, stack_vma);
        
        /* 预映射栈顶页 */
        uint32_t stack_top_page = user_stack_top - 4096;
        uint32_t paddr = pmm_alloc_frame();
        if (paddr && paddr < 0x400000) {
            memset((void*)(paddr + 0xC0000000), 0, 4096);
            vmm_map_page_in_directory(proc->page_dir, stack_top_page, paddr, 0x07);
        }
    }
    
    kprintf("[EXECVE] New VMAs created, program ready to execute\n");
    
    /* 7. 重置进程上下文（不使用ret_from_fork，直接跳转）*/
    /* Linux风格：execve从系统调用返回时直接进入新程序 */
    
    /* 这里简化处理：直接通过iret跳转到用户态 */
    uint32_t user_esp = user_stack_top - 4;
    
    kprintf("[EXECVE] Jumping to new program at 0x%08x...\n\n", ehdr.e_entry);
    
    /* 直接执行iret切换到用户态 */
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
        : "r"(ehdr.e_entry), "r"(user_esp)
        : "eax"
    );
    
    /* 不会返回 */
    return 0;
}

