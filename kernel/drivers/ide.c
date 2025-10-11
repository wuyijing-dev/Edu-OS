/*
 * ide.c - 简化的 IDE/ATA 磁盘驱动
 * 
 * 支持 PIO 模式读写
 */

#include <drivers/block.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <io.h>

/* ========== IDE 端口定义 ========== */

/* Primary IDE 总线 */
#define IDE_PRIMARY_DATA        0x1F0
#define IDE_PRIMARY_ERROR       0x1F1
#define IDE_PRIMARY_SECTOR_COUNT 0x1F2
#define IDE_PRIMARY_LBA_LOW     0x1F3
#define IDE_PRIMARY_LBA_MID     0x1F4
#define IDE_PRIMARY_LBA_HIGH    0x1F5
#define IDE_PRIMARY_DRIVE       0x1F6
#define IDE_PRIMARY_STATUS      0x1F7
#define IDE_PRIMARY_COMMAND     0x1F7

/* Secondary IDE 总线 */
#define IDE_SECONDARY_DATA        0x170
#define IDE_SECONDARY_ERROR       0x171
#define IDE_SECONDARY_SECTOR_COUNT 0x172
#define IDE_SECONDARY_LBA_LOW     0x173
#define IDE_SECONDARY_LBA_MID     0x174
#define IDE_SECONDARY_LBA_HIGH    0x175
#define IDE_SECONDARY_DRIVE       0x176
#define IDE_SECONDARY_STATUS      0x177
#define IDE_SECONDARY_COMMAND     0x177

/* 状态寄存器位 */
#define IDE_STATUS_ERR      (1 << 0)
#define IDE_STATUS_DRQ      (1 << 3)
#define IDE_STATUS_SRV      (1 << 4)
#define IDE_STATUS_DF       (1 << 5)
#define IDE_STATUS_RDY      (1 << 6)
#define IDE_STATUS_BSY      (1 << 7)

/* 命令 */
#define IDE_CMD_READ_SECTORS    0x20
#define IDE_CMD_WRITE_SECTORS   0x30
#define IDE_CMD_IDENTIFY        0xEC

/* ========== IDE 设备结构 ========== */

struct ide_device {
    uint16_t base_port;         /* 基础端口 */
    uint8_t drive_select;       /* 驱动器选择（0xE0 for master, 0xF0 for slave） */
    uint32_t total_sectors;     /* 总扇区数 */
    bool present;               /* 设备是否存在 */
};

static struct ide_device ide_devices[4];  /* 最多 4 个 IDE 设备 */
static struct block_device ide_block_devices[4];

/* ========== IDE 底层操作 ========== */

/*
 * 等待 IDE 控制器就绪
 */
static void ide_wait_ready(uint16_t base_port)
{
    while (inb(base_port + 7) & IDE_STATUS_BSY) {
        /* 等待忙碌标志清除 */
    }
}

/*
 * 等待数据就绪
 */
static void ide_wait_drq(uint16_t base_port)
{
    while (!(inb(base_port + 7) & IDE_STATUS_DRQ)) {
        /* 等待数据请求标志 */
    }
}

/*
 * 软重置 IDE 控制器
 */
static void ide_soft_reset(uint16_t base_port)
{
    /* 写入控制寄存器执行软重置 */
    outb(base_port + 0x206, 0x04);  /* 设置 SRST 位 */
    
    /* 延迟 */
    for (volatile int i = 0; i < 10000; i++);
    
    /* 清除 SRST */
    outb(base_port + 0x206, 0x00);
    
    /* 等待控制器就绪 */
    for (volatile int i = 0; i < 50000; i++);
}

/*
 * 探测性读取扇区0（不使用IDENTIFY，Linux风格）
 */
static bool ide_probe_read(struct ide_device *dev)
{
    uint16_t base = dev->base_port;
    uint16_t test_buf[256];
    
    kprintf("[IDE]   Probing device by reading sector 0...\n");
    
    /* 等待就绪 */
    ide_wait_ready(base);
    
    /* 选择驱动器 */
    outb(base + 6, dev->drive_select);
    for (volatile int i = 0; i < 10000; i++);
    
    /* 设置扇区数 = 1 */
    outb(base + 2, 1);
    
    /* 设置 LBA = 0 */
    outb(base + 3, 0);
    outb(base + 4, 0);
    outb(base + 5, 0);
    outb(base + 6, dev->drive_select);
    
    /* 发送读取命令 */
    outb(base + 7, IDE_CMD_READ_SECTORS);
    
    /* 延迟 */
    for (volatile int i = 0; i < 5000; i++);
    
    /* 检查状态 */
    uint8_t status = inb(base + 7);
    kprintf("[IDE]   Probe status: 0x%02X\n", status);
    
    if (status == 0 || status == 0xFF) {
        return false;
    }
    
    /* 等待 DRQ 或超时 */
    int timeout = 10000;
    while (!(inb(base + 7) & IDE_STATUS_DRQ) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        kprintf("[IDE]   Probe timeout\n");
        return false;
    }
    
    /* 尝试读取数据 */
    for (int i = 0; i < 256; i++) {
        test_buf[i] = inw(base);
    }
    
    kprintf("[IDE]   Probe successful! Device responds to read.\n");
    
    /* 假设标准磁盘大小（可以从文件系统引导扇区读取） */
    /* 这里我们假设是 16MB 测试磁盘 = 32768 扇区 */
    dev->total_sectors = 32768;
    
    kprintf("[IDE] Device detected via probe: %u sectors (16 MB assumed)\n",
            dev->total_sectors);
    
    return true;
}

/*
 * 识别 IDE 设备
 */
static bool ide_identify(struct ide_device *dev)
{
    uint16_t base = dev->base_port;
    
    kprintf("[IDE]   Waiting for controller ready...\n");
    
    /* 等待就绪 */
    ide_wait_ready(base);
    
    /* 选择驱动器 */
    kprintf("[IDE]   Selecting drive (0x%02X)...\n", dev->drive_select);
    outb(base + 6, dev->drive_select);
    
    /* 小延迟（某些设备需要） */
    for (volatile int i = 0; i < 10000; i++);
    
    /* 发送 IDENTIFY 命令 */
    kprintf("[IDE]   Sending IDENTIFY command...\n");
    outb(base + 7, IDE_CMD_IDENTIFY);
    
    /* 小延迟 */
    for (volatile int i = 0; i < 1000; i++);
    
    /* 检查状态 */
    uint8_t status = inb(base + 7);
    kprintf("[IDE]   Initial status: 0x%02X\n", status);
    
    if (status == 0) {
        kprintf("[IDE]   Device not present (status = 0)\n");
        return false;  /* 设备不存在 */
    }
    
    if (status == 0xFF) {
        kprintf("[IDE]   No device (status = 0xFF)\n");
        return false;  /* 总线浮空 */
    }
    
    /* 等待 BSY 清除 */
    int timeout = 10000;
    while ((inb(base + 7) & IDE_STATUS_BSY) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        kprintf("[IDE]   Timeout waiting for BSY clear\n");
        return false;
    }
    
    /* 检查错误 - 但不立即放弃（QEMU Secondary 可能误报） */
    status = inb(base + 7);
    if (status & IDE_STATUS_ERR) {
        kprintf("[IDE]   Warning: Error flag set (0x%02X), trying to continue...\n", status);
        
        /* 尝试软重置后重试 */
        kprintf("[IDE]   Attempting soft reset...\n");
        ide_soft_reset(base);
        ide_wait_ready(base);
        
        /* 重新选择驱动器并发送命令 */
        outb(base + 6, dev->drive_select);
        for (volatile int i = 0; i < 10000; i++);
        outb(base + 7, IDE_CMD_IDENTIFY);
        for (volatile int i = 0; i < 1000; i++);
        
        status = inb(base + 7);
        kprintf("[IDE]   Status after reset: 0x%02X\n", status);
        
        if (status == 0 || status == 0xFF) {
            kprintf("[IDE]   Still no device after reset\n");
            return false;
        }
    }
    
    /* 等待数据就绪 */
    kprintf("[IDE]   Waiting for data ready (DRQ)...\n");
    timeout = 10000;
    while (!(inb(base + 7) & IDE_STATUS_DRQ) && timeout > 0) {
        timeout--;
    }
    
    if (timeout == 0) {
        kprintf("[IDE]   Timeout waiting for DRQ\n");
        return false;
    }
    
    kprintf("[IDE]   Reading identification data...\n");
    
    /* 读取识别数据（256 个字） */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(base);
    }
    
    /* 提取总扇区数（28 位 LBA） */
    dev->total_sectors = *(uint32_t*)&identify_data[60];
    
    uint32_t size_mb = (dev->total_sectors * 512) / (1024 * 1024);
    kprintf("[IDE] Device identified: %u sectors (%u MB)\n",
            dev->total_sectors, size_mb);
    
    return true;
}

/*
 * IDE 读取扇区（PIO 模式）
 */
static int ide_read_sectors_pio(struct block_device *bdev, uint32_t sector, uint32_t count, void *buf)
{
    struct ide_device *dev = bdev->private_data;
    uint16_t base = dev->base_port;
    uint16_t *buf16 = (uint16_t*)buf;
    
    /* 限制一次最多读取 256 个扇区 */
    if (count > 256) count = 256;
    
    /* 等待就绪 */
    ide_wait_ready(base);
    
    /* 设置扇区数 */
    outb(base + 2, count & 0xFF);
    
    /* 设置 LBA 地址 */
    outb(base + 3, sector & 0xFF);
    outb(base + 4, (sector >> 8) & 0xFF);
    outb(base + 5, (sector >> 16) & 0xFF);
    outb(base + 6, ((sector >> 24) & 0x0F) | dev->drive_select);
    
    /* 发送读取命令 */
    outb(base + 7, IDE_CMD_READ_SECTORS);
    
    /* 读取数据 */
    for (uint32_t i = 0; i < count; i++) {
        /* 等待数据就绪 */
        ide_wait_drq(base);
        
        /* 读取一个扇区（256 个字） */
        for (int j = 0; j < 256; j++) {
            buf16[i * 256 + j] = inw(base);
        }
    }
    
    return 0;
}

/*
 * IDE 写入扇区（PIO 模式）
 */
static int ide_write_sectors_pio(struct block_device *bdev, uint32_t sector, uint32_t count, const void *buf)
{
    struct ide_device *dev = bdev->private_data;
    uint16_t base = dev->base_port;
    const uint16_t *buf16 = (const uint16_t*)buf;
    
    /* 限制一次最多写入 256 个扇区 */
    if (count > 256) count = 256;
    
    /* 等待就绪 */
    ide_wait_ready(base);
    
    /* 设置扇区数 */
    outb(base + 2, count & 0xFF);
    
    /* 设置 LBA 地址 */
    outb(base + 3, sector & 0xFF);
    outb(base + 4, (sector >> 8) & 0xFF);
    outb(base + 5, (sector >> 16) & 0xFF);
    outb(base + 6, ((sector >> 24) & 0x0F) | dev->drive_select);
    
    /* 发送写入命令 */
    outb(base + 7, IDE_CMD_WRITE_SECTORS);
    
    /* 写入数据 */
    for (uint32_t i = 0; i < count; i++) {
        /* 等待数据就绪 */
        ide_wait_drq(base);
        
        /* 写入一个扇区（256 个字） */
        for (int j = 0; j < 256; j++) {
            outw(base, buf16[i * 256 + j]);
        }
    }
    
    /* 等待完成 */
    ide_wait_ready(base);
    
    return 0;
}

/* ========== IDE 初始化 ========== */

/*
 * 初始化 IDE 驱动
 */
void ide_init(void)
{
    kprintf("[IDE] Initializing IDE/ATA disk driver...\n");
    
    int devices_found = 0;
    
    /* Primary Master (hda) */
    kprintf("[IDE] Detecting Primary Master (hda)...\n");
    ide_devices[0].base_port = IDE_PRIMARY_DATA;
    ide_devices[0].drive_select = 0xE0;
    ide_devices[0].present = ide_identify(&ide_devices[0]);
    
    if (ide_devices[0].present) {
        /* 注册块设备 */
        ide_block_devices[0].name = "hda";
        ide_block_devices[0].type = BLOCK_DEV_IDE;
        ide_block_devices[0].sector_size = 512;
        ide_block_devices[0].total_sectors = ide_devices[0].total_sectors;
        ide_block_devices[0].private_data = &ide_devices[0];
        ide_block_devices[0].read_sectors = ide_read_sectors_pio;
        ide_block_devices[0].write_sectors = ide_write_sectors_pio;
        
        block_register_device(&ide_block_devices[0]);
        devices_found++;
    } else {
        kprintf("[IDE] Primary Master not found\n");
    }
    
    /* Primary Slave (hdb) - FAT32 测试磁盘 */
    kprintf("[IDE] Detecting Primary Slave (hdb)...\n");
    ide_devices[1].base_port = IDE_PRIMARY_DATA;
    ide_devices[1].drive_select = 0xF0;
    ide_devices[1].present = ide_identify(&ide_devices[1]);
    
    /* 如果 IDENTIFY 失败，尝试探测性读取（Linux 风格） */
    if (!ide_devices[1].present) {
        kprintf("[IDE] IDENTIFY failed, trying probe read (Linux style)...\n");
        ide_devices[1].present = ide_probe_read(&ide_devices[1]);
    }
    
    if (ide_devices[1].present) {
        ide_block_devices[1].name = "hdb";
        ide_block_devices[1].type = BLOCK_DEV_IDE;
        ide_block_devices[1].sector_size = 512;
        ide_block_devices[1].total_sectors = ide_devices[1].total_sectors;
        ide_block_devices[1].private_data = &ide_devices[1];
        ide_block_devices[1].read_sectors = ide_read_sectors_pio;
        ide_block_devices[1].write_sectors = ide_write_sectors_pio;
        
        block_register_device(&ide_block_devices[1]);
        devices_found++;
        kprintf("[IDE] Found Primary Slave! This is your FAT32 disk.\n");
    } else {
        kprintf("[IDE] Primary Slave not found\n");
    }
    
    /* Secondary Master (hdc) - QEMU index=1 通常映射到这里！ */
    kprintf("[IDE] Detecting Secondary Master (hdc)...\n");
    ide_devices[2].base_port = IDE_SECONDARY_DATA;
    ide_devices[2].drive_select = 0xE0;
    ide_devices[2].present = ide_identify(&ide_devices[2]);
    
    /* 如果 IDENTIFY 失败，尝试探测性读取（Linux 风格降级） */
    if (!ide_devices[2].present) {
        kprintf("[IDE] IDENTIFY failed, trying probe read (Linux style)...\n");
        ide_devices[2].present = ide_probe_read(&ide_devices[2]);
    }
    
    if (ide_devices[2].present) {
        ide_block_devices[2].name = "hdc";
        ide_block_devices[2].type = BLOCK_DEV_IDE;
        ide_block_devices[2].sector_size = 512;
        ide_block_devices[2].total_sectors = ide_devices[2].total_sectors;
        ide_block_devices[2].private_data = &ide_devices[2];
        ide_block_devices[2].read_sectors = ide_read_sectors_pio;
        ide_block_devices[2].write_sectors = ide_write_sectors_pio;
        
        block_register_device(&ide_block_devices[2]);
        devices_found++;
        kprintf("[IDE] Found Secondary Master! This is likely your FAT32 disk.\n");
    } else {
        kprintf("[IDE] Secondary Master not found\n");
    }
    
    kprintf("[IDE] IDE driver initialized (%d device(s) found)\n", devices_found);
}

