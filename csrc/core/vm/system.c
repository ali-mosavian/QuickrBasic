/* QBasic's system routines: the clock, INKEY$, SLEEP, COMMAND$, the environment, SHELL, SETMEM,
 * ERDEV and RANDOMIZE.
 *
 * # How it works
 * syst_init copies the environment and the command line from the platform. ENVIRON changes the
 * copy, as QBasic's own does; SHELL's child gets it. The clock and keyboard come from the platform
 * (plat/pclk, plat/pkey). A string result goes through cons.sbuf and syst_ret.
 *
 * # Example
 *   ENVIRON "A=1" : PRINT ENVIRON$("A")      1
 *
 * # asm structure (src/vm/sys.asm)
 * | asm | here | |
 * |---|---|---|
 * | sys_init | syst_init | MIRROR |
 * | sx_sret | syst_ret | MIRROR |
 * | g_env, ENVSZ | Syst.env, ENVSZ | MIRROR |
 * | g_cmd, g_cmdn | Syst.cmd, Syst.cmdlen | MIRROR |
 * | sc_timer, sc_date, sc_time, sc_sdate, sc_stime, sc_inkey | syst_timer, syst_date, syst_time, syst_sdate, syst_stime, syst_inkey | MIRROR |
 * | sc_shell, sc_shell0, sc_cmd, sc_envs, sc_envset, sc_setmem | syst_shell, syst_shell0, syst_cmd, syst_envs, syst_envset, syst_setmem | MIRROR |
 * | sc_erdev, sc_erdevs, sc_sleep, sc_rndq, sc_rndz | syst_erdev, syst_erdevs, syst_sleep, syst_rndq, syst_rndz | MIRROR |
 * | sx_d2, sx_parts, sx_find | two_digits, env_find, static | MIRROR |
 * | g_part, g_shtl, g_shcs, g_shcm, g_k100, u_seed, u_redo | static | MIRROR |
 */
#include "vm/system.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/array.h"
#include "mem/string.h"
#include "num/double.h"
#include "num/fpmath.h"
#include "num/random.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "vm/event.h"

enum {
    TAIL_SIZE = 128,      /* a DOS command tail */
    SHELL_MAX = 122,      /* SHELL's command, after " /C " and before the tail's CR */
    ENV_VALUE_MAX = 255,  /* ENVIRON$'s chars */
    DATE_PART_MAX = 9999, /* a DATE$ or TIME$ part */
    EXTENDED_KEY = 0xE0,  /* an enhanced keyboard's prefix for its own keys */
};

/* A DATE$ or TIME$ part not given. */
static constexpr uint16_t NO_PART = UINT16_MAX;

static const char COMSPEC[] = "COMSPEC";

static uint32_t skip_spaces(const char *const s, uint32_t i, const uint32_t n)
{
    while (i < n && s[i] == ' ')
        i++;

    return i;
}

void syst_init(Syst *const sys)
{
    psys_environ(sys->env, ENVSZ - 1);
    sys->env[ENVSZ - 2] = sys->env[ENVSZ - 1] = 0;
    char tail[CMDSZ];
    const uint32_t n = psys_cmdline(tail, sizeof tail);
    uint32_t i = skip_spaces(tail, 0, n);

    while (i < n && (uint8_t)tail[i] > ' ') /* past the file's name */
        i++;

    i = skip_spaces(tail, i, n);
    uint32_t len = 0;

    for (; i < n && tail[i] != '\r'; i++)
        sys->cmd[len++] = (char)upcase((uint8_t)tail[i]);
    while (len && (uint8_t)sys->cmd[len - 1] <= ' ') /* no trailing blanks */
        len--;

    sys->cmdlen = (uint16_t)len;
}

void syst_ret(Session *const S, const uint32_t len)
{
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, (uint16_t)len));
}

/* ---- the clock ------------------------------------------------------------------------------ */

/* v as two digits at p; past them. */
static uint8_t *two_digits(uint8_t *const p, const uint8_t v)
{
    p[0] = '0' + v / 10;
    p[1] = '0' + v % 10;
    return p + 2;
}

/* TIMER ( -- f ): the seconds since midnight, the runtime's. */
void syst_timer(Session *const S) { exec_push(&S->vm, (Cell){.f = *B_TIMR()}); }

/* DATE$ ( -- s ): mm-dd-yyyy. */
void syst_date(Session *const S)
{
    PclkDate d;
    pclk_date(&d);
    uint8_t *p = S->cons.sbuf;
    p = two_digits(p, d.month);
    *p++ = '-';
    p = two_digits(p, d.day);
    *p++ = '-';
    p = two_digits(p, d.year / 100 % 100);
    p = two_digits(p, d.year % 100);
    syst_ret(S, (uint32_t)(p - S->cons.sbuf));
}

/* TIME$ ( -- s ): hh:mm:ss. */
void syst_time(Session *const S)
{
    PclkTime t;
    pclk_time(&t);
    uint8_t *p = S->cons.sbuf;
    p = two_digits(p, t.hour);
    *p++ = ':';
    p = two_digits(p, t.min);
    *p++ = ':';
    p = two_digits(p, t.sec);
    syst_ret(S, (uint32_t)(p - S->cons.sbuf));
}

/* The string on top as up to 3 numbers apart by sep1 or sep2, NO_PART none; how many. Illegal
   function call: another char, a number past DATE_PART_MAX, an empty part. */
static uint32_t parts(
    Session *const S,
    const uint8_t sep1,
    const uint8_t sep2,
    uint16_t part[static 3]
)
{
    const Desc s = strg_get(&S->strg, S->vm.sp->a);

    for (uint32_t i = 0; i < 3; i++)
        part[i] = NO_PART;

    uint32_t k = 0;
    uint32_t digits = 0;

    for (uint16_t i = 0; i < s.len; i++) {
        const uint8_t c = rd8(s.ptr + i);

        if (c == sep1 || c == sep2) {
            if (!digits || ++k >= 3)
                repl_error(S, E_IQ);

            digits = 0;
            continue;
        }

        const uint8_t digit = c - '0';

        if (digit > 9)
            repl_error(S, E_IQ);

        const uint32_t v = (part[k] == NO_PART ? 0 : part[k]) * 10u + digit;

        if (v > DATE_PART_MAX)
            repl_error(S, E_IQ);

        part[k] = (uint16_t)v;
        digits++;
    }

    if (!digits)
        repl_error(S, E_IQ);

    return k + 1;
}

/* DATE$ = s ( s -- ): mm-dd-yy or mm-dd-yyyy, - or /. A two-digit year from 1980. */
void syst_sdate(Session *const S)
{
    uint16_t part[3];

    if (parts(S, '-', '/', part) != 3)
        repl_error(S, E_IQ);

    const uint16_t y = part[2];
    const uint16_t year = y >= 100 ? y : y + (y < 80 ? 2000 : 1900);
    const PclkDate d = {.year = year, .month = (uint8_t)part[0], .day = (uint8_t)part[1]};

    if (!pclk_set_date(&d))
        repl_error(S, E_IQ);

    S->vm.sp++;
}

/* TIME$ = s ( s -- ): hh[:mm[:ss]]. */
void syst_stime(Session *const S)
{
    uint16_t part[3];
    parts(S, ':', ':', part);

    for (uint32_t i = 1; i < 3; i++)
        if (part[i] == NO_PART)
            part[i] = 0;

    const PclkTime t = {.hour = (uint8_t)part[0], .min = (uint8_t)part[1], .sec = (uint8_t)part[2]};

    if (!pclk_set_time(&t))
        repl_error(S, E_IQ);

    qbrt_clock_set(&t);
    S->vm.sp++;
}

/* SLEEP ( n -- ): n seconds or a key, which stays; 0 a key. An event or the STOP key ends it. */
void syst_sleep(Session *const S)
{
    cons_flush(&S->cons);
    const int32_t ticks = exec_popi(&S->vm) * 182 / 10; /* 18.2 a second */
    const uint32_t start = pclk_ticks();

    for (;;) {
        if (S->vm.stop & SF_TICK)
            evnt_poll(&S->evnt, &S->vm);
        if (S->vm.stop || pkey_ready())
            return;
        if (ticks && pclk_ticks() - start >= (uint32_t)ticks)
            return;
    }
}

/* ---- the keyboard and the command line ------------------------------------------------------ */

/* INKEY$ ( -- s ): a key waiting, else empty; an extended key is CHR$(0) and its scan code. */
void syst_inkey(Session *const S)
{
    cons_flush(&S->cons);

    if (!pkey_ready()) {
        syst_ret(S, 0);
        return;
    }

    const PkeyKey k = pkey_read();

    if (!k.ch || (k.ch == EXTENDED_KEY && k.scan)) {
        S->cons.sbuf[0] = 0;
        S->cons.sbuf[1] = k.scan;
        syst_ret(S, 2);
        return;
    }

    S->cons.sbuf[0] = k.ch;
    syst_ret(S, 1);
}

/* COMMAND$ ( -- s ). */
void syst_cmd(Session *const S)
{
    memcpy(S->cons.sbuf, S->syst.cmd, S->syst.cmdlen);
    syst_ret(S, S->syst.cmdlen);
}

/* ---- the environment ------------------------------------------------------------------------ */

/* The entry after e. */
static char *env_next(char *const e) { return e + strlen(e) + 1; }

/* The entry named by the len chars at name, case not told; null none. */
static char *env_find(Syst *const sys, const uint8_t *const name, const uint32_t len)
{
    for (char *e = sys->env; *e; e = env_next(e)) {
        uint32_t i = 0;

        while (i < len && upcase((uint8_t)e[i]) == upcase(name[i]))
            i++;
        if (i == len && e[i] == '=')
            return e;
    }

    return nullptr;
}

/* The last 0, after the entries' 0s. */
static char *env_end(Syst *const sys)
{
    char *e = sys->env;

    while (*e)
        e = env_next(e);

    return e;
}

/* SHELL ( s -- ): COMSPEC /C s; SHELL alone ( -- ): COMSPEC. Illegal function call: it did not
   run. */
static void shell(Session *const S, const Desc s)
{
    static const char run[] = " /C ";
    char tail[TAIL_SIZE];
    uint8_t len = 0;

    if (s.len) {
        const uint16_t n = s.len > SHELL_MAX ? SHELL_MAX : s.len;
        memcpy(tail, run, strlen(run));
        memcpy(tail + strlen(run), at(s.ptr), n);
        len = (uint8_t)(n + strlen(run));
    }

    const char *const e = env_find(&S->syst, (const uint8_t *)COMSPEC, strlen(COMSPEC));
    const char *const path = e ? e + strlen(COMSPEC) + 1 : "COMMAND.COM";
    cons_flush(&S->cons);

    if (!psys_exec(path, tail, len, S->syst.env, (uint32_t)(env_end(&S->syst) - S->syst.env) + 1))
        repl_error(S, E_IQ);
}
void syst_shell(Session *const S) { shell(S, strg_get(&S->strg, exec_popa(&S->vm))); }
void syst_shell0(Session *const S) { shell(S, (Desc){}); }

/* The 0-ended text at t as a string, ENV_VALUE_MAX chars at most. */
static void env_ret(Session *const S, const char *const t)
{
    uint32_t n = 0;

    for (; t[n] && n < ENV_VALUE_MAX; n++)
        S->cons.sbuf[n] = (uint8_t)t[n];

    syst_ret(S, n);
}

/* ENVIRON$ ( x by_name -- s ): x a name, its value; else x the entry, from 1, whole. Illegal
   function call: an entry under 1. */
void syst_envs(Session *const S)
{
    const bool by_name = exec_pop(&S->vm).u;

    if (by_name) {
        const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
        const char *const e = s.len ? env_find(&S->syst, at(s.ptr), s.len) : nullptr;
        env_ret(S, e ? e + s.len + 1 : "");
        return;
    }

    const int32_t n = exec_popi(&S->vm);

    if (n < 1)
        repl_error(S, E_IQ);

    char *e = S->syst.env;

    for (int32_t i = 1; i < n && *e; i++)
        e = env_next(e);

    env_ret(S, e);
}

/* ENVIRON s ( s -- ): NAME=value set, replaced, or with no value, removed. Illegal function
   call: no '=' or no name. Out of memory: no room. */
void syst_envset(Session *const S)
{
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    const uint8_t *const t = at(s.ptr);
    uint32_t name = 0;

    while (name < s.len && t[name] != '=')
        name++;
    if (name == s.len || !name)
        repl_error(S, E_IQ);

    char *const old = env_find(&S->syst, t, name);

    if (old) {
        char *const next = env_next(old);
        memmove(old, next, (size_t)(env_end(&S->syst) - next) + 1);
    }

    if (name + 1 >= s.len) /* no value: removed */
        return;

    char *const e = env_end(&S->syst);

    if (e + s.len + 2 > S->syst.env + ENVSZ)
        repl_error(S, E_OM);

    memcpy(e, t, s.len);
    e[s.len] = e[s.len + 1] = 0;
}

/* ---- the rest ------------------------------------------------------------------------------- */

/* SETMEM ( n -- n ): the free bytes; the heap is fixed. */
void syst_setmem(Session *const S)
{
    S->vm.sp++;
    strg_gc(&S->strg);
    exec_pushi(&S->vm, (int32_t)(S->allo.strings - S->allo.arrays));
}

/* ERDEV, ERDEV$: no device error is kept. */
void syst_erdev(Session *const S) { exec_pushi(&S->vm, 0); }
void syst_erdevs(Session *const S) { syst_ret(S, 0); }

/* RANDOMIZE alone ( -- d ): the seed asked for. */
void syst_rndq(Session *const S)
{
    for (;;) {
        cons_puts(&S->cons, "Random-number seed (-32768 to 32767)? ");
        fault_check(&S->fault, cons_read(&S->cons));
        const uint8_t *p = S->cons.inbuf;
        const double d = dubl_in(&S->fault, &p, nullptr);

        if (!*p) {
            exec_pushd(&S->vm, d);
            return;
        }

        cons_puts(&S->cons, "Redo from start\r"); /* QBasic's; NL is 13 */
    }
}

/* RANDOMIZE ( d -- ): the DOUBLE's two high words, xor-ed, the seed's middle word (QBasic's
   B$RNZP). */
void syst_rndz(Session *const S)
{
    const uint64_t r = exec_pop(&S->vm).raw;
    const uint32_t w = (uint16_t)(r >> 32 ^ r >> 48);
    S->rndm.seed = (S->rndm.seed & 0xFF0000FF) | w << 8;
}
