#include "vga.h"
#include "misc/util.h"

size_t term_row;
size_t term_column;
uint8_t term_color;
uint16_t* term_buffer = (uint16_t*)VGA_ADDR;

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

void term_putentryat(char c, uint8_t color, size_t x, size_t y)  {
	const size_t index = y * VGA_WIDTH + x;

	term_buffer[index] = vga_entry(c, color);
}

void term_update_cursor() {
    const size_t index = term_row * VGA_WIDTH + term_column;

    outb(0x3d4, 0x0f);
    outb(0x3d5, (uint8_t)(index & 0xff));
    outb(0x3d4, 0x0e);
    outb(0x3d5, (uint8_t)((index >> 8) & 0xff));
}

void term_setcolor(enum vga_color fg, enum vga_color bg) {
	term_color = fg | bg << 4;
}

void term_putchar(char c) {
    if (c == '\n') {
        term_column = 0;
        
        if (++term_row >= VGA_HEIGHT)
            term_row = 0;
    } else if (c == '\t') {
        if (++term_column >= VGA_WIDTH) {
            term_column = 0;

            if (++term_row >= VGA_HEIGHT)
                term_row = 0;
        }

        term_column += 3;
    } else if (c == '\b') {
        if (term_column <= 0) {
            if (term_row <= 0)
                term_row = VGA_HEIGHT - 1;
            else
                term_row--;

            term_column = VGA_WIDTH - 1;
        } else
            term_column--;

        term_putentryat(' ', term_color, term_column, term_row);
    } else {
        term_putentryat(c, term_color, term_column, term_row);

        if (++term_column >= VGA_WIDTH) {
            term_column = 0;

            if (++term_row >= VGA_HEIGHT)
                term_row = 0;
        }
    }

    term_update_cursor();
}

void term_write(const char *data) {
    size_t len = strlen(data);

    for (size_t i = 0; i < len; i++)
        term_putchar(data[i]);
}

void term_clear() {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            size_t index = y * VGA_WIDTH + x;

            term_buffer[index] = vga_entry(' ', term_color);
        }
    }
}

void term_init() {
    outb(0x3d4, 0x0c);
    outb(0x3d5, 0x00);
    outb(0x3d4, 0x0d);
    outb(0x3d5, 0x00);

    term_row = 0;
    term_column = 0;
    
    term_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    term_clear();
}