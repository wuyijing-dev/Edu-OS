/*
 * gui_font.c - 字体渲染（简化的8x16位图字体）
 */

#include "gui.h"
#include "../stdlib.h"
#include "../string.h"

/* 简化的字体结构 */
struct Font {
    int char_width;
    int char_height;
    const uint8_t *data;  /* 将使用内嵌的简单字体数据 */
};

/* 简化的8x8字体数据（部分ASCII字符） */
static const uint8_t simple_font_8x8[][8] = {
    /* 'A' (65) */
    {0x18, 0x24, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42},
    /* 'B' */
    {0x7C, 0x42, 0x42, 0x7C, 0x42, 0x42, 0x42, 0x7C},
    /* 'C' */
    {0x3C, 0x42, 0x40, 0x40, 0x40, 0x40, 0x42, 0x3C},
    /* ... 更多字符会在完整实现中添加 */
};

/*
 * 加载字体
 */
Font *gui_load_font(GuiContext *ctx)
{
    (void)ctx;
    
    Font *font = malloc(sizeof(Font));
    if (!font) return NULL;
    
    font->char_width = 8;
    font->char_height = 16;
    font->data = NULL;  /* 简化实现 */
    
    return font;
}

/*
 * 绘制单个字符（简化版：用矩形代替）
 */
static void draw_char_simple(GuiContext *ctx, int x, int y, char c, Color color)
{
    /* 简化实现：每个字符用一个小方块表示 */
    Rect char_rect = {x, y, 8, 16};
    
    if (c >= 'A' && c <= 'Z') {
        /* 大写字母：填充80% */
        gui_fill_rect(ctx, (Rect){x+1, y+2, 6, 12}, color);
    } else if (c >= 'a' && c <= 'z') {
        /* 小写字母：填充60% */
        gui_fill_rect(ctx, (Rect){x+1, y+4, 6, 8}, color);
    } else if (c == ' ') {
        /* 空格：不绘制 */
    } else {
        /* 其他字符：小方块 */
        gui_fill_rect(ctx, (Rect){x+2, y+6, 4, 4}, color);
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
