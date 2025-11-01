/*
 * gui_textbox.c - 文本输入框控件
 */

#include "gui.h"
#include "../libc/string/string.h"
#include "../libc/stdlib/stdlib.h"

/* 文本框结构 */
struct TextBox {
    Rect bounds;
    char *text;
    size_t capacity;
    size_t cursor_pos;
    bool focused;
    Color bg_color;
    Color text_color;
    Color border_color;
};

typedef struct TextBox TextBox;

/*
 * 创建文本框
 */
TextBox *gui_textbox_create(Rect bounds, size_t capacity)
{
    TextBox *tb = malloc(sizeof(TextBox));
    if (!tb) return NULL;
    
    tb->text = malloc(capacity);
    if (!tb->text) {
        free(tb);
        return NULL;
    }
    
    tb->bounds = bounds;
    tb->capacity = capacity;
    tb->cursor_pos = 0;
    tb->focused = false;
    tb->text[0] = '\0';
    
    tb->bg_color = 0xFFFFFFFF;
    tb->text_color = COLOR_TEXT_PRIMARY;
    tb->border_color = 0xFFCCCCCC;
    
    return tb;
}

/*
 * 销毁文本框
 */
void gui_textbox_destroy(TextBox *tb)
{
    if (tb) {
        if (tb->text) free(tb->text);
        free(tb);
    }
}

/*
 * 绘制文本框
 */
void gui_textbox_draw(GuiContext *ctx, TextBox *tb, Font *font)
{
    if (!ctx || !tb) return;
    
    /* 背景 */
    gui_fill_rounded_rect(ctx, tb->bounds, 4, tb->bg_color);
    
    /* 边框 */
    Color border = tb->focused ? COLOR_ACCENT_BLUE : tb->border_color;
    gui_draw_rounded_rect(ctx, tb->bounds, 4, border, 2);
    
    /* 文本 */
    gui_draw_text(ctx, font, tb->bounds.x + 8, tb->bounds.y + 8, 
                 tb->text, tb->text_color);
    
    /* 光标（如果聚焦）*/
    if (tb->focused) {
        int cursor_x = tb->bounds.x + 8 + tb->cursor_pos * 8;
        gui_fill_rect(ctx, (Rect){cursor_x, tb->bounds.y + 8, 2, 16}, 
                     COLOR_TEXT_PRIMARY);
    }
}

/*
 * 处理键盘输入
 */
void gui_textbox_input(TextBox *tb, char ch)
{
    if (!tb || !tb->focused) return;
    
    size_t len = strlen(tb->text);
    
    if (ch == '\b') {
        /* 退格 */
        if (tb->cursor_pos > 0) {
            tb->cursor_pos--;
            tb->text[tb->cursor_pos] = '\0';
        }
    } else if (ch >= 32 && ch <= 126) {
        /* 可打印字符 */
        if (len < tb->capacity - 1) {
            tb->text[tb->cursor_pos] = ch;
            tb->cursor_pos++;
            tb->text[tb->cursor_pos] = '\0';
        }
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
 * 处理鼠标点击
 */
bool gui_textbox_click(TextBox *tb, int x, int y)
{
    if (!tb) return false;
    
    if (point_in_rect(x, y, tb->bounds)) {
        tb->focused = true;
        return true;
    } else {
        tb->focused = false;
        return false;
    }
}

/*
 * 获取文本框内容
 */
const char *gui_textbox_get_text(TextBox *tb)
{
    return tb ? tb->text : NULL;
}

/*
 * 设置文本框内容
 */
void gui_textbox_set_text(TextBox *tb, const char *text)
{
    if (!tb || !text) return;
    
    strncpy(tb->text, text, tb->capacity - 1);
    tb->text[tb->capacity - 1] = '\0';
    tb->cursor_pos = strlen(tb->text);
}

