/*
 * CPU异常处理器
 * 
 * 处理所有CPU异常（中断0-31）
 */

#include <arch/i386/idt.h>
#include <kernel.h>
#include <vga.h>

/* 异常名称表 */
static const char *exception_messages[32] = {
    "Division By Zero",                 // 0
    "Debug",                            // 1
    "Non Maskable Interrupt",           // 2
    "Breakpoint",                       // 3
    "Overflow",                         // 4
    "Bound Range Exceeded",             // 5
    "Invalid Opcode",                   // 6
    "Device Not Available",             // 7
    "Double Fault",                     // 8
    "Coprocessor Segment Overrun",      // 9
    "Invalid TSS",                      // 10
    "Segment Not Present",              // 11
    "Stack-Segment Fault",              // 12
    "General Protection Fault",         // 13
    "Page Fault",                       // 14
    "Reserved",                         // 15
    "x87 FPU Error",                    // 16
    "Alignment Check",                  // 17
    "Machine Check",                    // 18
    "SIMD Floating-Point",              // 19
    "Virtualization",                   // 20
    "Control Protection",               // 21
    "Reserved",                         // 22
    "Reserved",                         // 23
    "Reserved",                         // 24
    "Reserved",                         // 25
    "Reserved",                         // 26
    "Reserved",                         // 27
    "Reserved",                         // 28
    "Reserved",                         // 29
    "Reserved",                         // 30
    "Reserved",                         // 31
};

/* 异常是否有错误码 */
static const bool exception_has_error_code[32] = {
    false, false, false, false, false, false, false, false,  // 0-7
    true,  false, true,  true,  true,  true,  true,  false, // 8-15
    false, true,  false, false, false, true,  false, false, // 16-23
    false, false, false, false, false, false, false, false, // 24-31
};

/*
 * 打印寄存器状态
 */
static void print_registers(struct interrupt_frame *frame)
{
    kprintf("\n=== CPU Registers ===\n");
    kprintf("EAX: 0x%08x  EBX: 0x%08x  ECX: 0x%08x  EDX: 0x%08x\n",
            frame->eax, frame->ebx, frame->ecx, frame->edx);
    kprintf("ESI: 0x%08x  EDI: 0x%08x  EBP: 0x%08x  ESP: 0x%08x\n",
            frame->esi, frame->edi, frame->ebp, frame->user_esp);
    kprintf("EIP: 0x%08x  EFLAGS: 0x%08x\n",
            frame->eip, frame->eflags);
    kprintf("CS: 0x%04x  DS: 0x%04x  ES: 0x%04x  FS: 0x%04x  GS: 0x%04x  SS: 0x%04x\n",
            frame->cs, frame->ds, frame->es, frame->fs, frame->gs, frame->ss);
}

/*
 * 打印栈回溯（简单版本）
 */
static void print_stack_trace(uint32_t *ebp, int max_frames)
{
    kprintf("\n=== Stack Trace ===\n");
    
    for (int frame = 0; frame < max_frames && ebp != NULL; frame++) {
        /* 检查EBP是否有效 */
        if ((uint32_t)ebp < 0x1000 || (uint32_t)ebp > 0xC0000000) {
            break;
        }
        
        uint32_t eip = ebp[1];  // 返回地址
        uint32_t *prev_ebp = (uint32_t*)ebp[0];  // 上一帧的EBP
        
        kprintf("  #%d: 0x%08x", frame, eip);
        
        /* 显示函数参数（猜测，可能不准确） */
        if ((uint32_t)ebp + 12 < 0xC0000000) {
            kprintf(" (args: 0x%x, 0x%x, 0x%x)",
                    ebp[2], ebp[3], ebp[4]);
        }
        kprintf("\n");
        
        ebp = prev_ebp;
    }
}

/*
 * 除零异常处理器
 */
static void handle_divide_error(struct interrupt_frame *frame)
{
    kprintf("\n!!! EXCEPTION: Division By Zero !!!\n");
    kprintf("Attempted to divide by zero at EIP: 0x%08x\n", frame->eip);
}

/*
 * 断点异常处理器
 */
static void handle_breakpoint(struct interrupt_frame *frame)
{
    kprintf("\n--- Breakpoint Hit ---\n");
    kprintf("EIP: 0x%08x\n", frame->eip);
}

/*
 * 通用保护故障处理器
 */
static void handle_general_protection(struct interrupt_frame *frame)
{
    kprintf("\n!!! EXCEPTION: General Protection Fault !!!\n");
    kprintf("Error Code: 0x%08x\n", frame->err_code);
    
    /* 解析错误码 */
    if (frame->err_code != 0) {
        bool external = frame->err_code & 0x1;
        uint8_t table = (frame->err_code >> 1) & 0x3;  // 0=GDT, 1=IDT, 2/3=LDT
        uint16_t index = (frame->err_code >> 3) & 0x1FFF;
        
        kprintf("  External: %s\n", external ? "Yes" : "No");
        kprintf("  Table: %s\n", 
                table == 0 ? "GDT" : table == 1 ? "IDT" : "LDT");
        kprintf("  Index: %d (0x%x)\n", index, index);
    }
}

/*
 * 缺页异常处理器
 */
static void handle_page_fault(struct interrupt_frame *frame)
{
    /* 读取CR2寄存器（引发缺页的地址） */
    uint32_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    /* 解析错误码 */
    bool present  = frame->err_code & 0x1;   // 页不存在
    bool write    = frame->err_code & 0x2;   // 写操作
    bool user     = frame->err_code & 0x4;   // 用户模式
    bool reserved = frame->err_code & 0x8;   // 保留位被设置
    bool ifetch   = frame->err_code & 0x10;  // 指令取指
    
    /* 
     * 处理策略：
     * 1. 如果在内核空间且页不存在，可能是按需分配
     * 2. 如果是权限错误，报告并panic
     * 3. 如果在用户空间，由进程管理器处理（当前未实现）
     */
    
    if (!present && !user && fault_addr >= 0xC0000000) {
        /* 内核空间缺页 - 这通常不应该发生 */
        /* 在完整实现中，这里可能是按需分配堆内存 */
    }
    
    /* 打印详细信息 */
    kprintf("\n!!! EXCEPTION: Page Fault !!!\n");
    kprintf("Fault Address: 0x%08x\n", fault_addr);
    kprintf("Error Code: 0x%08x\n", frame->err_code);
    kprintf("  Cause: %s %s in %s mode%s%s\n",
            write ? "Write" : ifetch ? "Instruction fetch" : "Read",
            present ? "protection violation" : "non-present page",
            user ? "user" : "kernel",
            reserved ? ", reserved bit set" : "",
            ifetch ? ", instruction fetch" : "");
    
    /* 如果在用户模式，可以尝试恢复；内核模式则必须panic */
    if (!user) {
        /* 内核空间缺页是严重错误 */
        panic("Kernel page fault");
    }
    
    /* TODO: 用户空间缺页 - 终止进程或按需分配 */
}

/*
 * 无效操作码处理器
 */
static void handle_invalid_opcode(struct interrupt_frame *frame)
{
    kprintf("\n!!! EXCEPTION: Invalid Opcode !!!\n");
    kprintf("Invalid instruction at EIP: 0x%08x\n", frame->eip);
    
    /* 尝试显示指令字节 */
    uint8_t *instr = (uint8_t*)frame->eip;
    kprintf("  Instruction bytes: %02x %02x %02x %02x\n",
            instr[0], instr[1], instr[2], instr[3]);
}

/*
 * 双重故障处理器
 */
static void handle_double_fault(struct interrupt_frame *frame)
{
    kprintf("\n!!! CRITICAL: Double Fault !!!\n");
    kprintf("Error Code: 0x%08x\n", frame->err_code);
    kprintf("This usually indicates a serious kernel bug!\n");
}

/*
 * 主异常处理函数
 * 
 * 由汇编ISR调用
 */
void exception_handler(struct interrupt_frame *frame)
{
    uint32_t int_no = frame->int_no;
    
    /* 设置红色文本表示错误 */
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    
    kprintf("\n");
    kprintf("╔════════════════════════════════════════════════════════════╗\n");
    kprintf("║             CPU EXCEPTION DETECTED                        ║\n");
    kprintf("╚════════════════════════════════════════════════════════════╝\n");
    
    /* 基本异常信息 */
    kprintf("\nException #%d: %s\n", int_no, exception_messages[int_no]);
    if (exception_has_error_code[int_no]) {
        kprintf("Error Code: 0x%08x\n", frame->err_code);
    }
    
    /* 调用特定异常处理器 */
    switch (int_no) {
        case EXCEPTION_DE:
            handle_divide_error(frame);
            break;
        case EXCEPTION_BP:
            handle_breakpoint(frame);
            break;
        case EXCEPTION_UD:
            handle_invalid_opcode(frame);
            break;
        case EXCEPTION_DF:
            handle_double_fault(frame);
            break;
        case EXCEPTION_GP:
            handle_general_protection(frame);
            break;
        case EXCEPTION_PF:
            handle_page_fault(frame);
            break;
        default:
            kprintf("No specific handler for this exception\n");
            break;
    }
    
    /* 打印寄存器状态 */
    print_registers(frame);
    
    /* 打印栈回溯 */
    print_stack_trace((uint32_t*)frame->ebp, 10);
    
    /* 恢复正常颜色 */
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    /* 
     * 生产环境处理策略：
     * 1. 如果是用户进程异常：终止进程
     * 2. 如果是内核异常：panic（系统崩溃）
     * 3. 如果是断点：返回（继续调试）
     */
    
    if (int_no == EXCEPTION_BP) {
        /* 断点异常：继续执行 */
        return;
    }
    
    /* 其他异常：内核崩溃 */
    kprintf("\n");
    panic("Unhandled CPU exception");
}

