/*
 * PS/2键盘驱动
 * 
 * 支持US布局键盘，包括修饰键（Shift/Ctrl/Alt）
 */

#include <drivers/keyboard.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <io.h>
#include <kernel.h>
#include <vga.h>

/* 键盘状态 */
static struct {
    uint8_t flags;                          // 状态标志
    char buffer[KEYBOARD_BUFFER_SIZE];      // 环形缓冲区
    uint32_t read_pos;                      // 读位置
    uint32_t write_pos;                     // 写位置
    uint32_t count;                         // 缓冲区中的字符数
    bool extended;                          // 扩展扫描码标志
} kbd_state = {0};

/* US键盘扫描码到ASCII转换表（小写） */
static const char scancode_to_ascii_lower[128] = {
    0,    27,  '1',  '2',  '3',  '4',  '5',  '6',     // 0x00-0x07
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',   // 0x08-0x0F
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',     // 0x10-0x17
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',     // 0x18-0x1F
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',     // 0x20-0x27
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',     // 0x28-0x2F
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',     // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,       // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',     // 0x40-0x47 (小键盘开始)
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',     // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,       // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x58-0x5F
};

/* US键盘扫描码到ASCII转换表（大写/Shift） */
static const char scancode_to_ascii_upper[128] = {
    0,    27,  '!',  '@',  '#',  '$',  '%',  '^',     // 0x00-0x07
    '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',   // 0x08-0x0F
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',     // 0x10-0x17
    'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',     // 0x18-0x1F
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',     // 0x20-0x27
    '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',     // 0x28-0x2F
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',     // 0x30-0x37
    0,    ' ',  0,    0,    0,    0,    0,    0,       // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    '7',     // 0x40-0x47
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',     // 0x48-0x4F
    '2',  '3',  '0',  '.',  0,    0,    0,    0,       // 0x50-0x57
    0,    0,    0,    0,    0,    0,    0,    0,       // 0x58-0x5F
};

/*
 * 等待键盘输入缓冲区空闲
 */
static void keyboard_wait_input(void)
{
    uint32_t timeout = 100000;
    while ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_FULL) && timeout--);
}

/*
 * 等待键盘输出缓冲区有数据
 */
static void keyboard_wait_output(void)
{
    uint32_t timeout = 100000;
    while (!(inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) && timeout--);
}

/*
 * 添加字符到键盘缓冲区
 */
static void keyboard_buffer_put(char ch)
{
    if (kbd_state.count < KEYBOARD_BUFFER_SIZE) {
        kbd_state.buffer[kbd_state.write_pos] = ch;
        kbd_state.write_pos = (kbd_state.write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        kbd_state.count++;
    }
}

/*
 * 从键盘缓冲区读取字符
 */
static char keyboard_buffer_get(void)
{
    if (kbd_state.count == 0) {
        return 0;
    }
    
    char ch = kbd_state.buffer[kbd_state.read_pos];
    kbd_state.read_pos = (kbd_state.read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    kbd_state.count--;
    
    return ch;
}

/*
 * 扫描码转ASCII
 */
static char scancode_to_char(uint8_t scancode)
{
    if (scancode >= 128) {
        return 0;
    }
    
    bool shift = (kbd_state.flags & (KBD_FLAG_SHIFT | KBD_FLAG_CAPS)) != 0;
    
    /* 如果是字母且Caps Lock开启，翻转大小写 */
    char ch;
    if (shift) {
        ch = scancode_to_ascii_upper[scancode];
    } else {
        ch = scancode_to_ascii_lower[scancode];
    }
    
    /* Caps Lock只影响字母 */
    if ((kbd_state.flags & KBD_FLAG_CAPS) && (ch >= 'a' && ch <= 'z')) {
        ch = ch - 'a' + 'A';
    } else if ((kbd_state.flags & KBD_FLAG_CAPS) && (ch >= 'A' && ch <= 'Z')) {
        ch = ch - 'A' + 'a';
    }
    
    return ch;
}

/*
 * 键盘中断处理函数 (IRQ1)
 */
static void keyboard_handler(struct interrupt_frame *frame)
{
    (void)frame;  // 未使用
    
    /* 读取扫描码 */
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    /* 处理扩展扫描码 */
    if (scancode == KEY_EXTENDED) {
        kbd_state.extended = true;
        return;
    }
    
    /* 判断按键按下还是释放 */
    bool released = (scancode & KEY_RELEASE_FLAG) != 0;
    scancode &= ~KEY_RELEASE_FLAG;
    
    /* 处理修饰键 */
    if (scancode == KEY_LEFT_SHIFT || scancode == KEY_RIGHT_SHIFT) {
        if (released) {
            kbd_state.flags &= ~KBD_FLAG_SHIFT;
        } else {
            kbd_state.flags |= KBD_FLAG_SHIFT;
        }
        kbd_state.extended = false;
        return;
    }
    
    if (scancode == KEY_LEFT_CTRL) {
        if (released) {
            kbd_state.flags &= ~KBD_FLAG_CTRL;
        } else {
            kbd_state.flags |= KBD_FLAG_CTRL;
        }
        kbd_state.extended = false;
        return;
    }
    
    if (scancode == KEY_LEFT_ALT) {
        if (released) {
            kbd_state.flags &= ~KBD_FLAG_ALT;
        } else {
            kbd_state.flags |= KBD_FLAG_ALT;
        }
        kbd_state.extended = false;
        return;
    }
    
    /* 处理锁定键 */
    if (scancode == KEY_CAPS_LOCK && !released) {
        kbd_state.flags ^= KBD_FLAG_CAPS;
        keyboard_set_leds(kbd_state.flags & KBD_FLAG_CAPS,
                         kbd_state.flags & KBD_FLAG_NUM,
                         kbd_state.flags & KBD_FLAG_SCROLL);
        kbd_state.extended = false;
        return;
    }
    
    if (scancode == KEY_NUM_LOCK && !released) {
        kbd_state.flags ^= KBD_FLAG_NUM;
        keyboard_set_leds(kbd_state.flags & KBD_FLAG_CAPS,
                         kbd_state.flags & KBD_FLAG_NUM,
                         kbd_state.flags & KBD_FLAG_SCROLL);
        kbd_state.extended = false;
        return;
    }
    
    if (scancode == KEY_SCROLL_LOCK && !released) {
        kbd_state.flags ^= KBD_FLAG_SCROLL;
        keyboard_set_leds(kbd_state.flags & KBD_FLAG_CAPS,
                         kbd_state.flags & KBD_FLAG_NUM,
                         kbd_state.flags & KBD_FLAG_SCROLL);
        kbd_state.extended = false;
        return;
    }
    
    /* 处理普通按键（仅按下事件） */
    if (!released) {
        /* 处理Ctrl组合键 */
        if (kbd_state.flags & KBD_FLAG_CTRL) {
            switch (scancode) {
                case 0x2E:  // Ctrl+C
                    kprintf("^C");
                    keyboard_buffer_put(3);  // ETX (End of Text)
                    break;
                case 0x20:  // Ctrl+D
                    keyboard_buffer_put(4);  // EOT (End of Transmission)
                    break;
                case 0x26:  // Ctrl+L
                    vga_clear();
                    break;
            }
        } else {
            /* 转换为ASCII并添加到缓冲区 */
            char ch = scancode_to_char(scancode);
            if (ch != 0) {
                keyboard_buffer_put(ch);
            }
        }
    }
    
    kbd_state.extended = false;
}

/*
 * 键盘初始化
 */
void keyboard_init(void)
{
    /* 清空键盘状态 */
    kbd_state.flags = 0;
    kbd_state.read_pos = 0;
    kbd_state.write_pos = 0;
    kbd_state.count = 0;
    kbd_state.extended = false;
    
    /* 清空键盘缓冲区（硬件） */
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_FULL) {
        inb(KEYBOARD_DATA_PORT);
    }
    
    /* 注册IRQ1处理函数 */
    irq_install_handler(IRQ_KEYBOARD, keyboard_handler);
    irq_enable(IRQ_KEYBOARD);
    
    /* 初始化LED状态 */
    keyboard_set_leds(false, false, false);
    
    kprintf("[KEYBOARD] PS/2 Keyboard initialized\n");
    kprintf("[KEYBOARD] Layout: US (QWERTY)\n");
}

/*
 * 读取按键（阻塞）
 */
char keyboard_getchar(void)
{
    while (kbd_state.count == 0) {
        __asm__ volatile("hlt");  // 等待IRQ1中断
    }
    
    return keyboard_buffer_get();
}

/*
 * 检查是否有按键（非阻塞）
 */
bool keyboard_haskey(void)
{
    return kbd_state.count > 0;
}

/*
 * 获取键盘状态标志
 */
uint8_t keyboard_get_flags(void)
{
    return kbd_state.flags;
}

/*
 * 设置LED状态
 */
void keyboard_set_leds(bool caps, bool num, bool scroll)
{
    uint8_t led_state = 0;
    if (caps)   led_state |= 0x04;
    if (num)    led_state |= 0x02;
    if (scroll) led_state |= 0x01;
    
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, 0xED);  // 设置LED命令
    
    keyboard_wait_input();
    outb(KEYBOARD_DATA_PORT, led_state);
}

/*
 * 清空键盘缓冲区
 */
void keyboard_flush(void)
{
    kbd_state.read_pos = 0;
    kbd_state.write_pos = 0;
    kbd_state.count = 0;
}

/*
 * 打印键盘信息
 */
void keyboard_print_info(void)
{
    kprintf("\n=== Keyboard Information ===\n");
    kprintf("Status Flags: 0x%02x\n", kbd_state.flags);
    kprintf("  SHIFT: %s\n", (kbd_state.flags & KBD_FLAG_SHIFT) ? "ON" : "OFF");
    kprintf("  CTRL:  %s\n", (kbd_state.flags & KBD_FLAG_CTRL) ? "ON" : "OFF");
    kprintf("  ALT:   %s\n", (kbd_state.flags & KBD_FLAG_ALT) ? "ON" : "OFF");
    kprintf("  CAPS:  %s\n", (kbd_state.flags & KBD_FLAG_CAPS) ? "ON" : "OFF");
    kprintf("  NUM:   %s\n", (kbd_state.flags & KBD_FLAG_NUM) ? "ON" : "OFF");
    kprintf("  SCROLL:%s\n", (kbd_state.flags & KBD_FLAG_SCROLL) ? "ON" : "OFF");
    kprintf("Buffer Count: %d / %d\n", kbd_state.count, KEYBOARD_BUFFER_SIZE);
    kprintf("\n");
}

