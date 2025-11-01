/*
 * desktop.c - EduOS桌面环境
 * 
 * 功能：
 * - 任务栏
 * - 应用程序启动器  
 * - 窗口管理
 * - 系统托盘
 */

#include "../../libgui/gui.h"
#include "../../libc/stdio/stdio.h"
#include "../../libc/stdlib/stdlib.h"
#include "../../libc/syscall/unistd.h"

/* 桌面配置 */
#define TASKBAR_HEIGHT  48
#define START_BUTTON_WIDTH  120

/* 应用程序定义 */
typedef struct {
    const char *name;
    const char *icon;  /* 简化：用首字母代替图标 */
    void (*launch)(void);
} Application;

/* 桌面状态 */
static struct {
    GuiContext *gui;
    int screen_width;
    int screen_height;
    bool running;
} desktop;

/*
 * 绘制任务栏
 */
static void draw_taskbar(void)
{
    Rect taskbar = {
        0,
        desktop.screen_height - TASKBAR_HEIGHT,
        desktop.screen_width,
        TASKBAR_HEIGHT
    };
    
    /* 任务栏背景 - 深色半透明 */
    gui_fill_rect(desktop.gui, taskbar, 0xE0202020);
    
    /* 开始按钮 */
    Rect start_btn = {
        8,
        desktop.screen_height - TASKBAR_HEIGHT + 8,
        START_BUTTON_WIDTH,
        TASKBAR_HEIGHT - 16
    };
    
    gui_fill_rounded_rect(desktop.gui, start_btn, 4, COLOR_ACCENT_BLUE);
    
    Font *font = gui_load_font(desktop.gui);
    gui_draw_text_centered(desktop.gui, font, start_btn, 
                          "EduOS", COLOR_WHITE);
    
    /* 系统时钟（右侧）*/
    Rect clock_area = {
        desktop.screen_width - 100,
        desktop.screen_height - TASKBAR_HEIGHT + 12,
        90,
        24
    };
    
    gui_draw_text_centered(desktop.gui, font, clock_area,
                          "12:00", COLOR_WHITE);
}

/*
 * 绘制桌面壁纸
 */
static void draw_wallpaper(void)
{
    /* Fluent Design风格渐变背景 */
    Rect screen = {0, 0, desktop.screen_width, desktop.screen_height};
    
    gui_fill_gradient_vertical(desktop.gui, screen,
        0xFF0078D4,  /* Windows蓝 */
        0xFF005A9E); /* 深蓝 */
}

/*
 * 绘制欢迎窗口
 */
static void draw_welcome_window(void)
{
    int win_width = 500;
    int win_height = 350;
    
    Rect window = {
        (desktop.screen_width - win_width) / 2,
        (desktop.screen_height - win_height) / 2,
        win_width,
        win_height
    };
    
    /* 窗口阴影 */
    gui_draw_shadow(desktop.gui, window, 12, 24, 0x50000000);
    
    /* 窗口背景 */
    gui_fill_rounded_rect(desktop.gui, window, 12, COLOR_BG_LAYER);
    
    /* 标题栏 */
    Rect titlebar = {window.x, window.y, win_width, 40};
    gui_fill_rect(desktop.gui, titlebar, 0xFFE0E0E0);
    
    Font *font = gui_load_font(desktop.gui);
    
    /* 窗口标题 */
    Rect title_rect = {window.x + 16, window.y + 12, win_width - 32, 20};
    gui_draw_text(desktop.gui, font, title_rect.x, title_rect.y,
                 "Welcome to EduOS", COLOR_TEXT_PRIMARY);
    
    /* 欢迎文本 */
    int text_y = window.y + 80;
    gui_draw_text_centered(desktop.gui, font,
        (Rect){window.x, text_y, win_width, 24},
        "Educational Operating System", COLOR_TEXT_PRIMARY);
    
    text_y += 40;
    gui_draw_text_centered(desktop.gui, font,
        (Rect){window.x, text_y, win_width, 20},
        "Version 1.0", COLOR_TEXT_SECONDARY);
    
    text_y += 60;
    gui_draw_text_centered(desktop.gui, font,
        (Rect){window.x, text_y, win_width, 20},
        "Features:", COLOR_TEXT_PRIMARY);
    
    text_y += 30;
    gui_draw_text(desktop.gui, font, window.x + 80, text_y,
                 "✓ Graphical User Interface", COLOR_SUCCESS);
    
    text_y += 25;
    gui_draw_text(desktop.gui, font, window.x + 80, text_y,
                 "✓ Multi-tasking", COLOR_SUCCESS);
    
    text_y += 25;
    gui_draw_text(desktop.gui, font, window.x + 80, text_y,
                 "✓ File System (FAT32)", COLOR_SUCCESS);
    
    text_y += 25;
    gui_draw_text(desktop.gui, font, window.x + 80, text_y,
                 "✓ System Calls", COLOR_SUCCESS);
    
    /* 底部按钮 */
    Rect ok_button = {
        window.x + win_width - 120,
        window.y + win_height - 60,
        100,
        40
    };
    
    gui_fill_rounded_rect(desktop.gui, ok_button, 6, COLOR_ACCENT_BLUE);
    gui_draw_text_centered(desktop.gui, font, ok_button,
                          "OK", COLOR_WHITE);
}

/*
 * 主循环
 */
static void desktop_main_loop(void)
{
    printf("[Desktop] Entering main loop...\n");
    
    desktop.running = true;
    
    while (desktop.running) {
        /* 简化版：静态渲染，无事件循环 */
        /* TODO: 实现事件处理（鼠标、键盘）*/
        
        /* 休眠一段时间 */
        for (volatile int i = 0; i < 10000000; i++);
    }
}

/*
 * 桌面程序入口
 */
void _start(void)
{
    printf("=== EduOS Desktop Starting ===\n");
    
    /* 初始化GUI */
    printf("[Desktop] Initializing GUI system...\n");
    desktop.gui = gui_init();
    if (!desktop.gui) {
        printf("[Desktop] ERROR: Failed to initialize GUI\n");
        exit(1);
    }
    
    desktop.screen_width = gui_get_width(desktop.gui);
    desktop.screen_height = gui_get_height(desktop.gui);
    
    printf("[Desktop] Screen: %dx%d\n", desktop.screen_width, desktop.screen_height);
    
    /* 渲染桌面 */
    printf("[Desktop] Rendering desktop environment...\n");
    
    draw_wallpaper();
    printf("[Desktop] Wallpaper rendered\n");
    
    draw_taskbar();
    printf("[Desktop] Taskbar rendered\n");
    
    draw_welcome_window();
    printf("[Desktop] Welcome window rendered\n");
    
    printf("[Desktop] Desktop environment ready!\n");
    printf("[Desktop] GUI rendering complete.\n");
    
    /* 进入主循环 */
    desktop_main_loop();
    
    /* 清理 */
    gui_shutdown(desktop.gui);
    
    printf("[Desktop] Exiting...\n");
    exit(0);
}

