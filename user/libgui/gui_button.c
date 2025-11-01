/*
 * gui_button.c - Fluent Design 按钮控件
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"
#include "../libc/string/string.h"

/* 按钮状态 */
typedef enum {
    BTN_STATE_NORMAL,
    BTN_STATE_HOVER,
    BTN_STATE_PRESSED,
    BTN_STATE_DISABLED
} ButtonState;

/* 按钮数据 */
typedef struct {
    ButtonState state;
    bool is_accent;  /* 是否为强调按钮 */
} ButtonData;

/*
 * 绘制普通按钮
 */
static void paint_button(Widget *widget, GuiContext *ctx)
{
    if (!widget || !ctx) return;
    
    ButtonData *data = (ButtonData*)widget->user_data;
    if (!data) return;
    
    Color bg_color, text_color, border_color;
    
    /* 根据状态选择颜色 */
    if (!widget->enabled) {
        bg_color = COLOR_BG_CHROME;
        text_color = COLOR_TEXT_DISABLED;
        border_color = COLOR_BORDER_DEFAULT;
    } else if (data->state == BTN_STATE_PRESSED) {
        bg_color = data->is_accent ? COLOR_ACCENT_BLUE_DARK : COLOR_GRAY_LIGHT;
        text_color = data->is_accent ? COLOR_TEXT_ON_ACCENT : COLOR_TEXT_PRIMARY;
        border_color = data->is_accent ? COLOR_ACCENT_BLUE_DARK : COLOR_BORDER_FOCUS;
    } else if (data->state == BTN_STATE_HOVER) {
        bg_color = data->is_accent ? COLOR_ACCENT_BLUE_LIGHT : COLOR_BG_CARD;
        text_color = data->is_accent ? COLOR_TEXT_ON_ACCENT : COLOR_TEXT_PRIMARY;
        border_color = data->is_accent ? COLOR_ACCENT_BLUE_LIGHT : COLOR_BORDER_FOCUS;
    } else {
        bg_color = data->is_accent ? COLOR_ACCENT_BLUE : COLOR_BG_LAYER;
        text_color = data->is_accent ? COLOR_TEXT_ON_ACCENT : COLOR_TEXT_PRIMARY;
        border_color = data->is_accent ? COLOR_ACCENT_BLUE : COLOR_BORDER_DEFAULT;
    }
    
    /* 绘制阴影（非强调按钮） */
    if (!data->is_accent && widget->enabled) {
        gui_draw_shadow(ctx, widget->bounds, 4, 8, 0x20000000);
    }
    
    /* 绘制圆角背景 */
    gui_fill_rounded_rect(ctx, widget->bounds, 4, bg_color);
    
    /* 绘制边框 */
    gui_draw_rounded_rect(ctx, widget->bounds, 4, border_color, 1);
    
    /* 绘制文本 */
    if (widget->text) {
        extern Font *gui_load_font(GuiContext *ctx);
        Font *font = gui_load_font(ctx);
        gui_draw_text_centered(ctx, font, widget->bounds, widget->text, text_color);
    }
}

/*
 * 按钮事件处理
 */
static void button_event_handler(Widget *widget, Event *event)
{
    if (!widget || !event) return;
    
    ButtonData *data = (ButtonData*)widget->user_data;
    if (!data || !widget->enabled) return;
    
    /* 检查鼠标是否在按钮内 */
    bool inside = (event->x >= widget->bounds.x && 
                   event->x < widget->bounds.x + widget->bounds.width &&
                   event->y >= widget->bounds.y && 
                   event->y < widget->bounds.y + widget->bounds.height);
    
    switch (event->type) {
        case GUI_EVENT_MOUSE_MOVE:
            if (inside && data->state == BTN_STATE_NORMAL) {
                data->state = BTN_STATE_HOVER;
            } else if (!inside && data->state == BTN_STATE_HOVER) {
                data->state = BTN_STATE_NORMAL;
            }
            break;
            
        case GUI_EVENT_MOUSE_DOWN:
            if (inside) {
                data->state = BTN_STATE_PRESSED;
            }
            break;
            
        case GUI_EVENT_MOUSE_UP:
            if (inside && data->state == BTN_STATE_PRESSED) {
                /* 触发点击事件 */
                data->state = BTN_STATE_HOVER;
            } else {
                data->state = BTN_STATE_NORMAL;
            }
            break;
            
        default:
            break;
    }
}

/*
 * 创建普通按钮
 */
Widget *gui_create_button(Rect bounds, const char *text)
{
    Widget *widget = gui_create_widget(bounds);
    if (!widget) return NULL;
    
    ButtonData *data = malloc(sizeof(ButtonData));
    if (!data) {
        gui_destroy_widget(widget);
        return NULL;
    }
    
    data->state = BTN_STATE_NORMAL;
    data->is_accent = false;
    
    widget->text = text;
    widget->user_data = data;
    widget->paint = paint_button;
    widget->on_event = button_event_handler;
    
    return widget;
}

/*
 * 创建强调按钮（Accent Button）
 */
Widget *gui_create_accent_button(Rect bounds, const char *text)
{
    Widget *widget = gui_create_button(bounds, text);
    if (!widget) return NULL;
    
    ButtonData *data = (ButtonData*)widget->user_data;
    data->is_accent = true;
    
    return widget;
}
