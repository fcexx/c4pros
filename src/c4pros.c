#include <stdint.h>
#include "c4pros.h"

inline int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

inline void* memcpy(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void c4pros_get_string(char *buf, uint16_t max) {
    static char echo[2];
    uint16_t i = 0;
    uint16_t pos = c4pros_cursor_get();
    uint8_t row = (uint8_t)(pos >> 8);
    uint8_t col = (uint8_t)(pos & 0xFF);
    if (max == 0) return;
    for (;;) {
        char c = (char)c4pros_get_char();
        if (c == '\n' || c == '\r') {
            c4pros_print_newline();
            break;
        }
        if (c == '\b') {   /* Backspace: стираем через вывод "\b \b", чтобы курсор PRos (INT 0x21) не расходился с нашим (INT 10h). */
            if (i > 0) {
                i--;
                buf[i] = '\0';
                if (col > 0)
                    col--;
                else if (row > 0) {
                    row--;
                    col = 79;
                }
                {
                    static const char bs_space_bs[4] = { '\b', ' ', '\b', '\0' };
                    c4pros_print_white(bs_space_bs);
                }
            }
            continue;
        }
        if (i + 1 < max) {
            buf[i++] = c;
            buf[i] = '\0';
        }
        echo[0] = c;
        echo[1] = '\0';
        c4pros_print_white(echo);
        col++;
        if (col > 79) {
            col = 0;
            row++;
        }
    }
    buf[i] = '\0';
}

/* Пиксель через BIOS INT 10h AH=0x0C */
inline void c4pros_set_pixel(uint8_t color, uint16_t x, uint16_t y) {
    __asm__ volatile (
        "movb $0x0C, %%ah\n\t"
        "movb %0, %%al\n\t"
        "xorw %%bx, %%bx\n\t"
        "movw %1, %%cx\n\t"
        "movw %2, %%dx\n\t"
        "int $0x10"
        :
        : "g" ((uint8_t)color), "g" ((uint16_t)x), "g" ((uint16_t)y)
        : "ax", "bx", "cx", "dx", "cc", "memory"
    );
}

/* Пиксель записью в 0xA000 (режим 0x12 planar). Без восстановления GC — быстрее. */
void c4pros_mem_pixel(uint8_t color, uint16_t x, uint16_t y) {
    uint16_t offset = (uint16_t)(y * 80u + (x / 8u));
    uint8_t bit_mask = (uint8_t)(1u << (7 - (x & 7u)));

    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %2, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        /* GC reg 0 = color */
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %0, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* GC reg 1 = 0xFF */
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* GC reg 8 = bit_mask */
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        /* read (latch) + write */
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "popw %%es"
        :
        : "g" (color), "g" (bit_mask), "g" (offset)
        : "ax", "dx", "di", "cc", "memory"
    );
}

/* После серии out_mem_pixel вызови один раз, если нужны стандартные регистры VGA. */
void c4pros_mem_pixel_gc_restore(void) {
    __asm__ volatile (
        "movw $0x3CE, %%dx\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx"
        :
        :
        : "ax", "dx", "cc"
    );
}
