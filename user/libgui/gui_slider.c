/*
 * gui_slider.c - 滑块控件
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"

/* 滑块结构 */
typedef struct Slider {
    Rect bounds;        /* 滑块区域 */
    int min_value;      /* 最小值 */
    int max_value;      /* 最大值 */
    int current_value;  /* 当前值 */
    bool dragging;      /* 是否正在拖动 */
} Slider;

/*
 * 创建滑块
 */
Slider *gui_create_slider(int x, int y, int width, int height, 
                          int min_val, int max_val, int initial_val)
{
    Slider *slider = (Slider*)malloc(sizeof(Slider));
    if (!slider) return NULL;
    
    slider->bounds.x = x;
    slider->bounds.y = y;
    slider->bounds.width = width;
    slider->bounds.height = height;
    slider->min_value = min_val;
    slider->max_value = max_val;
    slider->current_value = initial_val;
    slider->dragging = false;
    
    return slider;
}

/*
 * 绘制滑块
 */
void gui_draw_slider(GuiContext *ctx, Slider *slider)
{
    if (!ctx || !slider) return;
    
    /* 绘制滑轨（背景）*/
    Rect track = {
        slider->bounds.x,
        slider->bounds.y + slider->bounds.height / 2 - 2,
        slider->bounds.width,
        4
    };
    gui_fill_rect(ctx, track, 0xFFCCCCCC);
    
    /* 计算滑块位置 */
    int range = slider->max_value - slider->min_value;
    int value_offset = slider->current_value - slider->min_value;
    int thumb_x = slider->bounds.x + 
                  (value_offset * slider->bounds.width) / range;
    
    /* 绘制滑块手柄 */
    int thumb_size = 16;
    Rect thumb = {
        thumb_x - thumb_size / 2,
        slider->bounds.y,
        thumb_size,
        slider->bounds.height
    };
    
    Color thumb_color = slider->dragging ? COLOR_ACCENT_BLUE : 0xFF888888;
    gui_fill_rounded_rect(ctx, thumb, 4, thumb_color);
}

/*
 * 处理滑块鼠标事件
 */
bool gui_slider_mouse_event(Slider *slider, int mouse_x, int mouse_y, bool pressed)
{
    if (!slider) return false;
    
    /* 检查鼠标是否在滑块区域内 */
    bool in_bounds = (mouse_x >= slider->bounds.x && 
                     mouse_x < slider->bounds.x + slider->bounds.width &&
                     mouse_y >= slider->bounds.y && 
                     mouse_y < slider->bounds.y + slider->bounds.height);
    
    if (pressed && in_bounds) {
        slider->dragging = true;
    }
    
    if (!pressed) {
        slider->dragging = false;
    }
    
    /* 如果正在拖动，更新值 */
    if (slider->dragging) {
        int relative_x = mouse_x - slider->bounds.x;
        if (relative_x < 0) relative_x = 0;
        if (relative_x > slider->bounds.width) relative_x = slider->bounds.width;
        
        int range = slider->max_value - slider->min_value;
        slider->current_value = slider->min_value + 
                               (relative_x * range) / slider->bounds.width;
        
        return true;  /* 值已改变 */
    }
    
    return false;
}

/*
 * 获取滑块当前值
 */
int gui_slider_get_value(Slider *slider)
{
    return slider ? slider->current_value : 0;
}

/*
 * 设置滑块值
 */
void gui_slider_set_value(Slider *slider, int value)
{
    if (!slider) return;
    
    if (value < slider->min_value) value = slider->min_value;
    if (value > slider->max_value) value = slider->max_value;
    
    slider->current_value = value;
}

