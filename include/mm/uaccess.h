/*
 * uaccess.h - 用户空间访问辅助函数（Linux风格）
 * 
 * 提供安全访问用户空间内存的接口
 */

#ifndef _MM_UACCESS_H
#define _MM_UACCESS_H

#include <types.h>

/*
 * 地址验证
 */
bool is_user_address(const void *addr);
bool is_user_buffer(const void *addr, size_t size);

/*
 * 安全内存复制
 */
int copy_from_user(void *to, const void *from, size_t n);
int copy_to_user(void *to, const void *from, size_t n);

/*
 * 字符串操作
 */
long strncpy_from_user(char *dst, const char *src, long count);
long strnlen_user(const char *str, long count);

/*
 * 内存清零
 */
int clear_user(void *addr, size_t n);

/*
 * 便捷宏（类似Linux）
 */
#define get_user(x, ptr) \
    ({ \
        typeof(*(ptr)) __tmp; \
        int __ret = copy_from_user(&__tmp, ptr, sizeof(*ptr)); \
        (x) = __tmp; \
        __ret; \
    })

#define put_user(x, ptr) \
    ({ \
        typeof(*(ptr)) __tmp = (x); \
        copy_to_user(ptr, &__tmp, sizeof(*ptr)); \
    })

#endif /* _MM_UACCESS_H */



