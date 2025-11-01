/*
 * gui_menu.c - 菜单控件
 */

#include "gui.h"
#include "../libc/stdlib/stdlib.h"

#define MAX_MENU_ITEMS 16

/* 菜单项 */
typedef struct MenuItem {
    const char *text;
    bool enabled;
    void (*callback)(void);
} MenuItem;

/* 菜单结构 */
typedef struct Menu {
    Rect bounds;
    MenuItem items[MAX_MENU_ITEMS];
    int item_count;
    int selected_index;  /* -1 表示无选中 */
    bool visible;
} Menu;

/*
 * 创建菜单
 */
Menu *gui_create_menu(int x, int y, int width)
{
    Menu *menu = (Menu*)malloc(sizeof(Menu));
    if (!menu) return NULL;
    
    menu->bounds.x = x;
    menu->bounds.y = y;
    menu->bounds.width = width;
    menu->bounds.height = 0;  /* 根据项目数动态计算 */
    menu->item_count = 0;
    menu->selected_index = -1;
    menu->visible = false;
    
    return menu;
}

/*
 * 添加菜单项
 */
bool gui_menu_add_item(Menu *menu, const char *text, void (*callback)(void))
{
    if (!menu || menu->item_count >= MAX_MENU_ITEMS) return false;
    
    menu->items[menu->item_count].text = text;
    menu->items[menu->item_count].enabled = true;
    menu->items[menu->item_count].callback = callback;
    menu->item_count++;
    
    /* 更新菜单高度 */
    menu->bounds.height = menu->item_count * 32;
    
    return true;
}

/*
 * 绘制菜单
 */
void gui_draw_menu(GuiContext *ctx, Menu *menu, Font *font)
{
    if (!ctx || !menu || !menu->visible) return;
    
    /* 绘制菜单背景 */
    gui_fill_rounded_rect(ctx, menu->bounds, 4, COLOR_BG_LAYER);
    
    /* 绘制边框 */
    Rect border = menu->bounds;
    gui_draw_rect(ctx, border, 0xFF666666, 2);
    
    /* 绘制菜单项 */
    int item_height = 32;
    for (int i = 0; i < menu->item_count; i++) {
        Rect item_rect = {
            menu->bounds.x,
            menu->bounds.y + i * item_height,
            menu->bounds.width,
            item_height
        };
        
        /* 高亮选中项 */
        if (i == menu->selected_index) {
            gui_fill_rect(ctx, item_rect, COLOR_ACCENT_BLUE);
        }
        
        /* 绘制文本 */
        Color text_color = menu->items[i].enabled ? 
                          (i == menu->selected_index ? COLOR_WHITE : COLOR_TEXT_PRIMARY) :
                          COLOR_TEXT_SECONDARY;
        
        gui_draw_text(ctx, font, 
                     item_rect.x + 8, 
                     item_rect.y + 8,
                     menu->items[i].text, 
                     text_color);
    }
}

/*
 * 处理菜单鼠标事件
 */
bool gui_menu_mouse_event(Menu *menu, int mouse_x, int mouse_y, bool clicked)
{
    if (!menu || !menu->visible) return false;
    
    /* 检查鼠标是否在菜单区域内 */
    bool in_bounds = (mouse_x >= menu->bounds.x && 
                     mouse_x < menu->bounds.x + menu->bounds.width &&
                     mouse_y >= menu->bounds.y && 
                     mouse_y < menu->bounds.y + menu->bounds.height);
    
    if (!in_bounds) {
        menu->selected_index = -1;
        if (clicked) {
            menu->visible = false;  /* 点击菜单外部关闭菜单 */
        }
        return false;
    }
    
    /* 计算选中的菜单项 */
    int item_height = 32;
    int index = (mouse_y - menu->bounds.y) / item_height;
    
    if (index >= 0 && index < menu->item_count) {
        menu->selected_index = index;
        
        /* 如果点击，执行回调 */
        if (clicked && menu->items[index].enabled && menu->items[index].callback) {
            menu->items[index].callback();
            menu->visible = false;  /* 执行后关闭菜单 */
            return true;
        }
    }
    
    return false;
}

/*
 * 显示/隐藏菜单
 */
void gui_menu_set_visible(Menu *menu, bool visible)
{
    if (menu) {
        menu->visible = visible;
        if (!visible) {
            menu->selected_index = -1;
        }
    }
}

/*
 * 切换菜单可见性
 */
void gui_menu_toggle(Menu *menu)
{
    if (menu) {
        menu->visible = !menu->visible;
        if (!menu->visible) {
            menu->selected_index = -1;
        }
    }
}

