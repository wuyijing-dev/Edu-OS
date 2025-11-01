/*
 * test_minimal.c - 最小的用户态测试程序
 * 
 * 这个程序只做一件事：执行HLT指令
 * 用于测试用户态切换是否成功
 */

void _start(void)
{
    /* 最简单的测试：直接停止 */
    __asm__ volatile("hlt");
    
    /* 如果能执行到这里，说明HLT被跳过了 */
    while (1) {
        __asm__ volatile("nop");
    }
}
