/* Quickr's link: a checked program to what the VM runs, as link_start makes it for QBasic.
 *
 * # How it works
 * Each record, tuple and enum with data is a TYPE (comp/declare.h decl_newtype), one held by
 * value laid out first. Each fn is a procedure (comp/blocks.h), numbered in source order; a
 * record's or list's result is its last parameter. Each variable is one of the
 * symbol table's: a module's in its own cell, a fn's a local of its procedure's frame, a
 * parameter its ref cell. A name the table already has for that scope and type, another spelling
 * of it (x, X) or a shadowed one, gets a ' and a number: each variable its own cell.
 *
 * Each line is one statement at most: its code is compiled with a statement map of the statement
 * (body 0), its labels (qr/qcg_emit.c) and the line's end (BOFF), then placed in the cache
 * (link_place). The
 * store moves when the cache grows, and a string literal's address with it: the lines are
 * compiled again until it stays. Then each procedure's frame is laid out and each line resolved
 * (link_resolve): its variables placed, its jumps found.
 */
#include "qr/qlink.h"
#include "comp/bind.h"
#include "comp/blocks.h"
#include "comp/control.h"
#include "comp/declare.h"
#include "comp/export.h"
#include "comp/frame.h"
#include "comp/link.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/symbols.h"
#include "qr/qcg.h"
#include "util/grow.h"

enum { NAME_ROOM = 200 }; /* a name's bytes kept, its ' and number after */

/* Type t's ET_* and its variables' kind (mem/symbols.h): a record's ET_REC + its TYPE, a list's
   or an array's its element's (a short's ET_SH ..), VI_ARR. A scalar variable is a whole cell: a
   long's 8 bytes, a double's kind; a byte's, a short's .. an int's. */
static void qlink_kind(const QProg *const p, const QType t, uint32_t *const et, uint8_t *const kind)
{
    if (qast_array(p, t)) {
        const QType elem = qast_ty(p, t)->elem;
        const bool boxed = qast_record(p, elem) || qast_array(p, elem);
        qlink_kind(p, elem, et, kind);
        *kind = boxed ? VI_ARR : *kind | VI_ARR;

        if (qast_integer(elem))
            *et = qcg_scalar_et(elem);
        if (qast_array(p, elem))
            *et = ET_REC + qast_ty(p, elem)->td;

        return;
    }

    if (qast_wide(t)) {
        *et = ET_R8;
        *kind = VK_FLT | SK_DBL;
        return;
    }

    if (qast_record(p, t)) {
        *et = ET_REC + qast_ty(p, t)->td;
        *kind = VI_RECARR;
        return;
    }

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

/* A new variable named text's n bytes in scope: text, then text'k, text'k+1 .. till new, *k the
   next suffix tried. text has room for 12 bytes more. */
static uint32_t qlink_new(
    Symb *const sy,
    uint8_t *const text,
    const uint32_t n,
    const uint32_t et,
    const uint8_t kind,
    const uint32_t scope,
    uint32_t *const k
)
{
    for (;; ++*k) {
        uint32_t m = n;

        if (*k) {
            text[m++] = '\'';
            uint8_t rev[10];
            uint32_t d = 0;

            for (uint32_t v = *k; v; v /= 10)
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

/* A new variable named name in scope, of type t: the name, then name'1, name'2 .. till new. */
static uint32_t qlink_symb(
    Symb *const sy,
    const QProg *const p,
    const QTok *const name,
    const QType t,
    const uint32_t scope
)
{
    uint8_t text[NAME_ROOM + 12];
    const uint32_t n = name->len < NAME_ROOM ? name->len : NAME_ROOM;
    uint32_t et;
    uint8_t kind;
    uint32_t k = 0;
    qlink_kind(p, t, &et, &kind);
    memcpy(text, name->text, n);
    return qlink_new(sy, text, n, et, kind, scope, &k);
}

/* An inline record's hidden cell or word in scope, *k the next suffix. */
static uint32_t qlink_word(Symb *const sy, const uint32_t scope, uint32_t *const k)
{
    uint8_t text[2 + 12] = "~r";
    return qlink_new(sy, text, 2, ET_I4, VK_INT, scope, k);
}

static uint32_t qlink_name(Symb *const sy, const QTok *const t)
{
    return t ? symb_find_name(sy, 0, (const uint8_t *)t->text, (uint8_t)t->len) : 0;
}

/* An element of type t's ET_*: a record's or a boxed list's ET_REC + its TYPE. */
static uint32_t qlink_et(const QProg *const p, const QType t)
{
    uint32_t et;
    uint8_t kind;
    qlink_kind(p, t, &et, &kind);

    if (qast_integer(t))
        return qcg_scalar_et(t);

    return qast_array(p, t) ? ET_REC + qast_ty(p, t)->td : et;
}

static void qlink_lay(Comp *C, const QProg *p, QType t);

/* A field of type t added to TYPE td: a list's cell, a fixed array's elements inline, a record
   by value laid out first. */
static void qlink_field(Comp *const C, const QProg *const p, const uint32_t td, const QField *f)
{
    const QType t = f->type;
    const bool list = qast_array(p, t);
    const QType elem = list ? qast_ty(p, t)->elem : t;
    const bool dyn = list && qast_ty(p, t)->kind == QT_DYN;

    if ((qast_record(p, elem) || qast_array(p, elem)) && !dyn)
        qlink_lay(C, p, elem);

    decl_newfield(
        C->decl,
        td,
        qlink_name(C->symb, f->name),
        qlink_et(p, elem),
        list && !dyn ? qast_ty(p, t)->dims : 0,
        list && !dyn ? qast_ty(p, t)->counts : nullptr,
        dyn
    );
}

/* Record t's TYPE's fields, a record each holds by value laid first; each field's offset. A boxed
   list's TYPE, its one field the list. */
static void qlink_lay(Comp *const C, const QProg *const p, const QType t)
{
    QTy *const y = qast_ty(p, t);

    if (y->laid)
        return;

    y->laid = true;

    if (y->boxed) {
        const QField f = {.type = t};

        qlink_field(C, p, y->td, &f);
    }

    for (uint32_t k = 0; !y->boxed && k < y->nfields; k++) {
        qlink_field(C, p, y->td, &y->fields[k]);
        y->fields[k].off = decl_type(C->decl, y->td)->fields[k].off;
    }

    const TypeDef *const td = decl_type(C->decl, y->td);
    y->size = td->size;
    y->deep = decl_dynamic(td);
}

/* Record y's words from record of's fields, base bytes into y: each number's, while their count
   fits a byte. */
static void qlink_wordsof(
    const QProg *const p,
    QTy *const y,
    const QTy *const of,
    const uint32_t base
)
{
    for (uint32_t k = 0; k < of->nfields; k++) {
        const QType t = of->fields[k].type;
        const uint32_t off = base + of->fields[k].off;

        if (off >= QAST_WORDS || qast_array(p, t) || t == QY_STRING)
            continue;
        if (qast_record(p, t)) {
            if (qast_ty(p, t)->kind != QT_MAP)
                qlink_wordsof(p, y, qast_ty(p, t), off);

            continue;
        }

        if (y->nwords < UINT8_MAX)
            y->word[off] = (uint8_t)++y->nwords;
    }
}

/* Each record, tuple and enum with data a TYPE: each numbered, then laid out, its words found. */
static void qlink_types(Comp *const C, const QProg *const p, QMem *const m)
{
    for (uint32_t i = 0; i < p->tys.n; i++) /* a stage's before have theirs (qr/qr.c) */
        if (qast_record(p, QY_FIRST + i) && !p->tys.at[i].foreign && !p->tys.at[i].td)
            p->tys.at[i].td =
                decl_newtype(C->decl, C->symb, qlink_name(C->symb, p->tys.at[i].name));

    for (uint32_t i = 0; i < p->tys.n; i++) { /* each fixed array and list element: a TYPE */
        QTy *const y = &p->tys.at[i];
        QTy *const in =
            qast_array(p, QY_FIRST + i) && qast_array(p, y->elem) ? qast_ty(p, y->elem) : nullptr;
        QTy *const boxes[] = {y->kind == QT_ARRAY ? y : nullptr, in};

        for (uint32_t k = 0; k < countof(boxes); k++)
            if (boxes[k] && !boxes[k]->boxed) {
                boxes[k]->boxed = true;
                boxes[k]->td = decl_newtype(C->decl, C->symb, 0);
            }
    }

    for (uint32_t i = 0; i < p->tys.n; i++)
        if (qast_record(p, QY_FIRST + i) || p->tys.at[i].boxed)
            qlink_lay(C, p, QY_FIRST + i);

    for (uint32_t i = 0; i < p->tys.n; i++) {
        QTy *const y = &p->tys.at[i];

        if (!qast_record(p, QY_FIRST + i) || y->kind == QT_MAP || y->word)
            continue;

        y->word = qast_alloc(m, QAST_WORDS);
        qlink_wordsof(p, y, y, 0);
    }
}

/* Fn f's parameters a call passes: all but those bound to a lambda. */
static uint32_t qlink_npar(const QFn *const f)
{
    uint32_t n = 0;

    for (uint32_t k = 0; k < f->npar; k++)
        n += !f->params[k].lam;

    return n;
}

/* Does fn f run nowhere: unchecked, native, inline, a generic's model, a test without --test? */
static bool qlink_none(const QProg *const p, const QFn *const f)
{
    return !f->sig || f->idle || f->native || qast_inline(f) || qast_generic(f) ||
           (f->test && !p->testing);
}

/* Does QBasic call the fn token t names (QMod.called)? */
static bool qlink_called(const QProg *const p, const QTok *const t)
{
    return t && t->line < p->src.nlines && p->mods[p->src.lines[t->line].mod].called;
}

/* The module of the fn token t names: one QBasic calls's (mem/module.h), else 0. */
static uint32_t qlink_modu(const QProg *const p, const QTok *const t)
{
    return qlink_called(p, t) ? p->mods[p->src.lines[t->line].mod].modu : 0;
}

/* Each fn checked a procedure, a generic's instances but not it; its parameters and result
   linked in qlink_vars. One QBasic calls: named in its module, called as a FUNCTION or a SUB from
   its first line (PF_FOREIGN), a record's result its last parameter (PF_RESARG). */
static void qlink_procs(Comp *const C, const QProg *const p)
{
    for (QFn *f = p->fns; f; f = f->next) {
        if (qlink_none(p, f))
            continue;

        const bool qb = qlink_called(p, f->name);
        Proc *const pr = blok_add_proc(C->blok, C->fault);
        const uint32_t m = qlink_modu(p, f->name);
        f->proc = C->blok->procs.n - 1;
        const uint32_t name =
            symb_find_name(C->symb, m, (const uint8_t *)f->name->text, (uint8_t)f->name->len);
        symb_name(C->symb, name)->proc = f->proc;
        *pr = (Proc){
            .name = name,
            .kind = f->result == QY_VOID || (f->byarg && !qb) ? PK_SUB : PK_FUNC,
            .first = p->src.lines[f->def->line].at,
            .last = p->src.lines[f->end->line].at,
            .npar = (uint16_t)(qlink_npar(f) + f->byarg),
        };

        if (qb)
            *pr = (Proc){
                .name = pr->name,
                .kind = pr->kind,
                .flags = PF_FOREIGN | (f->byarg ? PF_RESARG : 0),
                .entry = pr->first,
                .first = pr->first,
                .last = pr->last,
                .module = sc_module(m),
                .npar = pr->npar,
            };
    }
}

/* Variable v's scope: its fn's procedure (an inline generator's its host's), else the module's.
   A number's result is the module's, as a QBasic FUNCTION's (PRET.FV): return stores it right
   before PRET, so no call between can overwrite it, and the frame is a cell smaller. A record's or
   list's is its last parameter. */
static uint32_t qlink_scope(const QProg *const p, const QVar *const v)
{
    const QFn *const f = qast_home(v->fn);
    return f && (v->kind != QV_RESULT || f->byarg) ? f->proc : sc_module(qlink_modu(p, v->name));
}

enum { INLINE_MAX = 4096 }; /* an inline record's bytes at most, as a fixed array's on the stack */

/* Does record variable v live inline, in cells: a let's, a var's or a temporary's record without
   strings or lists? */
static bool qlink_inline(const QProg *const p, const QVar *const v)
{
    if (!qast_record(p, v->type) || v->byref || v->cap ||
        (v->kind != QV_LET && v->kind != QV_VAR && v->kind != QV_TEMP))
        return false;

    const QTy *const y = qast_ty(p, v->type);
    return y->kind != QT_MAP && y->size <= INLINE_MAX;
}

/* Inline record v's words: each a variable. */
static uint32_t qlink_nwords(const QProg *const p, const QVar *const v)
{
    return qast_ty(p, v->type)->nwords;
}

/* Record v inline in scope: its cells, v's the lowest, then its words. A frame's newest local is
   its deepest (comp/frame.c), so there v's cell is made last. */
static void qlink_record(
    Symb *const sy,
    const QProg *const p,
    QVar *const v,
    const QTok *const name,
    const uint32_t scope,
    uint32_t *const k
)
{
    const uint32_t cells = (qast_ty(p, v->type)->size + sizeof(Cell) - 1) / sizeof(Cell);

    if (!scope)
        v->id = qlink_symb(sy, p, name, QY_INT, scope);

    for (uint32_t i = 1; i < cells; i++)
        qlink_word(sy, scope, k);

    if (scope)
        v->id = qlink_symb(sy, p, name, QY_INT, scope);

    /* an int's cells; the type is the debugger's (vm/inspect.c) */
    symb_var(sy, v->id)->type = ET_REC + qast_ty(p, v->type)->td;
    v->cells = true;

    for (uint32_t i = 0; i < qlink_nwords(p, v); i++) {
        const uint32_t w = qlink_word(sy, scope, k);
        v->words = i ? v->words : w;
    }
}

/* Each inline record's words out of their frames' layout: no cells of their own. */
static void qlink_unplace(Symb *const sy, const QProg *const p)
{
    for (const QVar *v = p->vars; v; v = v->next)
        for (uint32_t i = 0; v->words && i < qlink_nwords(p, v); i++)
            *symb_loc(sy, v->words + i) = (Loc){};
}

/* Each inline record's words placed in its cells, at their offsets from its lowest. */
static void qlink_words(Symb *const sy, const QProg *const p)
{
    for (const QVar *v = p->vars; v; v = v->next) {
        const QTy *const y = v->cells ? qast_ty(p, v->type) : nullptr;

        for (uint32_t off = 0; y && off < QAST_WORDS; off++) {
            if (!y->word[off])
                continue;

            const uint32_t w = v->words + y->word[off] - 1;
            *symb_loc(sy, w) = symb_placed(sy, v->id);
            symb_var(sy, w)->word = (uint8_t)off;
        }
    }
}

/* A QBasic program's guest's parameter v of type t: a short's SHORT, a bool's INTEGER (true -1). */
static void qlink_typed(Var *const v, const QType t)
{
    if (t == QY_SHORT)
        v->type = ET_SH;
    if (t == QY_BOOL)
        v->type = ET_I2;
}

/* Each variable but a const's in the symbol table; a fn's parameters its ref cells. */
static void qlink_vars(Comp *const C, const QProg *const p)
{
    static const QTok hidden = {.text = "~", .len = 1};

    Symb *const sy = C->symb;
    uint32_t k = 0;

    for (QVar *v = p->vars; v; v = v->next) {
        if (v->constant || v->place ||
            (v->fn && (v->fn->inl || v->fn->idle || v->fn->native || qast_generic(v->fn) ||
                       (v->fn->test && !p->testing))))
            continue;

        const QTok *const name = v->kind == QV_TEMP || v->kind == QV_RESULT ? &hidden : v->name;

        if (qlink_inline(p, v))
            qlink_record(sy, p, v, name, qlink_scope(p, v), &k);
        else
            v->id = qlink_symb(sy, p, name, v->type, qlink_scope(p, v));
    }

    for (const QFn *f = p->fns; f; f = f->next) {
        if (qlink_none(p, f))
            continue;

        Proc *const pr = btab_at(&C->blok->procs, f->proc);
        const bool qb = qlink_called(p, f->name);
        const uint32_t n = pr->npar;
        pr->result = f->res && !f->byarg ? f->res->id : 0;
        pr->params = arena_alloc(&C->blok->arena, (n ? n : 1) * sizeof *pr->params);

        if (!pr->params)
            fault_error(C->fault, E_OM);

        for (uint32_t k = 0, at = 0; k < n; k++, at++) {
            while (at < f->npar && f->params[at].lam)
                at++;

            const uint32_t id = at < f->npar ? f->params[at].var->id : f->res->id;
            pr->params[k] = (Param){.var = id, .byval = qb && at < f->npar && !f->params[at].mut};

            if (qb && at < f->npar) /* QBasic's by reference: its variable's type exactly */
                qlink_typed(symb_var(sy, id), f->params[at].var->type);

            symb_varx(sy, id)->par = 1;
            *symb_loc(sy, id) = loc_ref((uint32_t)(2 * sizeof(Cell) * (n - 1u - k)));
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

/* Each line compiled into the cache from link.end. */
static void qlink_pass(
    Comp *const C,
    const QProg *const p,
    QMem *const m,
    const QStmt *const *const of,
    const bool dbg
)
{
    const QCgEnv env = {
        .prog = p,
        .base = C->prog->base,
        .lines = p->src.lines,
        .sy = C->symb,
        .dbg = dbg,
    };

    QCg g = {.env = &env, .mem = m};

    for (uint32_t i = 0; i < p->src.nlines; i++)
        prog_line(C->prog, p->src.lines[i].at)->code = 0;

    for (uint32_t i = 0; i < p->src.nlines; i++) {
        const uint32_t at = p->src.lines[i].at;
        qlink_compile(C, &g, at, of[i]);
        link_place(C, at);
    }
}

/* Each line compiled into the cache, again till the store stays where it is; the QBasic modules'
   first when qb. */
static void qlink_lines(
    Comp *const C,
    const QProg *const p,
    QMem *const m,
    const QStmt *const *const of,
    const bool dbg,
    const bool qb
)
{
    const uint32_t guess = CACHE0 + (C->prog->end - PROGBASE) + XLIM + XSLK;

    if (allo_cap(C->allo, HCACHE) < guess)
        allo_grow(C->allo, HCACHE, guess);

    Addr base;

    do {
        base = C->prog->base;
        C->link->end = C->allo->cache + CACHE0;

        if (qb)
            link_lines_qb(C);

        qlink_pass(C, p, m, of, dbg);
    } while (C->prog->base != base);
}

/* Where code at code of line goes on to: past a line's end (EOL.TO) and a JMP, to the first op
   that does something; a few hops at most, so a loop of JMPs stays one. */
static void qlink_onward(const Comp *const C, uint32_t *const line, uint32_t *const code)
{
    for (uint32_t hop = 0; hop < 16; hop++) {
        const uint8_t *const op = at(C->allo->cache + *code);
        const uint32_t to = load32(op + 1);
        const uint32_t dest = load32(op + (*op == O_JMP ? 9 : 5));

        if (*op == O_EOL_TO && !prog_line(C->prog, to)->len)
            return;
        if (*op == O_JMP && dest <= TO_NEXT_STMT) /* not found */
            return;
        if (*op != O_EOL_TO && *op != O_JMP)
            return;

        *line = to;
        *code = dest;
    }
}

/* Each jump's target and each line's end gone on past what only passes control on: a line with
   no statement (a comment, a block lambda's lines where it is written), a block's end that only
   jumps, a yield's jump to its loop's body. */
static void qlink_thread(Comp *const C, const QProg *const p)
{
    for (uint32_t i = 0; i < p->src.nlines; i++) {
        uint8_t *op = at(C->allo->cache + prog_line(C->prog, p->src.lines[i].at)->code);

        for (; *op != O_EOL && *op != O_EOL_TO; op += link_opsize(op)) {
            uint8_t *const j = (uint8_t *)link_jump(op);
            uint32_t line = j ? load32(j) : 0, code = j ? load32(j + 8) : 0;

            if (!j || code <= TO_NEXT_STMT)
                continue;

            qlink_onward(C, &line, &code);
            store32(j, line);
            store32(j + 8, code);
        }

        uint32_t line = load32(op + 1), code = load32(op + 5);

        if (*op != O_EOL_TO || !prog_line(C->prog, line)->len)
            continue;

        qlink_onward(C, &line, &code);
        store32(op + 1, line);
        store32(op + 5, code);
    }
}

/* Line line, the line after module mod's last in the store, set as to in mod's code: each jump
   past a line's end and each line's end that goes there. */
static void qlink_retarget(
    Comp *const C,
    const QProg *const p,
    const QMod *const mod,
    const uint32_t line,
    const uint32_t to
)
{
    const uint32_t code = prog_line(C->prog, to)->code;

    for (uint32_t i = mod->first; i < mod->first + mod->nlines; i++) {
        uint8_t *op = at(C->allo->cache + prog_line(C->prog, p->src.lines[i].at)->code);

        for (; *op != O_EOL && *op != O_EOL_TO; op += link_opsize(op)) {
            uint8_t *const j = (uint8_t *)link_jump(op);

            if (j && load32(j) == line) {
                store32(j, to);
                store32(j + 8, code);
            }
        }

        if (*op == O_EOL_TO && (load32(op + 1) == line || !load32(op + 1))) {
            store32(op + 1, to);
            store32(op + 5, code);
        }
    }
}

/* The run in p->order, not the store's: what goes past each module's last line goes to the next
   module's first, past the main's to p->end's when set; the run's first line the first module's. */
static void qlink_chain(Comp *const C, const QProg *const p)
{
    const QMod *last = nullptr;
    C->link->start = 0;

    for (uint32_t i = 0; i <= p->nmods; i++) {
        const QMod *const mod = i < p->nmods ? &p->mods[p->order[i]] : nullptr;

        if (mod && !mod->nlines)
            continue;
        if (!mod && !p->end)
            break;

        const uint32_t first = p->src.lines[mod ? mod->first : p->end - 1].at;

        if (!last)
            C->link->start = first;

        if (last) {
            const uint32_t end = p->src.lines[last->first + last->nlines - 1].at;
            qlink_retarget(C, p, last, end + prog_line(C->prog, end)->len, first);
        }

        last = mod;
    }
}

/* Does s open a block: its lines till its end its own? */
static int32_t qlink_depth(const QStmt *const s)
{
    switch (s->kind) {
    case QS_IF:
        return !s->oneline;

    case QS_WHILE:
    case QS_FOR:
    case QS_DO:
    case QS_SELECT:
        return 1;

    case QS_ENDIF:
    case QS_ENDWHILE:
    case QS_ENDFOR:
    case QS_ENDDO:
    case QS_ENDSELECT:
        return -1;

    default:
        return 0;
    }
}

/* Each statement to its line's of: a generic's and an unrun test's none, their instances' and
   procedures' code their own. --test: the main's top level its declarations alone. */
static void qlink_stmts(const QProg *const p, const QStmt **const of)
{
    int32_t depth = 0;

    for (const QStmt *s = p->first; s; s = s->next) {
        if (s->kind == QS_FN &&
            (!s->fn->sig || s->fn->idle || qast_generic(s->fn) || (s->fn->test && !p->testing))) {
            s = s->fn->end;
            continue;
        }

        if (s->kind == QS_FN) {
            for (const QStmt *const end = s->fn->end; s != end; s = s->next)
                of[s->line] = s;

            of[s->line] = s;
            continue;
        }

        const bool main = !p->src.lines[s->line].mod;
        const int32_t d = qlink_depth(s);
        depth += d;

        if (!p->testing || !main || (!depth && !d && s->kind == QS_DECL))
            of[s->line] = s;
    }
}

/* Each QBasic procedure's PCALL Jump (comp/control.h), from its entry; a native one has none. */
static void qlink_foreign(Comp *const C, const QProg *const p)
{
    C->line = DLINE;

    for (QFn *f = p->foreign; f; f = f->next) {
        if (f->ncall)
            continue;

        const Jump j = ctrl_jump(C, blok_proc(C->blok, f->proc)->entry);
        f->entry[0] = j.line;
        f->entry[1] = j.body;
        f->entry[2] = j.dest;
    }
}

/* Each inline record's words placed, every frame laid out; each line resolved. */
static void qlink_resolve(Comp *const C, const QProg *const p)
{
    qlink_words(C->symb, p);

    for (uint32_t i = 0; i < p->src.nlines; i++) {
        const uint32_t at = p->src.lines[i].at;
        link_resolve(C, at, prog_line(C->prog, at)->code);
    }
}

void qlink_program(
    Comp *const C,
    QProg *const p,
    QMem *const m,
    const Exports *const ex,
    const bool dbg
)
{
    bool qb = false; /* QBasic modules: blocks paired, procedures made (comp/link.h link_prepare) */

    for (uint32_t k = 0; k < p->nmods; k++)
        qb |= p->mods[k].ex != nullptr;

    if (qb)
        link_prepare(C);
    else
        blok_reset(C->blok, C->fault);

    qlink_foreign(C, p);
    qlink_types(C, p, m);
    const QStmt *const *const of = qlink_guest_made(C, p, m, ex);

    if (qb) /* each QBasic import's name of a Quickr fn: its procedure */
        bind_procs(C->symb);

    qlink_lines(C, p, m, of, dbg, qb);

    if (qb)
        link_raise(C);

    qlink_unplace(C->symb, p);

    for (uint32_t i = 1; i < C->blok->procs.n; i++)
        frame_layout(C->symb, C->decl, btab_at(&C->blok->procs, i), i);

    C->link->nmiss = 0;

    if (qb)
        link_resolve_qb(C);

    qlink_resolve(C, p);
    qlink_chain(C, p);
    qlink_thread(C, p);

    link_check(C);
    C->link->linked = true;
}

void qlink_guest_types(Comp *const C, const QProg *const p, QMem *const m) { qlink_types(C, p, m); }

/* The exports ex of each module QBasic calls (by number): its consts literals; each fn QBasic
   can't call a procedure of no entry, an error where called (comp/refs.c). */
static void qlink_exports(Comp *const C, const QProg *const p, QMem *const m, const Exports *ex)
{
    for (uint32_t k = 1; k < p->nmods; k++)
        if (p->mods[k].called)
            expo_literals(C, p->mods[k].modu, &ex[k], &m->arena);

    for (uint32_t k = 1; k < p->nmods; k++)
        for (uint32_t i = 0; p->mods[k].called && i < ex[k].nprocs; i++) {
            const ExpProc *const e = &ex[k].procs[i];

            if (!e->why)
                continue;

            const uint32_t n =
                symb_find_name(C->symb, p->mods[k].modu, (const uint8_t *)e->name, (uint8_t)e->len);
            Proc *const pr = blok_add_proc(C->blok, C->fault);
            *pr = (Proc){.name = n, .kind = PK_SUB, .flags = PF_FOREIGN};

            symb_name(C->symb, n)->proc = C->blok->procs.n - 1;
        }
}

const QStmt *const *qlink_guest_made(
    Comp *const C,
    const QProg *const p,
    QMem *const m,
    const Exports *const ex
)
{
    qlink_foreign(C, p); /* its QBasic imports' procedures */
    qlink_procs(C, p);
    qlink_vars(C, p);
    symb_size(C->symb, 0);
    qlink_exports(C, p, m, ex);

    const QStmt **const of = qast_alloc(m, (p->src.nlines ? p->src.nlines : 1) * sizeof *of);

    qlink_stmts(p, of);

    for (const QFn *f = p->fns; f; f = f->next) /* a block lambda's lines are its own */
        for (const QStmt *s = f->lamof && f->sig ? f->def : nullptr; s; s = s->next)
            of[s->line] = s;

    if (p->atend)
        of[p->atend->line] = p->atend;

    for (uint32_t i = 0; i < p->nruns; i++) {
        of[p->runs[i]->line] = p->runs[i];
        of[p->runs[i]->link->line] = p->runs[i]->link;
    }

    if (p->testend)
        of[p->testend->line] = p->testend;

    return of;
}

void qlink_guest_lines(
    Comp *const C,
    const QProg *const p,
    QMem *const m,
    const QStmt *const *const of,
    const bool dbg
)
{
    qlink_pass(C, p, m, of, dbg);
    qlink_unplace(C->symb, p);
}

void qlink_guest_resolve(Comp *const C, const QProg *const p)
{
    qlink_resolve(C, p);
    qlink_thread(C, p);
}

bool qlink_direct(
    Comp *const C,
    QProg *const p,
    QMem *const m,
    QVar *const from,
    const QStmt *const s,
    const Addr text
)
{
    uint32_t k = 0;

    for (QVar *v = from; v; v = v->next, k++) {
        if (v->place) /* an inlined method's parameter, read where its argument is */
            continue;
        if (qast_record(p, v->type) || qast_array(p, v->type))
            return false;

        uint8_t name[12] = "~q"; /* ~q0, ~q1 ..: the next probe's again */
        uint32_t n = 2;
        uint32_t et;
        uint8_t kind;

        for (uint32_t d = k; n == 2 || d; d /= 10)
            name[n++] = (uint8_t)('0' + d % 10);

        qlink_kind(p, v->type, &et, &kind);
        const uint32_t id = symb_find_name(C->symb, 0, name, (uint8_t)n);
        v->id = symb_find_var(C->symb, id, et, kind, 0, false);
    }

    const QCgEnv env = {
        .prog = p,
        .base = text - DLINE - HDR, /* s's line's at is DLINE: its literals at text */
        .lines = p->src.lines,
        .sy = C->symb,
    };

    QCg g = {.env = &env, .mem = m};

    qlink_compile(C, &g, DLINE, s);
    return true;
}
