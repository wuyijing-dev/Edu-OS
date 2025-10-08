/* vga.c - VGA文本模式驱动 */

#include "vga.h"
#include "io.h"

static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t current_color = 0;
static volatile uint16_t *vga_buffer = (volatile uint16_t *)VGA_MEMORY;

static void update_cursor(void)
{
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_HIGH);
    outb(VGA_DATA_REGISTER, (pos >> 8) & 0xFF);
    outb(VGA_CTRL_REGISTER, VGA_CURSOR_LOW);
    outb(VGA_DATA_REGISTER, pos & 0xFF);
}

void vga_init(void)
{
    current_color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
    vga_enable_cursor(14, 15);
}

void vga_clear(void)
{
    uint16_t blank = vga_make_entry(' ', current_color);
    
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    current_color = vga_make_color(fg, bg);
}

uint8_t vga_get_color(void)
{
    return current_color;
}

void vga_scroll(void)
{
    uint16_t blank = vga_make_entry(' ', current_color);
    
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

void vga_putc(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) cursor_x--;
    } else if (c >= ' ') {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_make_entry(c, current_color);
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
    
    update_cursor();
}

void vga_puts(const char *str)
{
    while (*str) {
        vga_putc(*str++);
    }
}

void vga_putc_at(char c, uint8_t color, uint8_t x, uint8_t y)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_make_entry(c, color);
    }
}

void vga_set_cursor(uint8_t x, uint8_t y)
{
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        cursor_x = x;
        cursor_y = y;
        update_cursor();
    }
}

void vga_get_cursor(uint8_t *x, uint8_t *y)
{
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end)
{
    outb(VGA_CTRL_REGISTER, 0x0A);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xC0) | cursor_start);
    outb(VGA_CTRL_REGISTER, 0x0B);
    outb(VGA_DATA_REGISTER, (inb(VGA_DATA_REGISTER) & 0xE0) | cursor_end);
}

void vga_disable_cursor(void)
{
    outb(VGA_CTRL_REGISTER, 0x0A);
    outb(VGA_DATA_REGISTER, 0x20);
}

void vga_write_string(const char *str)
{
    vga_puts(str);
}

void vga_putchar(char c)
{
    vga_putc(c);
}
