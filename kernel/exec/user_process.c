/*
 * user_process.c - 用户态进程创建和管理
 */

#include <kernel.h>
#include <fs/vfs.h>
#include <process/process.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <string.h>

/*
 * 创建用户进程的页目录
 */
uint32_t *create_user_page_directory(void)
{
    /* 分配页目录 */
    uint32_t *pd = (uint32_t*)pmm_alloc_frame();
    if (!pd) {
        return NULL;
    }
    
    /* 转换为虚拟地址（内核空间） */
    uint32_t *pd_virt = (uint32_t*)((uint32_t)pd + 0xC0000000);
    
    /* 清空页目录 */
    memset(pd_virt, 0, 4096);
    
    /* 复制内核空间映射（3GB-4GB） */
    /* 用户进程需要访问内核（系统调用时） */
    /* 简化：使用当前页目录的内核映射 */
    uint32_t *current_pd;
    asm volatile("mov %%cr3, %0" : "=r"(current_pd));
    uint32_t *current_pd_virt = (uint32_t*)((uint32_t)current_pd + 0xC0000000);
    
    for (int i = 768; i < 1024; i++) {
        pd_virt[i] = current_pd_virt[i];
    }
    
    return pd;
}

/*
 * 为用户进程映射一个页
 */
int map_user_page(uint32_t *pd, uint32_t vaddr, uint32_t paddr, uint32_t flags)
{
    uint32_t pd_index = vaddr >> 22;
    uint32_t pt_index = (vaddr >> 12) & 0x3FF;
    
    /* 转换页目录为虚拟地址 */
    uint32_t *pd_virt = (uint32_t*)((uint32_t)pd + 0xC0000000);
    
    /* 检查页表是否存在 */
    if (!(pd_virt[pd_index] & 0x1)) {
        /* 分配新页表 */
        uint32_t pt = pmm_alloc_frame();
        if (!pt) {
            return -ENOMEM;
        }
        
        /* 清空页表 */
        uint32_t *pt_virt = (uint32_t*)(pt + 0xC0000000);
        memset(pt_virt, 0, 4096);
        
        /* 设置页目录项 */
        pd_virt[pd_index] = pt | flags | 0x1;  // Present
    }
    
    /* 获取页表 */
    uint32_t pt = pd_virt[pd_index] & ~0xFFF;
    uint32_t *pt_virt = (uint32_t*)(pt + 0xC0000000);
    
    /* 设置页表项 */
    pt_virt[pt_index] = paddr | flags | 0x1;
    
    return 0;
}

/*
 * 创建简单的用户进程（内置代码）
 */
int create_simple_user_process(void)
{
    kprintf("\n[USER] Creating first user process...\n");
    
    /* 简单的用户代码：调用 sys_exit(42) */
    uint8_t user_code[] = {
        0xB8, 0x01, 0x00, 0x00, 0x00,  // mov eax, 1 (SYS_exit)
        0xBB, 0x2A, 0x00, 0x00, 0x00,  // mov ebx, 42 (退出码)
        0xCD, 0x80,                     // int 0x80
        0xEB, 0xFE                      // jmp $ (不应执行到)
    };
    
    /* 创建页目录 */
    uint32_t *pd = create_user_page_directory();
    if (!pd) {
        kprintf("[ERROR] Failed to create page directory\n");
        return -ENOMEM;
    }
    
    kprintf("[OK] User page directory created at 0x%08x\n", (uint32_t)pd);
    
    /* 分配用户代码页（地址 0x08000000） */
    uint32_t code_page = pmm_alloc_frame();
    if (!code_page) {
        kprintf("[ERROR] Failed to allocate code page\n");
        return -ENOMEM;
    }
    
    /* 复制代码到物理页 */
    uint8_t *code_virt = (uint8_t*)(code_page + 0xC0000000);
    memcpy(code_virt, user_code, sizeof(user_code));
    
    /* 映射到用户空间 */
    int ret = map_user_page(pd, 0x08000000, code_page, 
                            0x04 | 0x02);  // USER | WRITABLE
    if (ret < 0) {
        kprintf("[ERROR] Failed to map code page\n");
        return ret;
    }
    
    kprintf("[OK] User code mapped at 0x08000000\n");
    
    /* 分配用户栈页（地址 0x08100000 - 8KB） */
    for (int i = 0; i < 2; i++) {
        uint32_t stack_page = pmm_alloc_frame();
        if (!stack_page) {
            kprintf("[ERROR] Failed to allocate stack page\n");
            return -ENOMEM;
        }
        
        /* 清零栈 */
        memset((void*)(stack_page + 0xC0000000), 0, 4096);
        
        /* 映射栈页 */
        ret = map_user_page(pd, 0x08100000 - (i + 1) * 4096, stack_page,
                           0x04 | 0x02);  // USER | WRITABLE
        if (ret < 0) {
            kprintf("[ERROR] Failed to map stack page\n");
            return ret;
        }
    }
    
    kprintf("[OK] User stack mapped at 0x08100000 (8KB)\n");
    
    kprintf("[USER] Verifying kernel mappings in user page directory...\n");
    
    /* 验证内核映射 */
    uint32_t *pd_virt = (uint32_t*)((uint32_t)pd + 0xC0000000);
    bool kernel_mapped = (pd_virt[768] & 0x1) != 0;  // 检查 3GB 处
    kprintf("[USER] Kernel space mapped: %s\n", kernel_mapped ? "YES" : "NO");
    
    if (!kernel_mapped) {
        kprintf("[ERROR] Kernel not mapped! Cannot switch page directory!\n");
        return -EINVAL;
    }
    
    kprintf("[USER] Ready to jump to user mode!\n");
    kprintf("[USER] User code entry: 0x08000000\n");
    kprintf("[USER] User stack top:   0x08100000\n");
    kprintf("[USER] Page directory:   0x%08x\n\n", (uint32_t)pd);
    
    kprintf("[USER] Executing user code (will call sys_exit(42))...\n");
    kprintf("[USER] Note: System will handle triple fault if something goes wrong\n\n");
    
    /* 刷新 TLB */
    asm volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
    
    /* 切换到用户页目录 */
    kprintf("[USER] Switching page directory...\n");
    asm volatile("mov %0, %%cr3" : : "r"(pd));
    kprintf("[OK] Page directory switched\n");
    
    /* 切换到用户态并执行 */
    asm volatile(
        /* 设置数据段为用户态 */
        "mov $0x23, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        /* 构造 IRET 栈帧 */
        "pushl $0x23\n"           // SS
        "pushl $0x08100000\n"     // ESP
        "pushf\n"                 // EFLAGS
        "popl %%eax\n"
        "orl $0x200, %%eax\n"     // IF=1
        "pushl %%eax\n"
        "pushl $0x1B\n"           // CS
        "pushl $0x08000000\n"     // EIP
        
        /* 跳转到用户态 */
        "iret\n"
        :
        :
        : "eax"
    );
    
    /* 不会执行到这里 */
    return 0;
}

