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
        if (c == '\b') {   /* Backspace: erase via "\b \b" output so the PRos cursor (INT 0x21) stays in sync with ours (INT 10h). */
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

/* Pixel via BIOS INT 10h AH=0x0C */
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

/* Pixel by writing to 0xA000 (mode 0x12 planar). Without restoring GC for speed. */
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

/* Horizontal line by byte writes (mode 0x12 planar). Much faster than per-pixel fill. */
void c4pros_mem_hline(uint8_t color, uint16_t x, uint16_t y, uint16_t len) {
    if (len == 0) return;
    uint16_t offset = (uint16_t)(y * 80u + (x / 8u));
    uint8_t start_shift = (uint8_t)(x & 7u);
    uint16_t first_n = 8u - start_shift;
    if (first_n > len) first_n = len;
    uint8_t first_mask = (first_n == 8u) ? 0xFFu : (uint8_t)(((1u << first_n) - 1u) << (8u - start_shift - first_n));
    uint16_t rest = len - first_n;
    uint16_t num_full = rest / 8u;
    uint16_t last_n = rest % 8u;
    uint8_t last_mask = (last_n == 0) ? 0u : (uint8_t)(((1u << last_n) - 1u) << (8u - last_n));

    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %2, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        /* GC reg 0 = color, reg 1 = 0xFF */
        "xorb %%al, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %0, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb $1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        /* First partial byte */
        "movb %1, %%al\n\t"
        "testb %%al, %%al\n\t"
        "jz 1f\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "incw %%di\n\t"
        "1:\n\t"
        /* Full bytes */
        "movw %3, %%cx\n\t"
        "jcxz 2f\n\t"
        "3:\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0xFF, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "incw %%di\n\t"
        "loop 3b\n\t"
        "2:\n\t"
        /* Last partial byte */
        "movb %4, %%al\n\t"
        "testb %%al, %%al\n\t"
        "jz 4f\n\t"
        "movb $8, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %4, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "decw %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb $0xFF, %%es:(%%di)\n\t"
        "4:\n\t"
        "popw %%es"
        :
        : "q" (color), "m" (first_mask), "m" (offset), "m" (num_full), "m" (last_mask)
        : "ax", "cx", "dx", "di", "cc", "memory"
    );
}

/* Read byte from 0xA000 (mode 0x12): plane 0..3, GC reg 4 = Read Map Select. */
uint8_t c4pros_mem_read_byte(uint16_t offset, uint8_t plane)
{
    uint8_t r;
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %1, %%di\n\t"
        "movw $0x3CE, %%dx\n\t"
        "movb $4, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb %%es:(%%di), %%al\n\t"
        "movb %%al, %0\n\t"
        "popw %%es"
        : "=m" (r)
        : "r" (offset), "q" (plane)
        : "ax", "dx", "di", "cc", "memory"
    );
    return r;
}

/* Write byte to a single plane: SC (0x3C4) reg 2 = Map Mask. plane 0..3. */
void c4pros_mem_write_byte(uint16_t offset, uint8_t plane, uint8_t byte)
{
    uint8_t mask = (uint8_t)(1u << plane);
    __asm__ volatile (
        "pushw %%es\n\t"
        "movw $0xA000, %%ax\n\t"
        "movw %%ax, %%es\n\t"
        "movw %0, %%di\n\t"
        "movw $0x3C4, %%dx\n\t"
        "movb $2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb %1, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "movb %2, %%al\n\t"
        "movb %%al, %%es:(%%di)\n\t"
        "popw %%es"
        :
        : "r" (offset), "q" (mask), "m" (byte)
        : "ax", "dx", "di", "cc", "memory"
    );
}

/* Restore Map Mask (SC 0x3C4 reg 2) = all planes after cursor operations. */
void c4pros_mem_sc_restore(void)
{
    __asm__ volatile (
        "movw $0x3C4, %%dx\n\t"
        "movb $2, %%al\n\t"
        "outb %%al, %%dx\n\t"
        "incw %%dx\n\t"
        "movb $0x0F, %%al\n\t"
        "outb %%al, %%dx"
        :
        :
        : "ax", "dx", "cc"
    );
}

/* Call once after a series of out_mem_pixel if you need default VGA registers. */
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

/* --- PS/2 Controller (ports 0x60, 0x64) --- */
#define PS2_STATUS  0x64
#define PS2_DATA    0x60
#define PS2_STAT_OBF  0x01u   /* output buffer full - data can be read from 0x60 */
#define PS2_STAT_IBF  0x02u   /* input buffer full - writing is not allowed */
#define PS2_STAT_AUX  0x20u   /* byte in 0x60 is from mouse */

static inline uint8_t ps2_inb(uint16_t port)
{
    uint8_t r;
    __asm__ volatile ("inb %%dx, %0" : "=a" (r) : "d" (port) : "memory");
    return r;
}
static inline void ps2_outb(uint8_t val, uint16_t port)
{
    __asm__ volatile ("outb %b0, %%dx" : : "a" (val), "d" (port) : "memory");
}

/* Wait until writing to the controller is allowed (IBF bit cleared). */
static void ps2_wait_write(void)
{
    unsigned n = 0;
    while ((ps2_inb(PS2_STATUS) & PS2_STAT_IBF) && ++n < 10000u) { }
}

/* Wait until a byte is available in 0x60 (OBF bit set). Returns 1 if data is from mouse (AUX). */
static int ps2_wait_read_mouse(uint8_t *from_mouse)
{
    unsigned n = 0;
    for (;;) {
        uint8_t s = ps2_inb(PS2_STATUS);
        if (s & PS2_STAT_OBF) {
            if (from_mouse) *from_mouse = (s & PS2_STAT_AUX) ? 1 : 0;
            return 1;
        }
        if (++n >= 20000u) return 0;
    }
}

static uint8_t ps2_read_data(void)
{
    return ps2_inb(PS2_DATA);
}

/* Send mouse command (0xD4 to 0x64, then cmd to 0x60). Wait for ACK 0xFA. */
static int ps2_send_mouse_cmd(uint8_t cmd)
{
    ps2_wait_write();
    ps2_outb(0xD4, PS2_STATUS);
    ps2_wait_write();
    ps2_outb(cmd, PS2_DATA);
    for (unsigned i = 0; i < 50; i++) {
        uint8_t from_mouse;
        if (!ps2_wait_read_mouse(&from_mouse)) return 0;
        uint8_t b = ps2_read_data();
        if (from_mouse && b == 0xFA) return 1;
    }
    return 0;
}

/* State for receiving a 3-byte mouse packet. */
static uint8_t ps2_mouse_packet_buf[3];
static uint8_t ps2_mouse_packet_idx;
static int ps2_mouse_inited;

/* PS/2 mouse initialization: enable aux and packet stream. Return: 0 = ok, -1 = error. */
int c4pros_ps2_mouse_init(void)
{
    ps2_mouse_packet_idx = 0;
    ps2_mouse_inited = 0;

    /* Enable auxiliary device */
    ps2_wait_write();
    ps2_outb(0xA8, PS2_STATUS);
    ps2_wait_write();

    /* Reset to defaults (optional, reduces garbage after enabling) */
    if (!ps2_send_mouse_cmd(0xF6)) return -1;

    /* Enable streaming (mouse sends packets on movement/button events) */
    if (!ps2_send_mouse_cmd(0xF4)) return -1;

    /* Clear leftover bytes from 0x60 (ACK 0xFA, etc.), otherwise first packet byte may be lost and poll won't complete a packet */
    while (ps2_inb(PS2_STATUS) & PS2_STAT_OBF)
        (void)ps2_read_data();

    ps2_mouse_packet_idx = 0;
    ps2_mouse_inited = 1;
    return 0;
}

/* Is there a byte in 0x60 buffer? We read both mouse and keyboard; mouse packets are detected by bit 3 of the first byte. */
int c4pros_ps2_mouse_byte_ready(void)
{
    uint8_t s = ps2_inb(PS2_STATUS);
    /* In QEMU and some hardware AUX bit is not set - check only OBF */
    return (s & PS2_STAT_OBF) ? 1 : 0;
}

/* Read one mouse packet (3 bytes) if ready. Return: 1 = packet read, 0 = no data/incomplete packet.
   Sync rule: first byte in Microsoft mouse packet format always has bit 3 = 1; otherwise treat as keyboard byte and skip.
   *buttons: bits 0=left, 1=right, 2=middle.
   *dx, *dy: relative movement (signed). */
int c4pros_ps2_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    if (!ps2_mouse_inited) return 0;

    while (c4pros_ps2_mouse_byte_ready()) {
        uint8_t b = ps2_read_data();
        if (ps2_mouse_packet_idx == 0u) {
            /* Only bytes with bit 3 are treated as mouse packet start (MS format) */
            if (!(b & 0x08u)) continue;
        }
        ps2_mouse_packet_buf[ps2_mouse_packet_idx++] = b;

        if (ps2_mouse_packet_idx >= 3u) {
            ps2_mouse_packet_idx = 0;
            uint8_t f = ps2_mouse_packet_buf[0];
            uint8_t dx8 = ps2_mouse_packet_buf[1];
            uint8_t dy8 = ps2_mouse_packet_buf[2];
            /* Sign extension for 9-bit deltas */
            int16_t dx16 = (int16_t)((f & 0x10u) ? (dx8 | 0xFF00u) : dx8);
            int16_t dy16 = (int16_t)((f & 0x20u) ? (dy8 | 0xFF00u) : dy8);
            if (buttons) *buttons = f & 0x07u;
            if (dx) *dx = dx16;
            if (dy) *dy = dy16;
            return 1;
        }
    }
    return 0;
}

/* Reset packet alignment state (if a byte was lost). */
void c4pros_ps2_mouse_reset_packet_state(void)
{
    ps2_mouse_packet_idx = 0;
}

/* --- Mouse: INT 33h (driver) or INT 15h (callback) --- */
extern int c4pros_mouse_int15_init(void);
extern void c4pros_mouse_int15_enable(void);
extern void c4pros_mouse_int15_disable(void);
extern volatile uint8_t mouse_event_ready;
extern int16_t mouse_dx;
extern int16_t mouse_dy;
extern uint8_t mouse_buttons;

static int mouse_use_int33;           /* 1 = poll via INT 33h AX=3 */
static int16_t mouse_last_x, mouse_last_y;
static int mouse_first_poll;          /* first poll after init */
static int mouse_int33_probed;        /* INT 33h AX=3 already tried in poll */
static unsigned int mouse_zero_count; /* consecutive (0,0) from INT 33h - switch to INT 15h */

/* INT 33h AX=0: reset, returns AX=0xFFFF when driver exists */
static int mouse_int33_available(void)
{
    uint16_t ax;
    __asm__ volatile ("movw $0, %%ax; int $0x33" : "=a" (ax) : : "bx", "cx", "dx", "cc");
    return (ax == 0xFFFF);
}

/* Try INT 33h AX=3 once in poll - in some environments driver exists but AX=0 is not 0xFFFF */
static int mouse_try_int33_once(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    uint16_t bx, cx, dx_val;
    __asm__ volatile ("movw $3, %%ax; int $0x33"
        : "=b" (bx), "=c" (cx), "=d" (dx_val) : : "ax", "cc");
    /* coordinates are usually 0..639/0..479 or 0..1023; garbage values are often larger */
    if (cx > 2047u || dx_val > 2047u) return 0;
    mouse_use_int33 = 1;
    mouse_first_poll = 1;
    mouse_last_x = (int16_t)cx;
    mouse_last_y = (int16_t)dx_val;
    mouse_first_poll = 0;
    if (buttons) *buttons = (uint8_t)(bx & 0x07u);
    if (dx) *dx = 0;
    if (dy) *dy = 0;
    return 1;
}

int c4pros_mouse_init(void)
{
    mouse_use_int33 = 0;
    mouse_first_poll = 1;
    mouse_int33_probed = 0;
    mouse_zero_count = 0;

    if (mouse_int33_available()) {
        mouse_use_int33 = 1;
        return 0;
    }
    return c4pros_mouse_int15_init();
}

void c4pros_mouse_enable(void)
{
    if (mouse_use_int33) {
        /* INT 33h AX=1 - enable cursor/tracking (some drivers return 0,0 without this) */
        __asm__ volatile ("movw $1, %%ax; int $0x33" : : : "ax", "bx", "cx", "dx", "cc");
    } else {
        mouse_dx = 320;
        mouse_dy = 240;
        c4pros_mouse_int15_enable();
    }
}

void c4pros_mouse_disable(void)
{
    if (!mouse_use_int33)
        c4pros_mouse_int15_disable();
}

int c4pros_mouse_returns_absolute(void)
{
    return mouse_use_int33;
}

int c4pros_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy)
{
    if (mouse_use_int33) {
        uint16_t bx, cx, dx_val;
        __asm__ volatile ("movw $3, %%ax; int $0x33"
            : "=b" (bx), "=c" (cx), "=d" (dx_val) : : "ax", "cc");
        /* Driver garbage (e.g. X:-15865) - switch immediately to INT 15h */
        if (cx > 1023u || dx_val > 1023u || (int16_t)cx < 0 || (int16_t)dx_val < 0) {
            mouse_use_int33 = 0;
            mouse_dx = 320;
            mouse_dy = 240;
            mouse_event_ready = 0;
            if (c4pros_mouse_int15_init() == 0) {
                c4pros_mouse_int15_enable();
                if (buttons) *buttons = 0;
                if (dx) *dx = 320;
                if (dy) *dy = 240;
                return 1;
            }
            mouse_use_int33 = 1; /* failed - stay on INT 33h */
        }
        if (buttons) *buttons = (uint8_t)(bx & 0x07u);
        if (dx) *dx = (int16_t)cx;
        if (dy) *dy = (int16_t)dx_val;
        /* Only zeros for too long - switch to INT 15h */
        if (cx == 0 && dx_val == 0) {
            mouse_zero_count++;
            if (mouse_zero_count > 100u) {
                mouse_use_int33 = 0;
                mouse_zero_count = 0;
                mouse_dx = 320;
                mouse_dy = 240;
                mouse_event_ready = 0;
                if (c4pros_mouse_int15_init() == 0)
                    c4pros_mouse_int15_enable();
            }
        } else
            mouse_zero_count = 0;
        return 1;
    }

    if (mouse_event_ready) {
        if (buttons) *buttons = mouse_buttons;
        if (dx) *dx = mouse_dx;
        if (dy) *dy = mouse_dy;
        mouse_event_ready = 0;
        return 1;
    }
    /* INT 15h callback did not fire - try INT 33h AX=3 once */
    if (!mouse_int33_probed) {
        mouse_int33_probed = 1;
        if (mouse_try_int33_once(buttons, dx, dy)) return 1;
    }
    return 0;
}
