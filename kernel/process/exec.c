/*
 * exec.c - 执行新程序（execve 系统调用）
 */

#include <process/process.h>
#include <elf.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <kernel.h>
#include <string.h>

/* 外部函数 */
extern int elf_load(const char *path, uint32_t *entry);

/*
 * execve 系统调用
 * 
 * 用新程序替换当前进程
 */
int do_execve(const char *path, char *const argv[], char *const envp[])
{
    struct process *current = process_get_current();
    
    if (!current) {
        return -ESRCH;
    }
    
    if (!path) {
        return -EINVAL;
    }
    
    kprintf("[EXEC] Executing: %s (PID %u)\n", path, current->pid);
    
    /* 加载 ELF */
    uint32_t entry = 0;
    int ret = elf_load(path, &entry);
    
    if (ret < 0) {
        kprintf("[EXEC] Failed to load ELF: %d\n", ret);
        return ret;
    }
    
    /* 清空当前进程的地址空间 */
    /* TODO: 实现地址空间清理 */
    
    /* 创建新的用户栈 */
    /* TODO: 实现用户栈创建 */
    
    /* 设置新的入口点 */
    current->context.eip = entry;
    
    /* 复制参数和环境变量到用户栈 */
    /* TODO: 实现参数复制 */
    (void)argv;
    (void)envp;
    
    kprintf("[EXEC] Program loaded, entry: 0x%08x\n", entry);
    
    /* execve 成功后不返回 */
    /* 直接跳转到新程序 */
    
    return 0;
}

