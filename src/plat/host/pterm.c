/* The host's terminal as the keyboard, and a window's keys.
 *
 * # How it works
 * Only when host_term_open is called, by the interactive binary (app/term.c), and standard input is
 * a terminal; else the keys come from the tests' queue (pkey.c). The terminal is non-canonical
 * without echo, SIGINT kept as the STOP key (psys.c). A key's escape sequence becomes its BIOS scan
 * code (ttyin.c); Ctrl+D, the terminal's end of input, exits. A window's keys come down a pipe
 * (host_term_feed), already BIOS keys, its shift flags beside them; its pipe closed, the window is.
 */
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "plat/host/host.h"

enum { ESC_WAIT_MS = 30, CTRL_D = 4, BROKEN = -2, INCOMPLETE = -3 };

static struct {
    bool on;
    struct termios saved, raw;
    int feed;        /* the window's keys' pipe, -1 none */
    uint8_t shift;   /* the shift flags */
    TtyIn in;        /* the terminal's input's state */
    uint8_t buf[64]; /* its bytes read, n of them, not yet taken */
    uint32_t n;
    int32_t pending; /* a key read ahead (host_term_ready), -1 none */
} term = {.feed = -1, .pending = -1};

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

void host_term_feed(const int fd) { term.feed = fd; }
void host_term_set_shift(const uint8_t flags)
{
    __atomic_store_n(&term.shift, flags, __ATOMIC_RELAXED);
}

bool host_term_on(void) { return term.on || term.feed >= 0; }
bool host_term_window(void) { return term.feed >= 0; }
uint8_t host_term_shift(void) { return __atomic_load_n(&term.shift, __ATOMIC_RELAXED); }

enum { TTY, FEED };

/* Which of the terminal and the pipe has input, waited for at most ms (-1 for ever); -1 neither,
   BROKEN on SIGINT. Other signals (the runtime's) interrupt the wait too: it goes on. */
static int32_t wait_in(const int32_t ms)
{
    struct pollfd p[] = {
        {.fd = term.on ? STDIN_FILENO : -1, .events = POLLIN},
        {.fd = term.feed, .events = POLLIN},
    };

    int r;

    while ((r = poll(p, 2, ms)) < 0 && errno == EINTR)
        if (host_stop_taken())
            return BROKEN;

    if (r <= 0)
        return -1;

    return p[FEED].revents ? FEED : TTY;
}

/* The terminal's bytes to term.buf, waited for at most ms first; false at its end. */
static bool fill(const int32_t ms)
{
    struct pollfd p = {.fd = STDIN_FILENO, .events = POLLIN};

    if (term.n == sizeof term.buf || poll(&p, 1, ms) <= 0)
        return true;

    const ssize_t r = read(STDIN_FILENO, term.buf + term.n, sizeof term.buf - term.n);

    if (r <= 0)
        return false;

    term.n += (uint32_t)r;
    return true;
}

/* The first event of term.buf taken (final: no more bytes are coming): the mouse and the shift
   flags set; a key returned, else -1; INCOMPLETE, the bytes may go on. */
static int32_t event(const bool final)
{
    TtyEvent e;
    const uint32_t used = tty_parse(&term.in, term.buf, term.n, final, &e);

    if (!used)
        return INCOMPLETE;

    __builtin_memmove(term.buf, term.buf + used, term.n - used);
    term.n -= used;
    host_term_set_shift(e.flags);

    if (e.kind == TTY_MOUSE)
        host_mouse_set(e.row, e.col, (uint16_t)(e.col * 8), (uint16_t)(e.row * 8), e.buttons);

    return e.kind == TTY_KEY ? e.key : -1;
}

/* One event from the terminal, read for at most ms: its key, else -1. One at a time, so the shift
   flags between two are seen (an Alt pressed, then let go). Ctrl+D, or the terminal's end, exits. */
static int32_t tty_step(const int32_t ms)
{
    if (!term.n && !fill(ms))
        exit(0);

    if (!term.n)
        return -1;

    if (term.buf[0] == CTRL_D)
        exit(0);

    int32_t k;

    while ((k = event(false)) == INCOMPLETE) {
        const uint32_t had = term.n;

        if (!fill(ESC_WAIT_MS))
            exit(0);

        if (term.n == had) /* no more came: an ESC alone is Esc */
            return event(true);
    }

    return k;
}

bool host_term_ready(void)
{
    if (term.pending >= 0)
        return true;

    struct pollfd p = {.fd = term.feed, .events = POLLIN};

    if (term.feed >= 0 && poll(&p, 1, 0) > 0)
        return true;

    if (term.on)
        term.pending = tty_step(0);

    return term.pending >= 0;
}

uint16_t host_term_key(void)
{
    for (;;) {
        if (term.pending >= 0) {
            const uint16_t k = (uint16_t)term.pending;
            term.pending = -1;
            return k;
        }

        const int32_t from = term.n ? TTY : wait_in(-1);

        if (from == BROKEN)
            return 0; /* Ctrl+Break */

        if (from == FEED) {
            uint16_t key;

            if (read(term.feed, &key, sizeof key) != sizeof key)
                exit(0);

            return key;
        }

        if (from == TTY)
            term.pending = tty_step(0);
    }
}
