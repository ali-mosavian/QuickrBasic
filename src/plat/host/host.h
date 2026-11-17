/* The host platform's own: what tests drive and inspect. Only plat/host and tests include it. */
#ifndef PLAT_HOST_H
#define PLAT_HOST_H
#include "base.h"
#include "plat/pscr.h"

/* A key for pkey_read and pkey_wait: scan code << 8 | character. */
void host_key_push(uint16_t key);
/* Page's cell at row, col. */
PscrCell host_cell(uint8_t page, uint8_t row, uint8_t col);

/* The screen as a display shows it: the page shown's cells, its cursor and shape, the 16 colours
   (6 bits a channel, as pscr_palette's). */
typedef struct {
    const PscrCell *cells;
    uint8_t page, rows, cols, row, col;
    uint16_t shape;
    uint8_t palette[48];
} HostScreen;
HostScreen host_screen(void);
/* The same of a page, shown or not. */
HostScreen host_screen_of(uint8_t page);
/* The display's own size (a terminal's), which pscr_text then takes: rows by cols. */
void host_text_fit(uint8_t rows, uint8_t cols);

/* The terminal as the keyboard, if standard input is one (pterm.c); on, a key is waiting, the next
   key (scan code << 8 | character; 0 Ctrl+Break). */
void host_term_open(void);
/* The terminal as it was, for a child process (yes), and back. */
void host_term_pause(bool yes);
bool host_term_on(void);
bool host_term_ready(void);
uint16_t host_term_key(void);
/* The shift flags (PKEY_*): none on a terminal. */
uint8_t host_term_shift(void);
/* The terminal reports the mouse (host_mouse_set), and the keys as kitty's protocol has them: an
   Alt pressed and let go, where it can. */
void host_term_rich(void);
/* A window's keys as well: BIOS keys (uint16_t) written to fd, its shift flags set as they change. */
void host_term_feed(int fd);
void host_term_set_shift(uint8_t flags);

/* The mouse a window has (pmou.c): its text cell and buttons as pmou_state's, set as it moves; on,
   it has one. shown is pmou_show's count less pmou_hide's. */
void host_mouse_on(void);
void host_mouse_set(uint8_t row, uint8_t col, uint8_t buttons);
bool host_mouse_shown(void);
/* SIGINT came since the last call (psys.c). */
bool host_stop_taken(void);
/* A key on a US keyboard by the character it types unshifted ('a', '1', ' '), or one of these. */
enum : uint32_t {
    HK_ENTER = 0x100,
    HK_ESC,
    HK_BACKSPACE,
    HK_TAB,
    HK_INSERT,
    HK_DELETE,
    HK_HOME,
    HK_END,
    HK_PGUP,
    HK_PGDN,
    HK_UP,
    HK_DOWN,
    HK_LEFT,
    HK_RIGHT,
    HK_F1, /* .. HK_F1 + 11, F12 */
};
/* Its BIOS key (scan code << 8 | character) with the shift flags (PKEY_*) held: typed the character
   it types, 0 for a US keyboard's; -1 none. */
int32_t host_bios_key(uint32_t key, uint32_t typed, uint8_t flags);
/* The key that types ch on a US keyboard, *shift whether with Shift. */
uint32_t host_key_of(uint8_t ch, bool *shift);

/* A terminal's input (ttyin.c): a key, the mouse (its cell, its buttons as INT 33h's), the shift
   flags changed (kitty's Alt alone), or nothing a BIOS has. flags: the shift flags after it. */
typedef enum { TTY_NONE, TTY_KEY, TTY_MOUSE, TTY_SHIFT } TtyKind;
typedef struct {
    TtyKind kind;
    uint16_t key;
    uint8_t flags, row, col, buttons;
} TtyEvent;
typedef struct {
    bool kitty;      /* its keys come as kitty's protocol has them */
    uint8_t mods;    /* the shift flags kitty's said are down */
    uint8_t buttons; /* the mouse's down */
} TtyIn;
/* The first event of in's n bytes to *e: the bytes it took; 0, they may be the start of a longer
   one (final: no more are coming, an ESC alone is Esc). */
uint32_t tty_parse(TtyIn *t, const uint8_t *in, uint32_t n, bool final, TtyEvent *e);
#endif
