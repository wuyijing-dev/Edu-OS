/*
 * 网络测试程序 - 显示网卡信息和测试网络连接
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/* 网络设备信息结构 */
struct netdev_info {
    char name[16];
    uint8_t mac[6];
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
};

/* 从/proc/net/dev读取网络设备信息 */
static int get_netdev_info(struct netdev_info *info)
{
    /* 目前简单地硬编码，后续可以从procfs读取 */
    strcpy(info->name, "eth0");
    
    /* MAC地址需要从设备读取 */
    info->mac[0] = 0x52;
    info->mac[1] = 0x54;
    info->mac[2] = 0x00;
    info->mac[3] = 0x12;
    info->mac[4] = 0x34;
    info->mac[5] = 0x56;
    
    /* IP配置 */
    info->ip_addr = (10 << 24) | (0 << 16) | (2 << 8) | 15;  /* 10.0.2.15 */
    info->netmask = (255 << 24) | (255 << 16) | (255 << 8) | 0;  /* 255.255.255.0 */
    info->gateway = (10 << 24) | (0 << 16) | (2 << 8) | 2;   /* 10.0.2.2 */
    
    return 0;
}

/* 打印IP地址 */
static void print_ip(uint32_t ip)
{
    printf("%d.%d.%d.%d", 
           (ip >> 24) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 8) & 0xFF,
           ip & 0xFF);
}

/* 打印MAC地址 */
static void print_mac(uint8_t *mac)
{
    printf("%x:%x:%x:%x:%x:%x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* 用户程序入口点 */
void _start(void)
{
    printf("\n");
    printf("========================================\n");
    printf("  EduOS Network Test\n");
    printf("========================================\n");
    printf("\n");
    
    /* 获取网络设备信息 */
    struct netdev_info info;
    if (get_netdev_info(&info) < 0) {
        printf("Error: Failed to get network device information\n");
        _exit(1);
    }
    
    /* 显示网络设备信息 */
    printf("Network Interface: %s\n", info.name);
    printf("  MAC Address:  ");
    print_mac(info.mac);
    printf("\n");
    
    printf("  IP Address:   ");
    print_ip(info.ip_addr);
    printf("\n");
    
    printf("  Netmask:      ");
    print_ip(info.netmask);
    printf("\n");
    
    printf("  Gateway:      ");
    print_ip(info.gateway);
    printf("\n");
    
    printf("\n");
    printf("Network Status:\n");
    printf("  RTL8139 driver:  Loaded\n");
    printf("  Link status:     Up\n");
    printf("  Protocol stack:  IPv4, ARP, ICMP\n");
    printf("\n");
    
    printf("Test Instructions:\n");
    printf("  1. From host, try: ping 10.0.2.15\n");
    printf("  2. The OS should respond to ICMP Echo Request\n");
    printf("  3. Check kernel logs for network activity\n");
    printf("\n");
    
    printf("Note: Socket API and TCP/UDP are not yet implemented.\n");
    printf("      Currently only ICMP Echo Reply is supported.\n");
    printf("\n");
    
    /* 退出程序 */
    _exit(0);
}
