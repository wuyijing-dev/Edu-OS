/*
 * gui_checkbox.c - 复选框控件
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"

#ifndef bool
#define bool int
#define true 1
#define false 0
#endif

/* 复选框结构 */
struct CheckBox {
    Rect bounds;
    bool checked;
    const char *label;
    Color bg_color;
    Color check_color;
    Color text_color;
};

typedef struct CheckBox CheckBox;

#define CHECKBOX_SIZE 20

/*
 * 创建复选框
 */
CheckBox *gui_checkbox_create(int x, int y, const char *label)
{
    CheckBox *cb = malloc(sizeof(CheckBox));
    if (!cb) return NULL;
    
    cb->bounds.x = x;
    cb->bounds.y = y;
    cb->bounds.width = CHECKBOX_SIZE;
    cb->bounds.height = CHECKBOX_SIZE;
    
    cb->checked = false;
    cb->label = label;
    cb->bg_color = 0xFFFFFFFF;
    cb->check_color = COLOR_ACCENT_BLUE;
    cb->text_color = COLOR_TEXT_PRIMARY;
    
    return cb;
}

/*
 * 销毁复选框
 */
void gui_checkbox_destroy(CheckBox *cb)
{
    if (cb) {
        free(cb);
    }
}

/*
 * 绘制复选框
 */
void gui_checkbox_draw(GuiContext *ctx, CheckBox *cb, Font *font)
{
    if (!ctx || !cb) return;
    
    /* 背景框 */
    gui_fill_rounded_rect(ctx, cb->bounds, 4, cb->bg_color);
    gui_draw_rounded_rect(ctx, cb->bounds, 4, 0xFFCCCCCC, 2);
    
    /* 勾选标记 */
    if (cb->checked) {
        Rect check = {
            cb->bounds.x + 4,
            cb->bounds.y + 4,
            cb->bounds.width - 8,
            cb->bounds.height - 8
        };
        gui_fill_rounded_rect(ctx, check, 2, cb->check_color);
    }
    
    /* 标签 */
    if (cb->label) {
        gui_draw_text(ctx, font, 
                     cb->bounds.x + cb->bounds.width + 8,
                     cb->bounds.y + 2,
                     cb->label,
                     cb->text_color);
    }
}

/*
 * 检查点是否在矩形内（临时函数）
 */
static bool point_in_rect(int x, int y, Rect r)
{
    return (x >= r.x && x < r.x + r.width &&
            y >= r.y && y < r.y + r.height);
}

/*
 * 处理点击
 */
bool gui_checkbox_click(CheckBox *cb, int x, int y)
{
    if (!cb) return false;
    
    if (point_in_rect(x, y, cb->bounds)) {
        cb->checked = !cb->checked;  /* 切换状态 */
        return true;
    }
    
    return false;
}

/*
 * 获取/设置状态
 */
bool gui_checkbox_is_checked(CheckBox *cb)
{
    return cb ? cb->checked : false;
}

void gui_checkbox_set_checked(CheckBox *cb, bool checked)
{
    if (cb) {
        cb->checked = checked;
    }
}

