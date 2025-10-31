/*
 * gui_core.c - GUI库核心实现
 */

#include "gui.h"
#include "../unistd.h"
#include "../stdio.h"
#include "../stdlib.h"
#include "../string.h"

/* GUI上下文 */
struct GuiContext {
    int fb_fd;              /* framebuffer文件描述符 */
    uint32_t *framebuffer;  /* framebuffer指针 */
    int width;
    int height;
    int bpp;
    Font *default_font;
    Window *windows;        /* 窗口链表 */
};

/* 窗口结构 */
struct Window {
    Rect bounds;
    const char *title;
    bool visible;
    Widget *widgets;        /* 控件链表 */
    Window *next;
};

/*
 * 初始化GUI系统
 */
GuiContext *gui_init(void)
{
    GuiContext *ctx = malloc(sizeof(GuiContext));
    if (!ctx) {
        return NULL;
    }
    
    /* 打开framebuffer设备 */
    ctx->fb_fd = open("/dev/fb0", 0);
    if (ctx->fb_fd < 0) {
        free(ctx);
        return NULL;
    }
    
    /* 获取屏幕信息（暂时硬编码，完整版应该用ioctl） */
    ctx->width = 1024;
    ctx->height = 768;
    ctx->bpp = 32;
    
    /* Linux方式：通过mmap映射framebuffer */
    size_t fb_size = ctx->width * ctx->height * (ctx->bpp / 8);
    
    /* PROT_READ | PROT_WRITE (0x3), MAP_SHARED (0x01) */
    ctx->framebuffer = mmap(NULL, fb_size, 0x3, 0x01, ctx->fb_fd, 0);
    
    if (ctx->framebuffer == (void*)-1 || ctx->framebuffer == NULL) {
        close(ctx->fb_fd);
        free(ctx);
        return NULL;
    }
    
    ctx->windows = NULL;
    ctx->default_font = NULL;
    
    return ctx;
}

/*
 * 关闭GUI系统
 */
void gui_shutdown(GuiContext *ctx)
{
    if (!ctx) return;
    
    if (ctx->fb_fd >= 0) {
        close(ctx->fb_fd);
    }
    
    free(ctx);
}

/*
 * 获取屏幕尺寸
 */
int gui_get_width(GuiContext *ctx)
{
    return ctx ? ctx->width : 0;
}

int gui_get_height(GuiContext *ctx)
{
    return ctx ? ctx->height : 0;
}

/*
 * 刷新屏幕（当前是直接写framebuffer，所以不需要）
 */
void gui_update(GuiContext *ctx)
{
    (void)ctx;
    /* TODO: 如果使用双缓冲，这里需要交换缓冲区 */
}

/*
 * 设置像素
 */
void gui_set_pixel(GuiContext *ctx, int x, int y, Color color)
{
    if (!ctx) return;
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
        return;
    }
    
    ctx->framebuffer[y * ctx->width + x] = color;
}

/*
 * 获取像素
 */
Color gui_get_pixel(GuiContext *ctx, int x, int y)
{
    if (!ctx) return 0;
    if (x < 0 || x >= ctx->width || y < 0 || y >= ctx->height) {
        return 0;
    }
    
    return ctx->framebuffer[y * ctx->width + x];
}

/*
 * 填充矩形
 */
void gui_fill_rect(GuiContext *ctx, Rect rect, Color color)
{
    if (!ctx) return;
    
    /* 裁剪到屏幕范围 */
    if (rect.x < 0) {
        rect.width += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.height += rect.y;
        rect.y = 0;
    }
    if (rect.x + rect.width > ctx->width) {
        rect.width = ctx->width - rect.x;
    }
    if (rect.y + rect.height > ctx->height) {
        rect.height = ctx->height - rect.y;
    }
    
    for (int y = 0; y < rect.height; y++) {
        for (int x = 0; x < rect.width; x++) {
            gui_set_pixel(ctx, rect.x + x, rect.y + y, color);
        }
    }
}

/*
 * 清屏
 */
void gui_fill_screen(GuiContext *ctx, Color color)
{
    if (!ctx) return;
    
    Rect screen = {0, 0, ctx->width, ctx->height};
    gui_fill_rect(ctx, screen, color);
}

/*
 * 绘制矩形边框
 */
void gui_draw_rect(GuiContext *ctx, Rect rect, Color color, int thickness)
{
    if (!ctx || thickness <= 0) return;
    
    /* 上边 */
    Rect top = {rect.x, rect.y, rect.width, thickness};
    gui_fill_rect(ctx, top, color);
    
    /* 下边 */
    Rect bottom = {rect.x, rect.y + rect.height - thickness, rect.width, thickness};
    gui_fill_rect(ctx, bottom, color);
    
    /* 左边 */
    Rect left = {rect.x, rect.y, thickness, rect.height};
    gui_fill_rect(ctx, left, color);
    
    /* 右边 */
    Rect right = {rect.x + rect.width - thickness, rect.y, thickness, rect.height};
    gui_fill_rect(ctx, right, color);
}

/*
 * 绘制线条（Bresenham算法）
 */
void gui_draw_line(GuiContext *ctx, int x1, int y1, int x2, int y2, Color color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int dx_abs = dx < 0 ? -dx : dx;
    int dy_abs = dy < 0 ? -dy : dy;
    int sx = dx < 0 ? -1 : 1;
    int sy = dy < 0 ? -1 : 1;
    
    int x = x1;
    int y = y1;
    
    if (dx_abs > dy_abs) {
        int d = 2 * dy_abs - dx_abs;
        for (int i = 0; i <= dx_abs; i++) {
            gui_set_pixel(ctx, x, y, color);
            if (d > 0) {
                y += sy;
                d -= 2 * dx_abs;
            }
            d += 2 * dy_abs;
            x += sx;
        }
    } else {
        int d = 2 * dx_abs - dy_abs;
        for (int i = 0; i <= dy_abs; i++) {
            gui_set_pixel(ctx, x, y, color);
            if (d > 0) {
                x += sx;
                d -= 2 * dy_abs;
            }
            d += 2 * dx_abs;
            y += sy;
        }
    }
}
