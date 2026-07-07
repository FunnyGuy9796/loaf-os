#ifndef VGA_H
#define VGA_H

#include <stdint.h>
#include <stddef.h>
#include "arch.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDR (KERNEL_VIRT_BASE + 0xb8000)

enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

extern size_t term_row;
extern size_t term_column;

void term_setcolor(enum vga_color fg, enum vga_color bg);
void term_putchar(char c);
void term_write(const char *data);
void term_clear();
void term_init();

#endif