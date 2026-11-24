/* Errors and the READY loop: reading a line, running it, reporting what went wrong.
 *
 * # How it works
 * repl_error never returns. It unwinds with longjmp to the first of these that holds:
 *
 *   1. a stage's catch (fault.h): compiling's, which ends the line with an ERR op, folding's, or
 *      a watch probe's (dbug_eval).
 *   2. not BREAK nor E_OS, not in the handler, not the direct line, and a handler found:
 *      flow_trap, which longjmps to vm.loop at the handler.
 *   3. else the innermost repl_call, which resets the stacks and returns the error. The READY
 *      loop reports it (the message, a file run's where, a compile error's place and words); the
 *      editor shows it.
 *
 * Before any of them lexr.more is cleared and the x87 cleared. repl_ready, a run's end, longjmps
 * to the same repl_call, which returns ST_OK.
 *
 * fault_qberr (fault.h) raises E_USR with vm.err.user = the QbErr. repl_enter takes a typed line and says
 * which it was: ENTER_DIRECT, ENTER_EMPTY or ENTER_STORED. repl_main's loop reads, enters and
 * runs lines; a front end hooks its places through its FrontEnd (null entries are the
 * console's).
 *
 * # asm structure (retired/asm32/io/repl.asm)
 * | asm | here | |
 * |---|---|---|
 * | err_raise | repl_error | MIRROR |
 * | err_sn, err_id, err_rg, err_us, err_cn, err_tm, err_st, err_iq, err_om, err_os, err_od | repl_error(E_SN) etc. | MIRROR |
 * | ch_err (io/chan.asm) | fault_qberr (fault.h) | MOVED |
 * | er_diag, rpl_mod | static | MIRROR |
 * | rpl_ready | repl_ready, repl_main | D1 |
 * | rpl_run, rpl_cont | repl_run, repl_cont | MIRROR |
 * | rpl_file | static | MIRROR; run by repl_main at the start |
 * | rpl_load, rpl_enter, rpl_relink | repl_load, repl_enter, repl_relink | MIRROR |
 * | ent_line, rpl_incl, rpl_line, rpl_read, rpl_inc, rpl_noinc | static | MIRROR |
 * | g_fnmb, g_finc | repl.file, repl.incl | MIRROR |
 * | r_* data | static | MIRROR |
 * | vm.rsp (the C stack's reset) | repl.top | D2 |
 *
 * - D1. Every `jmp rpl_ready` is repl_ready, a longjmp to repl.top; the loop itself is ready.
 * - D2. The innermost repl_call's jmp_buf: a run or a command returns to its caller.
 */
#include "io/repl.h"
#include <stddef.h>
#include "session.h"
#include "comp/bind.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/fold.h"
#include "comp/import.h"
#include "comp/lexer.h"
#include "comp/link.h"
#include "comp/listing.h"
#include "io/channel.h"
#include "io/console.h"
#include "io/diag.h"
#include "io/errmsg.h"
#include "io/locate.h"
#include "io/native.h"
#include "io/path.h"
#include "io/source.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "num/fpmath.h"
#include "plat/pfil.h"
#include "plat/psys.h"
#include "qbrt/qbrt.h"
#include "qr/qr.h"
#include "util/grow.h"
#include "vm/debug.h"
#include "vm/exec.h"
#include "vm/flow.h"
#include "vm/stmts.h"

void repl_error(Session *const S, uint8_t code)
{
    S->lexr.source = nullptr; /* a streamed line's lexing ended */
    fmth_clear();

    if (S->fault.catch) /* compiling, folding, a watch's probe: theirs */
        fault_throw(&S->fault, code);
    if (code != E_BRK && code != E_OS && !S->vm.err.active && S->vm.line != DLINE) {
        S->vm.err.raised = S->vm.line;
        uint32_t handler = flow_handler(S);

        if (handler)
            flow_trap(S, code, handler);
    }

    S->repl.err = code;
    longjmp(*S->repl.top, REPL_ERROR);
}

/* An error found off the code (the block pass, a declaration) set as if an ERR op at body offset
   at of line (a program store offset; FAULT_KEEP the current one) raised it. vm.opip is set to an
   ErrOp (vm/exec.h) of its own. */
static void place(Session *const S, const Failure *const e)
{
    static ErrOp op;
    op = (ErrOp){
        .op = O_ERR,
        .code = e->code,
        .dg = e->dg,
        .at = e->at,
        .state = DX_NOSTATE,
    };

    if (e->line != FAULT_KEEP)
        S->vm.line = e->line;

    S->vm.opip = e->op ? (uint8_t *)e->op : (uint8_t *)&op;
    S->vm.err.active = true;
    diag_other(&S->diag, e->other, e->len);
}

/* place's error raised, never trapped. */
[[noreturn]]
static void repl_raise_at(Session *const S, const Failure *const e)
{
    place(S, e);
    repl_error(S, e->code);
}

void repl_fault(Fault *const f, const Failure *const e)
{
    Session *const S = container_of(f, Session, fault);

    if (e->code == E_USR)
        S->vm.err.user = e->user;
    if (e->at != FAULT_KEEP)
        repl_raise_at(S, e);
    if (e->line != FAULT_KEEP)
        S->vm.line = e->line;

    repl_error(S, e->code);
}

void repl_ready(Session *const S) { longjmp(*S->repl.top, REPL_READY); }

/* A source file's channel; an include's the number below its includer's. */
enum { SOURCE_FILENO = FILENO_MAX, MODULE_WIDTH = 8 };

/* The module's name, as BC's: its file's at path without its folder and type, MODULE_WIDTH
   characters, blanks after. */
static void module_name(Session *const S, const char *s)
{
    if (!*s) /* a typed program's: the main's */
        s = S->repl.file;

    for (const char *p = s; *p; p++)
        if (*p == '\\' || *p == '/' || *p == ':')
            s = p + 1;

    uint32_t n = 0;

    for (; n < MODULE_WIDTH && s[n] && s[n] != '.'; n++)
        cons_putc(&S->cons, upcase((uint8_t)s[n]));
    for (; n < MODULE_WIDTH; n++)
        cons_putc(&S->cons, ' ');
}

/* Source file n's name: as it was opened; a typed program's, the main's. */
static const char *source_name(Session *const S, const uint16_t file)
{
    const char *const name = modu_path(&S->modu, file);
    return *name ? name : S->repl.file;
}

/* The error's line in its source file, its file in *file; 0 for a line not read from one. */
static uint16_t source_line(Session *const S, uint16_t *const file)
{
    if (S->vm.line == DLINE) {
        *file = S->prog.add_file;
        return S->repl.loading ? S->prog.add_src : 0;
    }

    const Line *const l = prog_line(&S->prog, S->vm.line);
    *file = l->file;
    return l->src;
}

/* "FILE:n" */
static void put_where(Session *const S, const uint16_t file, const uint16_t src)
{
    cons_puts(&S->cons, source_name(S, file));
    cons_putc(&S->cons, ':');
    cons_uint(&S->cons, src);
}

/* An error loading: reading a line, or declaring one read (vm.line its line). */
static bool load_error(const Session *const S) { return S->repl.loading; }

/* The ERR op a compile error raised; null for any other error. */
static const ErrOp *err_op(const Session *const S)
{
    const ErrOp *const e = (const ErrOp *)S->vm.opip;
    return e && e->op == O_ERR ? e : nullptr;
}

/* An error found before the run: loading the file, or compiling. */
static bool compile_error(const Session *const S) { return load_error(S) || err_op(S); }

/* A line below the line just printed, a mark at its column mark. */
static void put_mark(Session *const S, const uint32_t mark)
{
    cons_nl(&S->cons);
    cons_spaces(&S->cons, 2);

    for (uint32_t i = 0; i < mark; i++)
        cons_space(&S->cons);

    cons_putc(&S->cons, '^');
    cons_nl(&S->cons);
}

/* text on a line of its own, a mark under its column mark. */
static void put_marked(Session *const S, const uint8_t *const text, const uint32_t mark)
{
    cons_nl(&S->cons);
    cons_spaces(&S->cons, 2);
    cons_puts(&S->cons, (const char *)text);
    put_mark(S, mark);
}

/* A compile error's place and words: the line as LIST spells it, a mark under the place, what is
   wrong and what to do. */
static void diagnose(Session *const S, const ErrOp *const e)
{
    const uint8_t *const body = S->vm.line == DLINE || S->repl.loading
                                    ? S->comp.cbuf
                                    : at(prog_addr(&S->prog, S->vm.line) + HDR);
    const uint32_t col = list_text(&S->comp, body, e->at);
    const uint32_t mark = diag_at(
        &S->diag,
        S->vm.err.user,
        (const char *)S->cons.sbuf,
        col,
        e->code,
        e->dg,
        e->state
    );
    put_marked(S, S->cons.sbuf, mark);
    diag_say(&S->diag);
}

/* What cons.sbuf captured, on standard error: each NL a CR LF. */
static void warn_captured(Session *const S)
{
    const uint8_t *const t = S->cons.sbuf;
    uint32_t from = 0;

    for (uint32_t i = 0; i < S->cons.sblen; i++)
        if (t[i] == NL) {
            S->repl.fe->warn(S, (const char *)t + from, i - from);
            S->repl.fe->warn(S, "\r\n", 2);
            from = i + 1;
        }

    S->repl.fe->warn(S, (const char *)t + from, S->cons.sblen - from);
}

/* A file run's runtime error's place, on standard error so that standard output is BC's alone:
   FILE:n, then the line as LIST spells it, marked at the failing statement. */
static void diagnose_run(Session *const S, const uint16_t file, const uint16_t src)
{
    uint32_t left;
    const Stmt *const st = flow_stmtof(&S->prog, &S->allo, S->vm.line, S->vm.opip, &left);
    const uint8_t *const body = at(prog_addr(&S->prog, S->vm.line) + HDR);
    uint32_t col = list_text(&S->comp, body, st->body == BOFF ? 0 : st->body);

    while (S->cons.sbuf[col] == ' ') /* the statement's text starts after the blanks past ':' */
        col++;

    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    put_where(S, file, src);
    cons_nl(&S->cons);
    cons_spaces(&S->cons, 2);
    list_body(&S->comp, body);
    put_mark(S, col);
    S->cons.capture = 0;
    warn_captured(S);
}

/* repl_error's message. A file run's compile error: "FILE:n: " before it. Its runtime error, as
   BC's, after it: "in line n|No line number in module NAME at address X", X the op's bytecode
   offset, then its place on standard error. Else "in line n" if numbered. A compile error's
   FILE:n on a line of its own, if not before, and the line, marked. Or BREAK [IN n]. The stacks reset first; the editor takes it there. */
static void report(Session *const S, const uint8_t code)
{
    const uint32_t line = S->vm.line;

    if (code == E_BRK) {
        cons_msg(&S->cons, E_BRK); /* BREAK [IN n] */
        if (line != DLINE && prog_line(&S->prog, line)->num != NONUM) {
            cons_msg(&S->cons, MSG_IN);
            cons_uint(&S->cons, prog_line(&S->prog, line)->num);
        }

        return;
    }

    flow_drop_calls(S);
    S->vm.cont.line = 0;

    if (line != DLINE && prog_line(&S->prog, line)->flags & LF_QR) {
        qr_report(S, code);
        return;
    }

    uint16_t file;
    const uint16_t src = source_line(S, &file);
    const bool compiled = compile_error(S), lead = S->repl.batch && compiled && src;

    if (lead) {
        put_where(S, file, src);
        cons_puts(&S->cons, ": ");
    }

    cons_puts(&S->cons, emsg_text(S->vm.err.user, code));

    if (line != DLINE && !load_error(S)) {
        if (S->repl.batch && !compiled) {
            cons_puts(&S->cons, " in line ");
            const uint16_t erl = flow_erl(&S->prog, line);

            if (erl)
                cons_uint(&S->cons, erl);
            else
                cons_puts(&S->cons, "No line number");

            cons_puts(&S->cons, " in module ");
            module_name(S, modu_module_path(&S->modu, modu_of(&S->modu, file)));
            cons_puts(&S->cons, " at address ");
            const uint32_t off = (uint32_t)(S->vm.opip - at(S->allo.cache));
            cons_hex4(&S->cons, (uint16_t)(off >> 16));
            cons_hex4(&S->cons, (uint16_t)off);
        } else if (prog_line(&S->prog, line)->num !=
                   NONUM) { /* typed lines: in line n, if numbered */
            cons_puts(&S->cons, " in line ");
            cons_uint(&S->cons, prog_line(&S->prog, line)->num);
        }
    }

    if (compiled && src && !lead) {
        cons_nl(&S->cons);
        put_where(S, file, src);
    }

    const ErrOp *const e = err_op(S);

    if (e)
        diagnose(S, e);
    else if (load_error(S) && line == DLINE)
        put_marked(S, S->cons.inbuf, 0);
    else if (load_error(S)) { /* a line read: as LIST spells it */
        list_text(&S->comp, S->comp.cbuf, 0);
        put_marked(S, S->cons.sbuf, 0);
    } else if (S->repl.batch && line != DLINE) {
        cons_nl(&S->cons);

        if (src)
            diagnose_run(S, file, src);
    }
}

/* The input's end in a file run, or at READY: files closed, exit. */
[[noreturn]]
static void quit(Session *const S)
{
    fault_check(&S->fault, chnl_closeall(&S->chnl));
    cons_exit(&S->cons, 0);
}

void repl_run(Session *const S)
{
    S->vm.stop = 0; /* a key the editor or READY kept */
    comp_line(&S->comp);
    flow_dcode(S);
    const uint32_t eol = S->link.eol - S->allo.cache; /* an older direct line's frames: its EOL */
    for (uint32_t i = 0; i < vm_nframes(&S->vm); i++)
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

/* An error's end: the stacks reset, the console's channels again. */
static void reset(Session *const S)
{
    S->vm.err.active = false;
    S->vm.running = false; /* its loop's frame is gone */
    natv_stop(&S->natv);
    chnl_clear(&S->chnl);
    cons_fresh(&S->cons);
    S->vm.sp = flow_stack_base(&S->vm); /* below a stop's frames, which CONT and probes keep */
    S->strg.tmp_used = 0;
    S->strg.tmp_floor = 0;
}

Status repl_call(Session *const S, void (*const fn)(Session *S))
{
    jmp_buf here, *const outer = S->repl.top;
    S->repl.top = &here;
    const int how = setjmp(here);

    if (how == REPL_CALLED)
        fn(S);

    S->repl.top = outer;

    if (how != REPL_ERROR)
        return ST_OK;

    reset(S);
    return S->repl.err;
}

/* Lines read and entered till a direct one, which is left to run. */
static void read_lines(Session *const S)
{
    for (;;) {
        const int32_t len = cons_getline(&S->cons);

        if (len < 0) {
            if (!(S->vm.stop & SF_BRK))
                quit(S);

            S->vm.stop = 0; /* the STOP key at READY: nothing */
            continue;
        }

        const uint8_t r = repl_enter(S, (uint32_t)len);

        if (r == ENTER_DIRECT)
            return;
        if (r == ENTER_STORED)
            flow_drop_calls(S);
    }
}

/* Lines read till a direct one, then run. */
static void enter_and_run(Session *const S)
{
    read_lines(S);
    repl_run(S);
}

/* READY., then lines read and entered, a direct one run; again. */
[[noreturn]]
static void ready(Session *const S)
{
    for (;;) {
        if (S->repl.batch)
            quit(S);

        cons_fresh(&S->cons);
        cons_msg(&S->cons, MSG_READY);
        const Status st = repl_call(S, enter_and_run);

        if (st != ST_OK)
            report(S, (uint8_t)st);
    }
}

static void report(Session *S, uint8_t code);

/* --check's: an error as a run reports it. */
static void say(void *const ctx, const Failure *const e)
{
    Session *const S = ctx;
    place(S, e);
    report(S, e->code);
    cons_nl(&S->cons);
    S->vm.err.active = false;
}

/* To the front end's warn: "Warning: [line src: ]", the n texts, CR LF. */
static void repl_warn(
    Session *const S,
    const uint16_t src,
    const char *const text[],
    const uint32_t n
)
{
    char buf[24], *w = buf;
    memcpy(w, "Warning: ", 9);
    w += 9;

    if (src) {
        memcpy(w, "line ", 5);
        w += 5;
        char dig[5];
        int k = 0;

        for (uint32_t v = src; v; v /= 10)
            dig[k++] = (char)('0' + v % 10);
        while (k)
            *w++ = dig[--k];

        *w++ = ':';
        *w++ = ' ';
    }

    S->repl.fe->warn(S, buf, (uint32_t)(w - buf));

    for (uint32_t i = 0; i < n; i++)
        S->repl.fe->warn(S, text[i], (uint32_t)strlen(text[i]));

    S->repl.fe->warn(S, "\r\n", 2);
}

/* A DIM a() whose array nothing fills, to the front end's warn. */
static void warn_empty(void *const ctx, const LinkEmpty *const e)
{
    Session *const S = ctx;
    const uint8_t *const t = symb_text(&S->symb, symb_var(&S->symb, e->var)->name);
    char name[UINT8_MAX + 3];
    memcpy(name, t + 1 + t[0], t[0]); /* as first written */
    memcpy(name + t[0], "()", 3);
    const char *const text[] = {"nothing REDIMs, assigns or passes ", name};

    repl_warn(S, prog_line(&S->prog, e->line)->src, text, 2);
}

/* Is the command line at *s the option opt, a word: *s then past it and its spaces. */
static bool option(const char **const s, const char *const opt)
{
    const size_t n = strlen(opt);

    if (strncmp(*s, opt, n) || (uint8_t)(*s)[n] > ' ')
        return false;

    for (*s += n; **s == ' ';)
        (*s)++;

    return true;
}

/* Run the file the command line names, then exit; no name: back. --check FILE: every compile
   error reported, not run; the exit 1 if any. --test FILE, a Quickr one: its tests run instead. */
static void run_file(Session *const S)
{
    char tail[PFIL_PATH];
    psys_cmdline(tail, sizeof tail);
    const char *s = tail;

    while (*s == ' ')
        s++;

    S->repl.checking = option(&s, "--check");
    S->repl.testing = !S->repl.checking && option(&s, "--test");

    size_t n = 0;

    while ((uint8_t)*s > ' ' && n < sizeof S->repl.file - 1)
        S->repl.file[n++] = *s++;

    S->repl.file[n] = 0;

    if (!n)
        return;

    if (qr_is_file(S->repl.file)) {
        qr_run(S);
        return;
    }

    repl_load(S);
    session_drop(S);

    if (S->repl.checking) {
        const uint32_t errors = link_report(&S->comp, say, S);
        link_empties(&S->comp, warn_empty, S);
        fault_check(&S->fault, chnl_closeall(&S->chnl));
        cons_exit(&S->cons, errors ? 1 : 0);
    }

    link_start(&S->comp);
    link_empties(&S->comp, warn_empty, S);
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
    if (S->repl.fe->start) {
        const Status st = repl_call(S, S->repl.fe->start);

        if (st != ST_OK) {
            qbrt_text(); /* the message stays: the exit's text mode would clear a graphics screen */
            report(S, (uint8_t)st);

            if (S->repl.checking || S->repl.batch)
                cons_exit(&S->cons, 1);
        }
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

/* Is text, len long, a '$INCLUDE: 'f' metacommand (or REM's)? f to repl.incl. */
static bool is_include(Session *const S, const uint8_t *const text, const uint32_t len)
{
    const uint8_t *p = text;
    const uint8_t *const end = p + len;
    p = blanks(p, end);

    while (p < end && is_digit(*p)) /* past a line number */
        p++;

    p = blanks(p, end);

    if (p >= end || (*p != '\'' && upcase(*p) != 'R')) /* most lines: neither ' nor REM */
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

uint32_t repl_intern(LexrNames *const names, const uint8_t *const text, const uint8_t len)
{
    Session *const S = container_of(names, Session, names);
    return symb_find_name(&S->symb, S->repl.module, text, len);
}

/* The body just lexed: each statement with no effect in 32-bit mode warned of. */
static void warn_effectless(Session *const S, const uint8_t *const body)
{
    if (S->repl.fe->warn && S->lexr.escaped)
        for (const uint8_t *p = body; *p;) {
            const char *const what = decl_warning(&p);

            if (!what)
                break;

            const char *const text[] = {what, " has no effect in 32-bit mode"};

            repl_warn(S, S->repl.fline, text, 2);
        }
}

void repl_declare(Session *const S, const uint8_t *const body)
{
    decl_line(&S->decl, &S->scop, &S->symb, body, S->prog.entering, S->repl.loading);
}

Status repl_declare_check(Session *const S, const uint8_t *const body, ErrOp *const op)
{
    const bool active = S->vm.err.active;
    uint8_t *const opip = S->vm.opip;
    S->vm.opip = nullptr; /* set again: its place known */
    Catch c;
    fault_push(&S->fault, &c);

    if (!setjmp(c.jb)) {
        repl_declare(S, body);
        fault_pop(&S->fault);
        c.code = ST_OK;
    } else {
        const ErrOp *const e = err_op(S);
        *op = e ? *e : (ErrOp){.op = O_ERR, .code = (uint8_t)c.code, .state = DX_NOSTATE};
    }

    S->vm.err.active = active;
    S->vm.opip = opip;
    return c.code;
}

/* The line in comp_cbuf: vm.lnum its number, NONUM none; its body past the number. */
static const uint8_t *numbered(Session *const S)
{
    const uint8_t *p = S->comp.cbuf;
    S->vm.lnum = NONUM;

    if (comp_peek(&p) == M_INT) {
        S->vm.lnum = line_number(S, p);
        p += INT_TOKEN_SIZE;
    }

    return p;
}

/* The line in comp_cbuf, to be stored at place: its declarations; numbered's. */
static const uint8_t *declare(Session *const S, const uint32_t place)
{
    S->prog.entering = place;
    repl_declare(S, S->comp.cbuf);
    return numbered(S);
}

/* declare, and the line's number's and label's labels. */
static const uint8_t *enter_decls(Session *const S, const uint32_t place)
{
    const uint8_t *const p = declare(S, place);

    const uint32_t module = S->decl.module, own = S->decl.proc ? S->decl.proc : module;

    if (S->vm.lnum != NONUM)
        scop_label(&S->scop, &S->symb, module, SC_NUM | own, S->vm.lnum, S->prog.entering);
    if (comp_label(p))
        scop_label(&S->scop, &S->symb, module, own, comp_nameof(p), S->prog.entering);

    return p;
}

/* The editor's (repl.keep_pre): the text before inbuf's body, its indent and number as typed, a
   tab to the next column of 8, to repl.pre; its length. Out of memory. */
static uint32_t keep_pre(Session *const S)
{
    const uint8_t *const t = S->cons.inbuf;
    uint32_t i = 0, n = 0;

    for (uint8_t part = 0; part < 3; part++) /* blanks, digits, blanks */
        for (; part == 1 ? is_digit(t[i]) : is_blank(t[i]); i++) {
            const uint32_t to = t[i] == '\t' ? (n / 8 + 1) * 8 : n + 1;

            if (!GROW(S->repl.pre, S->repl.precap, to))
                repl_error(S, E_OM);

            memset(S->repl.pre + n, t[i] == '\t' ? ' ' : t[i], to - n);
            n = to;
        }

    return n;
}

uint8_t repl_enter(Session *const S, const uint32_t len)
{
    S->cons.inbuf[len] = 0;
    const uint32_t pre = S->repl.loading && S->repl.keep_pre ? keep_pre(S) : 0;
    S->vm.line = DLINE;
    const uint8_t *const end = lexr_line(&S->lexr, S->cons.inbuf, S->comp.cbuf);
    const uint32_t place = prog_at(&S->prog);

    if (!S->comp.cbuf[0])
        return ENTER_EMPTY;

    warn_effectless(S, S->comp.cbuf);

    if (S->repl.loading) { /* every line stored, at the end; declared once all are */
        const uint8_t *const body = numbered(S);
        S->prog.pre = S->repl.pre;
        S->prog.pre_len = pre;
        prog_add(&S->prog, S->vm.lnum, body, (uint32_t)(end - body));
        return ENTER_STORED;
    }

    const uint8_t *p = declare(S, place);

    if (S->vm.lnum == NONUM)
        return ENTER_DIRECT;

    comp_peek(&p);
    const uint8_t keep = S->prog.add_flags;
    S->prog.add_flags |= S->decl.deft ? 0 : LF_NODEF;
    prog_put(&S->prog, S->link.cursor, S->vm.lnum, p, (uint32_t)(end - p));
    S->prog.add_flags = keep;
    return ENTER_STORED;
}

/* Room for a source of len bytes, the code cache's then the store's, while the blocks above them
   are small: grown later, each moved all above it (the store and its line map, 600K for a 2600-line
   program). The store is about 1.5 times the text, its code about 1.4: the cache link_start wants
   (comp/link.c, the store's size) within twice it. Short, or no room: they grow as they fill. */
static void reserve(Session *const S, const uint32_t len)
{
    allo_grow(&S->allo, HCACHE, CACHE0 + len * 2 + XLIM + XSLK);
    allo_grow(&S->allo, HPROG, PROGBASE + len / 4 * 7);
}

/* An include's file to out: beside its includer, from, when there; else name as written, from the
   current folder. out, as name (repl.incl) is the next include's. */
static const char *include_path(const char *const from, const char *const name, char *const out)
{
    if (!path_join(out, PFIL_PATH, from, path_dir(from), name) || !path_exists(out))
        strcpy(out, name);

    return out;
}

/* An IMPORT or FROM line's modules being found, or its names bound: where to look, FROM's package
   folder or module. */
typedef struct {
    Session *S;
    Locate where;
    char from[PFIL_PATH]; /* the line's file's path: the table it is in may move as it grows */
    char package[PFIL_PATH];
    bool in_package; /* FROM's module is a folder: its names are its modules */
    uint32_t module; /* FROM's module's number */
} Imports;

/* A module not found: File not found, unless the editor's open skips it (repl.lenient). */
static void not_found(Session *const S)
{
    if (!S->repl.lenient)
        fault_qberr(&S->fault, QB_FILE_NOT_FOUND);
}

/* The module at path imported by the one being read: loaded once, declared before it. A .qbx's
   header is read now, so an error in it is at the IMPORT. */
static void import(Session *const S, const char *const path)
{
    const uint32_t m = modu_module(&S->modu, path);

    if (qbxf_is(path)) {
        natv_open(&S->natv, path);
        modu_mod(&S->modu, m)->native = true;
    }

    modu_import(&S->modu, S->repl.module, m);
}

/* One name the line gives: a module's file added to those to load, once. */
static void imported(void *const ctx, const ImptKind kind, const ImptName *const n)
{
    Imports *const im = ctx;
    char path[PFIL_PATH];

    switch (kind) {
    case IMPT_MODULE:
        if (locate(&im->where, n, path, nullptr))
            import(im->S, path);
        else
            not_found(im->S);

        return;

    case IMPT_FROM:
        if (!locate(&im->where, n, path, &im->in_package))
            not_found(im->S);
        else if (im->in_package)
            strcpy(im->package, path);
        else
            import(im->S, path);

        return;

    case IMPT_NAME: /* a module's export: its file is there already */
        if (!im->in_package)
            return;
        if (!n->sfx && locate_in(im->package, n, nullptr, path))
            import(im->S, path);
        else
            not_found(im->S);

        return;

    case IMPT_STAR:
        return;
    }
}

/* One name the line gives bound in the module being declared, its modules found when loaded. */
static void bound(void *const ctx, const ImptKind kind, const ImptName *const n)
{
    Imports *const im = ctx;
    Session *const S = im->S;
    const uint32_t m = sc_number(S->decl.module);
    char path[PFIL_PATH];

    switch (kind) {
    case IMPT_MODULE:
        locate(&im->where, n, path, nullptr);
        bind_module(&S->scop, &S->symb, &S->fault, m, n, modu_module(&S->modu, path));
        return;

    case IMPT_FROM:
        locate(&im->where, n, path, &im->in_package);

        if (im->in_package)
            strcpy(im->package, path);
        else
            im->module = modu_module(&S->modu, path);

        return;

    case IMPT_NAME:
        if (!im->in_package) {
            bind_name(&S->scop, &S->symb, &S->fault, m, im->module, n);
            return;
        }

        locate_in(im->package, n, nullptr, path);
        bind_module(&S->scop, &S->symb, &S->fault, m, n, modu_module(&S->modu, path));
        return;

    case IMPT_STAR:
        if (im->in_package) /* a package's every module: not ours */
            fault_error(&S->fault, E_AF);

        bind_star(&S->scop, &S->symb, m, im->module);
    }
}

/* The line just read (comp.cbuf), of the file at path: the modules its IMPORT or FROM names
   found, each to be loaded once after the program's lines. One in a SUB is the compiler's to
   refuse. File not found. */
static void find_imports(Session *const S, const char *const path)
{
    const uint8_t *body = S->comp.cbuf, *name;
    const uint8_t kind = decl_proc(body, &name);

    if (kind)
        S->repl.inproc = kind != PK_END;
    if (kind || S->repl.inproc)
        return;
    if (comp_peek(&body) == M_INT) /* past its number */
        body += INT_TOKEN_SIZE;

    Imports im = {
        .S = S,
        .where = {S->repl.file, syst_env(&S->syst, "BASICLIB"), im.from, nullptr}
    };

    strcpy(im.from, path);
    impt_line(body, imported, &im);
}

/* Enter the lines of file name from offset from on, read as file number num; an include's on the
   number below, in place. Then the console's again. */
static void read_file(
    Session *const S,
    const char *const name,
    const int32_t num,
    const uint32_t from
)
{
    Chnl *const cl = &S->chnl;
    Chan *c;
    fault_check(&S->fault, chnl_open(cl, num, name, FM_IN, RECLEN_DEFAULT, &c));

    if (from)
        fault_check(&S->fault, chnl_seek(c, from));

    uint32_t len;

    if (num == SOURCE_FILENO && chnl_lof(c, &len) == ST_OK)
        reserve(S, len);

    fault_check(&S->fault, chnl_in(cl, c));
    const uint16_t fline = S->repl.fline, file = S->prog.add_file;
    S->repl.fline = 0;
    S->prog.add_file = S->repl.nfile++;
    modu_file(&S->modu, S->prog.add_file, name, S->repl.module);

    if (num == SOURCE_FILENO) /* an include's lines may be a SUB's */
        S->repl.inproc = false;

    for (;;) {
        S->prog.add_src = S->repl.fline + 1; /* a continued line's first */
        const int32_t len = srcr_line(&S->srcr);
        fault_check(&S->fault, chnl_take(cl)); /* a read's error */

        if (len < 0)
            break;

        S->repl.fline++;
        const bool inc = is_include(S, S->cons.inbuf, (uint32_t)len);
        S->lexr.source = &S->srcr.src;
        repl_enter(S, (uint32_t)len);
        S->lexr.source = nullptr;
        find_imports(S, name);

        if (inc) { /* its lines next */
            char path[PFIL_PATH];
            read_file(S, include_path(name, S->repl.incl, path), num - 1, 0);
            fault_check(&S->fault, chnl_find(cl, num, &c));
            fault_check(&S->fault, chnl_in(cl, c));
        }
    }

    fault_check(&S->fault, chnl_find(cl, num, &c));
    fault_check(&S->fault, chnl_close(cl, c));
    chnl_clear(cl);
    S->repl.fline = fline;
    S->prog.add_src = 0;
    S->prog.add_file = file;
}

/* Stored line line in comp.cbuf as entered, its number then its body, vm.line it: numbered's. */
static const uint8_t *restore(Session *const S, const uint32_t line)
{
    S->vm.line = line;
    uint8_t *d = S->comp.cbuf;
    const uint32_t num = prog_line(&S->prog, line)->num;

    if (num != NONUM) {
        *d = M_INT;
        store32(d + 1, num);
        d += INT_TOKEN_SIZE;
    }

    prog_body(&S->prog, line, d);
    return numbered(S);
}

/* An END, LF_TOP, past the lines read: where a run of a module's code stops, by any way to the
   next line (comp/import.c). */
static void module_end(Session *const S)
{
    static const uint8_t end[] = {TK_END, 0};

    const uint8_t flags = S->prog.add_flags;
    S->prog.add_flags = flags | LF_TOP;
    prog_add(&S->prog, NONUM, end, sizeof end);
    S->prog.add_flags = flags;
}

/* The modules the program imports from module from on, after the store's lines, each once and in
   the order first named; a module's own imports after it. An END ends each module's lines, the
   main's too, its LF_TOP the next module's top. last: the LF_TOP past the lines of the modules
   before from, cut first; 0 none. */
static void read_modules(Session *const S, const uint32_t from, const uint32_t last)
{
    if (modu_count(&S->modu) == from)
        return;
    if (last)
        prog_cut(&S->prog, last);

    for (uint32_t m = from; m < modu_count(&S->modu); m++) {
        char path[PFIL_PATH]; /* the table may move as it grows */
        strcpy(path, modu_module_path(&S->modu, m));
        modu_top(&S->modu, m, prog_at(&S->prog));
        module_end(S);
        S->repl.module = m;

        if (!modu_mod(&S->modu, m)->native) {
            read_file(S, path, SOURCE_FILENO, 0);
            continue;
        }

        const uint8_t flags = S->prog.add_flags; /* a .qbx: its interface, its last part */
        S->prog.add_flags = flags | LF_NATIVE;
        read_file(S, path, SOURCE_FILENO, natv_open(&S->natv, path));
        S->prog.add_flags = flags;
    }

    module_end(S); /* the last module's */
    S->repl.module = 0;
}

/* An IMPORT or FROM statement's modules run, each to each. */
typedef struct {
    Imports im;
    ImptRun *each;
    void *ctx;
} Runs;

/* The module at path, as loaded, run. */
static void runs_one(Runs *const r, const char *const path)
{
    const uint32_t m = modu_find(&r->im.S->modu, path);

    if (m < modu_count(&r->im.S->modu)) /* one gone since: nothing run */
        r->each(r->ctx, m);
}

/* One name the statement gives: the module it runs. */
static void runs_name(void *const ctx, const ImptKind kind, const ImptName *const n)
{
    Runs *const r = ctx;
    Imports *const im = &r->im;
    char path[PFIL_PATH];

    switch (kind) {
    case IMPT_MODULE:
        if (locate(&im->where, n, path, nullptr))
            runs_one(r, path);

        return;

    case IMPT_FROM:
        if (!locate(&im->where, n, path, &im->in_package))
            return;
        if (im->in_package)
            strcpy(im->package, path);
        else
            runs_one(r, path);

        return;

    case IMPT_NAME:
        if (im->in_package && locate_in(im->package, n, nullptr, path))
            runs_one(r, path);

        return;

    case IMPT_STAR:
        return;
    }
}

void repl_runs(
    ImptFind *const f,
    const uint32_t line,
    const uint8_t *const stmt,
    ImptRun *const each,
    void *const ctx
)
{
    Session *const S = container_of(f, Session, impt);
    Runs r = {
        .im = {.S = S, .where = {S->repl.file, syst_env(&S->syst, "BASICLIB"), r.im.from, nullptr}}
    };

    r.each = each;
    r.ctx = ctx;
    strcpy(r.im.from, source_name(S, prog_line(&S->prog, line)->file));
    bool ok;
    impt_scan(stmt, runs_name, &r, &ok);
}

/* Stored line line's IMPORT or FROM, body in comp.cbuf: its names bound in its module. One in a SUB
   is the compiler's to refuse. */
static void bind_imports(Session *const S, const uint32_t line, const uint8_t *const body)
{
    if (S->decl.proc)
        return;

    Imports im = {
        .S = S,
        .where = {S->repl.file, syst_env(&S->syst, "BASICLIB"), im.from, nullptr}
    };

    strcpy(im.from, source_name(S, prog_line(&S->prog, line)->file));
    impt_line(body, bound, &im);
}

/* Stored line line's declarations, labels, LF_NODEF and imports' bindings. */
static void redeclare(Session *const S, const uint32_t line)
{
    const uint8_t *const body = restore(S, line);
    enter_decls(S, line);
    bind_imports(S, line, body);
    Line *const l = prog_line(&S->prog, line);
    l->flags = (uint8_t)((l->flags & ~LF_NODEF) | (S->decl.deft ? 0 : LF_NODEF));
}

/* The program's lines read, declared: each module's after those it imports, the main's last,
   its file's DEFtypes and OPTIONs from its first line. Then the main's again, for lines typed. */
static void declare_program(Session *const S)
{
    const uint32_t n = modu_count(&S->modu), *const order = modu_order(&S->modu);

    for (uint32_t i = 0; i < n; i++) {
        const uint32_t m = order[i], top = modu_top_of(&S->modu, m);
        const uint32_t from = m ? top + prog_line(&S->prog, top)->len : PROGBASE;
        const uint32_t to = m + 1 < n ? modu_top_of(&S->modu, m + 1) : prog_at(&S->prog);
        decl_module(&S->decl, &S->scop, &S->symb, sc_module(m), top);
        S->decl.native = modu_mod(&S->modu, m)->native;

        for (uint32_t line = from; line < to; line += prog_line(&S->prog, line)->len)
            redeclare(S, line);

        S->decl.native = false;
    }

    S->vm.line = DLINE;
    decl_defevents(&S->decl, &S->scop, &S->symb, &S->prog);
    decl_main(&S->decl, &S->symb);
}

void repl_load(Session *const S)
{
    S->repl.loading = true;
    S->repl.batch = true;
    S->repl.fline = 0;
    S->repl.nfile = 0;
    S->repl.module = 0;
    modu_clear(&S->modu, S->repl.file);
    read_file(S, S->repl.file, SOURCE_FILENO, 0);
    read_modules(S, 1, 0);
    declare_program(S);
    S->repl.loading = false;
}

/* Stored module m's records out, its LF_TOP first; the one past the lines too when m was the only
   module of n. */
static void drop_module(Session *const S, const uint32_t m, const uint32_t n)
{
    Prog *const pg = &S->prog;
    const uint32_t top = modu_top_of(&S->modu, m);
    uint32_t end = top + prog_line(pg, top)->len;

    while (!(prog_line(pg, end)->flags & LF_TOP)) /* one is past the last module's lines */
        end += prog_line(pg, end)->len;
    if (n == 2)
        end += prog_line(pg, end)->len;

    prog_cut_to(pg, top, end);
    modu_drop(&S->modu, m);
}

/* The program's lines in the store, entered as loaded: the includes' read again, each module's own
   lines its number's file (modu_relink). Lenient: an import not found skipped. A module no import
   reaches is dropped (modu_unreached), then the lines entered again. */
static void walk(Session *const S, const bool lenient)
{
    for (uint32_t line = PROGBASE; prog_line(&S->prog, line)->len;) /* the includes' out */
        if (prog_line(&S->prog, line)->flags & LF_INC)
            prog_cut(&S->prog, line);
        else
            line += prog_line(&S->prog, line)->len;

    S->repl.lenient = lenient;
    const uint32_t n = prog_modules(&S->prog);
    modu_relink(&S->modu, S->repl.file, n);
    S->repl.nfile = (uint16_t)n; /* the includes' numbers after */
    S->repl.module = 0;
    S->repl.inproc = false;
    uint32_t last = 0; /* the LF_TOP past the last module's lines */

    for (uint32_t line = PROGBASE; prog_line(&S->prog, line)->len;) {
        Line *const l = prog_line(&S->prog, line);

        if (l->flags & LF_TOP) {
            last = line;
            line += l->len;

            if (S->repl.module + 1 < n)
                modu_top(&S->modu, ++S->repl.module, last);

            S->repl.inproc = false;
            continue;
        }

        l->file = (uint16_t)S->repl.module;
        const uint8_t *body = restore(S, line);
        find_imports(S, modu_module_path(&S->modu, S->repl.module));
        const uint8_t tok = comp_peek(&body);
        line += prog_line(&S->prog, line)->len;

        if (tok != TK_REM)
            continue;

        list_text(&S->comp, body, 0); /* '$INCLUDE: the file's lines here */

        if (!is_include(S, S->cons.sbuf, S->cons.sblen))
            continue;

        S->prog.add_at = line;
        S->prog.add_flags = LF_INC;
        char path[PFIL_PATH];
        const char *const from = modu_module_path(&S->modu, S->repl.module);
        read_file(S, include_path(from, S->repl.incl, path), SOURCE_FILENO - 1, 0);
        line = S->prog.add_at;
        S->prog.add_at = 0;
        S->prog.add_flags = 0;
    }

    const uint32_t gone = modu_unreached(&S->modu, n);

    if (gone) {
        drop_module(S, gone, n);
        walk(S, lenient);
        return;
    }

    read_modules(S, n, last);
    S->repl.module = 0;
    S->repl.lenient = false;
}

/* The program's lines in the store, read again as loaded (walk), not declared. */
static void reread(Session *const S, const bool lenient)
{
    symb_vars_init(&S->symb);
    decl_reset(&S->decl, &S->scop, &S->symb);
    session_drop(S);
    S->repl.loading = true;
    walk(S, lenient);
}

/* repl_relink's, repl_open's (lenient). */
static void relink(Session *const S, const bool lenient)
{
    reread(S, lenient);
    declare_program(S);
    S->repl.loading = false;
}

void repl_relink(Session *const S) { relink(S, false); }
void repl_open(Session *const S) { relink(S, true); }
