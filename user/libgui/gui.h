/*
 * gui.h - EduOS GUI Library (Qt-like API with Fluent Design)
 * 
 * 用户态图形界面库
 */

#ifndef _GUI_H
#define _GUI_H

#include "../libc/syscall/types.h"

/* ========== 颜色定义 ========== */
typedef uint32_t Color;

/* Fluent Design 颜色主题 */
#define COLOR_TRANSPARENT       0x00000000

/* 主色调 - 蓝色系 */
#define COLOR_ACCENT_BLUE       0xFF0078D4
#define COLOR_ACCENT_BLUE_DARK  0xFF005A9E
#define COLOR_ACCENT_BLUE_LIGHT 0xFF429CE3

/* 背景色 - 亚克力效果 */
#define COLOR_BG_CHROME         0xFFF3F3F3
#define COLOR_BG_LAYER          0xFFFFFFFF
#define COLOR_BG_CARD           0xFFFAFAFA
#define COLOR_BG_OVERLAY        0xCC000000  /* 半透明 */

/* 文本颜色 */
#define COLOR_TEXT_PRIMARY      0xFF000000
#define COLOR_TEXT_SECONDARY    0xFF666666
#define COLOR_TEXT_DISABLED     0xFFCCCCCC
#define COLOR_TEXT_ON_ACCENT    0xFFFFFFFF

/* 边框颜色 */
#define COLOR_BORDER_DEFAULT    0xFFE1E1E1
#define COLOR_BORDER_FOCUS      0xFF0078D4

/* 状态颜色 */
#define COLOR_SUCCESS           0xFF107C10
#define COLOR_WARNING           0xFFFFB900
#define COLOR_ERROR             0xFFE81123

/* 基础颜色 */
#define COLOR_WHITE             0xFFFFFFFF
#define COLOR_BLACK             0xFF000000
#define COLOR_GRAY_LIGHT        0xFFF3F3F3
#define COLOR_GRAY              0xFF999999
#define COLOR_GRAY_DARK         0xFF333333

/* ========== 基础数据结构 ========== */

typedef struct {
    int x, y;
} Point;

typedef struct {
    int x, y, width, height;
} Rect;

typedef struct {
    int width, height;
} Size;

/* ========== GUI 系统 ========== */

typedef struct GuiContext GuiContext;

/* 初始化GUI */
GuiContext *gui_init(void);
void gui_shutdown(GuiContext *ctx);

/* 获取屏幕信息 */
int gui_get_width(GuiContext *ctx);
int gui_get_height(GuiContext *ctx);

/* 刷新屏幕 */
void gui_update(GuiContext *ctx);

/* ========== 基础绘图 API ========== */

/* 像素操作 */
void gui_set_pixel(GuiContext *ctx, int x, int y, Color color);
Color gui_get_pixel(GuiContext *ctx, int x, int y);

/* 像素操作 */
void gui_put_pixel(GuiContext *ctx, int x, int y, Color color);

/* 填充 */
void gui_fill_rect(GuiContext *ctx, Rect rect, Color color);
void gui_fill_screen(GuiContext *ctx, Color color);

/* 绘制矩形 */
void gui_draw_rect(GuiContext *ctx, Rect rect, Color color, int thickness);
void gui_draw_rounded_rect(GuiContext *ctx, Rect rect, int radius, Color color, int thickness);
void gui_fill_rounded_rect(GuiContext *ctx, Rect rect, int radius, Color color);

/* 绘制线条 */
void gui_draw_line(GuiContext *ctx, int x1, int y1, int x2, int y2, Color color);
void gui_draw_circle(GuiContext *ctx, int cx, int cy, int radius, Color color);

/* 渐变效果 */
void gui_fill_gradient_vertical(GuiContext *ctx, Rect rect, Color color1, Color color2);
void gui_fill_gradient_horizontal(GuiContext *ctx, Rect rect, Color color1, Color color2);

/* 阴影效果 */
void gui_draw_shadow(GuiContext *ctx, Rect rect, int radius, int blur, Color color);

/* ========== 文字渲染 ========== */

typedef struct Font Font;

/* 加载字体（8x16位图字体） */
Font *gui_load_font(GuiContext *ctx);

/* 绘制文字 */
void gui_draw_text(GuiContext *ctx, Font *font, int x, int y, const char *text, Color color);
void gui_draw_text_centered(GuiContext *ctx, Font *font, Rect rect, const char *text, Color color);

/* 测量文字尺寸 */
int gui_text_width(Font *font, const char *text);
int gui_text_height(Font *font);

/* ========== 控件基类 ========== */

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_KEY_UP,
    GUI_EVENT_PAINT
} EventType;

typedef struct {
    EventType type;
    int x, y;           /* 鼠标位置 */
    int button;         /* 鼠标按钮 */
    int key;            /* 键盘按键 */
} Event;

typedef struct Widget Widget;

typedef void (*WidgetPaintFunc)(Widget *widget, GuiContext *ctx);
typedef void (*WidgetEventFunc)(Widget *widget, Event *event);

struct Widget {
    Rect bounds;
    bool visible;
    bool enabled;
    
    Color bg_color;
    const char *text;
    
    WidgetPaintFunc paint;
    WidgetEventFunc on_event;
    
    void *user_data;
    Widget *next;  /* 链表 */
};

/* 控件基础操作 */
Widget *gui_create_widget(Rect bounds);
void gui_destroy_widget(Widget *widget);
void gui_show_widget(Widget *widget);
void gui_hide_widget(Widget *widget);
void gui_set_bounds(Widget *widget, Rect bounds);

/* ========== Fluent Design 控件 ========== */

/* 按钮 */
Widget *gui_create_button(Rect bounds, const char *text);
Widget *gui_create_accent_button(Rect bounds, const char *text);  /* 强调按钮 */

/* 文本框 */
Widget *gui_create_textbox(Rect bounds, const char *placeholder);

/* 标签 */
Widget *gui_create_label(Rect bounds, const char *text);

/* 卡片（Fluent卡片容器） */
Widget *gui_create_card(Rect bounds);

/* 窗口 */
typedef struct Window Window;

Window *gui_create_window(GuiContext *ctx, Rect bounds, const char *title);
void gui_window_add_widget(Window *window, Widget *widget);
void gui_window_show(Window *window);
void gui_window_close(Window *window);

/* ========== 事件循环 ========== */

void gui_event_loop(GuiContext *ctx);
bool gui_poll_event(GuiContext *ctx, Event *event);

/* ========== 高级控件 ========== */

/* 滑块 */
typedef struct Slider Slider;

Slider *gui_create_slider(int x, int y, int width, int height, 
                          int min_val, int max_val, int initial_val);
void gui_draw_slider(GuiContext *ctx, Slider *slider);
bool gui_slider_mouse_event(Slider *slider, int mouse_x, int mouse_y, bool pressed);
int gui_slider_get_value(Slider *slider);
void gui_slider_set_value(Slider *slider, int value);

/* 菜单 */
typedef struct Menu Menu;

Menu *gui_create_menu(int x, int y, int width);
bool gui_menu_add_item(Menu *menu, const char *text, void (*callback)(void));
void gui_draw_menu(GuiContext *ctx, Menu *menu, Font *font);
bool gui_menu_mouse_event(Menu *menu, int mouse_x, int mouse_y, bool clicked);
void gui_menu_set_visible(Menu *menu, bool visible);
void gui_menu_toggle(Menu *menu);

#endif /* _GUI_H */
