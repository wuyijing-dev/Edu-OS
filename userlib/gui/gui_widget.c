/*
 * gui_widget.c - 控件基类实现
 */

#include "gui.h"
#include "../stdlib.h"

/*
 * 创建控件
 */
Widget *gui_create_widget(Rect bounds)
{
    Widget *widget = malloc(sizeof(Widget));
    if (!widget) return NULL;
    
    widget->bounds = bounds;
    widget->visible = true;
    widget->enabled = true;
    widget->bg_color = COLOR_TRANSPARENT;
    widget->text = NULL;
    widget->paint = NULL;
    widget->on_event = NULL;
    widget->user_data = NULL;
    widget->next = NULL;
    
    return widget;
}

/*
 * 销毁控件
 */
void gui_destroy_widget(Widget *widget)
{
    if (!widget) return;
    
    if (widget->user_data) {
        free(widget->user_data);
    }
    
    free(widget);
}

/*
 * 显示控件
 */
void gui_show_widget(Widget *widget)
{
    if (widget) {
        widget->visible = true;
    }
}

/*
 * 隐藏控件
 */
void gui_hide_widget(Widget *widget)
{
    if (widget) {
        widget->visible = false;
    }
}

/*
 * 设置控件位置和大小
 */
void gui_set_bounds(Widget *widget, Rect bounds)
{
    if (widget) {
        widget->bounds = bounds;
    }
}

/*
 * 创建标签
 */
Widget *gui_create_label(Rect bounds, const char *text)
{
    Widget *widget = gui_create_widget(bounds);
    if (!widget) return NULL;
    
    widget->text = text;
    widget->bg_color = COLOR_TRANSPARENT;
    
    /* 标签的绘制函数 */
    widget->paint = NULL;  /* 将在gui_label.c中实现 */
    
    return widget;
}

/*
 * 创建卡片容器
 */
Widget *gui_create_card(Rect bounds)
{
    Widget *widget = gui_create_widget(bounds);
    if (!widget) return NULL;
    
    widget->bg_color = COLOR_BG_CARD;
    
    /* 卡片的绘制：圆角矩形 + 阴影 */
    /* 将在使用时通过paint函数绘制 */
    
    return widget;
}
