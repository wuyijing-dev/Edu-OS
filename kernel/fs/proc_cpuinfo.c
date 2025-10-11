/*
 * proc_cpuinfo.c - /proc/cpuinfo 实现
 * 
 * 显示 CPU 信息（Linux风格）
 */

#include <fs/procfs.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <string.h>

/*
 * 读取 CPU 信息
 */
int proc_cpuinfo_read(char *buf, size_t size, off_t *offset, void *data)
{
    (void)data;
    
    /* 生成 CPU 信息 */
    char content[1024];
    int len = snprintf(content, sizeof(content),
        "processor\t: 0\n"
        "vendor_id\t: EduOS\n"
        "cpu family\t: 6\n"
        "model\t\t: 0\n"
        "model name\t: EduOS Virtual CPU @ %.2f MHz\n"
        "stepping\t: 0\n"
        "microcode\t: 0x1\n"
        "cpu MHz\t\t: %.2f\n"
        "cache size\t: 256 KB\n"
        "physical id\t: 0\n"
        "siblings\t: 1\n"
        "core id\t\t: 0\n"
        "cpu cores\t: 1\n"
        "apicid\t\t: 0\n"
        "initial apicid\t: 0\n"
        "fpu\t\t: yes\n"
        "fpu_exception\t: yes\n"
        "cpuid level\t: 13\n"
        "wp\t\t: yes\n"
        "flags\t\t: fpu tsc msr pae mce cx8 apic\n"
        "bogomips\t: %.2f\n"
        "clflush size\t: 64\n"
        "cache_alignment\t: 64\n"
        "address sizes\t: 32 bits physical, 32 bits virtual\n"
        "power management:\n"
        "\n",
        (float)TIMER_FREQUENCY_HZ / 1000000.0,
        (float)TIMER_FREQUENCY_HZ / 1000000.0,
        (float)TIMER_FREQUENCY_HZ / 500000.0  // bogomips
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

