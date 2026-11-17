/* The host's terminal as the keyboard.
 *
 * # How it works
 * Only when host_term_open is called, by the interactive binary (app/term.c), and standard input is
 * a terminal; else the keys come from the tests' queue (pkey.c). The terminal is non-canonical
 * without echo, SIGINT kept as the STOP key (psys.c). A key's escape sequence becomes its BIOS scan
 * code; Ctrl+D, the terminal's end of input, exits.
 */
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "plat/host/host.h"

enum { ESC_WAIT_MS = 30, CTRL_D = 4, BROKEN = -2 };

static struct {
    bool on;
    struct termios saved, raw;
} term;

static void restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &term.saved); }

void host_term_open(void)
{
    if (!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &term.saved))
        return;

    term.raw = term.saved;
    term.raw.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    term.raw.c_iflag &= ~(tcflag_t)(ICRNL | IXON);
    term.raw.c_cc[VMIN] = 1;
    term.raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &term.raw))
        return;

    term.on = true;
    atexit(restore);
}

void host_term_pause(const bool yes)
{
    if (term.on)
        tcsetattr(STDIN_FILENO, TCSANOW, yes ? &term.saved : &term.raw);
}

bool host_term_on(void) { return term.on; }

/* The next byte, waited for at most ms (-1 for ever); -1 none, BROKEN on SIGINT. Other signals
   (the runtime's) interrupt the wait too: it goes on. */
static int32_t byte_in(const int32_t ms)
{
    struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};

    int r;
    uint8_t b;

    while ((r = poll(&p, 1, ms)) < 0 && errno == EINTR)
        if (host_stop_taken())
            return BROKEN;

    if (r <= 0 || read(STDIN_FILENO, &b, 1) != 1)
        return -1;

    return b;
}

bool host_term_ready(void)
{
    struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};

    return poll(&p, 1, 0) > 0;
}

/* An escape sequence's key past ESC: scan code << 8; ESC alone, 011Bh. */
static uint16_t escaped(void)
{
    static const struct {
        uint8_t n;
        uint16_t key;
    } tilde[] = {
        {1, 0x4700},
        {2, 0x5200},
        {3, 0x5300},
        {4, 0x4F00},
        {5, 0x4900},
        {6, 0x5100},
        {15, 0x3F00},
        {17, 0x4000},
        {18, 0x4100},
        {19, 0x4200},
        {20, 0x4300},
        {21, 0x4400},
    };

    const int32_t b = byte_in(ESC_WAIT_MS);

    if (b == 'O') { /* F1 .. F4 */
        const int32_t c = byte_in(ESC_WAIT_MS);
        return c >= 'P' && c <= 'S' ? (uint16_t)((0x3B + c - 'P') << 8) : 0x011B;
    }

    if (b != '[')
        return 0x011B;

    uint32_t n = 0;
    int32_t c;

    while ((c = byte_in(ESC_WAIT_MS)) >= '0' && c <= '9')
        n = n * 10 + (uint32_t)(c - '0');

    switch (c) {
    case 'A':
        return 0x4800;

    case 'B':
        return 0x5000;

    case 'C':
        return 0x4D00;

    case 'D':
        return 0x4B00;

    case 'H':
        return 0x4700;

    case 'F':
        return 0x4F00;

    case '~':
        for (uint32_t i = 0; i < sizeof tilde / sizeof tilde[0]; i++)
            if (tilde[i].n == n)
                return tilde[i].key;
    }

    return 0x011B;
}

uint16_t host_term_key(void)
{
    const int32_t b = byte_in(-1);

    switch (b) {
    case BROKEN:
        return 0; /* Ctrl+Break */
    case -1:
    case CTRL_D:
        exit(0);
    case 0x1B:
        return escaped();

    case '\r':
    case '\n':
        return 0x1C0D;

    case 0x7F:
    case 0x08:
        return 0x0E08;

    case '\t':
        return 0x0F09;

    default:
        return (uint16_t)b;
    }
}
