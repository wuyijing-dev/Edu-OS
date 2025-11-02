/*
 * uaccess.c - 用户空间访问辅助函数
 * 
 * 类似Linux的uaccess.h，用于安全访问用户空间内存
 */

#include <types.h>
#include <process/process.h>
#include <kernel.h>
#include <string.h>

/* errno错误码 */
#define EFAULT  14

/*
 * is_user_address - 检查地址是否在用户空间
 * 
 * @addr: 要检查的地址
 * 
 * 返回：true=用户空间地址，false=内核地址
 */
bool is_user_address(const void *addr)
{
    uint32_t va = (uint32_t)addr;
    
    /* 用户空间：0x00000000 - 0xBFFFFFFF */
    /* 内核空间：0xC0000000 - 0xFFFFFFFF */
    return (va < 0xC0000000);
}

/*
 * is_user_buffer - 检查缓冲区是否完全在用户空间
 * 
 * @addr: 缓冲区起始地址
 * @size: 缓冲区大小
 * 
 * 返回：true=安全，false=跨越边界或在内核空间
 */
bool is_user_buffer(const void *addr, size_t size)
{
    uint32_t start = (uint32_t)addr;
    uint32_t end = start + size;
    
    /* 检查溢出 */
    if (end < start) {
        return false;
    }
    
    /* 检查是否完全在用户空间 */
    return (start < 0xC0000000) && (end <= 0xC0000000);
}

/*
 * copy_from_user - 从用户空间安全复制数据到内核空间
 * 
 * @to: 内核空间目标地址
 * @from: 用户空间源地址
 * @n: 字节数
 * 
 * 返回：0=成功，-EFAULT=失败
 */
int copy_from_user(void *to, const void *from, size_t n)
{
    if (!is_user_buffer(from, n)) {
        kprintf("[UACCESS] copy_from_user: Invalid user address %p (size=%u)\n", 
                from, n);
        return -EFAULT;
    }
    
    /* TODO: 添加页表检查，确保页面已映射 */
    
    /* 执行复制 */
    memcpy(to, from, n);
    return 0;
}

/*
 * copy_to_user - 从内核空间安全复制数据到用户空间
 * 
 * @to: 用户空间目标地址
 * @from: 内核空间源地址
 * @n: 字节数
 * 
 * 返回：0=成功，-EFAULT=失败
 */
int copy_to_user(void *to, const void *from, size_t n)
{
    if (!is_user_buffer(to, n)) {
        kprintf("[UACCESS] copy_to_user: Invalid user address %p (size=%u)\n", 
                to, n);
        return -EFAULT;
    }
    
    /* TODO: 添加页表检查，确保页面已映射且可写 */
    
    /* 执行复制 */
    memcpy(to, from, n);
    return 0;
}

/*
 * strncpy_from_user - 从用户空间安全复制字符串到内核空间
 * 
 * @dst: 内核空间目标地址
 * @src: 用户空间源地址
 * @count: 最大复制字节数
 * 
 * 返回：复制的字节数（不含'\0'），-EFAULT=失败
 */
long strncpy_from_user(char *dst, const char *src, long count)
{
    if (!is_user_address(src)) {
        kprintf("[UACCESS] strncpy_from_user: Invalid user address %p\n", src);
        return -EFAULT;
    }
    
    long copied = 0;
    
    /* 逐字节复制直到'\0'或达到count */
    while (copied < count) {
        /* 检查当前字符是否仍在用户空间 */
        if (!is_user_address(src + copied)) {
            return -EFAULT;
        }
        
        dst[copied] = src[copied];
        if (dst[copied] == '\0') {
            break;
        }
        copied++;
    }
    
    return copied;
}

/*
 * strnlen_user - 获取用户空间字符串长度
 * 
 * @str: 用户空间字符串地址
 * @count: 最大长度
 * 
 * 返回：字符串长度（含'\0'），-EFAULT=失败
 */
long strnlen_user(const char *str, long count)
{
    if (!is_user_address(str)) {
        return -EFAULT;
    }
    
    long len = 0;
    
    while (len < count) {
        if (!is_user_address(str + len)) {
            return -EFAULT;
        }
        
        if (str[len] == '\0') {
            return len + 1;  /* 包含'\0' */
        }
        len++;
    }
    
    return count;
}

/*
 * clear_user - 清零用户空间内存
 * 
 * @addr: 用户空间地址
 * @n: 字节数
 * 
 * 返回：0=成功，-EFAULT=失败
 */
int clear_user(void *addr, size_t n)
{
    if (!is_user_buffer(addr, n)) {
        return -EFAULT;
    }
    
    memset(addr, 0, n);
    return 0;
}

