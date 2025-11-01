# 第4讲：物理内存管理（PMM）- 从零开始

> 管理计算机的物理内存 - 操作系统的基石

## 🎯 课程目标

学完本课，你将理解：

1. **什么是物理内存？为什么需要管理它？**
2. **如何检测系统有多少内存？**
3. **位图分配器（Bitmap Allocator）的原理**
4. **如何分配和释放物理页面？**
5. **内存碎片是什么？如何避免？**
6. **EduOS的实际PMM实现**

**前置知识：**
- ✅ 基础C语言
- ✅ 二进制和位操作
- ✅ 指针的概念
- ❌ 不需要任何内存管理经验

**涉及文件：**
- `kernel/mm/pmm.c` - 物理内存管理器
- `include/mm/pmm.h` - PMM接口定义

---

## 📖 第一课：什么是物理内存

### 1.1 从生活例子理解

**想象一个图书馆：**

```
图书馆 = 物理内存
书架位置 = 物理地址（0x00000000, 0x00001000, ...）
书 = 数据

管理员的工作：
  1. 知道有多少书架（检测内存大小）
  2. 记录哪些位置有书，哪些空着（位图）
  3. 有人要借书架 → 分配一个空位置
  4. 还书 → 标记位置为空
```

### 1.2 物理内存是什么？

**物理内存（RAM）：**

```
地址总线：32位 → 最大4GB地址空间
  0x00000000 ┌──────────────┐
             │ BIOS区域     │ ← 不能用
  0x00100000 ├──────────────┤
             │              │
             │ 可用内存     │ ← 我们管理这部分
             │              │
  0x3FFFFFFF ├──────────────┤
             │ 硬件保留     │ ← 不能用
  0xFFFFFFFF └──────────────┘
```

**为什么从0x00100000（1MB）开始？**

```
0x00000000-0x000003FF：中断向量表（IVT）
0x00000400-0x000004FF：BIOS数据区
0x00000500-0x00007BFF：可用（小）
0x00007C00-0x00007DFF：引导扇区
0x00007E00-0x0009FFFF：可用（小）
0x000A0000-0x000FFFFF：显卡、BIOS等

从1MB开始才是大块连续可用内存！
```

---

## 📖 第二课：内存检测

### 2.1 如何知道有多少内存？

**BIOS提供的内存映射（Memory Map）：**

在引导时，bootloader调用BIOS功能获取内存映射。

**GRUB传递的内存信息：**

```c
/* EduOS使用固定大小（简化） */
#define TOTAL_MEMORY_MB  128  // 假设128MB内存

uint32_t total_memory = TOTAL_MEMORY_MB * 1024 * 1024;
```

**更准确的方法（真实系统）：**

```c
// GRUB Multiboot会提供内存映射
struct multiboot_mmap_entry {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;  // 1=可用, 2=保留, 3=ACPI等
};

void parse_memory_map(struct multiboot_info *mbi)
{
    struct multiboot_mmap_entry *entry = 
        (struct multiboot_mmap_entry*)mbi->mmap_addr;
    
    while ((uint32_t)entry < mbi->mmap_addr + mbi->mmap_length) {
        if (entry->type == 1) {  // 可用内存
            kprintf("Memory: 0x%08llx - 0x%08llx (%llu MB)\n",
                    entry->base_addr,
                    entry->base_addr + entry->length,
                    entry->length / (1024 * 1024));
        }
        entry = (struct multiboot_mmap_entry*)
                ((uint32_t)entry + entry->size + sizeof(entry->size));
    }
}
```

### 2.2 EduOS的内存布局

**实际使用的布局（128MB系统）：**

```
0x00000000  ┌─────────────────┐
            │ 0-1MB: BIOS区域 │ ← 不管理
0x00100000  ├─────────────────┤
(1MB)       │ 内核代码+数据   │ ← 128KB（当前）
0x00120000  ├─────────────────┤
            │                 │
            │   可分配内存    │ ← PMM管理这里
            │   (~126MB)      │
            │                 │
0x08000000  ├─────────────────┤
(128MB)     │ 内核堆          │ ← kmalloc使用
            │ (16MB)          │
0x09000000  └─────────────────┘
```

---

## 📖 第三课：页（Page）的概念

### 3.1 为什么按页管理？

**问题：** 如果按字节管理内存...

```c
// 糟糕的方式
char memory[128 * 1024 * 1024];  // 128MB
bool allocated[128 * 1024 * 1024];  // 每字节一个标记

// 要128MB位图！太大了！
```

**解决：** 按页（Page）管理

```
页（Page）= 固定大小的内存块

x86标准页大小 = 4096 字节 = 4KB

为什么4KB？
  1. 不太大（浪费少）
  2. 不太小（管理简单）
  3. CPU硬件支持（页表单位）
  4. Linux、Windows都用4KB
```

**按页管理的优势：**

```
128MB ÷ 4KB = 32768 页

位图大小：
  32768 位 = 4096 字节 = 4KB

只需一页内存就能管理128MB！
```

### 3.2 页帧号（Page Frame Number）

**物理地址 ↔ 页帧号转换：**

```c
// 物理地址转页帧号
uint32_t paddr = 0x00123000;
uint32_t pfn = paddr / 4096;  // = 0x123

// 页帧号转物理地址
uint32_t pfn = 0x123;
uint32_t paddr = pfn * 4096;  // = 0x00123000

// 快速方法（位移）
uint32_t pfn = paddr >> 12;     // 除以4096
uint32_t paddr = pfn << 12;     // 乘以4096
```

**为什么页必须4KB对齐？**

```
4KB = 4096 = 0x1000

对齐的地址：
  0x00000000 ✓
  0x00001000 ✓
  0x00002000 ✓
  0x00123000 ✓
  
未对齐的地址：
  0x00000001 ✗
  0x00001234 ✗
  0x00123456 ✗
  
CPU的页表只能映射对齐的页！
```

---

## 📖 第四课：位图分配器原理

### 4.1 什么是位图？

**位图（Bitmap）：** 用一个bit表示一个状态

```
假设有8个页面：
  位图：[0][1][0][1][1][0][0][0]
          ↑  ↑  ↑  ↑  ↑
          │  │  │  │  └─ 页4：空闲
          │  │  │  └──── 页3：占用
          │  │  └─────── 页2：空闲
          │  └────────── 页1：占用
          └───────────── 页0：空闲
```

**在内存中的实际存储：**

```c
uint8_t bitmap[1];  // 1字节 = 8位

bitmap[0] = 0b01011000;  // 二进制
bitmap[0] = 0x58;        // 十六进制

每个bit代表一个页：
  Bit 0 (LSB) = 页0
  Bit 1       = 页1
  ...
  Bit 7       = 页7
```

### 4.2 位操作基础

**设置bit（标记为占用）：**

```c
// 设置bit 3
bitmap[0] |= (1 << 3);

过程：
  1 << 3  = 0b00001000
  bitmap[0] = 0b01011000
  |操作    = 0b01011000 | 0b00001000 = 0b01011000
```

**清除bit（标记为空闲）：**

```c
// 清除bit 3
bitmap[0] &= ~(1 << 3);

过程：
  1 << 3  = 0b00001000
  ~(...)  = 0b11110111
  bitmap[0] = 0b01011000
  &操作    = 0b01011000 & 0b11110111 = 0b01010000
```

**测试bit（检查是否占用）：**

```c
// 测试bit 3
bool is_used = bitmap[0] & (1 << 3);

过程：
  1 << 3  = 0b00001000
  bitmap[0] = 0b01011000
  &操作    = 0b01011000 & 0b00001000 = 0b00001000 (非0 = true)
```

### 4.3 管理32768个页

**位图大小计算：**

```
总页数 = 128MB / 4KB = 32768 页
位图大小 = 32768 / 8 = 4096 字节 = 4KB

需要1个页来存储位图！
```

**位图数组：**

```c
#define TOTAL_PAGES  32768
#define BITMAP_SIZE  (TOTAL_PAGES / 8)  // 4096字节

uint8_t bitmap[BITMAP_SIZE];

// 访问第N页的bit
uint32_t byte_index = pfn / 8;     // 第几个字节
uint32_t bit_index  = pfn % 8;     // 字节内第几位

bool is_used = bitmap[byte_index] & (1 << bit_index);
```

---

## 📖 第五课：PMM实现（EduOS真实代码）

### 5.1 PMM数据结构

基于 `kernel/mm/pmm.c`:

```c
/* PMM全局状态 */
static struct {
    uint32_t *bitmap;          // 位图指针
    uint32_t total_frames;     // 总页帧数
    uint32_t free_frames;      // 空闲页帧数
    uint32_t used_frames;      // 已用页帧数
    uint32_t mem_start;        // 可用内存起始地址
    uint32_t mem_end;          // 可用内存结束地址
    bool initialized;          // 是否已初始化
} pmm;

/* 常量定义 */
#define FRAME_SIZE       4096          // 页大小（4KB）
#define FRAMES_PER_BYTE  8             // 每字节管理8个页
#define BITMAP_INDEX(frame)  (frame / 32)      // 位图索引（uint32_t）
#define BITMAP_OFFSET(frame) (frame % 32)      // 位内偏移
```

**为什么用uint32_t而不是uint8_t？**

```c
// 方案1：uint8_t（8位）
uint8_t bitmap[4096];  // 每次只能操作8个页

// 方案2：uint32_t（32位）
uint32_t bitmap[1024]; // 每次可以操作32个页
                       // 查找更快！

EduOS选择uint32_t：更高效
```

### 5.2 PMM初始化（完整实现）

基于 `kernel/mm/pmm.c`:

```c
void pmm_init(uint32_t mem_size, uint32_t kernel_start, uint32_t kernel_end)
{
    memory_size = mem_size;
    kernel_start_phys = kernel_start;
    kernel_end_phys = kernel_end;
    
    /* 计算总页帧数 */
    pmm_stats.total_frames = mem_size / PAGE_SIZE;
    
    /* 计算位图大小（字节）*/
    pmm_bitmap_size = (pmm_stats.total_frames + 7) / 8;
    pmm_bitmap_size = PAGE_ALIGN_UP(pmm_bitmap_size);  // 向上对齐到4KB
    
    /* 位图放在内核结束后 */
    pmm_bitmap = (uint32_t*)kernel_end_phys;
    
    kprintf("[PMM] Initializing Physical Memory Manager...\n");
    kprintf("[PMM] Total Memory: %u MB (%u frames)\n", 
            mem_size / 1024 / 1024, pmm_stats.total_frames);
    kprintf("[PMM] Kernel: 0x%08x - 0x%08x (%u KB)\n",
            kernel_start, kernel_end, 
            (kernel_end - kernel_start) / 1024);
    kprintf("[PMM] Bitmap at: 0x%08x, size: %u bytes\n",
            (uint32_t)pmm_bitmap, pmm_bitmap_size);
    
    /* 步骤1：初始化位图 - 全部标记为已用 */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size);
    pmm_stats.used_frames = pmm_stats.total_frames;
    pmm_stats.free_frames = 0;
    
    /* 步骤2：标记保留区域（0-1MB） */
    uint32_t reserved_frames = 0x100000 / PAGE_SIZE;  // 256帧
    pmm_stats.reserved_frames = reserved_frames;
    
    /* 步骤3：标记内核占用区域 */
    uint32_t kernel_frames = (kernel_end - kernel_start) / PAGE_SIZE;
    if ((kernel_end - kernel_start) % PAGE_SIZE) {
        kernel_frames++;  // 向上取整
    }
    pmm_stats.kernel_frames = kernel_frames;
    
    /* 步骤4：标记位图自身占用的帧 */
    uint32_t bitmap_frames = pmm_bitmap_size / PAGE_SIZE;
    
    /* 步骤5：释放可用内存（位图之后到内存顶部） */
    uint32_t available_start = (uint32_t)pmm_bitmap + pmm_bitmap_size;
    available_start = PAGE_ALIGN_UP(available_start);
    
    uint32_t available_end = mem_size;
    
    for (uint32_t addr = available_start; addr < available_end; addr += PAGE_SIZE) {
        uint32_t frame = addr / PAGE_SIZE;
        if (frame < pmm_stats.total_frames) {
            pmm_clear_bit(frame);  // 清除bit = 标记为空闲
            pmm_stats.used_frames--;
            pmm_stats.free_frames++;
        }
    }
    
    kprintf("[PMM] Available: %u MB (%u frames)\n",
            pmm_stats.free_frames * 4 / 1024, 
            pmm_stats.free_frames);
    kprintf("[PMM] Initialization complete\n\n");
}
```

**初始化流程图解：**

```
内存布局：
0x00000000 ┌─────────────┐
           │ 保留(1MB)   │ ← 全1（已用）
0x00100000 ├─────────────┤
           │ 内核(128KB) │ ← 全1（已用）
0x00120000 ├─────────────┤
           │ 位图(4KB)   │ ← 全1（已用）
0x00121000 ├─────────────┤
           │             │
           │ 可用内存    │ ← 全0（空闲）← 循环清零
           │ (~126MB)    │
           │             │
0x08000000 └─────────────┘

位图初始化：
1. memset(bitmap, 0xFF, size)  ← 全部标记为1（已用）
2. 循环清除可用区域的bit  ← 变为0（空闲）
```

**为什么先全置1，再清0？**

```
保守策略（fail-safe）：
  1. 默认全部不可用（安全）
  2. 只释放确定可用的（谨慎）
  
如果反过来（先全0再置1）：
  → 容易遗漏保留区域
  → 可能覆盖BIOS/硬件数据
  → 系统崩溃！
```

---

## 📖 第六课：分配和释放页面

### 6.1 分配单个页面

基于 `kernel/mm/pmm.c` 的 `pmm_alloc_frame()`:

```c
uint32_t pmm_alloc_frame(void)
{
    /* 扫描位图找到第一个空闲帧 */
    for (uint32_t i = 0; i < pmm_stats.total_frames; i++) {
        if (!pmm_test_bit(i)) {
            /* 找到空闲帧 */
            pmm_set_bit(i);  // 标记为已用
            pmm_stats.used_frames++;
            pmm_stats.free_frames--;
            
            uint32_t addr = i * PAGE_SIZE;
            return addr;  // 返回物理地址
        }
    }
    
    /* 内存不足 */
    kprintf("[PMM] ERROR: Out of memory!\n");
    return 0;  // 失败返回0
}
```

**使用示例：**

```c
/* 分配一个物理页 */
uint32_t page = pmm_alloc_frame();
if (page == 0) {
    kprintf("Memory allocation failed!\n");
    return -1;
}

kprintf("Allocated page at: 0x%08x\n", page);

/* 使用这个页（比如建立页表映射） */
vmm_map_page(0xC0400000, page, PAGE_PRESENT | PAGE_WRITABLE);

/* 现在可以访问0xC0400000了 */
uint32_t *ptr = (uint32_t*)0xC0400000;
*ptr = 0x12345678;
```

**算法分析：**

```
时间复杂度：O(n)
  最好情况：O(1) - 第一个就是空闲
  最坏情况：O(n) - 扫描全部32768个帧
  平均情况：O(n/2)

空间复杂度：O(1)
  只需要位图，已经分配好了

优化方向：
  1. 记住上次分配的位置（next_free_frame）
  2. 使用多级位图（buddy system）
  3. 空闲链表（free list）
```

### 6.2 分配连续页面

基于 `kernel/mm/pmm.c` 的 `pmm_alloc_frames()`:

```c
uint32_t pmm_alloc_frames(uint32_t count)
{
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_frame();  // 优化
    
    /* 查找连续的空闲帧 */
    uint32_t found = 0;
    uint32_t start_frame = 0;
    
    for (uint32_t i = 0; i < pmm_stats.total_frames; i++) {
        if (!pmm_test_bit(i)) {
            if (found == 0) {
                start_frame = i;  // 记录起始位置
            }
            found++;
            
            if (found == count) {
                /* 找到足够的连续帧 → 全部标记为已用 */
                for (uint32_t j = 0; j < count; j++) {
                    pmm_set_bit(start_frame + j);
                }
                
                pmm_stats.used_frames += count;
                pmm_stats.free_frames -= count;
                
                return start_frame * PAGE_SIZE;
            }
        } else {
            found = 0;  // 遇到已用，重新计数
        }
    }
    
    /* 未找到足够的连续内存 */
    kprintf("[PMM] ERROR: Cannot allocate %u consecutive frames\n", count);
    return 0;
}
```

**使用场景：**

```c
/* DMA缓冲区需要连续物理内存 */
uint32_t dma_buffer = pmm_alloc_frames(16);  // 16页 = 64KB连续

/* 大对象分配 */
uint32_t page_table = pmm_alloc_frames(1);  // 页表
uint32_t framebuffer = pmm_alloc_frames(768);  // 3MB显存（1024x768x4）
```

**连续分配的挑战：**

```
内存碎片化：

初始状态：
  [空][空][空][空][空][空][空][空]
  
分配3个单页：
  [用][空][用][空][用][空][空][空]
  
想分配4个连续页：
  ✗ 失败！虽然总共有5个空闲，但不连续
  
这就是外部碎片（External Fragmentation）
```

### 6.3 释放页面

基于 `kernel/mm/pmm.c` 的 `pmm_free_frame()`:

```c
void pmm_free_frame(uint32_t frame_addr)
{
    if (frame_addr == 0) return;  // NULL指针保护
    
    uint32_t frame = frame_addr / PAGE_SIZE;
    
    /* 边界检查 */
    if (frame >= pmm_stats.total_frames) {
        kprintf("[PMM] WARNING: Attempt to free invalid frame 0x%08x\n", 
                frame_addr);
        return;
    }
    
    /* Double-Free检测 */
    if (!pmm_test_bit(frame)) {
        kprintf("[PMM] WARNING: Double free detected at 0x%08x\n", 
                frame_addr);
        return;  // 已经是空闲，不重复释放
    }
    
    /* 清除bit，标记为空闲 */
    pmm_clear_bit(frame);
    pmm_stats.used_frames--;
    pmm_stats.free_frames++;
}
```

**安全检查很重要：**

```c
/* Double-Free漏洞示例 */

// 正常使用
uint32_t page = pmm_alloc_frame();  // 分配
pmm_free_frame(page);               // 释放
pmm_free_frame(page);               // ← Double Free!

如果不检测：
  1. 第一次free：标记为空闲 ✓
  2. 第二次free：
     - 其他人可能已经分配了这个页
     - 再次标记为空闲
     - → 两个人同时使用同一个页
     - → 数据损坏！

EduOS的检测：
  if (!pmm_test_bit(frame)) {
      kprintf("Double free!\n");
      return;  // 拒绝释放
  }
```

---

## 📖 第七课：PMM统计与调试

### 7.1 统计信息结构

基于 `include/mm/pmm.h`:

```c
struct pmm_stats {
    uint32_t total_frames;      // 总页帧数
    uint32_t used_frames;       // 已用页帧数
    uint32_t free_frames;       // 空闲页帧数
    uint32_t reserved_frames;   // 保留页帧数（0-1MB）
    uint32_t kernel_frames;     // 内核占用页帧数
};
```

### 7.2 查看内存使用情况

```c
void pmm_print_stats(void)
{
    kprintf("\n=== Physical Memory Statistics ===\n");
    kprintf("Total Memory:    %u MB (%u frames)\n",
            pmm_stats.total_frames * 4 / 1024, 
            pmm_stats.total_frames);
    kprintf("Used Memory:     %u MB (%u frames)\n",
            pmm_stats.used_frames * 4 / 1024, 
            pmm_stats.used_frames);
    kprintf("Free Memory:     %u MB (%u frames)\n",
            pmm_stats.free_frames * 4 / 1024, 
            pmm_stats.free_frames);
    kprintf("Reserved:        %u MB (%u frames)\n",
            pmm_stats.reserved_frames * 4 / 1024, 
            pmm_stats.reserved_frames);
    kprintf("Kernel:          %u KB (%u frames)\n",
            pmm_stats.kernel_frames * 4, 
            pmm_stats.kernel_frames);
    kprintf("Usage:           %u%%\n",
            pmm_stats.used_frames * 100 / pmm_stats.total_frames);
}
```

**实际输出（EduOS）：**

```
=== Physical Memory Statistics ===
Total Memory:    128 MB (32768 frames)
Used Memory:     2 MB (512 frames)
Free Memory:     126 MB (32256 frames)
Reserved:        1 MB (256 frames)
Kernel:          128 KB (32 frames)
Usage:           1%
```

### 7.3 内存映射可视化

```c
void pmm_print_memory_map(void)
{
    kprintf("\n=== Physical Memory Map ===\n");
    kprintf("0x%08x - 0x%08x : Reserved (BIOS/Hardware)\n", 
            0, 0x100000);
    kprintf("0x%08x - 0x%08x : Kernel\n", 
            kernel_start_phys, kernel_end_phys);
    kprintf("0x%08x - 0x%08x : PMM Bitmap\n", 
            (uint32_t)pmm_bitmap, 
            (uint32_t)pmm_bitmap + pmm_bitmap_size);
    kprintf("0x%08x - 0x%08x : Available\n",
            PAGE_ALIGN_UP((uint32_t)pmm_bitmap + pmm_bitmap_size), 
            memory_size);
}
```

**输出示例：**

```
=== Physical Memory Map ===
0x00000000 - 0x00100000 : Reserved (BIOS/Hardware)
0x00100000 - 0x00120000 : Kernel
0x00120000 - 0x00121000 : PMM Bitmap
0x00121000 - 0x08000000 : Available
```

---

## 📖 第八课：实战练习

### 练习1：分配和释放页面

```c
void test_pmm_basic(void)
{
    kprintf("=== PMM Basic Test ===\n");
    
    /* 测试1：分配单页 */
    uint32_t page1 = pmm_alloc_frame();
    kprintf("Allocated page1: 0x%08x\n", page1);
    
    uint32_t page2 = pmm_alloc_frame();
    kprintf("Allocated page2: 0x%08x\n", page2);
    
    /* 测试2：释放页面 */
    pmm_free_frame(page1);
    kprintf("Freed page1\n");
    
    /* 测试3：再次分配（应该复用page1）*/
    uint32_t page3 = pmm_alloc_frame();
    kprintf("Allocated page3: 0x%08x\n", page3);
    
    if (page3 == page1) {
        kprintf("✓ Page reused correctly!\n");
    }
    
    /* 清理 */
    pmm_free_frame(page2);
    pmm_free_frame(page3);
}
```

### 练习2：测试连续分配

```c
void test_pmm_contiguous(void)
{
    kprintf("=== PMM Contiguous Allocation Test ===\n");
    
    /* 分配10个连续页（40KB）*/
    uint32_t base = pmm_alloc_frames(10);
    
    if (base == 0) {
        kprintf("✗ Allocation failed\n");
        return;
    }
    
    kprintf("✓ Allocated 10 frames at: 0x%08x\n", base);
    
    /* 验证连续性 */
    for (int i = 0; i < 10; i++) {
        uint32_t expected = base + i * PAGE_SIZE;
        uint32_t frame = (base / PAGE_SIZE) + i;
        
        if (pmm_is_frame_used(frame * PAGE_SIZE)) {
            kprintf("  Frame %d: 0x%08x ✓\n", i, expected);
        }
    }
    
    /* 释放 */
    pmm_free_frames(base, 10);
    kprintf("✓ Freed 10 frames\n");
}
```

### 练习3：内存压力测试

```c
void test_pmm_stress(void)
{
    kprintf("=== PMM Stress Test ===\n");
    
    #define MAX_ALLOCS 1000
    uint32_t pages[MAX_ALLOCS];
    int allocated = 0;
    
    /* 分配直到失败 */
    for (int i = 0; i < MAX_ALLOCS; i++) {
        pages[i] = pmm_alloc_frame();
        if (pages[i] == 0) {
            break;
        }
        allocated++;
    }
    
    kprintf("Allocated %d pages\n", allocated);
    
    struct pmm_stats stats;
    pmm_get_stats(&stats);
    kprintf("Free: %u MB\n", stats.free_frames * 4 / 1024);
    
    /* 全部释放 */
    for (int i = 0; i < allocated; i++) {
        pmm_free_frame(pages[i]);
    }
    
    pmm_get_stats(&stats);
    kprintf("After free: %u MB\n", stats.free_frames * 4 / 1024);
}
```

---

## 🧪 实际运行效果（EduOS）

### 启动时的PMM初始化

```
[PMM] Initializing Physical Memory Manager...
[PMM] Total Memory: 128 MB (32768 frames)
[PMM] Kernel: 0x00100000 - 0x0011f800 (126 KB)
[PMM] Bitmap at: 0x00120000, size: 4096 bytes
[PMM] Available: 126 MB (32256 frames)
[PMM] Initialization complete

Memory Layout:
  Reserved: 1 MB
  Kernel:   128 KB
  Bitmap:   4 KB
  Available: ~126 MB
  
Usage: 1%
```

### GUI程序加载时的内存分配

```
加载GUI程序（desktop.elf）：

分配页面统计：
  代码段：3页（12KB）
  数据段：2页（8KB）
  BSS段：17页（68KB）
  栈：1页（预映射）
  Framebuffer（按需）：768页（3MB）
  
总计：791页 = 3.08MB

全部通过Page Fault按需分配！
```

---

## 💡 常见问题

### Q1：为什么不按字节分配？

**A：** 太浪费了！

```
按字节：
  128MB需要128MB位图（1:1）
  
按页（4KB）：
  128MB只需4KB位图（1:32768）
  
节省：32768倍！
```

### Q2：4KB页太大了，浪费怎么办？

**A：** 内部碎片（Internal Fragmentation）

```
需要100字节：
  分配4KB页
  浪费3996字节
  
平均浪费：2KB（页的一半）

解决：
  1. 用户空间：kmalloc（堆分配器）
  2. 小对象池（slab allocator）
  3. 接受浪费（简单可靠）

EduOS选择：接受少量浪费，换取简单性
```

### Q3：位图扫描太慢了怎么办？

**A：** 多种优化方法

```
优化1：记住上次位置
  static uint32_t next_free = 0;
  从next_free开始扫描
  
优化2：多级位图
  Level 1：每bit代表32页
  Level 2：每bit代表1页
  先快速找到区域，再精确定位
  
优化3：空闲链表
  维护空闲页链表
  分配：O(1)
  释放：O(1)
  
Linux使用：Buddy System（伙伴系统）
```

---

## 🎓 本讲总结

### 核心概念

✅ **物理内存** - 实际的RAM硬件  
✅ **页（Page）** - 4KB固定大小的内存块  
✅ **页帧（Frame）** - 物理页的编号  
✅ **位图** - 用1bit表示一个页的状态  
✅ **PMM** - 物理内存管理器，分配/释放页帧  

### PMM核心API

```c
/* 初始化 */
void pmm_init(uint32_t mem_size, uint32_t kernel_start, uint32_t kernel_end);

/* 分配 */
uint32_t pmm_alloc_frame(void);           // 分配1页
uint32_t pmm_alloc_frames(uint32_t n);    // 分配n页（连续）

/* 释放 */
void pmm_free_frame(uint32_t addr);       // 释放1页
void pmm_free_frames(uint32_t addr, uint32_t n);  // 释放n页

/* 查询 */
bool pmm_is_frame_used(uint32_t addr);    // 检查是否被使用
void pmm_get_stats(struct pmm_stats *stats);  // 获取统计信息
```

### 重要规则

1. **总是检查分配是否成功** - 返回0表示失败
2. **不要Double Free** - PMM会检测并警告
3. **页必须4KB对齐** - 使用PAGE_ALIGN_UP宏
4. **保护保留区域** - 0-1MB不可分配

### EduOS的PMM特点

- ✅ **简单可靠** - 位图算法容易理解
- ✅ **Double-Free检测** - 避免内存损坏
- ✅ **统计信息** - 实时监控内存使用
- ✅ **连续分配支持** - DMA等场景需要
- ⚠️ **线性扫描** - 大内存时较慢（可优化）

### 下一步

学习 **第5讲：虚拟内存管理（VMM）**，理解分页、页表、地址转换！

---

**物理内存管理 - 一切内存操作的基础！** 💾

