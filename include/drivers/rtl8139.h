#ifndef DRIVERS_RTL8139_H
#define DRIVERS_RTL8139_H

#include <stdint.h>

/* RTL8139 PCI设备ID */
#define RTL8139_VENDOR_ID   0x10EC
#define RTL8139_DEVICE_ID   0x8139

/* RTL8139寄存器偏移 */
#define RTL8139_IDR0        0x00    /* MAC地址 */
#define RTL8139_MAR0        0x08    /* 多播地址 */
#define RTL8139_TSD0        0x10    /* 发送状态 */
#define RTL8139_TSAD0       0x20    /* 发送地址 */
#define RTL8139_RBSTART     0x30    /* 接收缓冲区起始地址 */
#define RTL8139_CR          0x37    /* 命令寄存器 */
#define RTL8139_CAPR        0x38    /* 当前地址读指针 */
#define RTL8139_CBR         0x3A    /* 当前缓冲区地址 */
#define RTL8139_IMR         0x3C    /* 中断屏蔽寄存器 */
#define RTL8139_ISR         0x3E    /* 中断状态寄存器 */
#define RTL8139_TCR         0x40    /* 发送配置寄存器 */
#define RTL8139_RCR         0x44    /* 接收配置寄存器 */
#define RTL8139_CONFIG1     0x52    /* 配置寄存器1 */

/* 命令寄存器位 */
#define RTL8139_CR_RST      0x10    /* 复位 */
#define RTL8139_CR_RE       0x08    /* 接收使能 */
#define RTL8139_CR_TE       0x04    /* 发送使能 */
#define RTL8139_CR_BUFE     0x01    /* 缓冲区空 */

/* 中断状态/屏蔽位 */
#define RTL8139_INT_ROK     0x0001  /* 接收OK */
#define RTL8139_INT_RER     0x0002  /* 接收错误 */
#define RTL8139_INT_TOK     0x0004  /* 发送OK */
#define RTL8139_INT_TER     0x0008  /* 发送错误 */
#define RTL8139_INT_RXOVW   0x0010  /* 接收溢出 */
#define RTL8139_INT_PUN     0x0020  /* 链路改变 */
#define RTL8139_INT_FOVW    0x0040  /* FIFO溢出 */
#define RTL8139_INT_SERR    0x8000  /* 系统错误 */

/* 接收配置寄存器位 */
#define RTL8139_RCR_AAP     0x00000001  /* 接受所有包 */
#define RTL8139_RCR_APM     0x00000002  /* 接受物理匹配 */
#define RTL8139_RCR_AM      0x00000004  /* 接受多播 */
#define RTL8139_RCR_AB      0x00000008  /* 接受广播 */
#define RTL8139_RCR_AR      0x00000010  /* 接受错误包 */
#define RTL8139_RCR_WRAP    0x00000080  /* 环绕模式 */

/* 发送配置寄存器位 */
#define RTL8139_TCR_CLRABT  0x00000001  /* 清除中止 */
#define RTL8139_TCR_IFG96   0x03000000  /* 帧间隙 */

/* 缓冲区大小 */
#define RTL8139_RX_BUF_SIZE (8192 + 16 + 1500)
#define RTL8139_TX_BUF_SIZE 1536

/* RTL8139私有数据 */
struct rtl8139_private {
    uint32_t iobase;            /* IO基地址 */
    uint8_t irq;                /* IRQ号 */
    
    /* 接收缓冲区 */
    uint8_t *rx_buffer;
    uint32_t rx_buffer_phys;
    uint32_t cur_rx;
    
    /* 发送缓冲区 */
    uint8_t *tx_buffer[4];
    uint32_t tx_buffer_phys[4];
    uint32_t cur_tx;
    uint32_t dirty_tx;
};

/* RTL8139驱动初始化 */
int rtl8139_init(void);

#endif /* DRIVERS_RTL8139_H */

