/* A key on a US keyboard as the BIOS's (INT 16h AH=10h): the window's keys (plat/sdl) and the
 * terminal's (ttyin.c). */
#include "plat/host/host.h"
#include "plat/pkey.h"

static const char unshifted[] = "`1234567890-=[]\\;',./";
static const char shifted[] = "~!@#$%^&*()_+{}|:\"<>?";

/* The scan codes of the typed keys, by the character they type unshifted. */
static uint8_t typed_scan(const uint32_t key)
{
    static const uint8_t letters[26] = {
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
        0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
    };

    static const uint8_t others[] = {0x29, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                     0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x1A,
                                     0x1B, 0x2B, 0x27, 0x28, 0x33, 0x34, 0x35};

    if (key >= 'a' && key <= 'z')
        return letters[key - 'a'];

    if (key == ' ')
        return 0x39;

    for (uint32_t i = 0; unshifted[i]; i++)
        if (unshifted[i] == (char)key)
            return others[i];

    return 0;
}

uint32_t host_key_of(const uint8_t ch, bool *const shift)
{
    *shift = false;

    if (ch >= 'A' && ch <= 'Z') {
        *shift = true;
        return ch | 0x20;
    }

    for (uint32_t i = 0; shifted[i]; i++)
        if (shifted[i] == (char)ch) {
            *shift = true;
            return (uint8_t)unshifted[i];
        }

    return ch;
}

static uint8_t shift_of(const uint32_t key)
{
    if (key >= 'a' && key <= 'z')
        return (uint8_t)(key & ~0x20u);

    for (uint32_t i = 0; unshifted[i]; i++)
        if (unshifted[i] == (char)key)
            return (uint8_t)shifted[i];

    return (uint8_t)key;
}

int32_t host_bios_key(const uint32_t key, const uint32_t typed, const uint8_t flags)
{
    /* the grey keys' scan codes: normal, with Ctrl, with Alt; their character E0h (0 with Alt) */
    static const uint8_t grey[][3] = {
        [HK_INSERT - HK_INSERT] = {0x52, 0x92, 0xA2},
        [HK_DELETE - HK_INSERT] = {0x53, 0x93, 0xA3},
        [HK_HOME - HK_INSERT] = {0x47, 0x77, 0x97},
        [HK_END - HK_INSERT] = {0x4F, 0x75, 0x9F},
        [HK_PGUP - HK_INSERT] = {0x49, 0x84, 0x99},
        [HK_PGDN - HK_INSERT] = {0x51, 0x76, 0xA1},
        [HK_UP - HK_INSERT] = {0x48, 0x8D, 0x98},
        [HK_DOWN - HK_INSERT] = {0x50, 0x91, 0xA0},
        [HK_LEFT - HK_INSERT] = {0x4B, 0x73, 0x9B},
        [HK_RIGHT - HK_INSERT] = {0x4D, 0x74, 0x9D},
    };

    const bool alt = flags & PKEY_ALT, ctrl = flags & PKEY_CTRL, shift = flags & PKEY_SHIFT;

    if (key >= HK_F1 && key < HK_F1 + 10) {
        const uint32_t base = alt ? 0x68 : ctrl ? 0x5E : shift ? 0x54 : 0x3B;
        return (int32_t)((base + key - HK_F1) << 8);
    }

    if (key == HK_F1 + 10 || key == HK_F1 + 11) {
        const uint32_t base = alt ? 0x8B : ctrl ? 0x89 : shift ? 0x87 : 0x85;
        return (int32_t)((base + key - HK_F1 - 10) << 8);
    }

    if (key >= HK_INSERT && key <= HK_RIGHT) {
        const uint8_t *const g = grey[key - HK_INSERT];
        return alt ? g[2] << 8 : (ctrl ? g[1] : g[0]) << 8 | 0xE0;
    }

    switch (key) {
    case HK_ENTER:
        return alt ? 0xA600 : 0x1C00 | (ctrl ? 0x0A : 0x0D);

    case HK_ESC:
        return alt ? 0x0100 : 0x011B;

    case HK_BACKSPACE:
        return alt ? 0x0E00 : 0x0E00 | (ctrl ? 0x7F : 0x08);

    case HK_TAB:
        return alt ? 0xA500 : ctrl ? 0x9400 : shift ? 0x0F00 : 0x0F09;

    default:
        break;
    }

    const uint8_t scan = typed_scan(key);

    if (!scan)
        return -1;

    if (alt) {
        const uint8_t digit = key >= '1' && key <= '9' ? (uint8_t)(0x78 + key - '1')
                              : key == '0'             ? 0x81
                              : key == '-'             ? 0x82
                              : key == '='             ? 0x83
                                                       : 0;
        return (digit ? digit : scan) << 8;
    }

    if (ctrl) {
        if (key >= 'a' && key <= 'z')
            return scan << 8 | (int32_t)(key - 'a' + 1);

        switch (key) {
        case '[':
            return scan << 8 | 0x1B;

        case '\\':
            return scan << 8 | 0x1C;

        case ']':
            return scan << 8 | 0x1D;

        case ' ':
            return scan << 8 | 0x20;

        default:
            return -1;
        }
    }

    uint32_t ch = typed ? typed : shift ? shift_of(key) : key;

    if (!typed && flags & PKEY_CAPS && key >= 'a' && key <= 'z')
        ch ^= 0x20;

    return ch >= ' ' && ch < 0x7F ? scan << 8 | (int32_t)ch : -1;
}
