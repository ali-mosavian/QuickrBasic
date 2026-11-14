/* Errors and the READY loop (io/repl.asm). */
#include "io/repl.h"
#include <stddef.h>
#include "session.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/fold.h"
#include "comp/lexer.h"
#include "comp/link.h"
#include "comp/listing.h"
#include "io/channel.h"
#include "io/console.h"
#include "io/diag.h"
#include "io/errmsg.h"
#include "io/source.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "plat/pfil.h"
#include "plat/psys.h"
#include "vm/debug.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmts.h"


void repl_qberr(Session *const S, const QbErr qb)
{
    S->vm.err.user = qb;
    repl_error(S, E_USR);
}

void repl_error(Session *const S, uint8_t code)
{
    fold_fail(S);
    S->lexr.more = nullptr; /* a streamed line's lexing ended */
    if (S->dbug.probing)
        dbug_fail(S, code);
    fmth_clear();
    if (S->comp.busy)
        comp_fail(S, code);
    if (code != E_BRK && code != E_OS && !S->vm.err.active && S->vm.line != DLINE) {
        uint32_t handler = flow_handler(S);
        if (handler)
            flow_trap(S, code, handler);
    }
    S->repl.err = code;
    longjmp(S->repl.top, REPL_ERROR);
}

[[noreturn]] void repl_fault(Fault *const f, const Failure *const e)
{
    Session *const S = (Session *)((uint8_t *)f - offsetof(Session, fault));
    if (e->at != FAULT_KEEP)
        diag_raise(S, e->code, e->dg, e->at, e->line, e->other, e->len);
    if (e->line != FAULT_KEEP)
        S->vm.line = e->line;
    repl_error(S, e->code);
}

void repl_ready(Session *const S) { longjmp(S->repl.top, REPL_READY); }

/* A source file's channel; an include's the number below its includer's. */
enum { SOURCE_FILENO = FILENO_MAX, MODULE_WIDTH = 8 };

/* The module's name, as BC's: the file's without its path and type, MODULE_WIDTH characters,
   blanks after. */
static void module_name(Session *const S)
{
    const char *s = S->repl.file;
    for (const char *p = s; *p; p++)
        if (*p == '\\' || *p == ':')
            s = p + 1;
    uint32_t n = 0;
    for (; n < MODULE_WIDTH && s[n] && s[n] != '.'; n++)
        cons_putc(S, upcase((uint8_t)s[n]));
    for (; n < MODULE_WIDTH; n++)
        cons_putc(S, ' ');
}

/* A compile error's place and words under BC's message: the line as LIST spells it, a mark under
   the place, what is wrong and what to do. Any other error: nothing. */
static void diagnose(Session *const S)
{
    const ErrOp *const e = (const ErrOp *)S->vm.opip;
    if (!e || e->op != O_ERR)
        return;
    const uint8_t *const body = S->vm.line == DLINE ? S->comp.cbuf : at(prog_addr(&S->prog, S->vm.line) + HDR);
    const uint32_t col = list_text(S, body, e->at);
    const uint32_t mark = diag_at(S, (const char *)S->cons.sbuf, col, e->code, e->dg, e->state);
    cons_nl(S);
    cons_spaces(S, 2);
    cons_puts(S, (const char *)S->cons.sbuf);
    cons_nl(S);
    cons_spaces(S, 2);
    for (uint32_t i = 0; i < mark; i++)
        cons_space(S);
    cons_putc(S, '^');
    cons_nl(S);
    diag_say(S);
}

/* repl_error's message, a file run's where, as BC's: "in line n|No line number in module NAME at
   address s:o"; or BREAK [IN n]. The stacks reset first; the editor takes it there. */
static void report(Session *const S, const uint8_t code)
{
    S->vm.err.active = false;
    chnl_clear(S);
    cons_fresh(S);
    S->vm.sp = S->vm.rsp;
    S->strg.tmp_used = 0;
    S->strg.tmp_floor = 0;
    if (S->repl.fe->error)
        S->repl.fe->error(S, code);
    const uint32_t line = S->vm.line;
    if (code == E_BRK) {
        cons_msg(S, E_BRK); /* BREAK [IN n] */
        if (line != DLINE && prog_line(&S->prog, line)->num != NONUM) {
            cons_msg(S, MSG_IN);
            cons_uint(S, prog_line(&S->prog, line)->num);
        }
        return;
    }
    S->vm.nframes = 0;
    S->vm.cont.line = 0;
    cons_puts(S, emsg_text(S, code));
    if (line != DLINE) {
        if (S->repl.batch) {
            cons_puts(S, " in line ");
            const uint16_t erl = flow_erl(S, line);
            if (erl)
                cons_uint(S, erl);
            else
                cons_puts(S, "No line number");
            cons_puts(S, " in module ");
            module_name(S);
            cons_puts(S, " at address ");
            const uint32_t off = (uint32_t)(S->vm.opip - at(S->allo.cache));
            cons_hex4(S, (uint16_t)(off >> 16));
            cons_putc(S, ':');
            cons_hex4(S, (uint16_t)off);
        } else if (prog_line(&S->prog, line)->num != NONUM) { /* typed lines: in line n, if numbered */
            cons_puts(S, " in line ");
            cons_uint(S, prog_line(&S->prog, line)->num);
        }
    }
    diagnose(S);
}

/* The input's end in a file run, or at READY: files closed, exit. */
[[noreturn]] static void quit(Session *const S)
{
    chnl_closeall(S);
    cons_exit(S, 0);
}

void repl_run(Session *const S)
{
    S->vm.stop = 0; /* a key the editor or READY kept */
    comp_line(S);
    flow_dcode(S);
    const uint32_t eol = S->link.eol - S->allo.cache; /* an older direct line's frames: its EOL */
    for (uint32_t i = 0; i < S->vm.nframes; i++)
        if (S->vm.frames[i].ret.line == DLINE)
            S->vm.frames[i].ret.code = eol;
    S->vm.ip = at(S->link.code); /* past its statement map */
    exec_run(S);
}

void repl_cont(Session *const S)
{
    S->vm.stop = 0;
    stmt_cont(S);
    exec_run(S);
}

/* READY., then lines read and entered. */
[[noreturn]] static void ready(Session *const S)
{
    if (S->repl.fe->ready)
        S->repl.fe->ready(S);
    if (S->repl.batch)
        quit(S);
    cons_fresh(S);
    cons_msg(S, MSG_READY);
    for (;;) {
        const int32_t len = cons_getline(S);
        if (len < 0) {
            if (!(S->vm.stop & SF_BRK))
                quit(S);
            S->vm.stop = 0; /* the STOP key at READY: nothing */
            continue;
        }
        const uint8_t r = repl_enter(S, (uint32_t)len);
        if (r == ENTER_DIRECT)
            repl_run(S);
        if (r == ENTER_STORED)
            S->vm.nframes = 0;
    }
}

/* Run the file the command line names, then exit; no name: back. */
static void run_file(Session *const S)
{
    char tail[128];
    psys_cmdline(tail, sizeof tail);
    const char *s = tail;
    while (*s == ' ')
        s++;
    size_t n = 0;
    while ((uint8_t)*s > ' ' && n < sizeof S->repl.file - 1)
        S->repl.file[n++] = *s++;
    S->repl.file[n] = 0;
    if (!n)
        return;
    repl_load(S);
    prog_drop(S);
    link_start(S);
    stmt_runfirst(S);
    exec_run(S);
}

static void warn_stderr(Session *const S, const char *const text, const uint32_t len)
{
    uint32_t put;
    pfil_write(PFIL_STDERR, text, len, &put);
}

const FrontEnd repl_batch = {.start = run_file, .warn = warn_stderr};

void repl_main(Session *const S)
{
    switch (setjmp(S->repl.top)) {
    case REPL_START:
        if (S->repl.fe->start)
            S->repl.fe->start(S);
        break;
    case REPL_ERROR:
        report(S, S->repl.err);
        break;
    default:
        break;
    }
    ready(S);
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
static bool is_include(Session *const S, const uint32_t len)
{
    const uint8_t *p = S->cons.inbuf;
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
            S->repl.incl[n] = 0;
            return true;
        }
        S->repl.incl[n++] = (char)c;
        if (n >= sizeof S->repl.incl - 1)
            return false;
    }
}

/* A line number in the body at p (M_INT n): Syntax error 64000 or over. */
static uint16_t line_number(Session *const S, const uint8_t *const p)
{
    const uint32_t num = load32(p + 1);
    if (num >= LNUM_LIMIT)
        repl_error(S, E_SN);
    return (uint16_t)num;
}

/* The line in comp_cbuf, to be stored at place: its declarations. vm.lnum its number, NONUM
   none; returns its body past the number. */
static const uint8_t *declare(Session *const S, const uint32_t place)
{
    S->prog.entering = place;
    decl_line(S, S->comp.cbuf);
    const uint8_t *p = S->comp.cbuf;
    S->vm.lnum = NONUM;
    if (comp_peek(&p) == M_INT) {
        S->vm.lnum = line_number(S, p);
        p += INT_TOKEN_SIZE;
    }
    return p;
}

/* declare, and the line's number's and label's labels. */
static const uint8_t *enter_decls(Session *const S, const uint32_t place)
{
    const uint8_t *const p = declare(S, place);
    if (S->vm.lnum != NONUM)
        scop_label(S, SC_NUM | S->decl.proc, S->vm.lnum, S->prog.entering);
    if (comp_label(p))
        scop_label(S, S->decl.proc, comp_nameof(p), S->prog.entering);
    return p;
}

uint8_t repl_enter(Session *const S, const uint32_t len)
{
    S->cons.inbuf[len] = 0;
    S->vm.line = DLINE;
    const uint8_t *const end = lexr_line(S, S->cons.inbuf, S->comp.cbuf);
    const uint32_t place = prog_at(S);
    if (!S->comp.cbuf[0])
        return ENTER_EMPTY;
    if (S->repl.loading) { /* every line stored, at the end */
        const uint8_t *const body = enter_decls(S, place);
        prog_add(S, S->vm.lnum, body, (uint32_t)(end - body));
        return ENTER_STORED;
    }
    const uint8_t *p = declare(S, place);
    if (S->vm.lnum == NONUM)
        return ENTER_DIRECT;
    comp_peek(&p);
    prog_put(S, S->vm.lnum, p, (uint32_t)(end - p));
    return ENTER_STORED;
}

/* Enter the lines of file name, read as file number num; an include's on the number below, in
   place. Then the console's again. */
static void read_file(Session *const S, const char *const name, const int32_t num)
{
    chnl_in(S, chnl_open(S, num, name, FM_IN, RECLEN_DEFAULT));
    for (;;) {
        const int32_t len = srcr_line(S);
        if (len < 0)
            break;
        S->repl.fline++;
        const bool inc = is_include(S, (uint32_t)len);
        S->lexr.more = srcr_more;
        repl_enter(S, (uint32_t)len);
        S->lexr.more = nullptr;
        if (inc) { /* its lines next */
            read_file(S, S->repl.incl, num - 1);
            chnl_in(S, chnl_find(S, num));
        }
    }
    chnl_close(S, chnl_find(S, num));
    chnl_clear(S);
}

void repl_load(Session *const S)
{
    S->repl.loading = true;
    S->repl.batch = true;
    S->repl.fline = 0;
    read_file(S, S->repl.file, SOURCE_FILENO);
    S->repl.loading = false;
    S->repl.fline = 0;
}

void repl_relink(Session *const S)
{
    for (uint32_t line = PROGBASE; prog_line(&S->prog, line)->len;) /* the include's lines out */
        if (prog_line(&S->prog, line)->flags & LF_INC)
            prog_cut(S, line);
        else
            line += prog_line(&S->prog, line)->len;
    symb_vars_init(S);
    decl_reset(S);
    prog_drop(S);
    S->repl.loading = true;
    for (uint32_t line = PROGBASE; prog_line(&S->prog, line)->len;) {
        S->vm.line = line;
        uint8_t *d = S->comp.cbuf; /* its number, then its body */
        const uint32_t num = prog_line(&S->prog, line)->num;
        if (num != NONUM) {
            *d = M_INT;
            store32(d + 1, num);
            d += INT_TOKEN_SIZE;
        }
        prog_body(S, line, d);
        const uint8_t *body = enter_decls(S, line);
        const uint8_t tok = comp_peek(&body);
        line += prog_line(&S->prog, line)->len;
        if (tok != TK_REM)
            continue;
        list_text(S, body, 0); /* '$INCLUDE: the file's lines here */
        memcpy(S->cons.inbuf, S->cons.sbuf, S->cons.sblen + 1);
        if (!is_include(S, S->cons.sblen))
            continue;
        S->prog.add_at = line;
        S->prog.add_flags = LF_INC;
        read_file(S, S->repl.incl, SOURCE_FILENO - 1);
        line = S->prog.add_at;
        S->prog.add_at = 0;
        S->prog.add_flags = 0;
    }
    S->repl.loading = false;
}
