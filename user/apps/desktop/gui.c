/*
 * gui_demo.c - EduOS GUI库演示程序
 * 
 * 展示Fluent Design风格的现代化GUI
 */

#include "../userlib/gui/gui.h"
#include "../userlib/stdio.h"
#include "../userlib/unistd.h"

void _start(void)
{
    /* 添加调试输出 */
    printf("=== GUI Demo Started ===\n");
    printf("[GUI Demo] _start() called\n");
    
    /* 初始化GUI系统 */
    printf("[GUI Demo] Calling gui_init()...\n");
    GuiContext *gui = gui_init();
    if (!gui) {
        printf("[GUI Demo] ERROR: Failed to initialize GUI\n");
        exit(1);
    }
    
    printf("[GUI Demo] GUI initialized successfully!\n");
    
    /* 获取屏幕尺寸 */
    int screen_w = gui_get_width(gui);
    int screen_h = gui_get_height(gui);
    
    printf("[GUI Demo] Screen: %dx%d\n", screen_w, screen_h);
    
    /* ========== 背景 ========== */
    
    /* Fluent Design 渐变背景 */
    gui_fill_gradient_vertical(gui, 
        (Rect){0, 0, screen_w, screen_h},
        0xFF0078D4,  /* 蓝色 */
        0xFF005A9E); /* 深蓝 */
    
    printf("[GUI Demo] Background rendered\n");
    
    /* ========== 标题卡片 ========== */
    
    Rect title_card = {
        (screen_w - 600) / 2,  /* 居中 */
        80,
        600,
        120
    };
    
    /* 绘制阴影 */
    gui_draw_shadow(gui, title_card, 8, 16, 0x40000000);
    
    /* 绘制卡片 */
    gui_fill_rounded_rect(gui, title_card, 8, COLOR_BG_CARD);
    
    /* 标题文本 */
    Font *font = gui_load_font(gui);
    Rect title_text_rect = {title_card.x, title_card.y + 30, title_card.width, 30};
    gui_draw_text_centered(gui, font, title_text_rect, 
                          "EduOS Desktop", COLOR_TEXT_PRIMARY);
    
    Rect subtitle_rect = {title_card.x, title_card.y + 70, title_card.width, 20};
    gui_draw_text_centered(gui, font, subtitle_rect,
                          "Fluent Design System", COLOR_TEXT_SECONDARY);
    
    // printf("[GUI Demo] Title card rendered\n");
    
    /* ========== 功能卡片1 - 系统信息 ========== */
    
    Rect info_card = {
        100,
        250,
        380,
        300
    };
    
    gui_draw_shadow(gui, info_card, 8, 12, 0x30000000);
    gui_fill_rounded_rect(gui, info_card, 8, COLOR_BG_LAYER);
    
    /* 卡片标题 */
    Rect card_title = {info_card.x + 20, info_card.y + 20, 340, 24};
    gui_draw_text(gui, font, card_title.x, card_title.y, 
                 "System Information", COLOR_ACCENT_BLUE);
    
    /* 分隔线 */
    gui_fill_rect(gui, (Rect){info_card.x + 20, info_card.y + 50, 340, 2}, 
                 COLOR_BORDER_DEFAULT);
    
    /* 信息文本 */
    gui_draw_text(gui, font, info_card.x + 20, info_card.y + 70,
                 "OS: EduOS v1.0", COLOR_TEXT_PRIMARY);
    gui_draw_text(gui, font, info_card.x + 20, info_card.y + 100,
                 "Memory: 128 MB", COLOR_TEXT_PRIMARY);
    gui_draw_text(gui, font, info_card.x + 20, info_card.y + 130,
                 "Resolution: 1024x768", COLOR_TEXT_PRIMARY);
    
    /* 强调按钮 */
    Rect info_btn = {info_card.x + 20, info_card.y + 240, 340, 40};
    gui_fill_rounded_rect(gui, info_btn, 4, COLOR_ACCENT_BLUE);
    gui_draw_text_centered(gui, font, info_btn, "Details", COLOR_TEXT_ON_ACCENT);
    
    // printf("[GUI Demo] Info card rendered\n");
    
    /* ========== 功能卡片2 - 快捷操作 ========== */
    
    Rect action_card = {
        540,
        250,
        380,
        300
    };
    
    gui_draw_shadow(gui, action_card, 8, 12, 0x30000000);
    gui_fill_rounded_rect(gui, action_card, 8, COLOR_BG_LAYER);
    
    /* 卡片标题 */
    gui_draw_text(gui, font, action_card.x + 20, action_card.y + 20,
                 "Quick Actions", COLOR_ACCENT_BLUE);
    
    /* 分隔线 */
    gui_fill_rect(gui, (Rect){action_card.x + 20, action_card.y + 50, 340, 2},
                 COLOR_BORDER_DEFAULT);
    
    /* 三个普通按钮 */
    Rect btn1 = {action_card.x + 20, action_card.y + 80, 340, 40};
    gui_fill_rounded_rect(gui, btn1, 4, COLOR_BG_CARD);
    gui_draw_rounded_rect(gui, btn1, 4, COLOR_BORDER_DEFAULT, 1);
    gui_draw_text_centered(gui, font, btn1, "Files", COLOR_TEXT_PRIMARY);
    
    Rect btn2 = {action_card.x + 20, action_card.y + 140, 340, 40};
    gui_fill_rounded_rect(gui, btn2, 4, COLOR_BG_CARD);
    gui_draw_rounded_rect(gui, btn2, 4, COLOR_BORDER_DEFAULT, 1);
    gui_draw_text_centered(gui, font, btn2, "Settings", COLOR_TEXT_PRIMARY);
    
    Rect btn3 = {action_card.x + 20, action_card.y + 200, 340, 40};
    gui_fill_rounded_rect(gui, btn3, 4, COLOR_BG_CARD);
    gui_draw_rounded_rect(gui, btn3, 4, COLOR_BORDER_DEFAULT, 1);
    gui_draw_text_centered(gui, font, btn3, "Terminal", COLOR_TEXT_PRIMARY);
    
    // printf("[GUI Demo] Action card rendered\n");
    
    /* ========== 底部状态栏 ========== */
    
    Rect status_bar = {0, screen_h - 40, screen_w, 40};
    
    /* 半透明背景 */
    gui_fill_rect(gui, status_bar, 0xE0FFFFFF);
    
    /* 状态文本 */
    gui_draw_text(gui, font, 20, screen_h - 28, 
                 "Ready | Fluent Design Demo", COLOR_TEXT_SECONDARY);
    
    // printf("[GUI Demo] Status bar rendered\n");
    
    /* ========== 装饰性元素 ========== */
    
    /* 右上角的圆形装饰 */
    gui_draw_circle(gui, screen_w - 60, 60, 30, 0x40FFFFFF);
    gui_draw_circle(gui, screen_w - 60, 60, 20, 0x80FFFFFF);
    
    // printf("[GUI Demo] All elements rendered successfully!\n");
    // printf("[GUI Demo] Press any key to exit...\n");
    
    /* 更新屏幕 */
    gui_update(gui);
    
    /* 简单的等待循环 */
    for (volatile int i = 0; i < 100000000; i++);
    
    /* 关闭GUI */
    gui_shutdown(gui);
    
    // printf("[GUI Demo] Exiting...\n");
    exit(0);
}
