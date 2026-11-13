/* QBasic's system routines (src/vm/sys.asm): the clock, INKEY$, SLEEP, COMMAND$, the environment,
   SHELL, SETMEM, ERDEV, RANDOMIZE. */
#include "vm/syst.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/strg.h"
#include "num/dubl.h"
#include "num/fmth.h"
#include "num/rndm.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "plat/psys.h"
#include "vm/evnt.h"

enum {
    TAIL_SIZE = 128,         /* a DOS command tail */
    SHELL_MAX = 122,         /* SHELL's command, after " /C " and before the tail's CR */
    ENV_VALUE_MAX = 255,     /* ENVIRON$'s chars */
    DATE_PART_MAX = 9999,    /* a DATE$ or TIME$ part */
    EXTENDED_KEY = 0xE0,     /* an enhanced keyboard's prefix for its own keys */
};

/* A DATE$ or TIME$ part not given. */
static constexpr uint16_t NO_PART = UINT16_MAX;

static const char COMSPEC[] = "COMSPEC";

Syst syst;

static uint32_t skip_spaces(const char *const s, uint32_t i, const uint32_t n)
{
    while (i < n && s[i] == ' ')
        i++;
    return i;
}

void syst_init(void)
{
    psys_environ(syst.env, ENVSZ - 1);
    syst.env[ENVSZ - 2] = syst.env[ENVSZ - 1] = 0;
    char tail[CMDSZ];
    const uint32_t n = psys_cmdline(tail, sizeof tail);
    uint32_t i = skip_spaces(tail, 0, n);
    while (i < n && (uint8_t)tail[i] > ' ') /* past the file's name */
        i++;
    i = skip_spaces(tail, i, n);
    uint32_t len = 0;
    for (; i < n && tail[i] != '\r'; i++)
        syst.cmd[len++] = (char)upcase((uint8_t)tail[i]);
    while (len && (uint8_t)syst.cmd[len - 1] <= ' ') /* no trailing blanks */
        len--;
    syst.cmdlen = (uint16_t)len;
}

void syst_ret(Vm *const vm, const uint32_t len) { exec_pusha(vm, strg_buftmp((uint16_t)len)); }

/* ---- the clock ------------------------------------------------------------------------------ */

/* v as two digits at p; past them. */
static uint8_t *two_digits(uint8_t *const p, const uint8_t v)
{
    p[0] = '0' + v / 10;
    p[1] = '0' + v % 10;
    return p + 2;
}

/* TIMER ( -- f ): the seconds since midnight, to 1/100. */
void syst_timer(Vm *const vm)
{
    PclkTime t;
    pclk_time(&t);
    const int32_t hundredths = ((t.hour * 60 + t.min) * 60 + t.sec) * 100 + t.hund;
    exec_pusha(vm, fmth_div(fmth_from_i32(hundredths), SNGL_HUNDRED));
}

/* DATE$ ( -- s ): mm-dd-yyyy. */
void syst_date(Vm *const vm)
{
    PclkDate d;
    pclk_date(&d);
    uint8_t *p = cons.sbuf;
    p = two_digits(p, d.month);
    *p++ = '-';
    p = two_digits(p, d.day);
    *p++ = '-';
    p = two_digits(p, d.year / 100 % 100);
    p = two_digits(p, d.year % 100);
    syst_ret(vm, (uint32_t)(p - cons.sbuf));
}

/* TIME$ ( -- s ): hh:mm:ss. */
void syst_time(Vm *const vm)
{
    PclkTime t;
    pclk_time(&t);
    uint8_t *p = cons.sbuf;
    p = two_digits(p, t.hour);
    *p++ = ':';
    p = two_digits(p, t.min);
    *p++ = ':';
    p = two_digits(p, t.sec);
    syst_ret(vm, (uint32_t)(p - cons.sbuf));
}

/* The string on top as up to 3 numbers apart by sep1 or sep2, NO_PART none; how many. Illegal
   function call: another char, a number past DATE_PART_MAX, an empty part. */
static uint32_t parts(Vm *const vm, const uint8_t sep1, const uint8_t sep2, uint16_t part[static 3])
{
    const Desc s = strg_get(vm->sp->a);
    for (uint32_t i = 0; i < 3; i++)
        part[i] = NO_PART;
    uint32_t k = 0;
    uint32_t digits = 0;
    for (uint16_t i = 0; i < s.len; i++) {
        const uint8_t c = rd8(s.ptr + i);
        if (c == sep1 || c == sep2) {
            if (!digits || ++k >= 3)
                repl_error(E_IQ);
            digits = 0;
            continue;
        }
        const uint8_t digit = c - '0';
        if (digit > 9)
            repl_error(E_IQ);
        const uint32_t v = (part[k] == NO_PART ? 0 : part[k]) * 10u + digit;
        if (v > DATE_PART_MAX)
            repl_error(E_IQ);
        part[k] = (uint16_t)v;
        digits++;
    }
    if (!digits)
        repl_error(E_IQ);
    return k + 1;
}

/* DATE$ = s ( s -- ): mm-dd-yy or mm-dd-yyyy, - or /. A two-digit year from 1980. */
void syst_sdate(Vm *const vm)
{
    uint16_t part[3];
    if (parts(vm, '-', '/', part) != 3)
        repl_error(E_IQ);
    const uint16_t y = part[2];
    const uint16_t year = y >= 100 ? y : y + (y < 80 ? 2000 : 1900);
    const PclkDate d = {.year = year, .month = (uint8_t)part[0], .day = (uint8_t)part[1]};
    if (!pclk_set_date(&d))
        repl_error(E_IQ);
    vm->sp++;
}

/* TIME$ = s ( s -- ): hh[:mm[:ss]]. */
void syst_stime(Vm *const vm)
{
    uint16_t part[3];
    parts(vm, ':', ':', part);
    for (uint32_t i = 1; i < 3; i++)
        if (part[i] == NO_PART)
            part[i] = 0;
    const PclkTime t = {.hour = (uint8_t)part[0], .min = (uint8_t)part[1], .sec = (uint8_t)part[2]};
    if (!pclk_set_time(&t))
        repl_error(E_IQ);
    vm->sp++;
}

/* SLEEP ( n -- ): n seconds or a key, which stays; 0 a key. An event or the STOP key ends it. */
void syst_sleep(Vm *const vm)
{
    cons_flush();
    const int32_t ticks = exec_popi(vm) * 182 / 10; /* 18.2 a second */
    const uint32_t start = pclk_ticks();
    for (;;) {
        if (vm->stop & SF_TICK)
            evnt_poll(vm);
        if (vm->stop || pkey_ready())
            return;
        if (ticks && pclk_ticks() - start >= (uint32_t)ticks)
            return;
    }
}

/* ---- the keyboard and the command line ------------------------------------------------------ */

/* INKEY$ ( -- s ): a key waiting, else empty; an extended key is CHR$(0) and its scan code. */
void syst_inkey(Vm *const vm)
{
    cons_flush();
    if (!pkey_ready()) {
        syst_ret(vm, 0);
        return;
    }
    const PkeyKey k = pkey_read();
    if (!k.ch || (k.ch == EXTENDED_KEY && k.scan)) {
        cons.sbuf[0] = 0;
        cons.sbuf[1] = k.scan;
        syst_ret(vm, 2);
        return;
    }
    cons.sbuf[0] = k.ch;
    syst_ret(vm, 1);
}

/* COMMAND$ ( -- s ). */
void syst_cmd(Vm *const vm)
{
    memcpy(cons.sbuf, syst.cmd, syst.cmdlen);
    syst_ret(vm, syst.cmdlen);
}

/* ---- the environment ------------------------------------------------------------------------ */

/* The entry after e. */
static char *env_next(char *const e) { return e + strlen(e) + 1; }

/* The entry named by the len chars at name, case not told; null none. */
static char *env_find(const uint8_t *const name, const uint32_t len)
{
    for (char *e = syst.env; *e; e = env_next(e)) {
        uint32_t i = 0;
        while (i < len && upcase((uint8_t)e[i]) == upcase(name[i]))
            i++;
        if (i == len && e[i] == '=')
            return e;
    }
    return nullptr;
}

/* The last 0, after the entries' 0s. */
static char *env_end(void)
{
    char *e = syst.env;
    while (*e)
        e = env_next(e);
    return e;
}

/* SHELL ( s -- ): COMSPEC /C s; SHELL alone ( -- ): COMSPEC. Illegal function call: it did not
   run. */
static void shell(const Desc s)
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
    const char *const e = env_find((const uint8_t *)COMSPEC, strlen(COMSPEC));
    const char *const path = e ? e + strlen(COMSPEC) + 1 : "COMMAND.COM";
    cons_flush();
    if (!psys_exec(path, tail, len, syst.env, (uint32_t)(env_end() - syst.env) + 1))
        repl_error(E_IQ);
}
void syst_shell(Vm *const vm) { shell(strg_get(exec_popa(vm))); }
void syst_shell0(Vm *const) { shell((Desc){}); }

/* The 0-ended text at t as a string, ENV_VALUE_MAX chars at most. */
static void env_ret(Vm *const vm, const char *const t)
{
    uint32_t n = 0;
    for (; t[n] && n < ENV_VALUE_MAX; n++)
        cons.sbuf[n] = (uint8_t)t[n];
    syst_ret(vm, n);
}

/* ENVIRON$ ( x by_name -- s ): x a name, its value; else x the entry, from 1, whole. Illegal
   function call: an entry under 1. */
void syst_envs(Vm *const vm)
{
    const bool by_name = exec_pop(vm).u;
    if (by_name) {
        const Desc s = strg_get(exec_popa(vm));
        const char *const e = s.len ? env_find(at(s.ptr), s.len) : nullptr;
        env_ret(vm, e ? e + s.len + 1 : "");
        return;
    }
    const int32_t n = exec_popi(vm);
    if (n < 1)
        repl_error(E_IQ);
    char *e = syst.env;
    for (int32_t i = 1; i < n && *e; i++)
        e = env_next(e);
    env_ret(vm, e);
}

/* ENVIRON s ( s -- ): NAME=value set, replaced, or with no value, removed. Illegal function
   call: no '=' or no name. Out of memory: no room. */
void syst_envset(Vm *const vm)
{
    const Desc s = strg_get(exec_popa(vm));
    const uint8_t *const t = at(s.ptr);
    uint32_t name = 0;
    while (name < s.len && t[name] != '=')
        name++;
    if (name == s.len || !name)
        repl_error(E_IQ);
    char *const old = env_find(t, name);
    if (old) {
        char *const next = env_next(old);
        memmove(old, next, (size_t)(env_end() - next) + 1);
    }
    if (name + 1 >= s.len) /* no value: removed */
        return;
    char *const e = env_end();
    if (e + s.len + 2 > syst.env + ENVSZ)
        repl_error(E_OM);
    memcpy(e, t, s.len);
    e[s.len] = e[s.len + 1] = 0;
}

/* ---- the rest ------------------------------------------------------------------------------- */

/* SETMEM ( n -- n ): the free bytes; the heap is fixed. */
void syst_setmem(Vm *const vm)
{
    vm->sp++;
    strg_gc();
    exec_pushi(vm, (int32_t)(strg.low - arry.end));
}

/* ERDEV, ERDEV$: no device error is kept. */
void syst_erdev(Vm *const vm) { exec_pushi(vm, 0); }
void syst_erdevs(Vm *const vm) { syst_ret(vm, 0); }

/* RANDOMIZE alone ( -- d ): the seed asked for. */
void syst_rndq(Vm *const vm)
{
    for (;;) {
        cons_puts("Random-number seed (-32768 to 32767)? ");
        cons_read();
        const uint8_t *p = cons.inbuf;
        const double d = dubl_in(&p, nullptr);
        if (!*p) {
            exec_pushd(vm, d);
            return;
        }
        cons_puts("Redo from start\r"); /* QBasic's; NL is 13 */
    }
}

/* RANDOMIZE ( d -- ): the DOUBLE's two high words, xor-ed, the seed's middle word (QBasic's
   B$RNZP). */
void syst_rndz(Vm *const vm)
{
    const uint64_t r = exec_pop(vm).raw;
    const uint32_t w = (uint16_t)(r >> 32 ^ r >> 48);
    rndm.seed = (rndm.seed & 0xFF0000FF) | w << 8;
}
