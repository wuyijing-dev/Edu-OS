/*
 * GCC运行时库兼容函数
 * 
 * 提供64位整数除法/取模运算的支持函数
 * 在裸机环境下，GCC需要这些辅助函数来处理64位运算
 */

#include <types.h>

/*
 * 64位无符号除法
 * 
 * 算法：长除法（二进制）
 */
uint64_t __udivdi3(uint64_t dividend, uint64_t divisor)
{
    if (divisor == 0) {
        return 0;  // 除零保护
    }
    
    if (divisor > dividend) {
        return 0;
    }
    
    if (divisor == dividend) {
        return 1;
    }
    
    /* 快速路径：32位除法 */
    if ((dividend >> 32) == 0 && (divisor >> 32) == 0) {
        return (uint32_t)dividend / (uint32_t)divisor;
    }
    
    /* 长除法算法 */
    uint64_t quotient = 0;
    uint64_t remainder = 0;
    
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;
        
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1ULL << i);
        }
    }
    
    return quotient;
}

/*
 * 64位无符号取模
 */
uint64_t __umoddi3(uint64_t dividend, uint64_t divisor)
{
    if (divisor == 0) {
        return 0;  // 除零保护
    }
    
    if (divisor > dividend) {
        return dividend;
    }
    
    if (divisor == dividend) {
        return 0;
    }
    
    /* 快速路径：32位取模 */
    if ((dividend >> 32) == 0 && (divisor >> 32) == 0) {
        return (uint32_t)dividend % (uint32_t)divisor;
    }
    
    /* 长除法算法求余数 */
    uint64_t remainder = 0;
    
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;
        
        if (remainder >= divisor) {
            remainder -= divisor;
        }
    }
    
    return remainder;
}

/*
 * 64位有符号除法
 */
int64_t __divdi3(int64_t dividend, int64_t divisor)
{
    if (divisor == 0) {
        return 0;  // 除零保护
    }
    
    /* 处理符号 */
    int sign = 1;
    uint64_t udividend = dividend;
    uint64_t udivisor = divisor;
    
    if (dividend < 0) {
        sign = -sign;
        udividend = -dividend;
    }
    
    if (divisor < 0) {
        sign = -sign;
        udivisor = -divisor;
    }
    
    /* 使用无符号除法 */
    uint64_t result = __udivdi3(udividend, udivisor);
    
    return sign < 0 ? -(int64_t)result : (int64_t)result;
}

/*
 * 64位有符号取模
 */
int64_t __moddi3(int64_t dividend, int64_t divisor)
{
    if (divisor == 0) {
        return 0;  // 除零保护
    }
    
    /* 余数的符号与被除数相同 */
    int sign = 1;
    uint64_t udividend = dividend;
    uint64_t udivisor = divisor;
    
    if (dividend < 0) {
        sign = -1;
        udividend = -dividend;
    }
    
    if (divisor < 0) {
        udivisor = -divisor;
    }
    
    /* 使用无符号取模 */
    uint64_t result = __umoddi3(udividend, udivisor);
    
    return sign < 0 ? -(int64_t)result : (int64_t)result;
}

/*
 * 64位无符号左移（某些GCC版本需要）
 */
uint64_t __ashldi3(uint64_t value, int shift)
{
    if (shift >= 64) {
        return 0;
    }
    return value << shift;
}

/*
 * 64位无符号右移（某些GCC版本需要）
 */
uint64_t __lshrdi3(uint64_t value, int shift)
{
    if (shift >= 64) {
        return 0;
    }
    return value >> shift;
}

/*
 * 64位有符号右移（某些GCC版本需要）
 */
int64_t __ashrdi3(int64_t value, int shift)
{
    if (shift >= 64) {
        return value < 0 ? -1 : 0;
    }
    return value >> shift;
}

/*
 * 性能说明：
 * 
 * 1. 这些实现使用了简单的二进制长除法算法
 * 2. 对于32位以内的运算，使用了快速路径优化
 * 3. 在实际生产环境中，可以考虑更高效的算法（如牛顿迭代法）
 * 4. 对于嵌入式系统，也可以考虑使用硬件除法指令（如果CPU支持）
 * 
 * 教学价值：
 * - 理解为什么裸机环境需要这些辅助函数
 * - 学习二进制长除法算法
 * - 了解编译器的运行时支持机制
 */

