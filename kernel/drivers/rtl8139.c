/*
 * RTL8139网卡驱动 - 类似Linux drivers/net/ethernet/realtek/8139too.c
 */

#include <drivers/rtl8139.h>
#include <drivers/pci.h>
#include <net/netdev.h>
#include <net/ethernet.h>
#include <net/ip.h>
#include <mm/kmalloc.h>
#include <mm/pmm.h>
#include <io.h>
#include <arch/i386/irq.h>
#include <string.h>

/* 全局网络设备 */
static struct net_device *rtl8139_dev = NULL;
static struct rtl8139_private *rtl8139_priv = NULL;

/*
 * 读取RTL8139寄存器
 */
static inline uint8_t rtl8139_read8(struct rtl8139_private *priv, uint16_t reg)
{
    return inb(priv->iobase + reg);
}

static inline uint16_t rtl8139_read16(struct rtl8139_private *priv, uint16_t reg)
{
    return inw(priv->iobase + reg);
}

static inline uint32_t rtl8139_read32(struct rtl8139_private *priv, uint16_t reg)
{
    return inl(priv->iobase + reg);
}

/*
 * 写入RTL8139寄存器
 */
static inline void rtl8139_write8(struct rtl8139_private *priv, uint16_t reg, uint8_t val)
{
    outb(priv->iobase + reg, val);
}

static inline void rtl8139_write16(struct rtl8139_private *priv, uint16_t reg, uint16_t val)
{
    outw(priv->iobase + reg, val);
}

static inline void rtl8139_write32(struct rtl8139_private *priv, uint16_t reg, uint32_t val)
{
    outl(priv->iobase + reg, val);
}

/*
 * RTL8139中断处理
 */
static void rtl8139_interrupt_handler(struct interrupt_frame *frame)
{
    (void)frame;
    
    if (!rtl8139_dev || !rtl8139_priv) {
        return;
    }
    
    struct net_device *dev = rtl8139_dev;
    struct rtl8139_private *priv = rtl8139_priv;
    
    /* 读取中断状态 */
    uint16_t status = rtl8139_read16(priv, RTL8139_ISR);
    
    /* 清除中断 */
    rtl8139_write16(priv, RTL8139_ISR, status);
    
    /* 处理接收中断 */
    if (status & RTL8139_INT_ROK) {
        /* 读取接收缓冲区 */
        while ((rtl8139_read8(priv, RTL8139_CR) & RTL8139_CR_BUFE) == 0) {
            uint32_t rx_status = *(uint32_t *)(priv->rx_buffer + priv->cur_rx);
            uint32_t rx_size = rx_status >> 16;
            
            if (rx_status & 0x01) {  /* 接收OK */
                /* 分配sk_buff */
                struct sk_buff *skb = alloc_skb(rx_size + 2, 0);
                if (skb) {
                    skb->dev = dev;
                    skb_reserve(skb, 2);  /* 16字节对齐 */
                    
                    /* 复制数据 */
                    uint8_t *data = skb_put(skb, rx_size - 4);  /* 去除CRC */
                    memcpy(data, priv->rx_buffer + priv->cur_rx + 4, rx_size - 4);
                    
                    /* 提交到网络层 */
                    netif_rx(skb);
                }
            }
            
            /* 更新读指针 */
            priv->cur_rx = (priv->cur_rx + rx_size + 4 + 3) & ~3;
            rtl8139_write16(priv, RTL8139_CAPR, priv->cur_rx - 16);
        }
    }
    
    /* 处理发送中断 */
    if (status & RTL8139_INT_TOK) {
        /* 发送完成，释放缓冲区 */
        priv->dirty_tx++;
    }
}

/*
 * 打开网络设备
 */
static int rtl8139_open(struct net_device *dev)
{
    struct rtl8139_private *priv = (struct rtl8139_private *)dev->priv;
    
    /* 启用接收和发送 */
    rtl8139_write8(priv, RTL8139_CR, RTL8139_CR_RE | RTL8139_CR_TE);
    
    /* 配置接收模式 */
    rtl8139_write32(priv, RTL8139_RCR, 
                    RTL8139_RCR_AB | RTL8139_RCR_AM | RTL8139_RCR_APM | RTL8139_RCR_AAP);
    
    /* 配置发送模式 */
    rtl8139_write32(priv, RTL8139_TCR, RTL8139_TCR_IFG96);
    
    /* 启用中断 */
    rtl8139_write16(priv, RTL8139_IMR, 
                    RTL8139_INT_ROK | RTL8139_INT_TOK | RTL8139_INT_RER | RTL8139_INT_TER);
    
    dev->flags |= NETDEV_UP | NETDEV_RUNNING;
    
    return 0;
}

/*
 * 关闭网络设备
 */
static int rtl8139_stop(struct net_device *dev)
{
    struct rtl8139_private *priv = (struct rtl8139_private *)dev->priv;
    
    /* 禁用中断 */
    rtl8139_write16(priv, RTL8139_IMR, 0);
    
    /* 停止接收和发送 */
    rtl8139_write8(priv, RTL8139_CR, 0);
    
    dev->flags &= ~(NETDEV_UP | NETDEV_RUNNING);
    
    return 0;
}

/*
 * 发送数据包
 */
static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct rtl8139_private *priv = (struct rtl8139_private *)dev->priv;
    
    /* 选择发送描述符 */
    uint32_t entry = priv->cur_tx % 4;
    
    /* 复制数据到发送缓冲区 */
    memcpy(priv->tx_buffer[entry], skb->data, skb->len);
    
    /* 设置发送地址 */
    rtl8139_write32(priv, RTL8139_TSAD0 + entry * 4, priv->tx_buffer_phys[entry]);
    
    /* 设置发送长度并启动发送 */
    rtl8139_write32(priv, RTL8139_TSD0 + entry * 4, skb->len);
    
    priv->cur_tx++;
    
    free_skb(skb);
    return 0;
}

/*
 * 网络设备操作接口
 */
static const struct net_device_ops rtl8139_netdev_ops = {
    .ndo_open = rtl8139_open,
    .ndo_stop = rtl8139_stop,
    .ndo_start_xmit = rtl8139_start_xmit,
};

/*
 * 初始化RTL8139设备
 */
static int rtl8139_probe(struct pci_device *pdev)
{
    /* 分配网络设备 */
    struct net_device *dev = kmalloc(sizeof(struct net_device));
    if (!dev) {
        return -1;
    }
    memset(dev, 0, sizeof(struct net_device));
    
    /* 分配私有数据 */
    struct rtl8139_private *priv = kmalloc(sizeof(struct rtl8139_private));
    if (!priv) {
        kfree(dev);
        return -1;
    }
    memset(priv, 0, sizeof(struct rtl8139_private));
    
    dev->priv = priv;
    dev->netdev_ops = &rtl8139_netdev_ops;
    strcpy(dev->name, "eth0");
    
    /* 获取IO基地址 */
    priv->iobase = pci_read_config(pdev->bus, pdev->device, pdev->function, 0x10) & ~3;
    priv->irq = pci_read_config(pdev->bus, pdev->device, pdev->function, 0x3C) & 0xFF;
    
    /* 启用PCI总线主控 */
    uint16_t cmd = pci_read_config(pdev->bus, pdev->device, pdev->function, 0x04);
    cmd |= 0x05;  /* IO Space + Bus Master */
    pci_write_config(pdev->bus, pdev->device, pdev->function, 0x04, cmd);
    
    /* 软复位 */
    rtl8139_write8(priv, RTL8139_CR, RTL8139_CR_RST);
    while (rtl8139_read8(priv, RTL8139_CR) & RTL8139_CR_RST);
    
    /* 读取MAC地址 */
    for (int i = 0; i < ETH_ALEN; i++) {
        dev->dev_addr[i] = rtl8139_read8(priv, RTL8139_IDR0 + i);
    }
    
    /* 分配接收缓冲区 */
    priv->rx_buffer = kmalloc(RTL8139_RX_BUF_SIZE + 16);
    if (!priv->rx_buffer) {
        kfree(priv);
        kfree(dev);
        return -1;
    }
    priv->rx_buffer_phys = (uint32_t)priv->rx_buffer;  /* TODO: 虚拟地址转物理地址 */
    priv->cur_rx = 0;
    
    /* 分配发送缓冲区 */
    for (int i = 0; i < 4; i++) {
        priv->tx_buffer[i] = kmalloc(RTL8139_TX_BUF_SIZE);
        if (!priv->tx_buffer[i]) {
            for (int j = 0; j < i; j++) {
                kfree(priv->tx_buffer[j]);
            }
            kfree(priv->rx_buffer);
            kfree(priv);
            kfree(dev);
            return -1;
        }
        priv->tx_buffer_phys[i] = (uint32_t)priv->tx_buffer[i];  /* TODO: 虚拟地址转物理地址 */
    }
    priv->cur_tx = 0;
    priv->dirty_tx = 0;
    
    /* 设置接收缓冲区地址 */
    rtl8139_write32(priv, RTL8139_RBSTART, priv->rx_buffer_phys);
    
    /* 保存全局指针 */
    rtl8139_priv = priv;
    
    /* 注册中断处理 */
    irq_install_handler(priv->irq, rtl8139_interrupt_handler);
    
    /* 注册网络设备 */
    register_netdev(dev);
    
    /* 配置IP地址 (默认) */
    dev->ip_addr = IPADDR(10, 0, 2, 15);
    dev->netmask = IPADDR(255, 255, 255, 0);
    dev->gateway = IPADDR(10, 0, 2, 2);
    
    /* 打开设备 */
    rtl8139_open(dev);
    
    rtl8139_dev = dev;
    
    return 0;
}

/*
 * RTL8139驱动初始化
 */
int rtl8139_init(void)
{
    extern void kprintf(const char *fmt, ...);
    
    kprintf("[RTL8139] Scanning PCI bus for RTL8139 (vendor=0x%x, device=0x%x)...\n", 
            RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    
    /* 扫描PCI总线查找RTL8139设备 (只扫描总线0) */
    for (uint8_t device = 0; device < 32; device++) {
        for (uint8_t function = 0; function < 8; function++) {
            uint16_t vendor = pci_read_config(0, device, function, 0x00) & 0xFFFF;
            if (vendor == 0xFFFF || vendor == 0x0000) {
                continue;
            }
            
            uint16_t device_id = (pci_read_config(0, device, function, 0x00) >> 16) & 0xFFFF;
            
            kprintf("[RTL8139] Found PCI device: bus=0, dev=%d, func=%d, vendor=0x%04x, device=0x%04x\n",
                    device, function, vendor, device_id);
            
            if (vendor == RTL8139_VENDOR_ID && device_id == RTL8139_DEVICE_ID) {
                kprintf("[RTL8139] ✓ Found RTL8139 network card!\n");
                struct pci_device pdev = {
                    .bus = 0,
                    .device = device,
                    .function = function,
                    .vendor_id = vendor,
                    .device_id = device_id
                };
                return rtl8139_probe(&pdev);
            }
        }
    }
    
    kprintf("[RTL8139] No RTL8139 found on PCI bus\n");
    return -1;
}

