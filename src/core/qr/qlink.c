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
 * (body 0), its labels (qr/qcg.c) and the line's end (BOFF), then placed in the cache
 * (link_place). The
 * store moves when the cache grows, and a string literal's address with it: the lines are
 * compiled again until it stays. Then each procedure's frame is laid out and each line resolved
 * (link_resolve): its variables placed, its jumps found.
 */
#include "qr/qlink.h"
#include "comp/blocks.h"
#include "comp/declare.h"
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
        qlink_kind(p, elem, et, kind);
        *kind = qast_record(p, elem) ? VI_ARR : *kind | VI_ARR;

        if (qast_integer(elem))
            *et = qcg_scalar_et(elem);

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

/* Record t's TYPE's fields, a record each holds by value laid first; each field's offset. */
static void qlink_lay(Comp *const C, const QProg *const p, const QType t)
{
    QTy *const y = qast_ty(p, t);

    if (y->laid)
        return;

    y->laid = true;

    for (uint32_t k = 0; k < y->nfields; k++) {
        QField *const f = &y->fields[k];
        const bool list = qast_array(p, f->type);
        const QType elem = list ? qast_ty(p, f->type)->elem : f->type;
        const bool dyn = list && qast_ty(p, f->type)->kind == QT_LIST;
        uint32_t et;
        uint8_t kind;

        if (qast_record(p, elem) && !dyn)
            qlink_lay(C, p, elem);

        qlink_kind(p, elem, &et, &kind);

        if (qast_integer(elem))
            et = qcg_scalar_et(elem);

        decl_newfield(
            C->decl,
            y->td,
            qlink_name(C->symb, f->name),
            et,
            list && !dyn ? qast_ty(p, f->type)->count : 0,
            dyn
        );
        f->off = decl_type(C->decl, y->td)->fields[k].off;
    }

    const TypeDef *const td = decl_type(C->decl, y->td);
    y->size = td->size;
    y->deep = decl_dynamic(td);
}

/* Each record, tuple and enum with data a TYPE: each numbered, then laid out. */
static void qlink_types(Comp *const C, const QProg *const p)
{
    for (uint32_t i = 0; i < p->tys.n; i++)
        if (qast_record(p, QY_FIRST + i))
            p->tys.at[i].td =
                decl_newtype(C->decl, C->symb, qlink_name(C->symb, p->tys.at[i].name));

    for (uint32_t i = 0; i < p->tys.n; i++)
        if (qast_record(p, QY_FIRST + i))
            qlink_lay(C, p, QY_FIRST + i);
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
    return !f->sig || f->native || qast_inline(f) || qast_generic(f) || (f->test && !p->testing);
}

/* Each fn checked a procedure, a generic's instances but not it; its parameters and result
   linked in qlink_vars. */
static void qlink_procs(Comp *const C, const QProg *const p)
{
    for (QFn *f = p->fns; f; f = f->next) {
        if (qlink_none(p, f))
            continue;

        Proc *const pr = blok_add_proc(C->blok, C->fault);
        f->proc = C->blok->procs.n - 1;
        const uint32_t name =
            symb_find_name(C->symb, 0, (const uint8_t *)f->name->text, (uint8_t)f->name->len);
        symb_name(C->symb, name)->proc = f->proc;
        *pr = (Proc){
            .name = name,
            .kind = f->result == QY_VOID || f->byarg ? PK_SUB : PK_FUNC,
            .first = p->src.lines[f->def->line].at,
            .last = p->src.lines[f->end->line].at,
            .npar = (uint16_t)(qlink_npar(f) + f->byarg),
        };
    }
}

/* Variable v's scope: its fn's procedure (an inline generator's its host's), else the module's.
   A number's result is the module's, as a QBasic FUNCTION's (PRET.FV): return stores it right
   before PRET, so no call between can overwrite it, and the frame is a cell smaller. A record's or
   list's is its last parameter. */
static uint32_t qlink_scope(const QVar *const v)
{
    const QFn *const f = qast_home(v->fn);
    return f && (v->kind != QV_RESULT || f->byarg) ? f->proc : 0;
}

enum { INLINE_MAX = 256 }; /* an inline record's bytes at most: a variable for each 4 */

/* Does record variable v live inline, in cells: a let's, a var's or a temporary's record without
   strings or lists? */
static bool qlink_inline(const QProg *const p, const QVar *const v)
{
    if (!qast_record(p, v->type) || v->byref || v->cap ||
        (v->kind != QV_LET && v->kind != QV_VAR && v->kind != QV_TEMP))
        return false;

    const QTy *const y = qast_ty(p, v->type);
    return y->kind != QT_MAP && !y->deep && y->size <= INLINE_MAX;
}

/* Inline record v's 4-byte words: a field of 4 or 8 bytes at a multiple of 4 is one's variable. */
static uint32_t qlink_nwords(const QProg *const p, const QVar *const v)
{
    const uint32_t size = qast_ty(p, v->type)->size;
    return size < 4 ? 1 : size / 4;
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

    v->words = qlink_word(sy, scope, k);

    for (uint32_t i = 1; i < qlink_nwords(p, v); i++)
        qlink_word(sy, scope, k);
}

/* Each inline record's words out of their frames' layout: no cells of their own. */
static void qlink_unplace(Symb *const sy, const QProg *const p)
{
    for (const QVar *v = p->vars; v; v = v->next)
        for (uint32_t i = 0; v->words && i < qlink_nwords(p, v); i++)
            *symb_loc(sy, v->words + i) = (Loc){};
}

/* Each inline record's words placed in its cells, 4 bytes apart from its lowest. */
static void qlink_words(Symb *const sy, const QProg *const p)
{
    for (const QVar *v = p->vars; v; v = v->next)
        for (uint32_t i = 0; v->words && i < qlink_nwords(p, v); i++) {
            *symb_loc(sy, v->words + i) = symb_placed(sy, v->id);
            symb_var(sy, v->words + i)->word = (uint8_t)i;
        }
}

/* Each variable but a const's in the symbol table; a fn's parameters its ref cells. */
static void qlink_vars(Comp *const C, const QProg *const p)
{
    static const QTok hidden = {.text = "~", .len = 1};

    Symb *const sy = C->symb;
    uint32_t k = 0;

    for (QVar *v = p->vars; v; v = v->next) {
        if (v->constant || v->place ||
            (v->fn &&
             (v->fn->inl || v->fn->native || qast_generic(v->fn) || (v->fn->test && !p->testing))))
            continue;

        const QTok *const name = v->kind == QV_TEMP ? &hidden : v->name;

        if (qlink_inline(p, v))
            qlink_record(sy, p, v, name, qlink_scope(v), &k);
        else
            v->id = qlink_symb(sy, p, name, v->type, qlink_scope(v));
    }

    for (const QFn *f = p->fns; f; f = f->next) {
        if (qlink_none(p, f))
            continue;

        Proc *const pr = btab_at(&C->blok->procs, f->proc);
        const uint32_t n = pr->npar;
        pr->result = f->res && !f->byarg ? f->res->id : 0;
        pr->params = arena_alloc(&C->blok->arena, (n ? n : 1) * sizeof *pr->params);

        if (!pr->params)
            fault_error(C->fault, E_OM);

        for (uint32_t k = 0, at = 0; k < n; k++, at++) {
            while (at < f->npar && f->params[at].lam)
                at++;

            const uint32_t id = at < f->npar ? f->params[at].var->id : f->res->id;
            pr->params[k] = (Param){.var = id};

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
            .prog = p,
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
    case QS_MATCH:
        return 1;

    case QS_ENDIF:
    case QS_ENDWHILE:
    case QS_ENDFOR:
    case QS_ENDDO:
    case QS_ENDMATCH:
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
            (!s->fn->sig || qast_generic(s->fn) || (s->fn->test && !p->testing))) {
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

void qlink_program(Comp *const C, QProg *const p, QMem *const m, const bool dbg)
{
    blok_reset(C->blok, C->fault);
    qlink_types(C, p);
    qlink_procs(C, p);
    qlink_vars(C, p);
    symb_size(C->symb, 0);

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

    qlink_lines(C, p, m, of, dbg);

    qlink_unplace(C->symb, p);

    for (uint32_t i = 1; i < C->blok->procs.n; i++)
        frame_layout(C->symb, C->decl, btab_at(&C->blok->procs, i), i);

    qlink_words(C->symb, p);

    C->link->nmiss = 0;

    for (uint32_t i = 0; i < p->src.nlines; i++) {
        const uint32_t at = p->src.lines[i].at;
        link_resolve(C, at, prog_line(C->prog, at)->code);
    }

    qlink_chain(C, p);
    qlink_thread(C, p);

    link_check(C);
    C->link->linked = true;
}
