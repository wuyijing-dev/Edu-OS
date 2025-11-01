# 第5讲：虚拟内存管理（VMM）- 从零开始

> 分页机制 - 让每个程序都拥有完整的4GB地址空间

## 🎯 课程目标

学完本课，你将理解：

1. **什么是虚拟内存？为什么需要它？**
2. **分页机制（Paging）如何工作？**
3. **页表（Page Table）和页目录（Page Directory）的结构**
4. **地址转换：虚拟地址 → 物理地址**
5. **TLB（快表）是什么？为什么要刷新？**
6. **如何实现内核高地址映射？**
7. **用户进程的独立地址空间**

**前置知识：**
- ✅ 物理内存管理（第4讲）
- ✅ 二进制和位操作
- ✅ 指针的概念
- ❌ 不需要任何虚拟内存经验

**涉及文件：**
- `kernel/mm/vmm.c` - 虚拟内存管理器
- `include/mm/vmm.h` - VMM接口定义
- `boot/boot_stage2.asm` - 启用分页

---

## 📖 第一课：为什么需要虚拟内存

### 1.1 没有虚拟内存的问题

**场景：两个程序同时运行**

```c
// 程序A
int *ptr = (int*)0x00100000;
*ptr = 100;

// 程序B  
int *ptr = (int*)0x00100000;  // ← 同样的地址！
*ptr = 200;

问题：
  程序A的数据被程序B覆盖了！
  没有隔离 → 互相干扰
```

**更严重的问题：**

```c
// 用户程序（恶意或有bug）
int *ptr = (int*)0xC0100000;  // 内核代码地址
*ptr = 0x90909090;            // 覆盖内核！

结果：
  内核被破坏
  系统崩溃
  安全隐患！
```

### 1.2 虚拟内存的解决方案

**给每个程序一个"虚拟"的地址空间：**

```
程序A看到的地址空间：
0x00000000 ┌──────────┐
           │ 程序A的  │
           │ 代码和   │
           │ 数据     │
0xFFFFFFFF └──────────┘

程序B看到的地址空间：
0x00000000 ┌──────────┐
           │ 程序B的  │
           │ 代码和   │
           │ 数据     │
0xFFFFFFFF └──────────┘

它们互不影响！
```

**实际物理内存映射：**

```
虚拟地址       物理地址
程序A: 0x00001000 → 0x00200000
程序B: 0x00001000 → 0x00300000

同样的虚拟地址，不同的物理地址！
```

**类比：**

```
虚拟内存 = 门牌号系统

你的家：
  虚拟地址 = "xx街1号"
  物理位置 = GPS坐标(34.567, 123.456)

我的家：
  虚拟地址 = "yy街1号"  ← 同样是1号！
  物理位置 = GPS坐标(34.789, 123.789)

"1号"可以重复，但物理位置不同
操作系统 = 邮递员，知道真实位置
```

---

## 📖 第二课：分页机制基础

### 2.1 什么是分页？

**分页（Paging）：** 把内存分成固定大小的块

```
虚拟内存空间：
┌─────┬─────┬─────┬─────┐
│ 页0 │ 页1 │ 页2 │ 页3 │ ...
└─────┴─────┴─────┴─────┘
 4KB   4KB   4KB   4KB

物理内存空间：
┌─────┬─────┬─────┬─────┐
│帧100│帧101│帧102│帧103│ ...
└─────┴─────┴─────┴─────┘
 4KB   4KB   4KB   4KB

映射关系（页表）：
  页0 → 帧100
  页1 → 帧101
  页2 → 帧102
  页3 → 帧103
```

**为什么是4KB？**

```
1. CPU硬件限制（x86规定）
2. 不太大（浪费少）
3. 不太小（管理简单）
4. 2的幂次（位运算快）

4KB = 4096 = 2^12 = 0x1000
```

### 2.2 地址转换过程

**32位虚拟地址的分解：**

```
虚拟地址：0x12345678

二进制：0001 0010 0011 0100 0101 0110 0111 1000
        
拆分：
┌───────────┬───────────┬──────────────┐
│ 10位      │ 10位      │ 12位         │
│ 页目录索引│ 页表索引  │ 页内偏移     │
│ Dir Index │ Table Idx │ Offset       │
└───────────┴───────────┴──────────────┘
  0x048       0x145       0x678
  (72)        (325)       (1656字节)

解释：
  - 页目录索引：第72个页目录项
  - 页表索引：第325个页表项
  - 页内偏移：页内第1656字节
```

**地址转换公式：**

```c
// 提取各部分
uint32_t virt = 0x12345678;

uint32_t pd_index  = (virt >> 22) & 0x3FF;  // 高10位
uint32_t pt_index  = (virt >> 12) & 0x3FF;  // 中10位
uint32_t offset    = virt & 0xFFF;          // 低12位

// 转换过程
步骤1：查页目录[pd_index] → 找到页表物理地址
步骤2：查页表[pt_index] → 找到物理页地址
步骤3：物理地址 = 物理页地址 + offset
```

### 2.3 二级页表结构

**为什么需要两级？**

```
如果只有一级页表：
  4GB地址空间 / 4KB页 = 1,048,576 个页
  每个页表项4字节
  页表大小 = 1048576 × 4 = 4MB！
  
  每个进程需要4MB页表 → 太大了！

两级页表：
  页目录：1024项 × 4字节 = 4KB（1页）
  页表：1024项 × 4字节 = 4KB（1页）
  
  总共：1个页目录 + 最多1024个页表
  
  优势：按需分配页表
    如果程序只用1MB内存：
      只需1个页目录 + 1个页表 = 8KB
```

**结构图解：**

```
虚拟地址： 0x12345678
           │
           ▼
     ┌────────────┐
     │ CR3寄存器  │ ─────┐
     └────────────┘      │
                         ▼
                   ┌──────────────────┐
                   │  页目录(4KB)     │
                   │  1024个PDE       │
                   │ [0]  [1]  [2]... │
                   │  ▲              │
                   │  │              │
                   │ [72]◄───────────┼─ pd_index=72
                   │  │              │
                   └──┼──────────────┘
                      │
                      ▼
                ┌──────────────────┐
                │  页表(4KB)       │
                │  1024个PTE       │
                │ [0]  [1]  [2]... │
                │  ▲              │
                │  │              │
                │ [325]◄──────────┼─ pt_index=325
                │  │              │
                └──┼──────────────┘
                   │
                   ▼
             ┌──────────────────┐
             │ 物理页(4KB)      │
             │                  │
             │ 0x000~0xFFF     │
             │   ▲             │
             │   │             │
             │ [0x678]◄────────┼─ offset=0x678
             │   │             │
             └───┼─────────────┘
                 │
                 ▼
            最终物理地址
```

---

## 📖 第三课：页表项（PTE）结构

### 3.1 PTE/PDE的32位结构

**页表项（Page Table Entry）：**

```
32位PTE：
┌──────────────────────────┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┬─┐
│    物理页地址[31:12]     │A│D│A│C│W│U│R│P│
│       (20位)             │V│ │C│D│T│S│W│ │
└──────────────────────────┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┘
 31                    12  11 10 9 8 7 6 5 4 3 2 1 0

各位含义：
  P  (0): Present      - 1=页存在, 0=页不存在
  RW (1): Read/Write   - 1=可写, 0=只读
  US (2): User/Super   - 1=用户可访问, 0=仅内核
  WT (3): Write-Through- 写穿透缓存
  CD (4): Cache Disable- 禁用缓存
  AC (5): Accessed     - CPU访问过此页
  D  (6): Dirty        - 页被写入过
  AT (7): Page Attribute Table
  G  (8): Global       - 全局页（TLB不刷新）
  AV (9-11): Available - 操作系统自用
  
  物理地址[31:12]: 物理页基地址（页对齐，低12位总是0）
```

### 3.2 常用标志组合

**EduOS的标志定义（基于实际代码）：**

```c
/* 页表标志位（kernel/mm/vmm.c） */
#define PAGE_PRESENT    0x01    // 页存在
#define PAGE_WRITABLE   0x02    // 可写
#define PAGE_USER       0x04    // 用户可访问
#define PAGE_WRITETHROUGH 0x08  // 写穿透
#define PAGE_CACHE_DISABLE 0x10 // 禁用缓存
#define PAGE_ACCESSED   0x20    // 已访问
#define PAGE_DIRTY      0x40    // 已修改
#define PAGE_4MB        0x80    // 4MB大页
#define PAGE_GLOBAL     0x100   // 全局页

/* 常用组合 */
#define PAGE_KERNEL   (PAGE_PRESENT | PAGE_WRITABLE)  // 0x03
#define PAGE_USER_RO  (PAGE_PRESENT | PAGE_USER)      // 0x05
#define PAGE_USER_RW  (PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER)  // 0x07
```

**使用示例：**

```c
// 内核代码页（只读）
pte = phys_addr | PAGE_PRESENT;

// 内核数据页（可写）
pte = phys_addr | PAGE_KERNEL;

// 用户程序代码（用户可访问，只读）
pte = phys_addr | PAGE_USER_RO;

// 用户程序数据（用户可访问，可写）
pte = phys_addr | PAGE_USER_RW;

// 设备内存（禁用缓存）
pte = phys_addr | PAGE_KERNEL | PAGE_CACHE_DISABLE;
```

---

## 📖 第四课：启用分页

### 4.1 CR0和CR3寄存器

**控制寄存器（Control Registers）：**

```
CR0：系统控制寄存器
  Bit 0: PE (Protection Enable) - 保护模式
  Bit 31: PG (Paging) - 分页使能
  
CR3：页目录基地址寄存器（PDBR）
  存储页目录的物理地址
  
CR2：页故障线性地址
  Page Fault时，存储触发异常的虚拟地址
```

**启用分页的代码（汇编）：**

```asm
; 设置CR3（页目录物理地址）
mov eax, page_directory_phys
mov cr3, eax

; 启用分页（设置CR0.PG）
mov eax, cr0
or eax, 0x80000000  ; 设置bit 31
mov cr0, eax

; 现在分页已启用！
; 所有内存访问都通过页表转换
```

### 4.2 恒等映射（Identity Mapping）

**问题：** 启用分页后，EIP还指向低地址怎么办？

```
启用分页前：
  EIP = 0x00101000（物理地址）
  
启用分页：
  mov cr0, eax  ← 执行这条指令
  下一条指令地址：0x00101004
  
  但现在0x00101004是虚拟地址！
  如果没有映射 → Page Fault → 崩溃！
```

**解决：** 恒等映射

```c
// 低端4MB恒等映射（虚拟地址 = 物理地址）
for (uint32_t i = 0; i < 1024; i++) {
    // 0x00000000-0x003FFFFF 映射到自己
    page_table[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_WRITABLE;
}
```

**映射后：**

```
虚拟 0x00101004 → 物理 0x00101004
虚拟 0x00200000 → 物理 0x00200000

EIP可以继续执行！
```

---

## 📖 第五课：EduOS的内核高地址映射

### 5.1 为什么需要高地址映射？

**目标布局：**

```
虚拟地址空间：
0x00000000 ┌──────────────┐
           │ 用户空间     │ ← 用户程序使用
           │ (3GB)        │
0xC0000000 ├──────────────┤
           │ 内核空间     │ ← 内核使用
           │ (1GB)        │
0xFFFFFFFF └──────────────┘

优势：
  1. 所有进程共享同一个内核映射
  2. 用户程序不能访问内核（保护）
  3. 切换进程时，内核部分不变（效率）
```

### 5.2 高地址映射的实现

**映射方案：**

```
虚拟地址    →  物理地址
0xC0000000  →  0x00000000
0xC0001000  →  0x00001000
0xC0002000  →  0x00002000
...
0xC03FFFFF  →  0x003FFFFF

公式：
  物理地址 = 虚拟地址 - 0xC0000000
```

**页目录设置：**

```c
/* 高地址映射（EduOS实际实现）*/

// 页目录索引768 = 0xC0000000 >> 22
// 映射0xC0000000-0xC03FFFFF（4MB）

uint32_t pde_index = 768;  // 0xC0000000的页目录索引

// 创建页表
uint32_t page_table_phys = pmm_alloc_frame();
uint32_t *page_table = (uint32_t*)(page_table_phys + 0xC0000000);

// 填充页表（1024个页）
for (int i = 0; i < 1024; i++) {
    page_table[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
}

// 设置页目录项
page_directory[pde_index] = page_table_phys | PAGE_PRESENT | PAGE_WRITABLE;
```

**现在内核可以用高地址了：**

```c
// 访问物理地址0x00100000的两种方式

// 方式1：低地址（恒等映射）
uint32_t *ptr1 = (uint32_t*)0x00100000;

// 方式2：高地址（内核映射）
uint32_t *ptr2 = (uint32_t*)0xC0100000;

// 它们指向同一个物理位置！
*ptr1 = 100;
kprintf("%d\n", *ptr2);  // 输出100
```

---

## 📖 第六课：VMM核心实现（EduOS真实代码）

### 6.1 VMM数据结构

基于 `kernel/mm/vmm.c`:

```c
/* 页目录结构 */
struct page_directory {
    uint32_t physical_addr;      // 页目录的物理地址
    uint32_t *virtual_addr;      // 页目录的虚拟地址（递归映射）
    uint32_t ref_count;          // 引用计数
};

/* VMM全局状态 */
static struct {
    struct page_directory *kernel_directory;  // 内核页目录
    struct page_directory *current_directory; // 当前页目录
    bool paging_enabled;
} vmm;

/* 页大小常量 */
#define PAGE_SIZE  4096
#define PAGE_ALIGN_DOWN(addr)  ((addr) & ~0xFFF)
#define PAGE_ALIGN_UP(addr)    (((addr) + 0xFFF) & ~0xFFF)
```

### 6.2 映射单个页面

基于 `kernel/mm/vmm.c` 的 `vmm_map_page()`:

```c
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
    /* 页对齐检查 */
    virt &= 0xFFFFF000;
    phys &= 0xFFFFF000;
    
    /* 提取页目录索引和页表索引 */
    uint32_t pd_index = PD_INDEX(virt);  // virt >> 22
    uint32_t pt_index = PT_INDEX(virt);  // (virt >> 12) & 0x3FF
    
    /* 获取页目录 */
    uint32_t *pd = GET_PAGE_DIRECTORY();
    
    /* 检查页表是否存在 */
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        /* 页表不存在，需要创建 */
        uint32_t pt_phys = pmm_alloc_frame();
        if (pt_phys == 0) {
            kprintf("[VMM] ERROR: Cannot allocate page table\n");
            return;
        }
        
        /* 设置页目录项 */
        pd[pd_index] = pde_create(pt_phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
        
        /* 清零新页表 */
        uint32_t *pt = GET_PAGE_TABLE(pd_index);
        memset(pt, 0, PAGE_SIZE);
    }
    
    /* 获取页表 */
    uint32_t *pt = GET_PAGE_TABLE(pd_index);
    
    /* 设置页表项 */
    pt[pt_index] = pte_create(phys, flags);
    
    /* 刷新TLB（关键！）*/
    vmm_invlpg(virt);
}
```

**步骤图解：**

```
1. 输入：virt=0x12345000, phys=0x00ABC000, flags=0x07

2. 提取索引：
   pd_index = 0x12345000 >> 22 = 72
   pt_index = (0x12345000 >> 12) & 0x3FF = 837
   
3. 检查页表：
   if (pd[72] & 0x01 == 0) {
       // 页表不存在
       pt_phys = pmm_alloc_frame();  // 分配新页表
       pd[72] = pt_phys | 0x07;      // 设置PDE
       memset(page_table, 0, 4096);  // 清零
   }
   
4. 设置页表项：
   pt = GET_PAGE_TABLE(72);
   pt[837] = 0x00ABC000 | 0x07;
   
5. 刷新TLB：
   invlpg(0x12345000);
   
完成！虚拟地址0x12345000现在映射到物理地址0x00ABC000
```

### 6.3 虚拟地址转物理地址

基于 `kernel/mm/vmm.c` 的 `vmm_virt_to_phys()`:

```c
uint32_t vmm_virt_to_phys(uint32_t virt)
{
    uint32_t pd_index = PD_INDEX(virt);
    uint32_t pt_index = PT_INDEX(virt);
    uint32_t offset = PAGE_OFFSET(virt);
    
    /* 获取页目录 */
    uint32_t *pd = GET_PAGE_DIRECTORY();
    
    /* 检查页目录项 */
    if (!(pd[pd_index] & PAGE_PRESENT)) {
        return 0;  // 页表不存在
    }
    
    /* 获取页表 */
    uint32_t *pt = GET_PAGE_TABLE(pd_index);
    
    /* 检查页表项 */
    if (!(pt[pt_index] & PAGE_PRESENT)) {
        return 0;  // 页不存在
    }
    
    /* 提取物理页地址 */
    uint32_t phys_page = pte_get_addr(pt[pt_index]);
    
    /* 加上页内偏移 */
    return phys_page + offset;
}
```

**使用示例：**

```c
/* 查询0xC0100000的物理地址 */
uint32_t phys = vmm_virt_to_phys(0xC0100000);

if (phys != 0) {
    kprintf("Virtual 0xC0100000 → Physical 0x%08x\n", phys);
} else {
    kprintf("Virtual 0xC0100000 is not mapped\n");
}
```

---

## 📖 第七课：TLB（快表）

### 7.1 什么是TLB？

**TLB = Translation Lookaside Buffer（转换后备缓冲区）**

**问题：** 每次访问内存都要查两次页表，太慢了！

```
访问虚拟地址0x12345678：
  1. 读取CR3（页目录地址）
  2. 读取PDE（页目录项）→ 1次内存访问
  3. 读取PTE（页表项）→ 2次内存访问
  4. 读取实际数据 → 3次内存访问

一次访问变成三次！性能下降！
```

**解决：** TLB缓存

```
TLB = 虚拟地址→物理地址的缓存

第一次访问0x12345678：
  查页表（慢）→ 物理地址0x00ABC678
  缓存到TLB：0x12345000 → 0x00ABC000
  
第二次访问0x12345678：
  查TLB（快！）→ 直接得到0x00ABC678
  
性能提升：100倍以上！
```

### 7.2 TLB失效问题

**问题：** 修改页表后，TLB还是旧数据

```c
// 建立映射
vmm_map_page(0x40000000, 0x00200000, PAGE_PRESENT | PAGE_WRITE);

// TLB缓存了：0x40000000 → 0x00200000

// 修改映射
vmm_map_page(0x40000000, 0x00300000, PAGE_PRESENT | PAGE_WRITE);

// 但TLB还是旧的！
// CPU访问0x40000000还会去0x00200000
// → 数据错误！
```

**解决：刷新TLB**

```c
/* 方法1：刷新单个页（快）*/
__asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");

/* 方法2：刷新整个TLB（慢）*/
uint32_t cr3 = vmm_get_cr3();
vmm_set_cr3(cr3);  // 重新加载CR3会刷新TLB
```

**EduOS的实践（关键修复）：**

```c
/* 在vma.c的Page Fault处理中 */
vmm_map_page_in_directory(pd, vaddr, paddr, page_flags);

/* 刷新TLB（关键！之前缺少这行导致bug）*/
__asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");

return 0;  // 成功
```

**没有刷新TLB的后果：**

```
实际遇到的bug：
  1. Page Fault触发
  2. 分配物理页，建立映射
  3. 返回，重新执行指令
  4. CPU查TLB → 找不到（因为是新映射）
  5. CPU查页表 → 找到了
  6. 但...TLB没更新
  7. 下次访问 → TLB还是没有
  8. 又触发Page Fault！
  9. 无限循环！

解决：加一行invlpg
```

---

## 📖 第八课：递归页表映射

### 8.1 访问页表的难题

**问题：** 页表在物理内存中，如何访问？

```
页表物理地址 = 0x00123000

如果分页已启用：
  我们只能用虚拟地址！
  
但页表没有固定的虚拟地址...
怎么办？
```

**笨办法：**

```c
// 临时关闭分页
vmm_disable_paging();

// 访问物理地址
uint32_t *pt = (uint32_t*)0x00123000;
pt[0] = ...;

// 重新启用分页
vmm_enable_paging();

问题：太麻烦了！而且危险！
```

### 8.2 递归映射的巧妙技巧

**关键想法：** 让页目录的最后一项指向自己！

```c
// 页目录的第1023项指向页目录自身
page_directory[1023] = page_directory_phys | PAGE_PRESENT | PAGE_WRITE;
```

**魔法效果：**

```
访问虚拟地址0xFFC00000时：
  pd_index = 0xFFC00000 >> 22 = 1023
  pt_index = (0xFFC00000 >> 12) & 0x3FF = 0
  
  步骤1：查PD[1023] → 得到页目录物理地址（它指向自己！）
  步骤2：把页目录当作页表，查"页表"[0]
  步骤3：实际上查的是PD[0]！
  
  结果：0xFFC00000可以访问第0个页表！
        0xFFC01000可以访问第1个页表！
        ...
        0xFFFFF000可以访问页目录本身！
```

**EduOS的使用：**

```c
/* 获取第N个页表的虚拟地址 */
#define GET_PAGE_TABLE(n)  ((uint32_t*)(0xFFC00000 + (n) * 0x1000))

/* 获取页目录的虚拟地址 */
#define GET_PAGE_DIRECTORY()  ((uint32_t*)0xFFFFF000)

// 使用
uint32_t *pd = GET_PAGE_DIRECTORY();
uint32_t *pt = GET_PAGE_TABLE(72);

// 不需要物理地址了！直接用虚拟地址访问！
```

---

## 📖 第九课：创建用户进程页目录

### 9.1 为什么每个进程需要独立页目录？

**目标：** 每个进程看到不同的地址空间

```
进程A的地址空间：
0x08000000 → 物理0x00200000（A的代码）

进程B的地址空间：
0x08000000 → 物理0x00300000（B的代码）

通过切换CR3实现：
  运行A时：CR3 = A的页目录
  运行B时：CR3 = B的页目录
```

### 9.2 创建新页目录（EduOS实现）

基于 `kernel/mm/vmm.c` 的 `vmm_create_page_directory()`:

```c
struct page_directory *vmm_create_page_directory(void)
{
    /* 分配page_directory结构 */
    struct page_directory *pd = kmalloc(sizeof(struct page_directory));
    if (!pd) return NULL;
    
    /* 分配物理页目录（4KB）*/
    pd->physical_addr = pmm_alloc_frame();
    if (pd->physical_addr == 0) {
        kfree(pd);
        return NULL;
    }
    
    /* 访问新页目录（通过临时映射）*/
    // 在0xFFBFF000处临时映射新页目录
    vmm_map_page(0xFFBFF000, pd->physical_addr, PAGE_PRESENT | PAGE_WRITE);
    
    uint32_t *new_pd = (uint32_t*)0xFFBFF000;
    
    /* 清空页目录 */
    memset(new_pd, 0, PAGE_SIZE);
    
    /* Linux风格：复制内核空间映射（768-1022项）
     * 所有进程共享同一个内核映射
     */
    uint32_t *kernel_pd = GET_PAGE_DIRECTORY();
    for (int i = 768; i < 1023; i++) {
        new_pd[i] = kernel_pd[i];
    }
    
    /* 设置递归映射（第1023项指向自己） */
    new_pd[1023] = pd->physical_addr | PAGE_PRESENT | PAGE_WRITE;
    
    /* 解除临时映射 */
    vmm_unmap_page(0xFFBFF000);
    
    return pd;
}
```

**关键点：**

```
新进程的页目录：
  PD[0-767]：用户空间（0-3GB）← 空的，进程独有
  PD[768-1022]：内核空间（3GB-4GB）← 复制内核映射
  PD[1023]：递归映射 ← 指向自己
  
优势：
  1. 切换进程时，内核映射不变
  2. 所有进程共享内核代码/数据
  3. 效率高（不需要重复映射内核）
```

---

## 📖 第十课：地址空间切换

### 10.1 切换页目录（context switch）

```c
void vmm_switch_page_directory(struct page_directory *pd)
{
    if (!pd) return;
    
    /* 保存当前页目录 */
    current_directory = pd;
    
    /* 切换CR3 */
    vmm_set_cr3(pd->physical_addr);
    
    /* CR3改变会自动刷新TLB */
}
```

**进程切换时的使用：**

```c
void context_switch(struct process *prev, struct process *next)
{
    /* 保存旧进程状态 */
    if (prev) {
        save_context(&prev->context);
    }
    
    /* 切换页表（关键！）*/
    if (next->page_dir) {
        vmm_switch_page_directory(next->page_dir);
    }
    
    /* 恢复新进程状态 */
    restore_context(&next->context);
}
```

### 10.2 用户页表 vs 内核页表

**EduOS的地址空间布局：**

```
内核页表：
0x00000000 ┌──────────────┐
           │ (恒等映射)   │ ← PD[0-1]，临时使用
0x00400000 ├──────────────┤
           │ 未映射       │
           │              │
0xC0000000 ├──────────────┤
           │ 内核代码     │ ← PD[768]
0xC0400000 ├──────────────┤
           │ 内核堆       │ ← PD[769-...]
           │ (16MB)       │
0xE0000000 ├──────────────┤
           │ 设备内存     │ ← PD[896]，framebuffer
0xFFFFFFFF └──────────────┘

用户页表（进程A）：
0x00000000 ┌──────────────┐
           │ 未映射       │
0x08000000 ├──────────────┤
           │ 用户代码     │ ← PD[32]，按需加载
0x08010000 ├──────────────┤
           │ 用户数据     │ ← PD[32-33]
0x08020000 ├──────────────┤
           │ BSS段        │ ← PD[33-34]
0x08080000 ├──────────────┤
           │ 用户栈       │ ← PD[34-...]，按需扩展
0x40000000 ├──────────────┤
           │ mmap区域     │ ← PD[256-...]，如framebuffer
           │              │
0xC0000000 ├──────────────┤
           │ 内核空间     │ ← 与内核页表相同
           │ (共享)       │
0xFFFFFFFF └──────────────┘
```

---

## 📖 第十一课：实战练习

### 练习1：建立简单映射

```c
void test_vmm_basic(void)
{
    kprintf("=== VMM Basic Test ===\n");
    
    /* 分配物理页 */
    uint32_t phys = pmm_alloc_frame();
    kprintf("Allocated physical page: 0x%08x\n", phys);
    
    /* 建立映射 */
    uint32_t virt = 0xA0000000;  // 任意虚拟地址
    vmm_map_page(virt, phys, PAGE_PRESENT | PAGE_WRITE);
    kprintf("Mapped virtual 0x%08x → physical 0x%08x\n", virt, phys);
    
    /* 写入数据 */
    uint32_t *ptr = (uint32_t*)virt;
    *ptr = 0x12345678;
    kprintf("Wrote 0x12345678 to virtual address\n");
    
    /* 验证 */
    uint32_t value = *ptr;
    kprintf("Read back: 0x%08x\n", value);
    
    if (value == 0x12345678) {
        kprintf("✓ Mapping works!\n");
    }
    
    /* 清理 */
    vmm_unmap_page(virt);
    pmm_free_frame(phys);
}
```

### 练习2：测试地址转换

```c
void test_address_translation(void)
{
    kprintf("=== Address Translation Test ===\n");
    
    /* 测试内核地址 */
    uint32_t virt = 0xC0100000;
    uint32_t phys = vmm_virt_to_phys(virt);
    
    kprintf("Virtual:  0x%08x\n", virt);
    kprintf("Physical: 0x%08x\n", phys);
    kprintf("Expected: 0x00100000\n");
    
    if (phys == 0x00100000) {
        kprintf("✓ Kernel high address mapping correct!\n");
    }
}
```

### 练习3：创建用户页目录

```c
void test_user_page_directory(void)
{
    kprintf("=== User Page Directory Test ===\n");
    
    /* 创建新页目录 */
    struct page_directory *user_pd = vmm_create_page_directory();
    kprintf("Created page directory at phys: 0x%08x\n", 
            user_pd->physical_addr);
    
    /* 在用户空间建立映射 */
    uint32_t phys = pmm_alloc_frame();
    vmm_map_page_in_directory(user_pd, 0x08000000, phys, PAGE_USER_RW);
    
    kprintf("Mapped user space: 0x08000000 → 0x%08x\n", phys);
    
    /* 切换到用户页目录 */
    vmm_switch_page_directory(user_pd);
    kprintf("Switched to user page directory\n");
    
    /* 验证映射 */
    uint32_t test_phys = vmm_virt_to_phys(0x08000000);
    if (test_phys == phys) {
        kprintf("✓ User mapping works!\n");
    }
    
    /* 切回内核页目录 */
    vmm_switch_page_directory(&kernel_directory);
    
    /* 清理 */
    vmm_destroy_page_directory(user_pd);
    pmm_free_frame(phys);
}
```

---

## 🧪 实际运行效果（EduOS）

### VMM初始化输出

```
[VMM] Initializing Virtual Memory Manager...
[VMM] Inherited Page Directory from bootloader: 0x00009000
[VMM] Page Directory entries:
      PD[0] (0-4MB identity map): 0x0000a003
      PD[768] (3GB+ kernel map): 0x0000a003
[VMM] Recursive mapping set at PD[1023]
[VMM] Current address space layout:
      Identity mapping: 0x00000000 - 0x003FFFFF (0-4MB)
      Kernel mapping:   0xC0000000 - 0xC03FFFFF (3GB-3GB+4MB)
      Kernel physical:  0x00100000 - 0x0011f800
[VMM] Virtual Memory Manager initialized
```

### 创建用户进程页目录

```
[USER_PROC] Created user page directory at phys 0x00175000
[USER_PROC] Framebuffer (0xE0000000) shared to user space
[USER_PROC] VGA memory (0xB8000) shared to user space
```

---

## 💡 常见问题

### Q1：为什么内核要在高地址（3GB+）？

**A：** 保护和效率

```
方案1：内核在低地址（0-1GB）
  用户程序：1GB-4GB
  
  问题：
    每个进程的地址空间不同
    →切换进程时，所有用户映射都要改
    →慢！
    
方案2：内核在高地址（3GB-4GB）
  用户程序：0-3GB
  
  优势：
    所有进程的3GB-4GB都是内核
    →切换进程时，内核映射不变
    →快！
    →而且用户程序无法访问3GB+地址（保护）
```

### Q2：如果程序需要超过3GB内存怎么办？

**A：** 64位系统

```
32位：最多4GB地址空间
  内核：1GB
  用户：3GB
  
64位：理论上16EB（1600万TB）
  实际使用：48位 = 256TB
  足够了！
  
EduOS是32位教学系统，3GB够用
```

### Q3：页表会占用很多内存吗？

**A：** 按需分配，不会

```
最坏情况（4GB全映射）：
  1个页目录 = 4KB
  1024个页表 = 4MB
  总计 = 4MB + 4KB
  
实际情况（用户程序1MB）：
  1个页目录 = 4KB
  1个页表 = 4KB
  总计 = 8KB
  
EduOS：按需分配页表，用多少分配多少
```

---

## 🎓 本讲总结

### 核心概念

✅ **虚拟内存** - 给每个程序独立的地址空间  
✅ **分页** - 4KB为单位管理内存  
✅ **页表** - 虚拟地址到物理地址的映射  
✅ **二级页表** - 页目录+页表，节省空间  
✅ **TLB** - 地址转换缓存，必须刷新  
✅ **递归映射** - 巧妙访问页表的技巧  
✅ **高地址内核** - 所有进程共享内核映射  

### VMM核心API

```c
/* 初始化 */
void vmm_init(uint32_t kernel_end);

/* 页目录管理 */
struct page_directory *vmm_create_page_directory(void);
void vmm_destroy_page_directory(struct page_directory *pd);
void vmm_switch_page_directory(struct page_directory *pd);

/* 页面映射 */
void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void vmm_unmap_page(uint32_t virt);
void vmm_map_page_in_directory(struct page_directory *pd, 
                                uint32_t virt, uint32_t phys, uint32_t flags);

/* 地址转换 */
uint32_t vmm_virt_to_phys(uint32_t virt);
bool vmm_is_mapped(uint32_t virt);

/* TLB管理 */
void vmm_invlpg(uint32_t virt);    // 刷新单页
void vmm_flush_tlb(void);          // 刷新全部
```

### 重要规则

1. **地址必须4KB对齐** - 使用PAGE_ALIGN宏
2. **修改页表后必须刷新TLB** - invlpg或重载CR3
3. **内核映射必须在所有页目录中一致** - 复制PD[768-1022]
4. **访问物理内存要通过虚拟地址** - 递归映射或临时映射
5. **切换CR3会刷新TLB** - 性能考虑

### EduOS的VMM特点

- ✅ **递归映射** - 优雅访问页表
- ✅ **按需分配页表** - 节省内存
- ✅ **内核高地址映射** - 3GB-4GB
- ✅ **用户空间隔离** - 每个进程独立页目录
- ✅ **TLB管理** - 正确的刷新时机

### 下一步

学习 **第6讲：VFS文件系统**，理解"一切皆文件"的设计哲学！

---

**虚拟内存管理 - 让多任务成为可能！** 🗺️

