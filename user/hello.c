/*
 * hello.c - 简单的用户态程序
 * 
 * 演示系统调用的使用
 */

/* 系统调用号 */
#define SYS_exit    1
#define SYS_write   4

/* 系统调用包装函数 */
static inline int syscall1(int num, int arg1)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1)
    );
    return ret;
}

static inline int syscall3(int num, int arg1, int arg2, int arg3)
{
    int ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
    );
    return ret;
}

/* 包装函数 */
void exit(int status)
{
    syscall1(SYS_exit, status);
    while(1);  /* 不应该执行到这里 */
}

int write(int fd, const char *buf, int count)
{
    return syscall3(SYS_write, fd, (int)buf, count);
}

/* 简单的字符串长度 */
int strlen(const char *s)
{
    int len = 0;
    while (s[len]) len++;
    return len;
}

/* 用户程序入口 */
void _start(void)
{
    const char *msg = "Hello from user mode!\n";
    write(1, msg, strlen(msg));  /* 写到标准输出 */
    
    exit(0);  /* 正常退出 */
}

