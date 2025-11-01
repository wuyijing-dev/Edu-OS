#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stdint.h>

/* PCI配置空间端口 */
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

/* PCI设备结构 */
struct pci_device {
    uint16_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
};

/* PCI配置空间读取 */
static inline uint32_t pci_read_config(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset)
{
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    
    /* 写入地址 */
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"((uint16_t)PCI_CONFIG_ADDRESS));
    
    /* 读取数据 */
    uint32_t data;
    __asm__ volatile("inl %1, %0" : "=a"(data) : "Nd"((uint16_t)PCI_CONFIG_DATA));
    
    return data;
}

/* PCI配置空间写入 */
static inline void pci_write_config(uint16_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value)
{
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) |
                                  (function << 8) | (offset & 0xFC) | 0x80000000);
    
    /* 写入地址 */
    __asm__ volatile("outl %0, %1" : : "a"(address), "Nd"((uint16_t)PCI_CONFIG_ADDRESS));
    
    /* 写入数据 */
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"((uint16_t)PCI_CONFIG_DATA));
}

#endif /* DRIVERS_PCI_H */

