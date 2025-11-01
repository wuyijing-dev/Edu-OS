/*
 * test_simple.c - 最简单的用户程序，用于测试上下文切换
 * 不调用任何函数，只执行简单的循环
 */

void _start(void)
{
    /* 不做任何系统调用，只是简单的循环 */
    volatile int counter = 0;
    
    while (1) {
        counter++;
        
        /* 每计数到1000000时重置 */
        if (counter >= 1000000) {
            counter = 0;
        }
        
        /* 简单的延迟 */
        for (volatile int i = 0; i < 100; i++) {
            /* 空循环 */
        }
    }
    
    /* 永远不会到达这里 */
    while (1) {
        /* 死循环 */
    }
}
