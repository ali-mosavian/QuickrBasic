/* Quickr's link: a checked program to what the VM runs, as link_start makes it for QBasic.
 *
 * # How it works
 * Each fn is a procedure (comp/blocks.h), numbered in source order. Each variable is one of the
 * symbol table's: a module's in its own cell, a fn's a local of its procedure's frame, a
 * parameter its ref cell. A name the table already has for that scope and type, another spelling
 * of it (x, X) or a shadowed one, gets a ' and a number: each variable its own cell.
 *
 * Each line is one statement at most: its code is compiled with a statement map of the statement
 * (body 0), its labels (qr/qcg.c) and the line's end (BOFF), then placed in the cache
 * (link_place). The
 * store moves when the cache grows, and a string literal's address with it: the lines are
 * compiled again until it stays. Then each procedure's frame is laid out and each line resolved
 * (link_resolve): its variables placed, its jumps found.
 */
#include "qr/qlink.h"
#include "comp/blocks.h"
#include "comp/frame.h"
#include "comp/link.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/symbols.h"
#include "qr/qcg.h"
#include "util/grow.h"

enum { NAME_ROOM = 200 }; /* a name's bytes kept, its ' and number after */

/* Type t's ET_* and its variables' kind (mem/symbols.h). */
static void qlink_kind(const QType t, uint32_t *const et, uint8_t *const kind)
{
    switch (t) {
    case QY_SINGLE:
        *et = ET_R4;
        *kind = VK_FLT;
        return;

    case QY_DOUBLE:
        *et = ET_R8;
        *kind = VK_FLT | SK_DBL;
        return;

    case QY_STRING:
        *et = ET_SD;
        *kind = VK_STR;
        return;

    default:
        *et = ET_I4;
        *kind = VK_INT;
    }
}

/* A new variable named text in scope, of type t: the name, then name'1, name'2 .. till new. */
static uint32_t qlink_symb(
    Symb *const sy,
    const QTok *const name,
    const QType t,
    const uint32_t scope
)
{
    uint8_t text[NAME_ROOM + 12];
    const uint32_t n = name->len < NAME_ROOM ? name->len : NAME_ROOM;
    uint32_t et;
    uint8_t kind;
    qlink_kind(t, &et, &kind);
    memcpy(text, name->text, n);

    for (uint32_t k = 0;; k++) {
        uint32_t m = n;

        if (k) {
            text[m++] = '\'';
            uint8_t rev[10];
            uint32_t d = 0;

            for (uint32_t v = k; v; v /= 10)
                rev[d++] = (uint8_t)('0' + v % 10);
            while (d)
                text[m++] = rev[--d];
        }

        const uint32_t before = sy->nvars;
        const uint32_t id =
            symb_find_var(sy, symb_find_name(sy, 0, text, (uint8_t)m), et, kind, scope, false);

        if (sy->nvars != before)
            return id;
    }
}

/* Each fn a procedure; its parameters and result linked in qlink_vars. */
static void qlink_procs(Comp *const C, const QProg *const p)
{
    for (QFn *f = p->fns; f; f = f->next) {
        Proc *const pr = blok_add_proc(C->blok, C->fault);
        f->proc = C->blok->procs.n - 1;
        const uint32_t name =
            symb_find_name(C->symb, 0, (const uint8_t *)f->name->text, (uint8_t)f->name->len);
        symb_name(C->symb, name)->proc = f->proc;
        *pr = (Proc){
            .name = name,
            .kind = f->result == QY_VOID ? PK_SUB : PK_FUNC,
            .first = p->src.lines[f->def->tok->line].at,
            .last = p->src.lines[f->end->tok->line].at,
            .npar = (uint16_t)f->npar,
        };
    }
}

/* Variable v's scope: its fn's procedure, else the module's. A result is the module's, as a
   QBasic FUNCTION's (PRET.FV): return stores it right before PRET, so no call between can
   overwrite it, and the frame is a cell smaller. */
static uint32_t qlink_scope(const QVar *const v)
{
    return v->fn && v->kind != QV_RESULT ? v->fn->proc : 0;
}

/* Each variable but a const's in the symbol table; a fn's parameters its ref cells. */
static void qlink_vars(Comp *const C, const QProg *const p)
{
    static const QTok hidden = {.text = "~", .len = 1};

    Symb *const sy = C->symb;

    for (QVar *v = p->vars; v; v = v->next)
        if (!v->constant)
            v->id = qlink_symb(sy, v->kind == QV_TEMP ? &hidden : v->name, v->type, qlink_scope(v));

    for (const QFn *f = p->fns; f; f = f->next) {
        Proc *const pr = btab_at(&C->blok->procs, f->proc);
        pr->result = f->res ? f->res->id : 0;
        pr->params = arena_alloc(&C->blok->arena, (f->npar ? f->npar : 1) * sizeof *pr->params);

        if (!pr->params)
            fault_error(C->fault, E_OM);

        for (uint32_t k = 0; k < f->npar; k++) {
            const uint32_t id = f->params[k].var->id;
            pr->params[k] = (Param){.var = id};

            symb_varx(sy, id)->par = 1;
            *symb_loc(sy, id) = loc_ref((uint32_t)(2 * sizeof(Cell) * (f->npar - 1u - k)));
        }
    }
}

/* A statement map's entry: body offset body at code bytes into the line's code. */
static void qlink_map(Comp *const C, const uint32_t body, const uint32_t code)
{
    Stmt *const e = (Stmt *)C->map - (C->nstmt + 1);
    *e = (Stmt){.body = body, .code = code};

    C->nstmt++;
}

/* Line's code to comp_xbuf: statement s's (null none), its labels, then the line's end; the
   buffer grown to fit. */
static void qlink_compile(Comp *const C, QCg *const g, const uint32_t line, const QStmt *const s)
{
    for (;;) {
        Code *const c = C->code;
        const uint32_t top = C->xcap < XLIM + COMP_TAIL ? C->xcap : XLIM + COMP_TAIL;
        c->buf = c->out = C->xbuf;
        c->full = false;
        c->iload = c->iloadend = c->cmp = c->cmpend = c->shend = nullptr;
        C->map = c->buf + top;
        c->lim = C->map - 2 * sizeof(Stmt) - COMP_TAIL;
        C->nstmt = 0;
        C->line = line;
        qlink_map(C, 0, 0);
        g->c = c;
        g->line = line;
        g->nlabels = 0;

        if (s)
            qcg_stmt(g, s);

        for (uint32_t i = 0; i < g->nlabels; i++)
            qlink_map(C, g->labels[i].body, g->labels[i].code);

        qlink_map(C, BOFF, (uint32_t)(c->out - c->buf));
        *c->out++ = O_EOL;

        if (!c->full)
            return;
        if (C->xcap >= XLIM + COMP_TAIL || !GROW(C->xbuf, C->xcap, 2 * C->xcap))
            fault_error(C->fault, E_OM);
    }
}

/* Each line compiled into the cache, again till the store stays where it is. */
static void qlink_lines(
    Comp *const C,
    const QProg *const p,
    QMem *const m,
    const QStmt **const of,
    const bool dbg
)
{
    const uint32_t guess = CACHE0 + (C->prog->end - PROGBASE) + XLIM + XSLK;

    if (allo_cap(C->allo, HCACHE) < guess)
        allo_grow(C->allo, HCACHE, guess);

    Addr base;

    do {
        base = C->prog->base;
        const QCgEnv env = {
            .base = base,
            .lines = p->src.lines,
            .sy = C->symb,
            .dbg = dbg,
        };

        QCg g = {.env = &env, .mem = m};

        C->link->end = C->allo->cache + CACHE0;

        for (uint32_t i = 0; i < p->src.nlines; i++)
            prog_line(C->prog, p->src.lines[i].at)->code = 0;

        for (uint32_t i = 0; i < p->src.nlines; i++) {
            const uint32_t at = p->src.lines[i].at;
            qlink_compile(C, &g, at, of[i]);
            link_place(C, at);
        }
    } while (C->prog->base != base);
}

void qlink_program(Comp *const C, QProg *const p, QMem *const m, const bool dbg)
{
    blok_reset(C->blok, C->fault);
    qlink_procs(C, p);
    qlink_vars(C, p);
    symb_size(C->symb, 0);

    const QStmt **const of = qast_alloc(m, (p->src.nlines ? p->src.nlines : 1) * sizeof *of);

    for (const QStmt *s = p->first; s; s = s->next)
        of[s->tok->line] = s;

    qlink_lines(C, p, m, of, dbg);

    for (uint32_t i = 1; i < C->blok->procs.n; i++)
        frame_layout(C->symb, C->decl, btab_at(&C->blok->procs, i), i);

    C->link->nmiss = 0;

    for (uint32_t i = 0; i < p->src.nlines; i++) {
        const uint32_t at = p->src.lines[i].at;
        link_resolve(C, at, prog_line(C->prog, at)->code);
    }

    link_check(C);
    C->link->linked = true;
}
