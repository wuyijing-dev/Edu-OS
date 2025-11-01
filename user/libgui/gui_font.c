/*
 * gui_font.c - 字体渲染（VGA 8x16位图字体）
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"
#include "../libc/string/string.h"
#include "font_8x16.h"

/* 字体结构 */
struct Font {
    int char_width;
    int char_height;
};

/*
 * 加载字体
 */
Font *gui_load_font(GuiContext *ctx)
{
    (void)ctx;
    
    Font *font = malloc(sizeof(Font));
    if (!font) return NULL;
    
    font->char_width = VGA_FONT_WIDTH;
    font->char_height = VGA_FONT_HEIGHT;
    
    return font;
}

/*
 * 绘制单个字符（VGA 8x16位图字体）
 */
static void draw_char_simple(GuiContext *ctx, int x, int y, char c, Color color)
{
    /* 只支持可打印ASCII字符（32-122）*/
    if (c < 32 || c > 122) {
        c = '?';  /* 不支持的字符显示为? */
    }
    
    int index = c - 32;
    if (index >= VGA_FONT_CHARS) {
        return;
    }
    
    const uint8_t *bitmap = vga_font_8x16[index];
    
    /* 逐行绘制字符 */
    for (int row = 0; row < VGA_FONT_HEIGHT; row++) {
        uint8_t line = bitmap[row];
        for (int col = 0; col < VGA_FONT_WIDTH; col++) {
            /* 测试位：从低位到高位（修复镜像问题）*/
            if (line & (1 << col)) {
                gui_put_pixel(ctx, x + col, y + row, color);
            }
        }
    }
}

/*
 * 绘制文本
 */
void gui_draw_text(GuiContext *ctx, Font *font, int x, int y, const char *text, Color color)
{
    if (!ctx || !text) return;
    
    int char_width = font ? font->char_width : 8;
    int pos_x = x;
    
    while (*text) {
        if (*text == '\n') {
            pos_x = x;
            y += 16;
        } else {
            draw_char_simple(ctx, pos_x, y, *text, color);
            pos_x += char_width;
        }
        text++;
    }
}

/*
 * 居中绘制文本
 */
void gui_draw_text_centered(GuiContext *ctx, Font *font, Rect rect, const char *text, Color color)
{
    if (!ctx || !text) return;
    
    int text_len = strlen(text);
    int char_width = font ? font->char_width : 8;
    int char_height = font ? font->char_height : 16;
    
    int text_width = text_len * char_width;
    int text_x = rect.x + (rect.width - text_width) / 2;
    int text_y = rect.y + (rect.height - char_height) / 2;
    
    gui_draw_text(ctx, font, text_x, text_y, text, color);
}

/*
 * 测量文本宽度
 */
int gui_text_width(Font *font, const char *text)
{
    if (!text) return 0;
    
    int char_width = font ? font->char_width : 8;
    return strlen(text) * char_width;
}

/*
 * 获取字体高度
 */
int gui_text_height(Font *font)
{
    return font ? font->char_height : 16;
}
