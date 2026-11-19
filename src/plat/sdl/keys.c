/* SDL's keys as the BIOS's: a key's place on a US keyboard (host_bios_key), the character the
 * layout types with it. */
#include "plat/host/host.h"
#include "plat/pkey.h"
#include "sdl.h"

/* The key at s by what it types unshifted on a US keyboard, or its HK_ name; 0 none. */
static uint32_t us_key(const SDL_Scancode s)
{
    static const struct {
        SDL_Scancode s;
        uint32_t key;
    } keys[] = {
        {SDL_SCANCODE_RETURN, HK_ENTER},  {SDL_SCANCODE_KP_ENTER, HK_ENTER},
        {SDL_SCANCODE_ESCAPE, HK_ESC},    {SDL_SCANCODE_BACKSPACE, HK_BACKSPACE},
        {SDL_SCANCODE_TAB, HK_TAB},       {SDL_SCANCODE_INSERT, HK_INSERT},
        {SDL_SCANCODE_DELETE, HK_DELETE}, {SDL_SCANCODE_HOME, HK_HOME},
        {SDL_SCANCODE_END, HK_END},       {SDL_SCANCODE_PAGEUP, HK_PGUP},
        {SDL_SCANCODE_PAGEDOWN, HK_PGDN}, {SDL_SCANCODE_UP, HK_UP},
        {SDL_SCANCODE_DOWN, HK_DOWN},     {SDL_SCANCODE_LEFT, HK_LEFT},
        {SDL_SCANCODE_RIGHT, HK_RIGHT},   {SDL_SCANCODE_GRAVE, '`'},
        {SDL_SCANCODE_MINUS, '-'},        {SDL_SCANCODE_EQUALS, '='},
        {SDL_SCANCODE_LEFTBRACKET, '['},  {SDL_SCANCODE_RIGHTBRACKET, ']'},
        {SDL_SCANCODE_BACKSLASH, '\\'},   {SDL_SCANCODE_SEMICOLON, ';'},
        {SDL_SCANCODE_APOSTROPHE, '\''},  {SDL_SCANCODE_COMMA, ','},
        {SDL_SCANCODE_PERIOD, '.'},       {SDL_SCANCODE_SLASH, '/'},
        {SDL_SCANCODE_SPACE, ' '},
    };

    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z)
        return 'a' + (uint32_t)(s - SDL_SCANCODE_A);

    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_9)
        return '1' + (uint32_t)(s - SDL_SCANCODE_1);

    if (s == SDL_SCANCODE_0)
        return '0';

    if (s >= SDL_SCANCODE_F1 && s <= SDL_SCANCODE_F12)
        return HK_F1 + (uint32_t)(s - SDL_SCANCODE_F1);

    for (uint32_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
        if (keys[i].s == s)
            return keys[i].key;

    return 0;
}

uint8_t sdl_shift(const SDL_Keymod mod)
{
    return (uint8_t)((mod & SDL_KMOD_RSHIFT ? PKEY_RSHIFT : 0) |
                     (mod & SDL_KMOD_LSHIFT ? PKEY_LSHIFT : 0) |
                     (mod & SDL_KMOD_CTRL ? PKEY_CTRL : 0) | (mod & SDL_KMOD_ALT ? PKEY_ALT : 0) |
                     (mod & SDL_KMOD_NUM ? PKEY_NUM : 0) | (mod & SDL_KMOD_CAPS ? PKEY_CAPS : 0));
}

uint8_t sdl_scan(const SDL_Scancode s)
{
    const uint32_t key = us_key(s);

    return key ? (uint8_t)(host_bios_key(key, 0, 0) >> 8 & 0x7F) : 0;
}

int32_t sdl_bios_key(const SDL_KeyboardEvent *const e)
{
    const uint32_t key = us_key(e->scancode);

    if (!key)
        return -1;

    /* the character the layout types, when it is one of CP437's ASCII half */
    const SDL_Keycode k =
        SDL_GetKeyFromScancode(e->scancode, e->mod & (SDL_KMOD_SHIFT | SDL_KMOD_CAPS), false);
    return host_bios_key(key, k >= ' ' && k < 0x7F ? k : 0, sdl_shift(e->mod));
}
