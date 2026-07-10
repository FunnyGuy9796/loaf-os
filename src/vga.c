#include "vga.h"
#include "misc/util.h"

#define ESC_MAX_PARAMS 10

typedef enum {
    ESC_NONE,
    ESC_START,
    ESC_PARAMS
} esc_state_t;

static esc_state_t esc_state = ESC_NONE;
static int esc_params[ESC_MAX_PARAMS];
static int esc_param_count;
static int esc_cur_param;
static int esc_param_active;

size_t term_row;
size_t term_column;
uint8_t term_color;
uint16_t* term_buffer = (uint16_t*)VGA_ADDR;

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

static void term_scroll() {
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++)
            term_buffer[(y - 1) * VGA_WIDTH + x] = term_buffer[y * VGA_WIDTH + x];
    }

    for (size_t x = 0; x < VGA_WIDTH; x++)
        term_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', term_color);
}

static void term_advance_row() {
    if (++term_row >= VGA_HEIGHT) {
        term_scroll();
        
        term_row = VGA_HEIGHT - 1;
    }
}

static void term_putentryat(char c, uint8_t color, size_t x, size_t y)  {
	const size_t index = y * VGA_WIDTH + x;

	term_buffer[index] = vga_entry(c, color);
}

static void term_update_cursor() {
    const size_t index = term_row * VGA_WIDTH + term_column;

    outb(0x3d4, 0x0f);
    outb(0x3d5, (uint8_t)(index & 0xff));
    outb(0x3d4, 0x0e);
    outb(0x3d5, (uint8_t)((index >> 8) & 0xff));
}

static uint8_t ansi_to_vga_color(int code) {
    return (uint8_t)(code & 0x7);
}

static void esc_execute(char final) {
    switch (final) {
        case 'H': {
            int row = esc_param_count > 0 ? esc_params[0] : 1;
            int col = esc_param_count > 1 ? esc_params[1] : 1;

            if (row < 1)
                row = 1;

            if (col < 1)
                col = 1;

            if (row > (int)VGA_HEIGHT)
                row = VGA_HEIGHT;

            if (col > (int)VGA_WIDTH)
                col = VGA_WIDTH;

            term_row = (size_t)(row - 1);
            term_column = (size_t)(col - 1);

            break;
        }

        case 'J': {
            int mode = esc_param_count > 0 ? esc_params[0] : 0;

            if (mode == 2 || mode == 3)
                term_clear();

            break;
        }

        case 'K': {
            for (size_t x = term_column; x < VGA_WIDTH; x++)
                term_putentryat(' ', term_color, x, term_row);

            break;
        }

        case 'm': {
            if (esc_param_count == 0) {
                term_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
                
                break;
            }

            uint8_t fg = term_color & 0x0f;
            uint8_t bg = (term_color >> 4) & 0x0f;

            for (int i = 0; i < esc_param_count; i++) {
                int p = esc_params[i];

                if (p == 0) {
                    fg = VGA_COLOR_WHITE;
                    bg = VGA_COLOR_BLACK;
                } else if (p >= 30 && p <= 37)
                    fg = ansi_to_vga_color(p - 30);
                else if (p >= 40 && p <= 47)
                    bg = ansi_to_vga_color(p - 40);
            }

            term_color = fg | (bg << 4);

            break;
        }

        default:
            break;
    }

    term_update_cursor();
}

void term_setcolor(enum vga_color fg, enum vga_color bg) {
	term_color = fg | bg << 4;
}

void term_putchar(char c) {
    if (esc_state == ESC_START) {
        if (c == '[') {
            esc_state = ESC_PARAMS;
            esc_param_count = 0;
            esc_cur_param = 0;
            esc_param_active = 0;
        } else
            esc_state = ESC_NONE;

        return;
    }

    if (esc_state == ESC_PARAMS) {
        if (c >= '0' && c <= '9') {
            esc_cur_param = esc_cur_param * 10 + (c - '0');
            esc_param_active = 1;
        } else if (c == ';') {
            if (esc_param_count < ESC_MAX_PARAMS)
                esc_params[esc_param_count++] = esc_param_active ? esc_cur_param : 0;

            esc_cur_param = 0;
            esc_param_active = 0;
        } else if ((c >= '@' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            if (esc_param_count < ESC_MAX_PARAMS)
                esc_params[esc_param_count++] = esc_param_active ? esc_cur_param : 0;

            esc_state = ESC_NONE;

            esc_execute(c);
        }

        return;
    }

    if (c == 0x1b) {
        esc_state = ESC_START;

        return;
    }

    if (c == '\n') {
        term_column = 0;

        term_advance_row();
    } else if (c == '\t') {
        term_column = (term_column + 4) & ~3;

        if (term_column >= VGA_WIDTH) {
            term_column = 0;

            term_advance_row();
        }
    } else if (c == '\b') {
        if (term_column == 0) {
            if (term_row > 0)
                term_row--;

            term_column = VGA_WIDTH - 1;
        } else
            term_column--;

        term_putentryat(' ', term_color, term_column, term_row);
    } else {
        term_putentryat(c, term_color, term_column, term_row);

        if (++term_column >= VGA_WIDTH) {
            term_column = 0;

            term_advance_row();
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
    term_column = 0;
    term_row = 0;
    
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