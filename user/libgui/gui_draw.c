/*
 * gui_draw.c - Fluent Design 高级绘图效果
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"

/* gui_put_pixel在gui_core.c中实现，因为需要访问GuiContext内部 */

/* 辅助函数：Alpha混合 */
static Color blend_color(Color bg, Color fg)
{
    uint8_t alpha = (fg >> 24) & 0xFF;
    if (alpha == 0xFF) return fg;
    if (alpha == 0x00) return bg;
    
    uint8_t r_fg = (fg >> 16) & 0xFF;
    uint8_t g_fg = (fg >> 8) & 0xFF;
    uint8_t b_fg = fg & 0xFF;
    
    uint8_t r_bg = (bg >> 16) & 0xFF;
    uint8_t g_bg = (bg >> 8) & 0xFF;
    uint8_t b_bg = bg & 0xFF;
    
    uint8_t r = (r_fg * alpha + r_bg * (255 - alpha)) / 255;
    uint8_t g = (g_fg * alpha + g_bg * (255 - alpha)) / 255;
    uint8_t b = (b_fg * alpha + b_bg * (255 - alpha)) / 255;
    
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/*
 * 绘制圆角矩形（填充）
 */
void gui_fill_rounded_rect(GuiContext *ctx, Rect rect, int radius, Color color)
{
    if (!ctx) return;
    if (radius <= 0) {
        gui_fill_rect(ctx, rect, color);
        return;
    }
    
    /* 限制圆角半径 */
    if (radius > rect.width / 2) radius = rect.width / 2;
    if (radius > rect.height / 2) radius = rect.height / 2;
    
    /* 填充中间矩形 */
    Rect center = {
        rect.x + radius,
        rect.y,
        rect.width - 2 * radius,
        rect.height
    };
    gui_fill_rect(ctx, center, color);
    
    /* 填充左右矩形 */
    Rect left = {rect.x, rect.y + radius, radius, rect.height - 2 * radius};
    Rect right = {rect.x + rect.width - radius, rect.y + radius, radius, rect.height - 2 * radius};
    gui_fill_rect(ctx, left, color);
    gui_fill_rect(ctx, right, color);
    
    /* 绘制四个圆角（简化：用方块近似） */
    /* TODO: 实现真正的圆角（反走样） */
    int r_sq = radius * radius;
    
    /* 左上角 */
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = radius - x;
            int dy = radius - y;
            if (dx * dx + dy * dy <= r_sq) {
                gui_set_pixel(ctx, rect.x + x, rect.y + y, color);
            }
        }
    }
    
    /* 右上角 */
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = x;
            int dy = radius - y;
            if (dx * dx + dy * dy <= r_sq) {
                gui_set_pixel(ctx, rect.x + rect.width - radius + x, rect.y + y, color);
            }
        }
    }
    
    /* 左下角 */
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = radius - x;
            int dy = y;
            if (dx * dx + dy * dy <= r_sq) {
                gui_set_pixel(ctx, rect.x + x, rect.y + rect.height - radius + y, color);
            }
        }
    }
    
    /* 右下角 */
    for (int y = 0; y < radius; y++) {
        for (int x = 0; x < radius; x++) {
            int dx = x;
            int dy = y;
            if (dx * dx + dy * dy <= r_sq) {
                gui_set_pixel(ctx, rect.x + rect.width - radius + x, 
                             rect.y + rect.height - radius + y, color);
            }
        }
    }
}

/*
 * 绘制圆角矩形（边框）
 */
void gui_draw_rounded_rect(GuiContext *ctx, Rect rect, int radius, Color color, int thickness)
{
    if (!ctx || thickness <= 0) return;
    
    /* 简化实现：画两个圆角矩形 */
    gui_fill_rounded_rect(ctx, rect, radius, color);
    
    Rect inner = {
        rect.x + thickness,
        rect.y + thickness,
        rect.width - 2 * thickness,
        rect.height - 2 * thickness
    };
    
    /* 用背景色填充内部（TODO: 应该获取实际背景色） */
    gui_fill_rounded_rect(ctx, inner, radius - thickness, COLOR_BG_LAYER);
}

/*
 * 垂直渐变
 */
void gui_fill_gradient_vertical(GuiContext *ctx, Rect rect, Color color1, Color color2)
{
    if (!ctx || rect.height <= 0) return;
    
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    for (int y = 0; y < rect.height; y++) {
        int factor = (y * 256) / rect.height;
        
        uint8_t r = (r1 * (256 - factor) + r2 * factor) / 256;
        uint8_t g = (g1 * (256 - factor) + g2 * factor) / 256;
        uint8_t b = (b1 * (256 - factor) + b2 * factor) / 256;
        
        Color line_color = 0xFF000000 | (r << 16) | (g << 8) | b;
        
        for (int x = 0; x < rect.width; x++) {
            gui_set_pixel(ctx, rect.x + x, rect.y + y, line_color);
        }
    }
}

/*
 * 水平渐变
 */
void gui_fill_gradient_horizontal(GuiContext *ctx, Rect rect, Color color1, Color color2)
{
    if (!ctx || rect.width <= 0) return;
    
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    for (int x = 0; x < rect.width; x++) {
        int factor = (x * 256) / rect.width;
        
        uint8_t r = (r1 * (256 - factor) + r2 * factor) / 256;
        uint8_t g = (g1 * (256 - factor) + g2 * factor) / 256;
        uint8_t b = (b1 * (256 - factor) + b2 * factor) / 256;
        
        Color line_color = 0xFF000000 | (r << 16) | (g << 8) | b;
        
        for (int y = 0; y < rect.height; y++) {
            gui_set_pixel(ctx, rect.x + x, rect.y + y, line_color);
        }
    }
}

/*
 * 绘制阴影（简化的模糊阴影）
 */
void gui_draw_shadow(GuiContext *ctx, Rect rect, int radius, int blur, Color shadow_color)
{
    if (!ctx || blur <= 0) return;
    
    /* 简化实现：绘制多层半透明矩形 */
    uint8_t base_alpha = (shadow_color >> 24) & 0xFF;
    
    for (int i = blur; i > 0; i--) {
        int offset = blur - i;
        uint8_t alpha = (base_alpha * i) / blur / 2;
        
        Color layer_color = (alpha << 24) | (shadow_color & 0x00FFFFFF);
        
        Rect shadow_rect = {
            rect.x + offset,
            rect.y + offset,
            rect.width,
            rect.height
        };
        
        gui_fill_rounded_rect(ctx, shadow_rect, radius, layer_color);
    }
}

/*
 * 绘制圆
 */
void gui_draw_circle(GuiContext *ctx, int cx, int cy, int radius, Color color)
{
    if (!ctx || radius <= 0) return;
    
    int r_sq = radius * radius;
    
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= r_sq) {
                gui_set_pixel(ctx, cx + x, cy + y, color);
            }
        }
    }
}
