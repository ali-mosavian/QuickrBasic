/* Block pairing: each block statement's jump targets and each procedure's span, found before
 * a run.
 *
 * # How it works
 * blok_pair walks the program's statements in order with a stack of the open blocks. Each
 * block statement gets an entry (BlockEnt) in program order, so blok_find is a binary search
 * by position. A clause joins its block's chain; when the block closes, every entry on the
 * chain gets the target that leaves it. SUB, FUNCTION and DEF FN each get a Proc.
 *
 * A target is a position * 4 + a mode: 0 the statement, 1 past the JMP it starts with, 2 the
 * statement after it. The direct line's has PDIR and its body offset. A jump op holds it as a
 * TJUMP.
 *
 * # Example
 *   10 WHILE x        WHILE.done = the statement after WEND
 *   20   x = x - 1
 *   30 WEND           WEND.next = WHILE
 *
 * # asm structure (retired/asm32/comp/block.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_blocks | blok_pair | MIRROR; D1, D2 |
 * | bk_find | blok_find | MIRROR; the entry, not its index |
 * | bk_lineof | prog_lineof (mem/program.c) | MOVED |
 * | bk_proc | blok_procof | D3 |
 * | bk_line, bk_stmt, bk_next, bk_new, bk_push, bk_top, bk_fail, bk_clause, bk_close, bk_exits, bk_target | blok_scanline, blok_stmt, blok_next, blok_add, blok_push, blok_top, blok_fail, blok_clause, blok_close, blok_exits, blok_target | MIRROR |
 * | g_blk, g_prc, g_ppar, g_pbyv | Blok.blocks, Blok.procs, Proc.params | D4 |
 * | g_bst, NBST | BlokScan.open | MIRROR |
 *
 * - D1. The procedures' variables (result, parameters) are declared by link_start after the
 *   scan, in the same order.
 * - D2. Errors raise through Fault, not repl_error.
 * - D3. The procedure only; the caller sets the scopes (scop_proc).
 * - D4. The tables live in Blok's arena and link by pointer: a chain's next (link) and a
 *   CASE's SELECT (sel) are pointers, not indices.
 */
#include "comp/blocks.h"
#include <string.h>
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "io/msg.h"

enum { NBST = 64, BLOK_CHUNK = 0x4000 }; /* blocks open at most */

/* An open block: its kind, its opener, its last clause (elsed: an ELSE, no false test after it),
   its chain's head. */
typedef struct {
    uint8_t kind;
    bool elsed;
    BlockEnt *open, *last, *chain;
} BlokOpen;

/* blok_pair's scan. */
typedef struct {
    Blok *b;
    const Prog *pg;
    const Symb *sy;
    Fault *f;
    uint32_t line;       /* scanln: the line scanned */
    const uint8_t *body; /* (EBP) the body scanned */
    uint32_t off;        /* (ECX) the statement's offset in it */
    uint16_t depth;      /* bsp */
    BlokOpen open[NBST]; /* the open blocks */
    uint16_t blockcap, proccap;
    Proc *proc;   /* the procedure the scan is in, null none */
    bool isdef;   /* g_bkdef: the procedure is a DEF FN's */
    bool selcase; /* a SELECT scanned, its first CASE not yet */
} BlokScan;

/* By BK_*: an opener's word; the error a block of it left open gives. */
static const char *const words[] = {
    [BK_IF] = "IF",
    [BK_DO] = "DO",
    [BK_WHILE] = "WHILE",
    [BK_SELECT] = "SELECT",
    [BK_FOR] = "FOR",
};
static const uint8_t leftopen[] = {
    [BK_IF] = E_BIF,
    [BK_DO] = E_DWL,
    [BK_WHILE] = E_WWW,
    [BK_SELECT] = E_SWE,
    [BK_FOR] = E_FWN,
};

/* By the word after END: does it end a procedure? */
static const bool endproc[256] = {[TK_SUB] = true, [TK_FUNCTION] = true, [TK_DEF] = true};

[[noreturn]]
static void blok_fail(
    BlokScan *const sc,
    const uint8_t code,
    const uint8_t dg,
    const uint32_t at,
    const uint32_t line,
    const char *const other,
    const uint32_t len
)
{
    fault_raise(
        sc->f,
        (Failure){
            .code = code,
            .dg = dg,
            .at = at,
            .line = line,
            .other = other,
            .len = len,
        }

    );
}

[[noreturn]]
static void blok_failw(
    BlokScan *const sc,
    const uint8_t code,
    const uint8_t dg,
    const char *const word
)
{
    blok_fail(sc, code, dg, sc->off, sc->line, word, word ? (uint32_t)strlen(word) : 0);
}

/* @bk_er: Syntax error at the statement scanned. */
[[noreturn]]
static void blok_synerr(BlokScan *const sc)
{
    fault_raise(sc->f, (Failure){.code = E_SN, .at = sc->off, .line = sc->line});
}

[[noreturn]]
static void blok_nomem(BlokScan *const sc)
{
    fault_error(sc->f, E_OM);
}

static void *blok_alloc(BlokScan *const sc, const size_t size)
{
    void *const p = arena_alloc(&sc->b->arena, size);

    if (!p)
        blok_nomem(sc);

    return p;
}

/* Room in the pointer table *t, of n used and *cap, for one more. */
static void blok_room(BlokScan *const sc, void ***const t, const uint16_t n, uint16_t *const cap)
{
    if (n < *cap)
        return;

    const uint16_t cap2 = *cap ? *cap * 2 : 64;
    void **const g = arena_grow(&sc->b->arena, *t, *cap * sizeof **t, cap2 * sizeof **t);

    if (!g)
        blok_nomem(sc);

    *t = g;
    *cap = cap2;
}

/* The target of the statement at offset at of the line scanned. */
static uint32_t blok_target(const BlokScan *const sc, const uint32_t at, const JumpMode mode)
{
    return (sc->line + at + HDR) * 4 + mode;
}

BlockEnt *blok_find(const Blok *const b, const uint32_t line, const uint32_t at)
{
    const uint32_t key = line + at + HDR;
    uint32_t lo = 0;
    uint32_t hi = b->nblocks;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        const uint32_t pos = b->blocks[mid]->pos;

        if (pos == key)
            return b->blocks[mid];
        if (pos > key)
            hi = mid;
        else
            lo = mid + 1;
    }

    return nullptr;
}

const Proc blok_noproc;

/* The last procedure whose first line is at or before pos, 0 none: they are in the program's
   order, as blok_pair's scan met them. */
static uint8_t proc_before(const Blok *const b, const uint32_t pos)
{
    uint32_t lo = 0, hi = b->nprocs;

    while (hi - lo > 1) {
        const uint32_t mid = (lo + hi) / 2;

        if (b->procs[mid]->first <= pos)
            lo = mid;
        else
            hi = mid;
    }

    return (uint8_t)lo;
}

uint8_t blok_procof(const Blok *const b, const uint32_t line)
{
    const uint8_t i = proc_before(b, line);
    return i && line <= b->procs[i]->last ? i : 0;
}

uint32_t blok_line_before(const Blok *const b, const uint32_t pos)
{
    const uint8_t i = proc_before(b, pos);
    return i ? b->procs[i]->first : PROGBASE;
}

/* A new entry here, of kind. */
static BlockEnt *blok_add(BlokScan *const sc, const uint8_t kind)
{
    Blok *const b = sc->b;

    if (b->nblocks >= NBLK)
        blok_nomem(sc);

    blok_room(sc, (void ***)&b->blocks, b->nblocks, &sc->blockcap);
    BlockEnt *const e = blok_alloc(sc, sizeof *e);
    *e = (BlockEnt){.pos = sc->line + sc->off + HDR, .kind = kind};

    b->blocks[b->nblocks++] = e;
    return e;
}

/* Open a block of kind, its opener e. */
static void blok_push(BlokScan *const sc, const uint8_t kind, BlockEnt *const e)
{
    if (sc->depth >= NBST)
        blok_synerr(sc);

    sc->open[sc->depth++] = (BlokOpen){.kind = kind, .open = e, .last = e};
}

/* The innermost block, which must be of kind; else error code (X without Y), or blocks crossed. */
static BlokOpen *blok_top(BlokScan *const sc, const uint8_t kind, const uint8_t code)
{
    if (sc->depth) {
        BlokOpen *const o = &sc->open[sc->depth - 1];

        if (o->kind == kind)
            return o;
        for (uint32_t i = sc->depth; i-- > 0;)
            if (sc->open[i].kind == kind) /* open further out: crossed */
                blok_failw(sc, code, DG_BLOCKS_CROSSED, words[o->kind]);
    }

    blok_failw(sc, code, code == E_NF ? DG_NEXT_WITHOUT_FOR_END : 0, nullptr); /* by BC's name */
}

/* Clause e of block o: the last clause's false test comes here, mode; it joins END's chain. */
static void blok_clause(
    BlokScan *const sc,
    BlokOpen *const o,
    BlockEnt *const e,
    const JumpMode mode
)
{
    if (o->elsed) /* a clause after ELSE */
        blok_synerr(sc);

    o->last->next = blok_target(sc, sc->off, mode);
    o->last = e;
    o->elsed = e->kind == BK_ELSE;
    e->link = o->chain;
    o->chain = e;
}

/* The block o closes: its chain's clauses, its EXITs leave to t. */
static void blok_exits(BlokScan *const sc, const BlokOpen *const o, const uint32_t t)
{
    for (BlockEnt *c = o->chain; c; c = c->link)
        c->done = t;

    sc->depth--;
}

/* END IF, END SELECT here: the last clause's false test comes here, every clause leaves here. */
static void blok_close(BlokScan *const sc, const BlokOpen *const o)
{
    if (!o->elsed)
        o->last->next = blok_target(sc, sc->off, TO_STMT);

    blok_exits(sc, o, blok_target(sc, sc->off, TO_STMT));
}

static void blok_endif(BlokScan *const sc)
{
    const BlokOpen *const o = blok_top(sc, BK_IF, E_EIF);
    blok_add(sc, BK_ENDIF);
    blok_close(sc, o);
}

/* NEXT closes the innermost FOR, which counts with the variable at v (null any). Its entry, after
   prev (the NEXT's last, null none): next its FOR's body, sel its FOR. */
static BlockEnt *blok_next(BlokScan *const sc, const uint8_t *const v, BlockEnt *const prev)
{
    const BlokOpen *const o = blok_top(sc, BK_FOR, E_NF);

    if (v) {
        const uint8_t *const f = at(sc->pg->base + o->open->pos + 1); /* FOR v */
        if (*f == M_VAR) {
            if (comp_nameof(f) != comp_nameof(v) || comp_suffixof(f) != comp_suffixof(v))
                blok_fail(
                    sc,
                    E_NF,
                    DG_NEXT_WITHOUT_FOR_MISMATCH,
                    (uint32_t)(v - sc->body),
                    sc->line,
                    (const char *)f + 2,
                    f[1]
                ); /* the caret at NEXT's, {other} the FOR's */
        }
    }

    BlockEnt *e;

    if (prev) { /* NEXT a, b: b's on a's chain, not indexed */
        e = blok_alloc(sc, sizeof *e);
        *e = (BlockEnt){.pos = prev->pos, .kind = BK_NEXT};

        prev->link = e;
    } else
        e = blok_add(sc, BK_NEXT);

    e->sel = o->open;
    e->next = o->open->pos * 4 + TO_NEXT_STMT;

    const uint32_t t = blok_target(sc, sc->off, TO_NEXT_STMT);
    o->open->done = t;
    blok_exits(sc, o, t);
    return e;
}

/* The parameter list at q, past the '(': ( [BYVAL] n[()] [AS type] ), each into out (null: only
   counted); their count. */
static uint16_t blok_params(BlokScan *const sc, const uint8_t *q, Param *const out)
{
    uint16_t n = 0;

    for (;;) {
        const bool byval = *q == TK_BYVAL;

        if (byval)
            q++;
        if (*q != M_VAR)
            return n;
        if (out)
            out[n] = (Param){.at = (uint32_t)(q - at(sc->pg->base)), .byval = byval};

        n++;

        for (uint32_t depth = 0;; q = decl_item(q)) { /* on to its ',' or the ')' */
            const uint8_t c = *q;

            if (!c)
                return n;
            if (c == '(')
                depth++;
            if (c == ')') {
                if (!depth)
                    return n;

                depth--;
            }

            if (c == ',' && !depth) {
                q++;
                break;
            }
        }
    }
}

/* Past the parameter list at q, past the '('. */
static const uint8_t *blok_pastparams(const uint8_t *q)
{
    for (uint32_t depth = 0; *q; q = decl_item(q)) {
        if (*q == '(')
            depth++;
        if (*q == ')' && !depth--)
            return q + 1;
    }

    return q;
}

/* SUB, FUNCTION, DEF FN at s: a procedure's entry. Its variables are link_start's. */
static void blok_procstmt(BlokScan *const sc, const uint8_t *const s, const uint8_t kind)
{
    Blok *const b = sc->b;
    const uint32_t defoff = sc->off;

    if (sc->proc)
        blok_synerr(sc);
    if (s[1] >= TK_END) /* a keyword for its name */
        blok_fail(sc, E_SN, DG_NAME_OF_SUBPROGRAM_ILLEGAL_RESERVED, defoff + 1, sc->line, nullptr, 0);
    if (s[1] != M_VAR)
        blok_synerr(sc);
    if (b->nprocs >= NPROC)
        blok_nomem(sc);

    blok_room(sc, (void ***)&b->procs, b->nprocs, &sc->proccap);
    Proc *const p = blok_alloc(sc, sizeof *p);
    b->procs[b->nprocs++] = p;
    sc->proc = p;
    const uint8_t *q = s + 1;
    *p = (Proc){
        .at = (uint32_t)(q - at(sc->pg->base)),
        .name = comp_nameof(q),
        .kind = kind,
        .entry = blok_target(sc, sc->off, TO_NEXT_STMT), /* its body: the statement after */
        .first = sc->line,
        .last = sc->line,
        .skip = UINT32_MAX,
    };

    q = decl_item(q);

    if (*q == '(') {
        q++;
        p->npar = blok_params(sc, q, nullptr);

        if (p->npar) {
            p->params = blok_alloc(sc, p->npar * sizeof *p->params);
            blok_params(sc, q, p->params);
        }

        q = blok_pastparams(q);
    }

    if (*q == TK_STATIC)
        p->flags |= PF_STAT;
    if (sc->isdef) {
        sc->isdef = false; /* DEF FN: = e: the body is this statement's past its JMP */
        p->flags |= PF_DEF;

        if (*q == TK_EQ) {
            p->entry = blok_target(sc, defoff, PAST_JMP);
            p->skip = blok_target(sc, defoff, TO_NEXT_STMT);
            sc->proc = nullptr;
        }
    }
}

/* LOOP, WEND, kind: back to the opener (open), which leaves past it, as its EXITs do. */
static void blok_loopend(
    BlokScan *const sc,
    const uint8_t open,
    const uint8_t err,
    const uint8_t kind
)
{
    const BlokOpen *const o = blok_top(sc, open, err);
    BlockEnt *const e = blok_add(sc, kind);
    e->next = o->open->pos * 4 + TO_STMT;
    const uint32_t t = blok_target(sc, sc->off, TO_NEXT_STMT);
    o->open->done = t;
    blok_exits(sc, o, t);
}

/* The statement at s: a block statement's entry and the stack. */
static void blok_stmt(BlokScan *const sc, const uint8_t *const s)
{
    sc->off = (uint32_t)(s - sc->body);
    BlokOpen *o;
    BlockEnt *e;

    if (*s == TK_REM)
        return;
    if (sc->selcase && *s != TK_CASE && !(*s == TK_END && s[1] == TK_SELECT))
        blok_failw(sc, E_SLC, 0, nullptr);

    sc->selcase = false;

    switch (*s) {
    case TK_IF: { /* a block when THEN ends the line */
        const uint8_t *q = s;

        while (*q && *q != TK_THEN)
            q = decl_item(q);
        if (*q && (!q[1] || q[1] == TK_REM))
            blok_push(sc, BK_IF, blok_add(sc, BK_IF));

        return;
    }
    case TK_ELSEIF: /* the clause before goes here */
        o = blok_top(sc, BK_IF, E_ELS);
        blok_clause(sc, o, blok_add(sc, BK_ELSEIF), PAST_JMP);
        return;

    case TK_ELSE: /* at a statement's start: a block's */
        o = blok_top(sc, BK_IF, E_ELS);
        blok_clause(sc, o, blok_add(sc, BK_ELSE), PAST_JMP);
        return;

    case TK_ENDIF:
        blok_endif(sc);
        return;

    case TK_SELECT: /* its depth, for its value's variable */
        e = blok_add(sc, BK_SELECT);

        for (uint32_t k = 0; k < sc->depth; k++)
            if (sc->open[k].kind == BK_SELECT)
                e->aux++;

        blok_push(sc, BK_SELECT, e);
        sc->selcase = true;
        return;

    case TK_CASE: /* SELECT's or the case before's test goes here */
        o = blok_top(sc, BK_SELECT, E_CWS);
        e = blok_add(sc, BK_CASE);
        e->sel = o->open;
        blok_clause(sc, o, e, PAST_JMP);
        return;

    case TK_DO:
        blok_push(sc, BK_DO, blok_add(sc, BK_DO));
        return;

    case TK_WHILE:
        blok_push(sc, BK_WHILE, blok_add(sc, BK_WHILE));
        return;

    case TK_LOOP:
        blok_loopend(sc, BK_DO, E_LWD, BK_LOOP);
        return;

    case TK_WEND:
        blok_loopend(sc, BK_WHILE, E_WEW, BK_WEND);
        return;

    case TK_FOR: /* past its NEXT when not entered */
        blok_push(sc, BK_FOR, blok_add(sc, BK_FOR));
        return;

    case TK_NEXT: { /* NEXT a, b: a FOR closed for each, which must count with it */
        const uint8_t *v = nullptr;
        BlockEnt *e = nullptr;

        for (const uint8_t *q = s + 1;; q = decl_item(q)) {
            const uint8_t c = *q;

            if (!c || c == ':' || c == TK_ELSE || c == TK_REM)
                break;
            if (c == ',') {
                e = blok_next(sc, v, e);
                v = nullptr;
            }

            if (c == M_VAR)
                v = q;
        }

        blok_next(sc, v, e);
        return;
    }
    case TK_EXIT: { /* EXIT FOR, EXIT DO: on the chain of the innermost such block */
        const uint8_t kind = s[1] == TK_FOR ? BK_FOR : s[1] == TK_DO ? BK_DO : 0;

        if (!kind)
            return; /* EXIT SUB ..: not a block's */
        for (uint32_t k = sc->depth; k-- > 0;)
            if (sc->open[k].kind == kind) {
                e = blok_add(sc, BK_EXIT);
                e->aux = kind;
                e->link = sc->open[k].chain;
                sc->open[k].chain = e;
                return;
            }

        blok_failw(sc, E_EXW, 0, words[kind]);
    }
    case TK_SUB:
        blok_procstmt(sc, s, PK_SUB);
        return;

    case TK_FUNCTION:
        blok_procstmt(sc, s, PK_FUNC);
        return;

    case TK_DEF: /* DEF FNn: a FUNCTION of its own */
        if (s[1] != M_VAR)
            return;
        if (!(symb_name(sc->sy, comp_nameof(s + 1))->flags & NM_FN))
            blok_synerr(sc);

        sc->isdef = true;
        blok_procstmt(sc, s, PK_FUNC);
        return;

    case TK_END:
        if (s[1] == TK_IF)
            blok_endif(sc);
        else if (s[1] == TK_SELECT) {
            o = blok_top(sc, BK_SELECT, E_ESW);
            blok_add(sc, BK_ENDSEL);
            o->open->done = blok_target(sc, sc->off, TO_STMT);
            blok_close(sc, o);
        } else if (endproc[s[1]]) {
            if (!sc->proc)
                blok_failw(sc, E_EPW, 0, nullptr);

            sc->proc->last = sc->line;
            sc->proc->skip = blok_target(sc, sc->off, TO_NEXT_STMT);
            sc->proc = nullptr;
        }

        return;
    }
}

/* The statements of the body at b, and those after THEN and ELSE. */
static void blok_scanline(BlokScan *const sc, const uint8_t *const b)
{
    sc->body = b;
    const uint8_t *s = b;

    const bool label = *s == M_VAR && s[s[1] + 4] == ':';

    if (sc->selcase && (label || prog_line(sc->pg, sc->line)->num != NONUM)) {
        sc->off = 0;
        blok_failw(sc, E_SLC, 0, nullptr);
    }

    if (label) /* its statement after the ':' */
        s += s[1] + 5;
    for (;;) {
        while (*s == ':')
            s++;
        if (!*s)
            return;

        blok_stmt(sc, s);

        for (;;) {
            const uint8_t c = *s;

            if (!c || c == TK_REM)
                return;
            if (c == ':' || c == TK_ELSE || c == TK_THEN) { /* a one-line IF's: a statement after */
                s++;
                break;
            }

            if (c == TK_DATA) {
                s = comp_skipdata(s + 1);

                if (!*s)
                    return;

                continue;
            }

            s = decl_item(s);
        }
    }
}

void blok_pair(Blok *const b, const Prog *const pg, const Symb *const sy, Fault *const f)
{
    if (!b->arena.chunk)
        arena_init(&b->arena, BLOK_CHUNK);

    arena_reset(&b->arena);
    *b = (Blok){.arena = b->arena};

    BlokScan sc = {
        .b = b,
        .pg = pg,
        .sy = sy,
        .f = f,
    };

    blok_room(&sc, (void ***)&b->procs, 0, &sc.proccap);
    b->procs[0] = blok_alloc(&sc, sizeof(Proc)); /* the module */
    b->nprocs = 1;

    for (uint32_t ln = PROGBASE; prog_line(pg, ln)->len; ln += prog_line(pg, ln)->len) {
        sc.line = ln;
        blok_scanline(&sc, at(pg->base + ln + HDR));
    }

    uint32_t at_;

    if (sc.proc) { /* a SUB with no END SUB */
        const uint32_t ln = prog_lineof(pg, sc.proc->entry >> 2, &at_);
        blok_fail(&sc, E_PWE, 0, at_, ln, nullptr, 0);
    }

    if (sc.depth) { /* a block left open: at its opener */
        const BlokOpen *const o = &sc.open[sc.depth - 1];
        const uint32_t ln = prog_lineof(pg, o->open->pos, &at_);
        const uint8_t code = leftopen[o->kind];

        if (code == E_FWN) { /* FOR: {other} its variable */
            const uint8_t *const v = at(pg->base + ln + HDR + at_ + 1);

            if (*v == M_VAR)
                blok_fail(&sc, code, 0, at_, ln, (const char *)v + 2, v[1]);
        }

        blok_fail(&sc, code, 0, at_, ln, nullptr, 0);
    }
}
