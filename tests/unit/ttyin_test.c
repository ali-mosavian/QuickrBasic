/* plat/host/ttyin: a terminal's bytes as BIOS keys, the mouse, the shift flags. */
#include "plat/host/host.h"
#include "plat/pkey.h"
#include "unit.h"

/* The first event of s; its byte count to *used. */
static TtyEvent parse(TtyIn *const t, const char *const s, uint32_t *const used)
{
    TtyEvent e;
    *used = tty_parse(t, (const uint8_t *)s, (uint32_t)__builtin_strlen(s), true, &e);
    return e;
}

static uint16_t key_of(const char *const s)
{
    TtyIn t = {};

    uint32_t used;
    const TtyEvent e = parse(&t, s, &used);
    return e.kind == TTY_KEY && used == __builtin_strlen(s) ? e.key : 0xFFFF;
}

TEST(tty_plain_keys_are_the_bios_keys)
{
    CHECK(key_of("a") == 0x1E61 && key_of("A") == 0x1E41 && key_of("!") == 0x0221);
    CHECK(key_of("\r") == 0x1C0D && key_of("\x7f") == 0x0E08 && key_of("\t") == 0x0F09);
    CHECK(key_of("\x06") == 0x2106); /* Ctrl+F */
    CHECK(key_of("\x1b") == 0x011B); /* Esc alone, no more coming */
}

/* The menus' Alt+F was ESC then f: read as Esc, the f dropped. */
TEST(tty_esc_then_a_key_is_that_key_with_alt)
{
    CHECK(
        key_of(
            "\x1b"
            "f"
        ) == 0x2100
    );
    CHECK(
        key_of(
            "\x1b"
            "1"
        ) == 0x7800
    );
}

TEST(tty_xterm_keys_and_their_modifiers)
{
    CHECK(key_of("\x1b[A") == 0x48E0 && key_of("\x1b[1;5D") == 0x73E0); /* Up, Ctrl+Left */
    CHECK(key_of("\x1b[15~") == 0x3F00 && key_of("\x1bOP") == 0x3B00);  /* F5, F1 */
    TtyIn t = {};

    uint32_t used;
    const TtyEvent e = parse(&t, "\x1b[1;2B", &used); /* Shift+Down: shift held, for a selection */
    CHECK(e.key == 0x50E0 && e.flags & PKEY_SHIFT);
}

/* An Alt tapped alone opens the menus: only kitty's protocol tells a terminal's Alt going down
   and up. */
TEST(tty_kitty_reports_alt_alone_down_then_up)
{
    TtyIn t = {};

    uint32_t used;
    TtyEvent e = parse(&t, "\x1b[57443;3u", &used);
    CHECK(e.kind == TTY_SHIFT && e.flags & PKEY_ALT);
    e = parse(&t, "\x1b[57443;1:3u", &used);
    CHECK(e.kind == TTY_SHIFT && !(e.flags & PKEY_ALT));
    e = parse(&t, "\x1b[97;5u", &used); /* Ctrl+A */
    CHECK(e.kind == TTY_KEY && e.key == 0x1E01 && t.kitty);
    e = parse(&t, "\x1b[97:65;2u", &used); /* Shift+A: its shifted key */
    CHECK(e.kind == TTY_KEY && e.key == 0x1E41);
    e = parse(&t, "\x1b[97;1:3u", &used); /* a key let go: none */
    CHECK(e.kind == TTY_NONE && used == 9);
}

TEST(tty_sgr_mouse_reports_its_cell_and_buttons)
{
    TtyIn t = {};

    uint32_t used;
    TtyEvent e = parse(&t, "\x1b[<0;11;6M", &used);
    CHECK(e.kind == TTY_MOUSE && e.row == 5 && e.col == 10 && e.buttons == 1);
    e = parse(&t, "\x1b[<32;12;6M", &used); /* a drag */
    CHECK(e.col == 11 && e.buttons == 1);
    e = parse(&t, "\x1b[<0;12;6m", &used);
    CHECK(e.kind == TTY_MOUSE && e.buttons == 0);
}

TEST(tty_a_sequence_cut_short_waits_for_more)
{
    TtyIn t = {};

    TtyEvent e;
    CHECK(!tty_parse(&t, (const uint8_t *)"\x1b[1;5", 5, false, &e));
    CHECK(!tty_parse(&t, (const uint8_t *)"\x1b", 1, false, &e));
}
