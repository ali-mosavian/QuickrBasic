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

Syst syst;

static inline uint8_t upcase(uint8_t c) { return c >= 'a' && c <= 'z' ? c - 0x20 : c; }

void syst_init(void)
{
    psys_environ(syst.env, ENVSZ - 1);
    syst.env[ENVSZ - 2] = syst.env[ENVSZ - 1] = 0;
    char tail[CMDSZ];
    uint32_t n = psys_cmdline(tail, sizeof tail);
    uint32_t i = 0;
    while (i < n && tail[i] == ' ')
        i++;
    while (i < n && (uint8_t)tail[i] > ' ') /* past the file's name */
        i++;
    while (i < n && tail[i] == ' ')
        i++;
    uint32_t len = 0;
    for (; i < n && tail[i] != '\r'; i++)
        syst.cmd[len++] = (char)upcase((uint8_t)tail[i]);
    while (len && (uint8_t)syst.cmd[len - 1] <= ' ') /* no trailing blanks */
        len--;
    syst.cmdlen = (uint16_t)len;
}

void syst_ret(uint32_t len) { exec_pusha(strg_buftmp((uint16_t)len)); }

/* AL as two digits at *p. */
static void two(uint8_t **p, uint8_t v)
{
    *(*p)++ = '0' + v / 10;
    *(*p)++ = '0' + v % 10;
}

/* TIMER ( -- f ): the seconds since midnight, to 1/100. */
void syst_timer(void)
{
    PclkTime t;
    pclk_time(&t);
    int32_t n = ((t.hour * 60 + t.min) * 60 + t.sec) * 100 + t.hund;
    exec_pusha(fmth_div(fmth_from_i32(n), 0x42C80000)); /* 100.0 */
}

/* DATE$ ( -- s ): mm-dd-yyyy. */
void syst_date(void)
{
    PclkDate d;
    pclk_date(&d);
    uint8_t *p = cons.sbuf;
    two(&p, d.month);
    *p++ = '-';
    two(&p, d.day);
    *p++ = '-';
    two(&p, d.year / 100 % 100);
    two(&p, d.year % 100);
    syst_ret(10);
}

/* TIME$ ( -- s ): hh:mm:ss. */
void syst_time(void)
{
    PclkTime t;
    pclk_time(&t);
    uint8_t *p = cons.sbuf;
    two(&p, t.hour);
    *p++ = ':';
    two(&p, t.min);
    *p++ = ':';
    two(&p, t.sec);
    syst_ret(8);
}

/* sx_parts: the string at the stack's top as up to 3 numbers apart by sep1 or sep2, FFFFh none;
   how many. Illegal function call: another char, a number past 9999, an empty part. */
static uint32_t parts(uint8_t sep1, uint8_t sep2, uint16_t part[3])
{
    Desc s = strg_get(exec.sp->a);
    part[0] = part[1] = part[2] = 0xFFFF;
    uint32_t k = 0, digits = 0;
    for (uint16_t i = 0; i < s.len; i++) {
        uint8_t c = rd8(s.ptr + i);
        if (c == sep1 || c == sep2) {
            if (!digits || ++k >= 3)
                repl_error(E_IQ);
            digits = 0;
            continue;
        }
        c -= '0';
        if (c > 9)
            repl_error(E_IQ);
        uint32_t v = (part[k] == 0xFFFF ? 0 : part[k]) * 10u + c;
        if (v > 9999)
            repl_error(E_IQ);
        part[k] = (uint16_t)v;
        digits++;
    }
    if (!digits)
        repl_error(E_IQ);
    return k + 1;
}

/* DATE$ = s ( s -- ): mm-dd-yy or mm-dd-yyyy, - or /. */
void syst_sdate(void)
{
    uint16_t part[3];
    if (parts('-', '/', part) != 3)
        repl_error(E_IQ);
    uint16_t y = part[2];
    if (y < 100) {
        y += 1900;
        if (y < 1980)
            y += 100;
    }
    PclkDate d = {.year = y, .month = (uint8_t)part[0], .day = (uint8_t)part[1]};
    if (!pclk_set_date(&d))
        repl_error(E_IQ);
    exec.sp++;
}

/* TIME$ = s ( s -- ): hh[:mm[:ss]]. */
void syst_stime(void)
{
    uint16_t part[3];
    parts(':', ':', part);
    for (int i = 1; i < 3; i++)
        if (part[i] == 0xFFFF)
            part[i] = 0;
    PclkTime t = {.hour = (uint8_t)part[0], .min = (uint8_t)part[1], .sec = (uint8_t)part[2]};
    if (!pclk_set_time(&t))
        repl_error(E_IQ);
    exec.sp++;
}

/* INKEY$ ( -- s ): a key waiting, else empty; an extended key is CHR$(0) and its scan code. */
void syst_inkey(void)
{
    cons_flush();
    if (!pkey_ready()) {
        syst_ret(0);
        return;
    }
    PkeyKey k = pkey_read();
    if (!k.ch || (k.ch == 0xE0 && k.scan)) {
        cons.sbuf[0] = 0;
        cons.sbuf[1] = k.scan;
        syst_ret(2);
        return;
    }
    cons.sbuf[0] = k.ch;
    syst_ret(1);
}

/* sx_find: the environment's entry named by the len chars at name, case not told; null none. */
static char *env_find(const uint8_t *name, uint32_t len)
{
    for (char *e = syst.env; *e; e += strlen(e) + 1) {
        uint32_t i = 0;
        while (i < len && upcase((uint8_t)e[i]) == upcase(name[i]))
            i++;
        if (i == len && e[i] == '=')
            return e;
    }
    return nullptr;
}

/* The environment's last 0, after its entries' 0s. */
static char *env_end(void)
{
    char *e = syst.env;
    while (*e)
        e += strlen(e) + 1;
    return e;
}

/* SHELL ( s -- ): COMSPEC /C s; SHELL alone ( -- ): COMSPEC. Illegal function call: it did not
   run. */
static void shell(Desc s)
{
    char tail[128];
    uint8_t len = 0;
    if (s.len) {
        uint16_t n = s.len > 122 ? 122 : s.len;
        memcpy(tail, " /C ", 4);
        memcpy(tail + 4, at(s.ptr), n);
        len = (uint8_t)(n + 4);
    }
    char *e = env_find((const uint8_t *)"COMSPEC", 7);
    const char *path = e ? e + 8 : "COMMAND.COM";
    cons_flush();
    if (!psys_exec(path, tail, len, syst.env, (uint32_t)(env_end() - syst.env) + 1))
        repl_error(E_IQ);
}
void syst_shell(void) { shell(strg_get(exec_popa())); }
void syst_shell0(void) { shell((Desc){}); }

/* COMMAND$ ( -- s ). */
void syst_cmd(void)
{
    memcpy(cons.sbuf, syst.cmd, syst.cmdlen);
    syst_ret(syst.cmdlen);
}

/* The 0-ended text at t as a string, 255 chars at most. */
static void env_ret(const char *t)
{
    uint32_t n = 0;
    while (t[n] && n < 255) {
        cons.sbuf[n] = (uint8_t)t[n];
        n++;
    }
    syst_ret(n);
}

/* ENVIRON$ ( x flag -- s ): flag 1: x a name, its value; 0: x the entry, from 1, whole. Illegal
   function call: an entry under 1. */
void syst_envs(void)
{
    uint32_t flag = exec_pop().u;
    if (flag) {
        Desc s = strg_get(exec_popa());
        char *e = s.len ? env_find(at(s.ptr), s.len) : nullptr;
        env_ret(e ? e + s.len + 1 : "");
        return;
    }
    int32_t n = exec_popi();
    if (n < 1)
        repl_error(E_IQ);
    char *e = syst.env;
    while (*e && --n)
        e += strlen(e) + 1;
    env_ret(e);
}

/* ENVIRON s ( s -- ): NAME=value set, replaced, or with no value, removed. Illegal function
   call: no '=' or no name. Out of memory: no room. */
void syst_envset(void)
{
    Desc s = strg_get(exec_popa());
    const uint8_t *t = at(s.ptr);
    uint32_t name = 0;
    while (name < s.len && t[name] != '=')
        name++;
    if (name == s.len || !name)
        repl_error(E_IQ);
    char *e = env_find(t, name);
    if (e) { /* the old one out */
        char *next = e + strlen(e) + 1;
        char *end = env_end();
        memmove(e, next, (size_t)(end - next) + 1);
    }
    e = env_end();
    if (name + 1 >= s.len) /* no value: removed */
        return;
    if (e + s.len + 2 > syst.env + ENVSZ)
        repl_error(E_OM);
    memcpy(e, t, s.len);
    e[s.len] = e[s.len + 1] = 0;
}

/* SETMEM ( n -- n ): the free bytes; the heap is fixed. */
void syst_setmem(void)
{
    exec.sp++;
    strg_gc();
    exec_pushi((int32_t)(strg.low - arry.end));
}

/* ERDEV, ERDEV$: no device error is kept. */
void syst_erdev(void) { exec_pushi(0); }
void syst_erdevs(void) { syst_ret(0); }

/* SLEEP ( n -- ): n seconds or a key, which stays; 0 a key. An event or the STOP key ends it. */
void syst_sleep(void)
{
    cons_flush();
    int32_t ticks = exec_popi() * 182 / 10;
    uint32_t start = pclk_ticks();
    for (;;) {
        if (exec.stop & SF_TICK)
            evnt_poll();
        if (exec.stop || pkey_ready())
            return;
        if (ticks && pclk_ticks() - start >= (uint32_t)ticks)
            return;
    }
}

/* RANDOMIZE alone ( -- d ): the seed asked for. */
void syst_rndq(void)
{
    for (;;) {
        cons_puts("Random-number seed (-32768 to 32767)? ");
        cons_read();
        const uint8_t *p = cons.inbuf;
        double d = dubl_in(&p, nullptr);
        if (!*p) {
            exec_pushd(d);
            return;
        }
        cons_puts("Redo from start\r"); /* QBasic's; NL is 13 */
    }
}

/* RANDOMIZE ( d -- ): the DOUBLE's two high words, xor-ed, the seed's middle word (QBasic's
   B$RNZP). */
void syst_rndz(void)
{
    uint64_t r = exec_pop().raw;
    uint32_t w = (uint16_t)(r >> 32 ^ r >> 48);
    rndm.seed = (rndm.seed & 0xFF0000FF) | w << 8;
}
