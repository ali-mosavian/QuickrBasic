/* Errors and the READY loop (io/repl.asm). */
#include "io/repl.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "comp/lexr.h"
#include "comp/link.h"
#include "comp/list.h"
#include "io/chnl.h"
#include "io/cons.h"
#include "io/diag.h"
#include "io/emsg.h"
#include "io/srcr.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/scop.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "num/fmth.h"
#include "plat/psys.h"
#include "vm/dbug.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmt.h"

#if IDE
#include "ui/uimn.h"
#endif

Repl repl;

void repl_qberr(const QbErr qb)
{
    vm.err.user = qb;
    repl_error(E_USR);
}

void repl_error(uint8_t code)
{
    lexr.more = nullptr; /* a streamed line's lexing ended */
    if (dbug.probing)
        dbug_fail(code);
    fmth_clear();
    if (comp.busy)
        comp_fail(code);
    if (code != E_BRK && code != E_OS && !vm.err.active && vm.line != DLINE) {
        uint32_t handler = flow_handler(&vm);
        if (handler)
            flow_trap(&vm, code, handler);
    }
    repl.err = code;
    longjmp(repl.top, REPL_ERROR);
}

void repl_ready(void) { longjmp(repl.top, REPL_READY); }

/* A source file's channel; an include's the number below its includer's. */
enum { SOURCE_FILENO = FILENO_MAX, MODULE_WIDTH = 8 };

/* The module's name, as BC's: the file's without its path and type, MODULE_WIDTH characters,
   blanks after. */
static void module_name(void)
{
    const char *s = repl.file;
    for (const char *p = s; *p; p++)
        if (*p == '\\' || *p == ':')
            s = p + 1;
    uint32_t n = 0;
    for (; n < MODULE_WIDTH && s[n] && s[n] != '.'; n++)
        cons_putc(upcase((uint8_t)s[n]));
    for (; n < MODULE_WIDTH; n++)
        cons_putc(' ');
}

/* A compile error's place and words under BC's message: the line as LIST spells it, a mark under
   the place, what is wrong and what to do. Any other error: nothing. */
static void diagnose(void)
{
    const ErrOp *const e = (const ErrOp *)vm.opip;
    if (!e || e->op != O_ERR)
        return;
    const uint8_t *const body = vm.line == DLINE ? comp.cbuf : at(prog_addr(vm.line) + HDR);
    const uint32_t col = list_text(body, e->at);
    const uint32_t mark = diag_at((const char *)cons.sbuf, col, e->code, e->dg, e->state);
    cons_nl();
    cons_spaces(2);
    cons_puts((const char *)cons.sbuf);
    cons_nl();
    cons_spaces(2);
    for (uint32_t i = 0; i < mark; i++)
        cons_space();
    cons_putc('^');
    cons_nl();
    diag_say();
}

/* repl_error's message, a file run's where, as BC's: "in line n|No line number in module NAME at
   address s:o"; or BREAK [IN n]. The stacks reset first; the editor takes it there. */
static void report(const uint8_t code)
{
    vm.err.active = false;
    chnl_clear();
    cons_fresh();
    vm.sp = vm.rsp;
    strg.tmp_used = 0;
    strg.tmp_floor = 0;
#if IDE
    if (uimn.on)
        uimn_error(code);
#endif
    const uint32_t line = vm.line;
    if (code == E_BRK) {
        cons_msg(E_BRK); /* BREAK [IN n] */
        if (line != DLINE && prog_line(line)->num != NONUM) {
            cons_msg(MSG_IN);
            cons_uint(prog_line(line)->num);
        }
        return;
    }
    vm.nframes = 0;
    vm.cont.line = 0;
    cons_puts(emsg_text(code));
    if (line != DLINE) {
        if (repl.batch) {
            cons_puts(" in line ");
            const uint16_t erl = flow_erl(line);
            if (erl)
                cons_uint(erl);
            else
                cons_puts("No line number");
            cons_puts(" in module ");
            module_name();
            cons_puts(" at address ");
            const uint32_t off = (uint32_t)(vm.opip - at(allo.cache));
            cons_hex4((uint16_t)(off >> 16));
            cons_putc(':');
            cons_hex4((uint16_t)off);
        } else if (prog_line(line)->num != NONUM) { /* typed lines: in line n, if numbered */
            cons_puts(" in line ");
            cons_uint(prog_line(line)->num);
        }
    }
    diagnose();
}

/* The input's end in a file run, or at READY: files closed, exit. */
[[noreturn]] static void quit(void)
{
    chnl_closeall();
    cons_exit(0);
}

void repl_run(void)
{
    vm.stop = 0; /* a key the editor or READY kept */
    comp_line();
    flow_dcode();
    const uint32_t eol = link.eol - allo.cache; /* an older direct line's frames: its EOL */
    for (uint32_t i = 0; i < vm.nframes; i++)
        if (vm.frames[i].ret.line == DLINE)
            vm.frames[i].ret.code = eol;
    vm.ip = at(link.code); /* past its statement map */
    exec_run();
}

void repl_cont(void)
{
    vm.stop = 0;
    stmt_cont(&vm);
    exec_run();
}

/* READY., then lines read and entered. */
[[noreturn]] static void ready(void)
{
#if IDE
    if (uimn.on)
        uimn_ready();
#endif
    if (repl.batch)
        quit();
    cons_fresh();
    cons_msg(MSG_READY);
    for (;;) {
        const int32_t len = cons_getline();
        if (len < 0) {
            if (!(vm.stop & SF_BRK))
                quit();
            vm.stop = 0; /* the STOP key at READY: nothing */
            continue;
        }
        const uint8_t r = repl_enter((uint32_t)len);
        if (r == ENTER_DIRECT)
            repl_run();
        if (r == ENTER_STORED)
            vm.nframes = 0;
    }
}

#if !IDE
/* Run the file the command line names, then exit; no name: back. */
static void run_file(void)
{
    char tail[128];
    psys_cmdline(tail, sizeof tail);
    const char *s = tail;
    while (*s == ' ')
        s++;
    size_t n = 0;
    while ((uint8_t)*s > ' ' && n < sizeof repl.file - 1)
        repl.file[n++] = *s++;
    repl.file[n] = 0;
    if (!n)
        return;
    repl_load();
    prog_drop();
    link_start();
    stmt_runfirst();
    exec_run();
}
#endif

void repl_main(void)
{
    switch (setjmp(repl.top)) {
    case REPL_START:
#if IDE
        uimn_start();
#else
        run_file();
#endif
        break;
    case REPL_ERROR:
        report(repl.err);
        break;
    default:
        break;
    }
    ready();
}

static const uint8_t *blanks(const uint8_t *p, const uint8_t *const end)
{
    while (p < end && is_blank(*p))
        p++;
    return p;
}

/* The word w (capitals) at *p, any case: *p past it. */
static bool word(const uint8_t **const p, const uint8_t *const end, const char *const w)
{
    const uint8_t *const q = *p;
    const size_t n = strlen(w);
    if ((size_t)(end - q) < n)
        return false;
    for (size_t i = 0; i < n; i++)
        if (upcase(q[i]) != (uint8_t)w[i])
            return false;
    *p = q + n;
    return true;
}

/* Is inbuf, len long, a '$INCLUDE: 'f' metacommand (or REM's)? f to repl.incl. */
static bool is_include(const uint32_t len)
{
    const uint8_t *p = cons.inbuf;
    const uint8_t *const end = p + len;
    p = blanks(p, end);
    while (p < end && is_digit(*p)) /* past a line number */
        p++;
    p = blanks(p, end);
    if (p >= end)
        return false;
    if (*p == '\'')
        p++;
    else if (!word(&p, end, "REM"))
        return false;
    p = blanks(p, end);
    if (!word(&p, end, "$INCLUDE"))
        return false;
    p = blanks(p, end);
    if (p >= end || *p++ != ':')
        return false;
    p = blanks(p, end);
    if (p >= end || *p++ != '\'')
        return false;
    for (size_t n = 0;;) {
        if (p >= end)
            return false;
        const uint8_t c = *p++;
        if (c == '\'') {
            repl.incl[n] = 0;
            return true;
        }
        repl.incl[n++] = (char)c;
        if (n >= sizeof repl.incl - 1)
            return false;
    }
}

/* A line number in the body at p (M_INT n): Syntax error 64000 or over. */
static uint16_t line_number(const uint8_t *const p)
{
    const uint32_t num = load32(p + 1);
    if (num >= LNUM_LIMIT)
        repl_error(E_SN);
    return (uint16_t)num;
}

/* The line in comp_cbuf, to be stored at place: its declarations. vm.lnum its number, NONUM
   none; returns its body past the number. */
static const uint8_t *declare(const uint32_t place)
{
    prog.entering = place;
    decl_line(comp.cbuf);
    const uint8_t *p = comp.cbuf;
    vm.lnum = NONUM;
    if (comp_peek(&p) == M_INT) {
        vm.lnum = line_number(p);
        p += INT_TOKEN_SIZE;
    }
    return p;
}

/* declare, and the line's number's and label's labels. */
static const uint8_t *enter_decls(const uint32_t place)
{
    const uint8_t *const p = declare(place);
    if (vm.lnum != NONUM)
        scop_label(SC_NUM | decl.proc, vm.lnum, prog.entering);
    if (comp_label(p))
        scop_label(decl.proc, comp_nameof(p), prog.entering);
    return p;
}

uint8_t repl_enter(const uint32_t len)
{
    cons.inbuf[len] = 0;
    vm.line = DLINE;
    const uint8_t *const end = lexr_line(cons.inbuf, comp.cbuf);
    const uint32_t place = prog_at();
    if (!comp.cbuf[0])
        return ENTER_EMPTY;
    if (repl.loading) { /* every line stored, at the end */
        const uint8_t *const body = enter_decls(place);
        prog_add(vm.lnum, body, (uint32_t)(end - body));
        return ENTER_STORED;
    }
    const uint8_t *p = declare(place);
    if (vm.lnum == NONUM)
        return ENTER_DIRECT;
    comp_peek(&p);
    prog_put(vm.lnum, p, (uint32_t)(end - p));
    return ENTER_STORED;
}

/* Enter the lines of file name, read as file number num; an include's on the number below, in
   place. Then the console's again. */
static void read_file(const char *const name, const int32_t num)
{
    chnl_in(chnl_open(num, name, FM_IN, RECLEN_DEFAULT));
    for (;;) {
        const int32_t len = srcr_line();
        if (len < 0)
            break;
        repl.fline++;
        const bool inc = is_include((uint32_t)len);
        lexr.more = srcr_more;
        repl_enter((uint32_t)len);
        lexr.more = nullptr;
        if (inc) { /* its lines next */
            read_file(repl.incl, num - 1);
            chnl_in(chnl_find(num));
        }
    }
    chnl_close(chnl_find(num));
    chnl_clear();
}

void repl_load(void)
{
    repl.loading = true;
    repl.batch = true;
    repl.fline = 0;
    read_file(repl.file, SOURCE_FILENO);
    repl.loading = false;
    repl.fline = 0;
}

void repl_relink(void)
{
    for (uint32_t line = PROGBASE; prog_line(line)->len;) /* the include's lines out */
        if (prog_line(line)->flags & LF_INC)
            prog_cut(line);
        else
            line += prog_line(line)->len;
    symb_vars_init();
    decl_reset();
    prog_drop();
    repl.loading = true;
    for (uint32_t line = PROGBASE; prog_line(line)->len;) {
        vm.line = line;
        uint8_t *d = comp.cbuf; /* its number, then its body */
        const uint32_t num = prog_line(line)->num;
        if (num != NONUM) {
            *d = M_INT;
            store32(d + 1, num);
            d += INT_TOKEN_SIZE;
        }
        prog_body(line, d);
        const uint8_t *body = enter_decls(line);
        const uint8_t tok = comp_peek(&body);
        line += prog_line(line)->len;
        if (tok != TK_REM)
            continue;
        list_text(body, 0); /* '$INCLUDE: the file's lines here */
        memcpy(cons.inbuf, cons.sbuf, cons.sblen + 1);
        if (!is_include(cons.sblen))
            continue;
        prog.add_at = line;
        prog.add_flags = LF_INC;
        read_file(repl.incl, SOURCE_FILENO - 1);
        line = prog.add_at;
        prog.add_at = 0;
        prog.add_flags = 0;
    }
    repl.loading = false;
}
