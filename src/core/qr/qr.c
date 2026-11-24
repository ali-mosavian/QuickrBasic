/* Quickr, the modern dialect: a .qr file loaded, checked, linked and run.
 *
 * # How it works
 * Each source line becomes a line of the program store (LF_QR), its body the line's text, so the
 * editor, an error's place and the debugger work line by line as for QBasic. The front end then
 * runs in stages, each over the whole file: lex (qlex), parse (qpar), check (qsem). Every error
 * any stage finds is reported, in the source's order, before anything runs:
 *
 *   prog.qr:3:7: unknown name y
 *     print(y)
 *           ^
 *
 * A clean program is linked (qlink) and run by the VM as a QBasic one is. A runtime error stops
 * it at its line: prog.qr:5: Division by zero, then the line.
 */
#include "qr/qr.h"
#include "session.h"
#include "io/errmsg.h"
#include "plat/pfil.h"
#include "qr/qast.h"
#include "qr/qlex.h"
#include "qr/qlink.h"
#include "qr/qpar.h"
#include "qr/qsem.h"

enum { QR_CHUNK = 0x10000 }; /* the front end's arena's blocks at least */

bool qr_is_file(const char *const path)
{
    const size_t n = strlen(path);
    return n > 3 && path[n - 3] == '.' && downcase((uint8_t)path[n - 2]) == 'q' &&
           downcase((uint8_t)path[n - 1]) == 'r';
}

/* A platform error dos, if any, raised as QBasic's. */
static void qr_check(Session *const S, const uint16_t dos)
{
    if (dos)
        fault_check(&S->fault, chnl_doserr(dos));
}

/* The file's bytes in the arena, a 0 after them; *len how many. */
static char *qr_read(Session *const S, QMem *const m, uint32_t *const len)
{
    PfilHandle h;
    qr_check(S, pfil_open(S->repl.file, PFIL_READ, &h));
    uint32_t size = 0, got = 0;
    pfil_seek(h, 0, PFIL_END, &size);
    pfil_seek(h, 0, PFIL_SET, nullptr);
    char *const text = qast_alloc(m, size + 1);
    const uint16_t st = pfil_read(h, text, size, &got);
    pfil_close(h);
    qr_check(S, st);
    *len = got;
    return text;
}

/* The file's lines into the store, each a Quickr line, and into p's lines. */
static void qr_load(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t len, cap = 0;
    char *const text = qr_read(S, m, &len);
    modu_clear(&S->modu, S->repl.file);
    modu_file(&S->modu, 0, S->repl.file, 0);
    S->prog.add_flags = LF_QR;
    S->prog.add_file = 0;

    for (uint32_t at = 0, src = 1; at < len; src++) {
        uint32_t end = at;

        while (end < len && text[end] != '\n')
            end++;

        const uint32_t n = end - at - (end > at && text[end - 1] == '\r');
        text[at + n] = 0;
        p->src.lines = qast_room(m, p->src.lines, p->src.nlines, &cap, sizeof *p->src.lines);
        p->src.lines[p->src.nlines++] = (QLine){
            .at = prog_at(&S->prog),
            .text = text + at,
            .len = n,
            .src = (uint16_t)src,
        };

        S->prog.add_src = (uint16_t)src;
        prog_add(&S->prog, NONUM, (const uint8_t *)text + at, n + 1);
        at = end + 1;
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
}

/* The line text, then a ^ under its byte col, tabs kept so it lines up. */
static void qr_mark(Cons *const co, const char *const text, const uint32_t len, const uint32_t col)
{
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < len; i++)
        cons_putc(co, (uint8_t)text[i]);

    cons_nl(co);
    cons_spaces(co, 2);

    for (uint32_t i = 0; i < col && i < len; i++)
        cons_putc(co, text[i] == '\t' ? '\t' : ' ');

    cons_putc(co, '^');
    cons_nl(co);
}

/* file:line:col: message, then the line marked. */
static void qr_say(Session *const S, const QProg *const p, const QErr *const e)
{
    Cons *const co = &S->cons;
    const QLine *const ln = e->line < p->src.nlines ? &p->src.lines[e->line] : nullptr;
    cons_puts(co, S->repl.file);
    cons_putc(co, ':');
    cons_uint(co, ln ? ln->src : p->src.nlines);
    cons_putc(co, ':');
    cons_uint(co, e->col + 1);
    cons_puts(co, ": ");

    for (uint32_t i = 0; i < e->len; i++)
        cons_putc(co, (uint8_t)e->text[i]);

    cons_nl(co);

    if (ln)
        qr_mark(co, ln->text, ln->len, e->col);
}

/* Each instance's lines, copies of its generic's at the store's end, its statements on them; each
   block lambda's so too, its lines in the store a procedure's own. One code block a line, each copy
   keeping its source's number and text. */
static void qr_instances(Session *const S, QProg *const p, QMem *const m)
{
    uint32_t cap = p->src.nlines;
    S->prog.add_flags = LF_QR;
    S->prog.add_file = 0;

    for (const QFn *f = p->fns; f; f = f->next) {
        if (!f->of && !(f->lamof && f->sig))
            continue;

        const QFn *const from = f->of ? f->of : f;
        const uint32_t base = p->src.nlines;
        const uint32_t first = from->def->tok->line;

        for (uint32_t i = first; i <= from->end->tok->line; i++) {
            const QLine ln = p->src.lines[i];
            p->src.lines = qast_room(m, p->src.lines, p->src.nlines, &cap, sizeof *p->src.lines);
            p->src.lines[p->src.nlines++] = (QLine){
                .at = prog_at(&S->prog),
                .text = ln.text,
                .len = ln.len,
                .src = ln.src,
            };

            S->prog.add_src = ln.src;
            prog_add(&S->prog, NONUM, (const uint8_t *)ln.text, ln.len + 1);
        }

        for (QStmt *s = f->def;; s = s->next) {
            s->line = base + s->tok->line - first;

            if (s == f->end)
                break;
        }
    }

    S->prog.add_flags = 0;
    S->prog.add_src = 0;
}

uint32_t qr_build(Session *const S)
{
    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_CHUNK);
    QProg p = {.errs = {.mem = &m}};

    S->repl.batch = true;
    qr_load(S, &p, &m);
    session_drop(S);
    qlex_source(&p.src, &m, &p.errs);
    qpar_program(&p, &m);
    qsem_program(&p, &m);
    qast_sort(&p.errs);

    for (uint32_t i = 0; i < p.errs.n; i++)
        qr_say(S, &p, &p.errs.at[i]);

    if (!p.errs.n) {
        qr_instances(S, &p, &m);
        qlink_program(&S->comp, &p, &m, S->dbug.emit);
    }

    arena_reset(&m.arena);
    return p.errs.n;
}

void qr_run(Session *const S)
{
    const uint32_t errors = qr_build(S);

    if (errors || S->repl.checking)
        cons_exit(&S->cons, errors ? 1 : 0);

    stmt_runfirst(S);
    exec_run(S);
}

void qr_report(Session *const S, const uint8_t code)
{
    Cons *const co = &S->cons;
    const Line *const l = prog_line(&S->prog, S->vm.line);
    const char *const text = (const char *)at(prog_addr(&S->prog, S->vm.line) + HDR);
    cons_puts(co, S->repl.file);
    cons_putc(co, ':');
    cons_uint(co, l->src);
    cons_puts(co, ": ");
    cons_puts(co, emsg_text(S->vm.err.user, code));
    cons_nl(co);
    cons_spaces(co, 2);
    cons_puts(co, text);
    cons_nl(co);
}
