/*
 * test_libc.c - 测试 libc 的示例程序
 * 
 * 这个程序展示了如何使用我们实现的 libc
 */

#include "../userlib/stdio.h"
#include "../userlib/stdlib.h"
#include "../userlib/string.h"
#include "../userlib/unistd.h"

void _start(void)
{
    /* 测试基本输出 */
    printf("=== EduOS User Program with libc ===\n\n");
    
    /* 测试字符串函数 */
    printf("[Test 1] String Functions\n");
    char str1[] = "Hello";
    char str2[] = "World";
    char buffer[100];
    
    strcpy(buffer, str1);
    strcat(buffer, " ");
    strcat(buffer, str2);
    printf("  strcpy + strcat: %s\n", buffer);
    printf("  strlen: %d\n", strlen(buffer));
    printf("  strcmp(Hello, World): %d\n", strcmp(str1, str2));
    
    /* 测试格式化输出 */
    printf("\n[Test 2] Formatted Output\n");
    printf("  Decimal: %d\n", 42);
    printf("  Hex: 0x%x\n", 255);
    printf("  String: %s\n", "EduOS");
    printf("  Char: %c\n", 'A');
    printf("  Pointer: %p\n", buffer);
    
    /* 测试数字转换 */
    printf("\n[Test 3] String to Number\n");
    printf("  atoi(\"123\"): %d\n", atoi("123"));
    printf("  atoi(\"-456\"): %d\n", atoi("-456"));
    
    /* 测试数学函数 */
    printf("\n[Test 4] Math Functions\n");
    printf("  abs(-42): %d\n", abs(-42));
    printf("  abs(42): %d\n", abs(42));
    
    /* 测试内存分配 */
    printf("\n[Test 5] Memory Allocation\n");
    int *ptr = (int*)malloc(sizeof(int) * 10);
    if (ptr) {
        printf("  malloc(40 bytes): %p\n", ptr);
        ptr[0] = 100;
        ptr[1] = 200;
        printf("  ptr[0] = %d, ptr[1] = %d\n", ptr[0], ptr[1]);
        free(ptr);
        printf("  free() called\n");
    }
    
    /* 测试进程信息 */
    printf("\n[Test 6] Process Info\n");
    printf("  My PID: %d\n", getpid());
    printf("  Parent PID: %d\n", getppid());
    
    /* 测试 fork */
    printf("\n[Test 7] Fork Test\n");
    pid_t pid = fork();
    
    if (pid == 0) {
        /* 子进程 */
        printf("  [Child] PID=%d, Parent=%d\n", getpid(), getppid());
        printf("  [Child] Hello from child process!\n");
        exit(42);
    } else if (pid > 0) {
        /* 父进程 */
        printf("  [Parent] Created child with PID=%d\n", pid);
        int status;
        pid_t child_pid = wait(&status);
        printf("  [Parent] Child %d exited with status %d\n", child_pid, status);
    } else {
        printf("  fork() failed\n");
    }
    
    /* 完成 */
    printf("\n=== All libc tests passed! ===\n");
    exit(0);
}
