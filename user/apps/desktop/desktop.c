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
#include "../../libc/syscall/fcntl.h"

/* 鼠标数据包结构（与内核dev_mouse.c保持一致）*/
typedef struct {
    int16_t x, y;       /* 位置 */
    int16_t dx, dy;     /* 增量 */
    uint8_t buttons;    /* 按键状态 */
    uint8_t reserved;
} __attribute__((packed)) mouse_packet_t;

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
    int mouse_fd;  /* /dev/mouse 文件描述符 */
} desktop;

/* 鼠标状态 */
static struct {
    int x, y;
    int last_x, last_y;
    uint8_t buttons;
    uint8_t last_buttons;
} mouse_state;

/* OK按钮区域（全局，用于点击检测）*/
static Rect ok_button_rect;

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
    
    /* 底部按钮（保存到全局变量以便点击检测）*/
    ok_button_rect.x = window.x + win_width - 120;
    ok_button_rect.y = window.y + win_height - 60;
    ok_button_rect.width = 100;
    ok_button_rect.height = 40;
    
    gui_fill_rounded_rect(desktop.gui, ok_button_rect, 6, COLOR_ACCENT_BLUE);
    gui_draw_text_centered(desktop.gui, font, ok_button_rect,
                          "OK", COLOR_WHITE);
}

/*
 * 读取鼠标状态（从/dev/mouse）
 */
static void update_mouse(void)
{
    mouse_state.last_x = mouse_state.x;
    mouse_state.last_y = mouse_state.y;
    mouse_state.last_buttons = mouse_state.buttons;
    
    /* 从/dev/mouse读取鼠标状态 */
    if (desktop.mouse_fd >= 0) {
        mouse_packet_t packet;
        ssize_t ret = read(desktop.mouse_fd, &packet, sizeof(packet));
        
        if (ret == sizeof(packet)) {
            /* 更新鼠标位置和按键 */
            mouse_state.x = packet.x;
            mouse_state.y = packet.y;
            mouse_state.buttons = packet.buttons;
            
            /* 限制在屏幕范围内 */
            if (mouse_state.x < 0) mouse_state.x = 0;
            if (mouse_state.y < 0) mouse_state.y = 0;
            if (mouse_state.x >= desktop.screen_width) 
                mouse_state.x = desktop.screen_width - 1;
            if (mouse_state.y >= desktop.screen_height) 
                mouse_state.y = desktop.screen_height - 1;
        }
    }
}

/*
 * 检查按钮点击
 */
static bool check_button_click(Rect button, int x, int y)
{
    return (x >= button.x && x < button.x + button.width &&
            y >= button.y && y < button.y + button.height);
}

/*
 * 处理鼠标事件
 */
static void handle_mouse_events(void)
{
    /* 检测点击（按下→松开）*/
    bool was_pressed = (mouse_state.last_buttons & 0x01) != 0;
    bool is_pressed = (mouse_state.buttons & 0x01) != 0;
    
    if (was_pressed && !is_pressed) {
        /* 鼠标左键松开 = 点击 */
        if (check_button_click(ok_button_rect, mouse_state.x, mouse_state.y)) {
            printf("[Desktop] OK button clicked!\n");
            desktop.running = false;  /* 退出 */
        }
    }
}

/*
 * 绘制鼠标光标
 */
static void draw_cursor(int x, int y)
{
    /* 简单的箭头光标（8x12像素）*/
    for (int dy = 0; dy < 12; dy++) {
        for (int dx = 0; dx < 8 - dy/2; dx++) {
            gui_put_pixel(desktop.gui, x + dx, y + dy, COLOR_WHITE);
        }
    }
    /* 黑色轮廓 */
    for (int dy = 0; dy < 12; dy++) {
        gui_put_pixel(desktop.gui, x, y + dy, 0xFF000000);
        gui_put_pixel(desktop.gui, x + 8 - dy/2, y + dy, 0xFF000000);
    }
}

/*
 * 主循环（带事件处理）
 */
static void desktop_main_loop(void)
{
    printf("[Desktop] Entering main loop...\n");
    printf("[Desktop] Click OK button to exit\n");
    
    desktop.running = true;
    int frame = 0;
    
    while (desktop.running) {
        /* 更新鼠标状态 */
        update_mouse();
        
        /* 处理事件 */
        handle_mouse_events();
        
        /* 每30帧重绘一次光标 */
        if (frame % 30 == 0) {
            draw_cursor(mouse_state.x, mouse_state.y);
        }
        
        frame++;
        
        /* 简单延时 */
        for (volatile int i = 0; i < 100000; i++);
    }
    
    printf("[Desktop] Exiting main loop\n");
}

/*
 * 桌面程序入口
 */
void _start(void)
{
    /* 使用最基本的write测试，确保程序启动 */
    const char *msg = "=== EduOS Desktop Starting ===\n";
    write(1, msg, 32);
    
    printf("[Desktop] Initializing GUI system...\n");
    desktop.gui = gui_init();
    if (!desktop.gui) {
        printf("[Desktop] ERROR: Failed to initialize GUI\n");
        exit(1);
    }
    
    desktop.screen_width = gui_get_width(desktop.gui);
    desktop.screen_height = gui_get_height(desktop.gui);
    
    printf("[Desktop] Screen: %dx%d\n", desktop.screen_width, desktop.screen_height);
    
    /* 打开鼠标设备 */
    printf("[Desktop] Opening /dev/mouse...\n");
    desktop.mouse_fd = open("/mouse", O_RDONLY);
    if (desktop.mouse_fd < 0) {
        printf("[Desktop] WARNING: Failed to open /mouse (fd=%d)\n", desktop.mouse_fd);
        printf("[Desktop] Mouse input will not be available\n");
    } else {
        printf("[Desktop] Mouse device opened (fd=%d)\n", desktop.mouse_fd);
    }
    
    /* 初始化鼠标状态 */
    mouse_state.x = desktop.screen_width / 2;
    mouse_state.y = desktop.screen_height / 2;
    mouse_state.last_x = mouse_state.x;
    mouse_state.last_y = mouse_state.y;
    mouse_state.buttons = 0;
    mouse_state.last_buttons = 0;
    
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
    if (desktop.mouse_fd >= 0) {
        close(desktop.mouse_fd);
        printf("[Desktop] Mouse device closed\n");
    }
    
    gui_shutdown(desktop.gui);
    
    printf("[Desktop] Exiting...\n");
    exit(0);
}

