/* What the JS host gives the module (web/wasm.mjs): its imports, by their names there. */
#ifndef PLAT_WASM_H
#define PLAT_WASM_H
#include <stddef.h>
#include <stdint.h>

#define IMPORT(name) [[clang::import_module("qb"), clang::import_name(#name)]]

/* The files: each returns its result, or an errno negated. */
IMPORT(fs_open) int js_fs_open(const char *path, int flags);
IMPORT(fs_read) int js_fs_read(int fd, void *p, size_t n);
IMPORT(fs_write) int js_fs_write(int fd, const void *p, size_t n);
IMPORT(fs_seek) double js_fs_seek(int fd, double off, int whence);
IMPORT(fs_close) int js_fs_close(int fd);
IMPORT(fs_truncate) int js_fs_truncate(int fd, double size);
/* *out: mode, size, then the time it was changed in ms since 1970 (f64). */
IMPORT(fs_stat) int js_fs_stat(const char *path, int fd, void *out);
IMPORT(fs_mkdir) int js_fs_mkdir(const char *path);
IMPORT(fs_rmdir) int js_fs_rmdir(const char *path);
IMPORT(fs_unlink) int js_fs_unlink(const char *path);
IMPORT(fs_rename) int js_fs_rename(const char *from, const char *to);
IMPORT(fs_chmod) int js_fs_chmod(const char *path, int mode);
IMPORT(fs_chdir) int js_fs_chdir(const char *path);
IMPORT(fs_getcwd) int js_fs_getcwd(char *buf, size_t cap);
/* The directory's entry at index, its name's length in buf; -1 past the last. */
IMPORT(fs_readdir) int js_fs_readdir(const char *path, uint32_t index, char *buf, size_t cap);

/* The time in ms since 1970, and the local time's offset from it then, in minutes. */
IMPORT(now) double js_now(void);
IMPORT(tz) double js_tz(double ms);
/* The process: its arguments (space separated, cap bytes), its exit. */
IMPORT(args) int js_args(char *buf, size_t cap);
[[noreturn]]
IMPORT(exit) void js_exit(int code);
/* A tone of hz, 0 none. */
IMPORT(tone) void js_tone(uint32_t hz);
/* A synthesizer's voice set: v its DevVoice (qbrt's dev/voice.h), read before the call returns. */
IMPORT(voice) void js_voice(uint32_t voice, const void *v);
/* A native extension's side module, bytes at slice, instantiated here: qx_init_<name>'s table
   index, 0 none (plat/wasm/pext.c). */
IMPORT(ext_load) uint32_t js_ext_load(const void *slice, uint32_t bytes, const char *name, size_t len);

/* What the page's thread writes while the module runs, in its shared memory (irq()'s address): the
   tick's and STOP's flags and bits, ORed in as the interrupts would; the keys typed, a ring it
   fills at tail and the module takes from head, Atomics.notify on tail; the mouse. */
enum { WASM_NKEY = 64 };
typedef struct {
    volatile uint8_t *tick;
    volatile uint8_t *stop;
    uint8_t tick_bit, stop_bit;
    uint8_t shift;    /* the shift flags, pkey's PKEY_* */
    uint8_t keyboard; /* the ring is the keyboard (the page), else standard input is */
    uint32_t head, tail;
    uint16_t keys[WASM_NKEY];
    uint8_t mouse_row, mouse_col, mouse_buttons; /* the page's, as INT 33h AX=3 */
    uint8_t mouse_on;                            /* pmou_init found it: the page has a pointer */
    int32_t mouse_shown;                         /* pmou_show's count less pmou_hide's */
    uint16_t mouse_x, mouse_y;                   /* the pixel under it, as pmou_state's */
    uint32_t press_count[2];                     /* the left's and the right's, the page adds */
    uint32_t press_xy[2];                        /* the last press's x, y << 16 */
    uint16_t press_cell[2];                      /* its row, col << 8 */
} WasmIrq;
extern WasmIrq wasm_irq;

#endif
