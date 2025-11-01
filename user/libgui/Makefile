# ===========================================================================
# EduOS User Space - libgui Makefile
# ===========================================================================

CC := gcc
AR := ar
CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
          -fno-stack-protector -O2 -Wall -I../libc -I.

BUILD_DIR := ../build/libgui

SOURCES := gui_core.c gui_draw.c gui_font.c gui_widget.c \
           gui_button.c gui_textbox.c gui_checkbox.c \
           gui_slider.c gui_menu.c

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))

all: $(BUILD_DIR)/libgui.a

$(BUILD_DIR)/libgui.a: $(OBJECTS)
	@echo "Creating libgui.a..."
	$(AR) rcs $@ $^
	@echo "✓ libgui.a created"

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

