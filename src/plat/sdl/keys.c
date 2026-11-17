/* SDL's keys as the BIOS's (INT 16h AH=10h), the US layout's scan codes; the web's keys.mjs. */
#include "plat/pkey.h"
#include "sdl.h"

/* The typed keys' scan codes, by SDL's scancode (the US layout's place). */
static uint8_t typed_scan(const SDL_Scancode s)
{
    static const uint8_t letters[26] = {
        0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
        0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,
    };

    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z)
        return letters[s - SDL_SCANCODE_A];

    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0)
        return (uint8_t)(0x02 + s - SDL_SCANCODE_1);

    switch (s) {
    case SDL_SCANCODE_GRAVE:
        return 0x29;

    case SDL_SCANCODE_MINUS:
        return 0x0C;

    case SDL_SCANCODE_EQUALS:
        return 0x0D;

    case SDL_SCANCODE_LEFTBRACKET:
        return 0x1A;

    case SDL_SCANCODE_RIGHTBRACKET:
        return 0x1B;

    case SDL_SCANCODE_BACKSLASH:
        return 0x2B;

    case SDL_SCANCODE_SEMICOLON:
        return 0x27;

    case SDL_SCANCODE_APOSTROPHE:
        return 0x28;

    case SDL_SCANCODE_COMMA:
        return 0x33;

    case SDL_SCANCODE_PERIOD:
        return 0x34;

    case SDL_SCANCODE_SLASH:
        return 0x35;

    case SDL_SCANCODE_SPACE:
        return 0x39;

    default:
        return 0;
    }
}

/* The grey keys' scan codes: normal, with Ctrl, with Alt; their character E0h (0 with Alt). */
static bool grey(const SDL_Scancode s, uint8_t codes[3])
{
    static const struct {
        SDL_Scancode s;
        uint8_t codes[3];
    } keys[] = {
        {SDL_SCANCODE_HOME, {0x47, 0x77, 0x97}},
        {SDL_SCANCODE_UP, {0x48, 0x8D, 0x98}},
        {SDL_SCANCODE_PAGEUP, {0x49, 0x84, 0x99}},
        {SDL_SCANCODE_LEFT, {0x4B, 0x73, 0x9B}},
        {SDL_SCANCODE_RIGHT, {0x4D, 0x74, 0x9D}},
        {SDL_SCANCODE_END, {0x4F, 0x75, 0x9F}},
        {SDL_SCANCODE_DOWN, {0x50, 0x91, 0xA0}},
        {SDL_SCANCODE_PAGEDOWN, {0x51, 0x76, 0xA1}},
        {SDL_SCANCODE_INSERT, {0x52, 0x92, 0xA2}},
        {SDL_SCANCODE_DELETE, {0x53, 0x93, 0xA3}},
    };

    for (uint32_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (keys[i].s == s) {
            for (uint32_t j = 0; j < 3; j++)
                codes[j] = keys[i].codes[j];

            return true;
        }

    return false;
}

static uint8_t alt_digit(const SDL_Scancode s)
{
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0)
        return (uint8_t)(0x78 + s - SDL_SCANCODE_1);

    return s == SDL_SCANCODE_MINUS ? 0x82 : s == SDL_SCANCODE_EQUALS ? 0x83 : 0;
}

uint8_t sdl_shift(const SDL_Keymod mod)
{
    return (uint8_t)((mod & SDL_KMOD_RSHIFT ? PKEY_RSHIFT : 0) |
                     (mod & SDL_KMOD_LSHIFT ? PKEY_LSHIFT : 0) |
                     (mod & SDL_KMOD_CTRL ? PKEY_CTRL : 0) | (mod & SDL_KMOD_ALT ? PKEY_ALT : 0) |
                     (mod & SDL_KMOD_NUM ? PKEY_NUM : 0) | (mod & SDL_KMOD_CAPS ? PKEY_CAPS : 0));
}

int32_t sdl_bios_key(const SDL_KeyboardEvent *const e)
{
    const SDL_Scancode s = e->scancode;
    const bool alt = e->mod & SDL_KMOD_ALT, ctrl = e->mod & SDL_KMOD_CTRL,
               shift = e->mod & SDL_KMOD_SHIFT;

    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F10) {
        const uint32_t base = alt ? 0x68 : ctrl ? 0x5E : shift ? 0x54 : 0x3B;
        return (int32_t)((base + s - SDL_SCANCODE_F1) << 8);
    }

    if (s == SDL_SCANCODE_F11 || s == SDL_SCANCODE_F12) {
        const uint32_t base = alt ? 0x8B : ctrl ? 0x89 : shift ? 0x87 : 0x85;
        return (int32_t)((base + s - SDL_SCANCODE_F11) << 8);
    }

    uint8_t g[3];

    if (grey(s, g))
        return alt ? g[2] << 8 : (ctrl ? g[1] : g[0]) << 8 | 0xE0;

    switch (s) {
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        return alt ? 0xA600 : 0x1C00 | (ctrl ? 0x0A : 0x0D);

    case SDL_SCANCODE_ESCAPE:
        return alt ? 0x0100 : 0x011B;

    case SDL_SCANCODE_BACKSPACE:
        return alt ? 0x0E00 : 0x0E00 | (ctrl ? 0x7F : 0x08);

    case SDL_SCANCODE_TAB:
        return alt ? 0xA500 : ctrl ? 0x9400 : shift ? 0x0F00 : 0x0F09;

    default:
        break;
    }

    const uint8_t scan = typed_scan(s);

    if (!scan)
        return -1;

    if (alt)
        return (alt_digit(s) ? alt_digit(s) : scan) << 8;

    if (ctrl) {
        if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z)
            return scan << 8 | (s - SDL_SCANCODE_A + 1);

        switch (s) {
        case SDL_SCANCODE_LEFTBRACKET:
            return scan << 8 | 0x1B;

        case SDL_SCANCODE_BACKSLASH:
            return scan << 8 | 0x1C;

        case SDL_SCANCODE_RIGHTBRACKET:
            return scan << 8 | 0x1D;

        case SDL_SCANCODE_SPACE:
            return scan << 8 | 0x20;

        default:
            return -1;
        }
    }

    /* the character the layout types, when it is one of CP437's ASCII half */
    const SDL_Keycode k =
        SDL_GetKeyFromScancode(s, e->mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS), false);
    return k >= ' ' && k < 0x7F ? scan << 8 | (int32_t)k : -1;
}
