/*
 * fat32_lfn.c - FAT32 长文件名（LFN）支持
 * 
 * Windows 95 引入的长文件名扩展
 */

#include <fs/fat32.h>
#include <kernel.h>
#include <string.h>

/* ========== 长文件名结构 ========== */

#define LFN_MAX_LEN        255    /* 最大长文件名长度 */
#define LFN_CHARS_PER_ENTRY 13    /* 每个LFN条目包含13个字符 */
#define LFN_LAST_ENTRY     0x40   /* 最后一个LFN条目的标记 */

/*
 * 计算短文件名校验和
 */
static uint8_t lfn_checksum(const uint8_t *short_name)
{
    uint8_t sum = 0;
    
    for (int i = 0; i < 11; i++) {
        sum = ((sum & 1) << 7) + (sum >> 1) + short_name[i];
    }
    
    return sum;
}

/*
 * 从 LFN 条目提取字符
 */
static int lfn_extract_chars(struct fat32_lfn_entry *lfn, uint16_t *chars)
{
    int count = 0;
    
    /* 提取 name1（5 个字符） */
    for (int i = 0; i < 5; i++) {
        uint16_t ch = lfn->name1[i];
        if (ch == 0 || ch == 0xFFFF) {
            return count;
        }
        chars[count++] = ch;
    }
    
    /* 提取 name2（6 个字符） */
    for (int i = 0; i < 6; i++) {
        uint16_t ch = lfn->name2[i];
        if (ch == 0 || ch == 0xFFFF) {
            return count;
        }
        chars[count++] = ch;
    }
    
    /* 提取 name3（2 个字符） */
    for (int i = 0; i < 2; i++) {
        uint16_t ch = lfn->name3[i];
        if (ch == 0 || ch == 0xFFFF) {
            return count;
        }
        chars[count++] = ch;
    }
    
    return count;
}

/*
 * Unicode 转 ASCII（简化版）
 */
static void unicode_to_ascii(const uint16_t *unicode, int len, char *ascii)
{
    for (int i = 0; i < len; i++) {
        /* 简单处理：只取低字节 */
        ascii[i] = (char)(unicode[i] & 0xFF);
    }
    ascii[len] = '\0';
}

/*
 * ASCII 转 Unicode（简化版）
 */
static void ascii_to_unicode(const char *ascii, uint16_t *unicode, int max_len)
{
    int i;
    for (i = 0; i < max_len && ascii[i]; i++) {
        unicode[i] = (uint16_t)(unsigned char)ascii[i];
    }
    
    /* 填充剩余空间 */
    if (i < max_len) {
        unicode[i] = 0;  // 结束符
        i++;
        for (; i < max_len; i++) {
            unicode[i] = 0xFFFF;  // 填充
        }
    }
}

/* ========== 读取长文件名 ========== */

/*
 * 读取长文件名
 * 
 * @param entries: 目录项数组（从LFN开始）
 * @param max_entries: 最大条目数
 * @param name_out: 输出缓冲区
 * @return: LFN条目数，0表示没有LFN
 */
int fat32_read_lfn(
    struct fat32_dir_entry *entries,
    int max_entries,
    char *name_out)
{
    uint16_t lfn_buf[LFN_MAX_LEN];
    int lfn_pos = 0;
    int lfn_count = 0;
    uint8_t expected_order = 0;
    uint8_t checksum = 0;
    bool first = true;
    
    /* 向前扫描 LFN 条目 */
    for (int i = 0; i < max_entries; i++) {
        struct fat32_dir_entry *entry = &entries[i];
        
        /* 检查是否是 LFN 条目 */
        if (entry->attr != FAT_ATTR_LONG_NAME) {
            /* 到达短文件名条目 */
            if (lfn_count > 0) {
                /* 验证校验和 */
                uint8_t computed = lfn_checksum((uint8_t*)entry->name);
                if (computed != checksum) {
                    /* 校验和不匹配，忽略 LFN */
                    return 0;
                }
                
                /* 转换为 ASCII */
                unicode_to_ascii(lfn_buf, lfn_pos, name_out);
                return lfn_count;
            }
            break;
        }
        
        struct fat32_lfn_entry *lfn = (struct fat32_lfn_entry*)entry;
        
        /* 第一个 LFN 条目 */
        if (first) {
            if (!(lfn->order & LFN_LAST_ENTRY)) {
                /* 不是最后一个条目，格式错误 */
                return 0;
            }
            
            expected_order = lfn->order & 0x3F;
            checksum = lfn->checksum;
            first = false;
        } else {
            /* 检查顺序 */
            if (lfn->order != expected_order) {
                /* 顺序错误 */
                return 0;
            }
            
            /* 检查校验和 */
            if (lfn->checksum != checksum) {
                /* 校验和不一致 */
                return 0;
            }
        }
        
        /* 提取字符（插入到前面，因为LFN是反序的） */
        uint16_t chars[LFN_CHARS_PER_ENTRY];
        int char_count = lfn_extract_chars(lfn, chars);
        
        /* 插入到 lfn_buf 前面 */
        memmove(lfn_buf + char_count, lfn_buf, lfn_pos * sizeof(uint16_t));
        memcpy(lfn_buf, chars, char_count * sizeof(uint16_t));
        lfn_pos += char_count;
        
        lfn_count++;
        expected_order--;
        
        /* 是否是第一个（最后读取的）LFN 条目 */
        if (expected_order == 0) {
            break;
        }
    }
    
    return 0;  // 没有找到完整的 LFN
}

/* ========== 创建长文件名 ========== */

/*
 * 创建长文件名条目
 * 
 * @param name: 长文件名
 * @param short_name: 对应的短文件名
 * @param entries_out: 输出的目录项数组
 * @param max_entries: 最大条目数
 * @return: 创建的条目数（包括短文件名条目）
 */
int fat32_create_lfn(
    const char *name,
    const char *short_name,
    struct fat32_dir_entry *entries_out,
    int max_entries)
{
    int name_len = strlen(name);
    
    /* 如果名字符合 8.3 格式且全大写，不需要 LFN */
    const char *dot = strchr(name, '.');
    int base_len = dot ? (dot - name) : name_len;
    int ext_len = dot ? strlen(dot + 1) : 0;
    
    if (base_len <= 8 && ext_len <= 3) {
        bool need_lfn = false;
        for (int i = 0; name[i]; i++) {
            if (islower(name[i]) || name[i] == ' ') {
                need_lfn = true;
                break;
            }
        }
        
        if (!need_lfn) {
            return 0;  // 不需要 LFN
        }
    }
    
    /* 计算需要的 LFN 条目数 */
    int lfn_entries = (name_len + LFN_CHARS_PER_ENTRY - 1) / LFN_CHARS_PER_ENTRY;
    
    if (lfn_entries + 1 > max_entries) {
        return -EINVAL;  // 空间不足
    }
    
    /* 计算短文件名校验和 */
    uint8_t checksum = lfn_checksum((uint8_t*)short_name);
    
    /* 转换为 Unicode */
    uint16_t unicode_name[LFN_MAX_LEN + 1];
    ascii_to_unicode(name, unicode_name, name_len + 1);
    
    /* 创建 LFN 条目（从后往前） */
    for (int i = 0; i < lfn_entries; i++) {
        struct fat32_lfn_entry *lfn = (struct fat32_lfn_entry*)&entries_out[i];
        memset(lfn, 0, sizeof(struct fat32_lfn_entry));
        
        /* 设置序号 */
        lfn->order = lfn_entries - i;
        if (i == 0) {
            lfn->order |= LFN_LAST_ENTRY;  /* 标记最后一个 */
        }
        
        /* 设置属性和校验和 */
        lfn->attr = FAT_ATTR_LONG_NAME;
        lfn->type = 0;
        lfn->checksum = checksum;
        lfn->first_cluster_low = 0;
        
        /* 填充字符 */
        int char_start = (lfn_entries - i - 1) * LFN_CHARS_PER_ENTRY;
        
        /* name1 */
        for (int j = 0; j < 5; j++) {
            int idx = char_start + j;
            lfn->name1[j] = (idx <= name_len) ? unicode_name[idx] : 0xFFFF;
        }
        
        /* name2 */
        for (int j = 0; j < 6; j++) {
            int idx = char_start + 5 + j;
            lfn->name2[j] = (idx <= name_len) ? unicode_name[idx] : 0xFFFF;
        }
        
        /* name3 */
        for (int j = 0; j < 2; j++) {
            int idx = char_start + 11 + j;
            lfn->name3[j] = (idx <= name_len) ? unicode_name[idx] : 0xFFFF;
        }
    }
    
    return lfn_entries;
}

/*
 * 生成短文件名（数字尾缀方式）
 * 
 * @param long_name: 长文件名
 * @param short_name_out: 输出的短文件名（8.3格式）
 * @param tail_num: 尾缀数字（~1, ~2, ...）
 */
void fat32_generate_short_name(
    const char *long_name,
    char *short_name_out,
    int tail_num)
{
    memset(short_name_out, ' ', 11);
    
    /* 提取基本名和扩展名 */
    const char *dot = strrchr(long_name, '.');
    int base_len = dot ? (dot - long_name) : strlen(long_name);
    const char *ext = dot ? (dot + 1) : "";
    
    /* 转换基本名（只保留合法字符，转大写） */
    int j = 0;
    for (int i = 0; i < base_len && j < 6; i++) {
        char ch = toupper(long_name[i]);
        if (isalnum(ch) || ch == '_' || ch == '-') {
            short_name_out[j++] = ch;
        }
    }
    
    /* 添加数字尾缀 */
    if (tail_num > 0) {
        char tail[8];
        snprintf(tail, sizeof(tail), "~%d", tail_num);
        int tail_len = strlen(tail);
        
        /* 确保有空间 */
        if (j + tail_len > 8) {
            j = 8 - tail_len;
        }
        
        for (int i = 0; tail[i]; i++) {
            short_name_out[j++] = tail[i];
        }
    }
    
    /* 转换扩展名 */
    for (int i = 0; i < 3 && ext[i]; i++) {
        char ch = toupper(ext[i]);
        if (isalnum(ch) || ch == '_') {
            short_name_out[8 + i] = ch;
        }
    }
}

