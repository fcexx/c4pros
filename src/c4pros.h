#ifndef c4pros_H
#define c4pros_H

#include <stdint.h>

typedef unsigned int size_t;

extern inline int strcmp(const char *s1, const char *s2);
extern inline void* memcpy(void* dest, const void* src, size_t n);

extern void c4pros_out_init(void);

extern void c4pros_print_white(const char *s);
extern void c4pros_print_green(const char *s);
extern void c4pros_print_cyan(const char *s);
extern void c4pros_print_red(const char *s);
extern void c4pros_print_newline(void);
extern void c4pros_clear_screen(void);
extern void c4pros_set_color(uint8_t color);   /* 0x00-0x0F, 16 VGA colors */
extern void c4pros_print(const char *s);

// returns ASCII
extern uint8_t c4pros_get_char(void);
extern uint16_t c4pros_wait_key(void);

extern void c4pros_get_string(char *buf, uint16_t max);

extern void c4pros_set_pixel(uint8_t color, uint16_t x, uint16_t y);   /* INT 10h AH=0x0C */
extern void c4pros_mem_pixel(uint8_t color, uint16_t x, uint16_t y);   /* write to 0xA000 planar */
extern void c4pros_mem_hline(uint8_t color, uint16_t x, uint16_t y, uint16_t len);   /* fast horizontal line */
extern uint8_t c4pros_mem_read_byte(uint16_t offset, uint8_t plane);   /* 0xA000 planar, GC reg 4 = Read Map */
extern void c4pros_mem_write_byte(uint16_t offset, uint8_t plane, uint8_t byte);   /* 0xA000 planar, SC reg 2 = Map Mask */
extern void c4pros_mem_pixel_gc_restore(void);
extern void c4pros_mem_sc_restore(void);   /* SC reg 2 = 0xF (all planes) after cursor ops */
extern void c4pros_set_video_mode(uint8_t mode);   /* AH=0, AL=mode (e.g. 0x12, 0x13) */

extern void c4pros_cursor_set(uint8_t row, uint8_t col);
extern uint16_t c4pros_cursor_get(void);   /* high byte = row, low byte = col */

/* --- INT 0x22: File System API --- */
extern int c4pros_fs_init(void);   /* 0=ok, 1=error (CF) */

/* Current directory file list: buf receives names, size_lo/size_hi receive written size, count receives number of items. Return: 0=ok, otherwise error. */
extern int c4pros_fs_get_file_list(char *buf, uint16_t *size_lo, uint16_t *size_hi, uint16_t *count);

extern int c4pros_fs_load_file(const char *name, void *addr);   /* size or -1 */
extern int c4pros_fs_write_file(const char *name, const void *data, uint16_t size);
extern int c4pros_fs_file_exists(const char *name);   /* 1=exists, 0=not found */
extern int c4pros_fs_create_empty(const char *name);
extern int c4pros_fs_remove_file(const char *name);
extern int c4pros_fs_rename_file(const char *old_name, const char *new_name);
extern int c4pros_fs_get_file_size(const char *name);   /* size or -1 */

extern int c4pros_fs_chdir(const char *dir);
extern int c4pros_fs_parent_dir(void);
extern int c4pros_fs_mkdir(const char *dir);
extern int c4pros_fs_rmdir(const char *dir);
extern int c4pros_fs_is_dir(const char *name);   /* 1=directory */

extern void c4pros_fs_save_dir(void);
extern void c4pros_fs_restore_dir(void);

extern int c4pros_fs_load_huge_file(const char *name, uint16_t load_offset, uint16_t load_segment);

/* --- PS/2 mouse (ports 0x60/0x64), optional --- */
extern int c4pros_ps2_mouse_init(void);   /* 0=ok, -1=error */
extern int c4pros_ps2_mouse_byte_ready(void);
extern int c4pros_ps2_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy);
extern void c4pros_ps2_mouse_reset_packet_state(void);

/* --- Mouse via INT 15h (Pointing Device BIOS), as in the working NASM driver --- */
extern int c4pros_mouse_init(void);   /* 0=ok, -1=no mouse */
extern void c4pros_mouse_enable(void);
extern void c4pros_mouse_disable(void);
extern int c4pros_mouse_poll(uint8_t *buttons, int16_t *dx, int16_t *dy);   /* 1=event available */
extern int c4pros_mouse_returns_absolute(void);   /* 1 = dx,dy contain position (INT 33h), 0 = delta (INT 15h) */

#endif
