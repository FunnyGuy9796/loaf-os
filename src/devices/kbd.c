#include "kbd.h"
#include "../misc/printf.h"

#define KBD_DATA_PORT 0x60
#define KBD_BUFFER_SIZE 256

static char kbd_buffer[KBD_BUFFER_SIZE];
static volatile uint32_t kbd_read_pos = 0, kbd_write_pos = 0;
static uint8_t shift_pressed = 0;

static const char scancode_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
};

static const char scancode_ascii_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0, 'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?', 0,
    '*', 0, ' ', 0,
};

#define SCANCODE_LSHIFT_DOWN 0x2A
#define SCANCODE_RSHIFT_DOWN 0x36
#define SCANCODE_LSHIFT_UP   0xAA
#define SCANCODE_RSHIFT_UP   0xB6

void kbd_handler() {
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == SCANCODE_LSHIFT_DOWN || scancode == SCANCODE_RSHIFT_DOWN) {
        shift_pressed = 1;

        return;
    }

    if (scancode == SCANCODE_LSHIFT_UP || scancode == SCANCODE_RSHIFT_UP) {
        shift_pressed = 0;

        return;
    }

    if (scancode & 0x80)
        return;

    char c = shift_pressed ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];

    if (c) {
        kbd_buffer[kbd_write_pos] = c;
        kbd_write_pos = (kbd_write_pos + 1) % KBD_BUFFER_SIZE;
    }
}

int kbd_getchar() {
    if (kbd_read_pos == kbd_write_pos)
        return -1;
    
    char c = kbd_buffer[kbd_read_pos];

    kbd_read_pos = (kbd_read_pos + 1) % KBD_BUFFER_SIZE;

    return c;
}