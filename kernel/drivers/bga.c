/*
 * bga.c - Bochs Graphics Adapter 驱动实现
 * 
 * 提供任意分辨率的图形显示支持
 */

#include <drivers/bga.h>
#include <kernel.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <string.h>
#include <io.h>

/* 全局状态 */
static struct bga_mode_info g_mode_info;
static bool g_bga_available = false;

/* BGA物理地址（由QEMU/Bochs提供） */
#define BGA_FRAMEBUFFER_PHYSICAL    0xE0000000

/*
 * 写BGA寄存器
 */
static void bga_write_reg(uint16_t index, uint16_t value)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

/*
 * 读BGA寄存器
 */
static uint16_t bga_read_reg(uint16_t index)
{
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

/*
 * 检测BGA设备
 */
static bool bga_detect(void)
{
    uint16_t version = bga_read_reg(VBE_DISPI_INDEX_ID);
    
    kprintf("[BGA] Detecting Bochs Graphics Adapter...\n");
    kprintf("[BGA] Version ID: 0x%04x\n", version);
    
    /* 检查是否为有效的BGA版本 */
    if (version >= VBE_DISPI_ID0 && version <= VBE_DISPI_ID5) {
        kprintf("[BGA] Found compatible BGA device (version %d)\n", 
                version - VBE_DISPI_ID0);
        return true;
    }
    
    kprintf("[BGA] BGA device not found\n");
    return false;
}

/*
 * 初始化BGA驱动
 */
int bga_init(void)
{
    kprintf("[BGA] Initializing Bochs Graphics Adapter...\n");
    
    /* 检测BGA设备 */
    if (!bga_detect()) {
        return -1;
    }
    
    g_bga_available = true;
    
    /* 默认设置为1024x768x32 */
    if (bga_set_mode(1024, 768, 32) < 0) {
        kprintf("[BGA] Failed to set default mode\n");
        return -1;
    }
    
    kprintf("[BGA] Initialized successfully\n");
    kprintf("[BGA] Mode: %ux%ux%u\n", 
            g_mode_info.width, g_mode_info.height, g_mode_info.bpp);
    kprintf("[BGA] Framebuffer: phys=0x%08x, virt=0x%08x, size=%u KB\n",
            g_mode_info.fb_physical, g_mode_info.fb_virtual,
            g_mode_info.fb_size / 1024);
    
    return 0;
}

/*
 * 设置显示模式
 */
int bga_set_mode(uint16_t width, uint16_t height, uint16_t bpp)
{
    if (!g_bga_available) {
        return -1;
    }
    
    /* 检查参数 */
    if (width > VBE_DISPI_MAX_XRES || height > VBE_DISPI_MAX_YRES) {
        kprintf("[BGA] Invalid resolution: %ux%u\n", width, height);
        return -1;
    }
    
    if (bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32) {
        kprintf("[BGA] Invalid color depth: %u\n", bpp);
        return -1;
    }
    
    kprintf("[BGA] Setting mode: %ux%ux%u\n", width, height, bpp);
    
    /* 禁用显示 */
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    
    /* 设置分辨率和色深 */
    bga_write_reg(VBE_DISPI_INDEX_XRES, width);
    bga_write_reg(VBE_DISPI_INDEX_YRES, height);
    bga_write_reg(VBE_DISPI_INDEX_BPP, bpp);
    
    /* 启用显示（带Linear Framebuffer） */
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, 
                  VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    
    /* 更新模式信息 */
    g_mode_info.width = width;
    g_mode_info.height = height;
    g_mode_info.bpp = bpp;
    g_mode_info.pitch = width * (bpp / 8);
    g_mode_info.fb_size = g_mode_info.pitch * height;
    
    /* 尝试从PCI BAR0读取实际framebuffer地址 */
    /* QEMU的stdvga在PCI 0:2.0，BAR0是framebuffer */
    /* 但简化起见，先尝试常用地址 */
    
    /* 常见的framebuffer地址：
     * 0xE0000000 - Bochs默认
     * 0xFD000000 - QEMU stdvga
     * 0xFC000000 - 另一个常见地址
     */
    
    kprintf("[BGA] Trying common framebuffer addresses...\n");
    
    /* 尝试写入测试 */
    uint32_t test_addresses[] = {
        0xE0000000,  /* Bochs */
        0xFD000000,  /* QEMU stdvga */
        0xFC000000,  /* 备选 */
        0xF0000000   /* 备选 */
    };
    
    uint32_t working_addr = 0;
    for (int i = 0; i < 4; i++) {
        g_mode_info.fb_physical = test_addresses[i];
        
        kprintf("[BGA]   Testing 0x%08x...\n", test_addresses[i]);
        
        /* 临时映射第一页用于测试 */
        extern void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
        vmm_map_page(0xE0000000, test_addresses[i], 0x1B);
        
        /* 刷新TLB */
        asm volatile("invlpg (%0)" :: "r"(0xE0000000));
        
        /* 写入测试 */
        volatile uint32_t *test = (volatile uint32_t*)0xE0000000;
        test[0] = 0x12345678;
        test[1] = 0xABCDEF00;
        
        if (test[0] == 0x12345678 && test[1] == 0xABCDEF00) {
            kprintf("[BGA]   ✅ Found working framebuffer at 0x%08x!\n", test_addresses[i]);
            working_addr = test_addresses[i];
            break;
        } else {
            kprintf("[BGA]   ❌ No response (read: 0x%08x, 0x%08x)\n", test[0], test[1]);
        }
    }
    
    if (working_addr == 0) {
        kprintf("[BGA] ERROR: Could not find framebuffer!\n");
        return -1;
    }
    
    g_mode_info.fb_physical = working_addr;
    
    /* 映射framebuffer到虚拟内存 */
    uint32_t fb_pages = (g_mode_info.fb_size + 0xFFF) / 4096;
    g_mode_info.fb_virtual = 0xE0000000;  /* 固定映射到3.5GB */
    
    /* 创建物理->虚拟映射 - 直接操作页表 */
    kprintf("[BGA] Mapping framebuffer: phys=0x%08x, virt=0x%08x, pages=%u\n",
            g_mode_info.fb_physical, g_mode_info.fb_virtual, fb_pages);
    
    /* 获取当前页目录 */
    uint32_t *page_dir;
    asm volatile("mov %%cr3, %0" : "=r"(page_dir));
    page_dir = (uint32_t*)((uint32_t)page_dir + 0xC0000000);  /* 转为虚拟地址 */
    
    uint32_t pd_index = 0xE0000000 >> 22;  /* 页目录索引 (896) */
    
    kprintf("[BGA] PD index: %u, PDE before: 0x%08x\n", pd_index, page_dir[pd_index]);
    
    /* 确保页表存在 */
    if (!(page_dir[pd_index] & 0x01)) {
        extern uint32_t pmm_alloc_frame(void);
        uint32_t pt_phys = pmm_alloc_frame();
        page_dir[pd_index] = pt_phys | 0x03;  /* PRESENT | WRITABLE */
        
        /* 清空页表 */
        uint32_t *pt = (uint32_t*)(pt_phys + 0xC0000000);
        memset(pt, 0, 4096);
        
        kprintf("[BGA] Allocated new page table at phys 0x%08x\n", pt_phys);
    }
    
    /* 获取页表 */
    uint32_t pt_phys = page_dir[pd_index] & ~0xFFF;
    uint32_t *page_table = (uint32_t*)(pt_phys + 0xC0000000);
    
    kprintf("[BGA] Page table at phys 0x%08x, virt 0x%p\n", pt_phys, page_table);
    
    /* 映射所有页 */
    for (uint32_t i = 0; i < fb_pages; i++) {
        uint32_t phys = g_mode_info.fb_physical + (i * 4096);
        uint32_t pt_index = i;  /* 页表内索引 */
        
        /* PRESENT | WRITABLE | PWT | PCD */
        page_table[pt_index] = phys | 0x1B;
    }
    
    kprintf("[BGA] Mapped %u framebuffer pages\n", fb_pages);
    kprintf("[BGA] First PTE: 0x%08x\n", page_table[0]);
    
    /* 验证第一页映射 */
    extern uint32_t vmm_virt_to_phys(uint32_t virt);
    uint32_t mapped_phys = vmm_virt_to_phys(g_mode_info.fb_virtual);
    kprintf("[BGA] First page mapped: virt=0x%08x -> phys=0x%08x (expected 0x%08x)\n",
            g_mode_info.fb_virtual, mapped_phys, g_mode_info.fb_physical);
    
    /* 刷新TLB - 重新加载CR3 */
    uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    asm volatile("mov %0, %%cr3" :: "r"(cr3));
    
    kprintf("[BGA] Mode set successfully\n");
    kprintf("[BGA] TLB flushed, framebuffer ready\n");
    
    /* 测试framebuffer是否可写 */
    volatile uint32_t *test_fb = (volatile uint32_t*)g_mode_info.fb_virtual;
    kprintf("[BGA] Testing framebuffer write...\n");
    test_fb[0] = 0xDEADBEEF;
    uint32_t readback = test_fb[0];
    kprintf("[BGA] Write test: wrote 0xDEADBEEF, read back 0x%08x\n", readback);
    
    /* 清屏为黑色 */
    bga_clear_screen(COLOR_BLACK);
    
    return 0;
}

/*
 * 获取当前模式信息
 */
struct bga_mode_info *bga_get_mode_info(void)
{
    return &g_mode_info;
}

/*
 * 获取framebuffer指针
 */
uint32_t *bga_get_framebuffer(void)
{
    return (uint32_t*)g_mode_info.fb_virtual;
}

/*
 * 获取framebuffer物理地址（用于mmap）
 */
uint32_t bga_get_framebuffer_physical(void)
{
    return g_mode_info.fb_physical;
}

/*
 * 画单个像素
 */
void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= g_mode_info.width || y >= g_mode_info.height) {
        return;
    }
    
    uint32_t *fb = (uint32_t*)g_mode_info.fb_virtual;
    uint32_t offset = y * g_mode_info.width + x;
    fb[offset] = color;
}

/*
 * 读取像素颜色
 */
uint32_t bga_get_pixel(uint32_t x, uint32_t y)
{
    if (x >= g_mode_info.width || y >= g_mode_info.height) {
        return 0;
    }
    
    uint32_t *fb = (uint32_t*)g_mode_info.fb_virtual;
    uint32_t offset = y * g_mode_info.width + x;
    return fb[offset];
}

/*
 * 填充矩形
 */
void bga_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t i = 0; i < width; i++) {
            bga_put_pixel(x + i, y + j, color);
        }
    }
}

/*
 * 画矩形边框
 */
void bga_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color)
{
    /* 上边 */
    for (uint32_t i = 0; i < width; i++) {
        bga_put_pixel(x + i, y, color);
    }
    
    /* 下边 */
    for (uint32_t i = 0; i < width; i++) {
        bga_put_pixel(x + i, y + height - 1, color);
    }
    
    /* 左边 */
    for (uint32_t j = 0; j < height; j++) {
        bga_put_pixel(x, y + j, color);
    }
    
    /* 右边 */
    for (uint32_t j = 0; j < height; j++) {
        bga_put_pixel(x + width - 1, y + j, color);
    }
}

/*
 * 清屏
 */
void bga_clear_screen(uint32_t color)
{
    uint32_t *fb = (uint32_t*)g_mode_info.fb_virtual;
    uint32_t pixels = g_mode_info.width * g_mode_info.height;
    
    kprintf("[BGA] Clearing screen to color 0x%08x (%u pixels)\n", color, pixels);
    
    for (uint32_t i = 0; i < pixels; i++) {
        fb[i] = color;
    }
    
    kprintf("[BGA] Clear completed, testing write: fb[0]=0x%08x\n", fb[0]);
}
