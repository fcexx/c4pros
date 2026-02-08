#include <stdint.h>
#include "c4pros.h"

/* Пиксель через BIOS INT 10h AH=0x0C */
inline void out_set_pixel(uint8_t color, uint16_t x, uint16_t y) {
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
void out_mem_pixel(uint8_t color, uint16_t x, uint16_t y) {
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
void out_mem_pixel_gc_restore(void) {
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
