/*
 * test_loop.c - 最简单的用户程序
 * 只执行死循环，不调用任何函数
 */

/* 入口函数必须是 _start */
void _start(void)
{
    /* 无限循环，不做任何系统调用 */
    while (1) {
        /* 使用内联汇编确保循环不被优化掉 */
        __asm__ __volatile__("nop");
    }
}
