/* A terminal's input as keys, the mouse and the shift flags (host.h's tty_parse).
 *
 * # How it works
 * A byte is a key as the terminal sends it unmodified: Ctrl+letter its control code, ESC then a key
 * that key with Alt. ESC [ starts a CSI sequence: xterm's keys (an arrow, a ~ key) with their
 * modifiers; the kitty keyboard protocol's (u), which reports a modifier pressed or let go too, so
 * an Alt tapped alone opens the menus; SGR mouse reports (<). Alt is a shift flag only where the
 * terminal says when it is let go (kitty's): else it would stay down.
 */
#include "plat/host/host.h"
#include "plat/pkey.h"

enum { ESC = 0x1B, CSI_MAX = 40, FIELDS = 4, SUBS = 3 };

/* kitty's modifiers (its value less 1), its event types, its keys past Unicode's */
enum { K_SHIFT = 1, K_ALT = 2, K_CTRL = 4, K_SUPER = 8, K_CAPS = 64, K_NUM = 128 };
enum { K_PRESS = 1, K_REPEAT = 2, K_RELEASE = 3 };
enum : uint32_t {
    K_KP_0 = 57399,
    K_KP_9 = 57408,
    K_KP_ENTER = 57414,
    K_LSHIFT = 57441,
    K_LCTRL = 57442,
    K_LALT = 57443,
    K_RSHIFT = 57447,
    K_RCTRL = 57448,
    K_RALT = 57449,
    K_FIRST_MOD = 57441,
    K_LAST_MOD = 57452,
};

static uint8_t flags_of(const uint32_t m)
{
    return (uint8_t)((m & K_SHIFT ? PKEY_LSHIFT : 0) | (m & K_ALT ? PKEY_ALT : 0) |
                     (m & K_CTRL ? PKEY_CTRL : 0) | (m & K_CAPS ? PKEY_CAPS : 0) |
                     (m & K_NUM ? PKEY_NUM : 0));
}

static TtyKind key(TtyEvent *const e, const uint32_t k, const uint32_t typed, const uint8_t flags)
{
    const int32_t bios = host_bios_key(k, typed, flags);

    if (bios < 0)
        return e->kind = TTY_NONE;

    e->key = (uint16_t)bios;
    return e->kind = TTY_KEY;
}

/* A byte as the terminal sends a key: flags held besides (Alt after an ESC). */
static TtyKind byte(TtyIn *const t, TtyEvent *const e, const uint8_t c, const uint8_t flags)
{
    e->flags = t->kitty ? t->mods : flags & ~PKEY_ALT;

    switch (c) {
    case '\r':
    case '\n':
        return key(e, HK_ENTER, 0, flags);

    case '\t':
        return key(e, HK_TAB, 0, flags);

    case 0x7F:
    case 0x08:
        return key(e, HK_BACKSPACE, 0, flags);

    case ESC:
        return key(e, HK_ESC, 0, flags);

    case 0:
        return key(e, ' ', 0, flags | PKEY_CTRL);

    default:
        break;
    }

    if (c < 27)
        return key(e, (uint32_t)('a' + c - 1), 0, flags | PKEY_CTRL);

    if (c < ' ') /* Ctrl+\ ] ^ _ */
        return key(e, (uint32_t)"\\]^_"[c - 28], 0, flags | PKEY_CTRL);

    if (c >= 0x80) /* UTF-8's: no BIOS key */
        return e->kind = TTY_NONE;

    bool shift;
    const uint32_t k = host_key_of(c, &shift);
    return key(e, k, c, flags | (shift ? PKEY_LSHIFT : 0));
}

/* The named keys of xterm's CSI n ~. */
static uint32_t tilde(const uint32_t n)
{
    switch (n) {
    case 1:
    case 7:
        return HK_HOME;

    case 2:
        return HK_INSERT;

    case 3:
        return HK_DELETE;

    case 4:
    case 8:
        return HK_END;

    case 5:
        return HK_PGUP;

    case 6:
        return HK_PGDN;

    case 11:
    case 12:
    case 13:
    case 14:
        return HK_F1 + n - 11;

    case 15:
        return HK_F1 + 4;

    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
        return HK_F1 + 5 + n - 17;

    case 23:
    case 24:
        return HK_F1 + 10 + n - 23;

    default:
        return 0;
    }
}

/* The named keys of a CSI's final letter (xterm's, SS3's). */
static uint32_t lettered(const uint8_t f)
{
    switch (f) {
    case 'A':
        return HK_UP;

    case 'B':
        return HK_DOWN;

    case 'C':
        return HK_RIGHT;

    case 'D':
        return HK_LEFT;

    case 'H':
        return HK_HOME;

    case 'F':
        return HK_END;

    case 'P':
    case 'Q':
    case 'R':
    case 'S':
        return HK_F1 + f - 'P';

    case 'Z':
        return HK_TAB;

    default:
        return 0;
    }
}

static TtyKind mouse(
    TtyIn *const t,
    TtyEvent *const e,
    const uint32_t p[FIELDS][SUBS],
    const bool down
)
{
    const uint32_t b = p[0][0], which = b & 3;

    if (b & 64) /* the wheel */
        return e->kind = TTY_NONE;

    const uint8_t bit = which == 0 ? 1 : which == 1 ? 4 : which == 2 ? 2 : 0;

    if (!(b & 32)) /* a press or a release, not a move */
        t->buttons = down ? t->buttons | bit : t->buttons & ~bit;

    e->row = (uint8_t)(p[2][0] ? p[2][0] - 1 : 0);
    e->col = (uint8_t)(p[1][0] ? p[1][0] - 1 : 0);
    e->buttons = t->buttons;
    return e->kind = TTY_MOUSE;
}

/* kitty's CSI code[:shifted] ; mods[:event] u */
static TtyKind kitty(TtyIn *const t, TtyEvent *const e, const uint32_t p[FIELDS][SUBS])
{
    const uint32_t code = p[0][0], m = p[1][0] ? p[1][0] - 1 : 0,
                   event = p[1][1] ? p[1][1] : K_PRESS;
    uint8_t flags = flags_of(m);

    if (code >= K_FIRST_MOD && code <= K_LAST_MOD) {
        const uint8_t self = code == K_LSHIFT                     ? PKEY_LSHIFT
                             : code == K_RSHIFT                   ? PKEY_RSHIFT
                             : code == K_LCTRL || code == K_RCTRL ? PKEY_CTRL
                             : code == K_LALT || code == K_RALT   ? PKEY_ALT
                                                                  : 0;
        t->mods = event == K_RELEASE ? flags & ~self : flags | self;
        e->flags = t->mods;
        return e->kind = TTY_SHIFT;
    }

    t->mods = flags;
    e->flags = flags;

    if (event == K_RELEASE || m & K_SUPER) /* Cmd's keys are the terminal's */
        return e->kind = TTY_NONE;

    switch (code) {
    case '\r':
    case K_KP_ENTER:
        return key(e, HK_ENTER, 0, flags);

    case '\t':
        return key(e, HK_TAB, 0, flags);

    case 0x7F:
        return key(e, HK_BACKSPACE, 0, flags);

    case ESC:
        return key(e, HK_ESC, 0, flags);

    default:
        break;
    }

    if (code >= K_KP_0 && code <= K_KP_9)
        return key(e, '0' + code - K_KP_0, 0, flags & ~PKEY_SHIFT);

    if (code < ' ' || code >= 0x7F)
        return e->kind = TTY_NONE;

    const uint32_t typed = flags & PKEY_SHIFT && p[0][1] ? p[0][1] : 0;
    return key(e, code, typed < 0x7F ? typed : 0, flags);
}

/* ESC [ ...: the bytes it took; 0, it may go on. */
static uint32_t csi(
    TtyIn *const t,
    const uint8_t *const in,
    const uint32_t n,
    const bool final,
    TtyEvent *const e
)
{
    uint32_t p[FIELDS][SUBS] = {};

    uint32_t field = 0, sub = 0, i = 2;
    const uint8_t prefix = i < n && (in[i] == '<' || in[i] == '?' || in[i] == '>') ? in[i++] : 0;

    for (; i < n && i < CSI_MAX; i++) {
        const uint8_t c = in[i];

        if (c >= '0' && c <= '9') {
            if (field < FIELDS && sub < SUBS)
                p[field][sub] = p[field][sub] * 10 + (c - '0');
        } else if (c == ';') {
            field++;
            sub = 0;
        } else if (c == ':')
            sub++;
        else if (c >= 0x40 && c <= 0x7E)
            break;
        else { /* not a CSI's byte: dropped with what came before */
            e->kind = TTY_NONE;
            return i + 1;
        }
    }

    if (i >= n || i >= CSI_MAX) {
        if (!final && i < CSI_MAX)
            return 0;

        e->kind = TTY_NONE;
        return i < n ? i + 1 : n;
    }

    const uint8_t f = in[i];

    if (prefix == '<' && (f == 'M' || f == 'm'))
        mouse(t, e, p, f == 'M');
    else if (prefix)
        e->kind = TTY_NONE;
    else if (f == 'u') {
        t->kitty = true;
        kitty(t, e, p);
    } else {
        const uint32_t k = f == '~' ? tilde(p[0][0]) : lettered(f);
        const uint32_t m = p[1][0] ? p[1][0] - 1 : f == 'Z' ? K_SHIFT : 0;
        const uint8_t flags = flags_of(m);
        e->flags = t->kitty ? (t->mods = flags) : flags & ~PKEY_ALT;
        const uint8_t before = e->flags;

        if (!k || (t->kitty && p[1][1] == K_RELEASE))
            e->kind = TTY_NONE;
        else
            key(e, k, 0, flags);

        e->flags = before;
    }

    return i + 1;
}

uint32_t tty_parse(
    TtyIn *const t,
    const uint8_t *const in,
    const uint32_t n,
    const bool final,
    TtyEvent *const e
)
{
    *e = (TtyEvent){.kind = TTY_NONE, .flags = t->mods};

    if (!n)
        return 0;

    if (in[0] != ESC) {
        byte(t, e, in[0], 0);
        return 1;
    }

    if (n == 1) {
        if (!final)
            return 0;

        byte(t, e, ESC, 0);
        return 1;
    }

    if (in[1] == '[')
        return csi(t, in, n, final, e);

    if (in[1] == 'O') { /* SS3: F1 .. F4, an arrow */
        if (n < 3) {
            if (!final)
                return 0;

            byte(t, e, 'O', PKEY_ALT);
            return 2;
        }

        const uint32_t k = lettered(in[2]);
        e->flags = t->kitty ? t->mods : 0;

        if (k)
            key(e, k, 0, 0);

        return 3;
    }

    byte(t, e, in[1], PKEY_ALT); /* ESC then a key: that key with Alt */
    return 2;
}
