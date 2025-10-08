/*
 * =============================================================================
 * 64位整数除法辅助函数
 * GCC在32位系统上进行64位除法时需要这些函数
 * =============================================================================
 */

#include "types.h"

/*
 * 无符号64位除法（简化实现）
 * 
 * 对于统计用途，我们可以使用以下策略：
 * 1. 如果除数 <= 32位，使用简化算法
 * 2. 如果被除数和除数都很小，直接用32位除法
 */
uint64_t __udivdi3(uint64_t num, uint64_t den)
{
    if (den == 0) {
        return 0;  // 除零保护
    }
    
    // 快速路径：除数是32位
    if (den <= 0xFFFFFFFFULL) {
        uint32_t den32 = (uint32_t)den;
        uint32_t high = (uint32_t)(num >> 32);
        uint32_t low = (uint32_t)num;
        
        if (high == 0) {
            // 被除数也是32位，直接除
            return low / den32;
        }
        
        // 被除数是64位，除数是32位
        // 使用两次32位除法
        uint32_t q_high = high / den32;
        uint32_t r_high = high % den32;
        
        // 组合余数和低位
        uint64_t mid = ((uint64_t)r_high << 32) | low;
        
        // 如果mid还是太大，需要进一步处理
        if ((mid >> 32) == 0) {
            // mid 在32位范围内
            uint32_t q_low = (uint32_t)mid / den32;
            return ((uint64_t)q_high << 32) | q_low;
        } else {
            // mid 超过32位，再分解一次
            uint32_t mid_high = (uint32_t)(mid >> 32);
            uint32_t mid_low = (uint32_t)mid;
            
            uint32_t q_mid = mid_high / den32;
            uint32_t r_mid = mid_high % den32;
            
            uint64_t final = ((uint64_t)r_mid << 32) | mid_low;
            uint32_t q_low = (uint32_t)final / den32;
            
            return ((uint64_t)q_high << 32) | ((uint64_t)q_mid << 32) | q_low;
        }
    }
    
    // 除数也是64位：结果只能是0或1
    return num >= den ? 1 : 0;
}

/*
 * 有符号64位除法
 */
int64_t __divdi3(int64_t num, int64_t den)
{
    int sign = 0;
    
    if (num < 0) {
        num = -num;
        sign = !sign;
    }
    
    if (den < 0) {
        den = -den;
        sign = !sign;
    }
    
    uint64_t result = __udivdi3((uint64_t)num, (uint64_t)den);
    
    return sign ? -(int64_t)result : (int64_t)result;
}

/*
 * 无符号64位取模
 */
uint64_t __umoddi3(uint64_t num, uint64_t den)
{
    uint64_t quot = __udivdi3(num, den);
    return num - quot * den;
}

/*
 * 有符号64位取模
 */
int64_t __moddi3(int64_t num, int64_t den)
{
    int sign = 0;
    
    if (num < 0) {
        num = -num;
        sign = 1;
    }
    
    if (den < 0) {
        den = -den;
    }
    
    uint64_t result = __umoddi3((uint64_t)num, (uint64_t)den);
    
    return sign ? -(int64_t)result : (int64_t)result;
}

