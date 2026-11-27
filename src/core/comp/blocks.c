/* Block pairing: each block statement's jump targets and each procedure's span, found before
 * a run.
 *
 * # How it works
 * blok_pair walks the program's statements in order with a stack of the open blocks. Each
 * block statement gets an entry (BlockEnt) in program order, so blok_find is a binary search
 * by position. A clause joins its block's chain; when the block closes, every entry on the
 * chain gets the target that leaves it. SUB, FUNCTION and DEF FN each get a Proc.
 *
 * EXIT leaves the innermost loop of its kind, CONTINUE goes to that loop's closing statement
 * (NEXT, LOOP, WEND), which steps and tests as ever. A FOR's may name its counter: an outer loop.
 * Each counter of NEXT a, b is a part of its own, a statement to a jump (comp_part): leaving a's
 * loop comes to b's NEXT.
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
 * | g_bst, NBST | BlokScan.open | MIRROR; a block table (util/btab.h) |
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

enum { BLOK_CHUNK = 0x1000 };

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
    BTab open;           /* BlokOpen: the open blocks, the innermost last (bsp: its n) */
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

/* By the word after EXIT or CONTINUE: its loop's BK_*, 0 not a loop's. */
static const uint8_t loopkind[256] = {[TK_FOR] = BK_FOR, [TK_DO] = BK_DO, [TK_WHILE] = BK_WHILE};

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

/* A new entry of table t, zeroed. Out of memory. */
static void *blok_new(BlokScan *const sc, BTab *const t)
{
    void *const e = btab_add(t, &sc->b->arena);

    if (!e)
        blok_nomem(sc);

    return e;
}

/* Open block k, 0 the outermost. */
static BlokOpen *blok_open(const BlokScan *const sc, const uint32_t k) { return btab_at(&sc->open, k); }

/* The target of the statement at offset at of the line scanned. */
static uint32_t blok_target(const BlokScan *const sc, const uint32_t at, const JumpMode mode)
{
    return (sc->line + at + HDR) * 4 + mode;
}

BlockEnt *blok_find(const Blok *const b, const uint32_t line, const uint32_t at)
{
    const uint32_t key = line + at + HDR;
    uint32_t lo = 0;
    uint32_t hi = b->blocks.n;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        BlockEnt *const e = btab_at(&b->blocks, mid);
        const uint32_t pos = e->pos;

        if (pos == key)
            return e;
        if (pos > key)
            hi = mid;
        else
            lo = mid + 1;
    }

    return nullptr;
}

const Proc blok_noproc;

/* bylines made again when procedures were added since: blok_pair's in the program's order, then
   Quickr's (qr/qlink.c), each module's lines elsewhere in it, and placeholders of no line. Null:
   no memory, the procedures searched one by one. */
static void index_lines(Blok *const b)
{
    if (b->indexed == b->procs.n)
        return;

    b->indexed = b->procs.n;
    b->nbylines = 0;
    b->bylines = arena_alloc(&b->arena, b->procs.n * sizeof *b->bylines);

    for (uint32_t i = 1; b->bylines && i < b->procs.n; i++) {
        const uint32_t first = blok_proc(b, i)->first;
        uint32_t k = b->nbylines++;

        if (!first) {
            b->nbylines--;
            continue;
        }

        for (; k && blok_proc(b, b->bylines[k - 1])->first > first; k--)
            b->bylines[k] = b->bylines[k - 1];

        b->bylines[k] = i;
    }
}

/* The last procedure whose first line is at or before pos, 0 none. */
static uint32_t proc_before(Blok *const b, const uint32_t pos)
{
    index_lines(b);
    uint32_t best = 0;

    if (!b->bylines) {
        for (uint32_t i = 1; i < b->procs.n; i++) {
            const uint32_t first = blok_proc(b, i)->first;

            if (first && first <= pos && first >= blok_proc(b, best)->first)
                best = i;
        }

        return best;
    }

    uint32_t lo = 0, hi = b->nbylines;

    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;

        if (blok_proc(b, b->bylines[mid])->first <= pos)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo ? b->bylines[lo - 1] : 0;
}

uint32_t blok_procof(Blok *const b, const uint32_t line)
{
    const uint32_t i = proc_before(b, line);
    return i && line <= blok_proc(b, i)->last ? i : 0;
}

uint32_t blok_line_before(Blok *const b, const uint32_t pos)
{
    const uint32_t i = proc_before(b, pos);
    return i ? blok_proc(b, i)->first : PROGBASE;
}

/* A new entry here, of kind. */
static BlockEnt *blok_add(BlokScan *const sc, const uint8_t kind)
{
    Blok *const b = sc->b;

    BlockEnt *const e = blok_new(sc, &b->blocks);
    *e = (BlockEnt){.pos = sc->line + sc->off + HDR, .kind = kind};
    return e;
}

/* Open a block of kind, its opener e. */
static void blok_push(BlokScan *const sc, const uint8_t kind, BlockEnt *const e)
{
    *(BlokOpen *)blok_new(sc, &sc->open) = (BlokOpen){.kind = kind, .open = e, .last = e};
}

/* The innermost block, which must be of kind; else error code (X without Y), or blocks crossed. */
static BlokOpen *blok_top(BlokScan *const sc, const uint8_t kind, const uint8_t code)
{
    if (sc->open.n) {
        BlokOpen *const o = blok_open(sc, sc->open.n - 1);

        if (o->kind == kind)
            return o;
        for (uint32_t i = sc->open.n; i-- > 0;)
            if (blok_open(sc, i)->kind == kind) /* open further out: crossed */
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

/* The block o closes: its chain's clauses and EXITs leave to t, its CONTINUEs go to again. */
static void blok_exits(
    BlokScan *const sc,
    const BlokOpen *const o,
    const uint32_t t,
    const uint32_t again
)
{
    for (BlockEnt *c = o->chain; c; c = c->link)
        c->done = c->kind == BK_CONT ? again : t;

    sc->open.n--;
}

/* END IF, END SELECT here: the last clause's false test comes here, every clause leaves here. */
static void blok_close(BlokScan *const sc, const BlokOpen *const o)
{
    if (!o->elsed)
        o->last->next = blok_target(sc, sc->off, TO_STMT);

    const uint32_t t = blok_target(sc, sc->off, TO_STMT);
    blok_exits(sc, o, t, t);
}

static void blok_endif(BlokScan *const sc)
{
    const BlokOpen *const o = blok_top(sc, BK_IF, E_EIF);
    blok_add(sc, BK_ENDIF);
    blok_close(sc, o);
}

/* The counter of the FOR o: its name's token; null when no name follows FOR. */
static const uint8_t *blok_counter(const BlokScan *const sc, const BlokOpen *const o)
{
    const uint8_t *const f = at(sc->pg->base + o->open->pos + 1); /* FOR v */
    return *f == M_VAR ? f : nullptr;
}

/* The names at a and b are one variable's: the name and its suffix. */
static bool blok_same(const uint8_t *const a, const uint8_t *const b)
{
    return comp_nameof(a) == comp_nameof(b) && comp_suffixof(a) == comp_suffixof(b);
}

/* NEXT closes the innermost FOR, which counts with the variable at v (null any). Its entry, after
   prev (the NEXT's last, null none): next its FOR's body, sel its FOR. */
static BlockEnt *blok_next(BlokScan *const sc, const uint8_t *const v, BlockEnt *const prev)
{
    const BlokOpen *const o = blok_top(sc, BK_FOR, E_NF);
    const uint8_t *const f = blok_counter(sc, o);

    if (v && f && !blok_same(f, v))
        blok_fail(
            sc,
            E_NF,
            DG_NEXT_WITHOUT_FOR_MISMATCH,
            (uint32_t)(v - sc->body),
            sc->line,
            (const char *)f + 2,
            f[1]
        ); /* the caret at NEXT's, {other} the FOR's */

    BlockEnt *e;

    if (prev) { /* NEXT a, b: b's on a's chain, not indexed */
        e = blok_alloc(sc, sizeof *e);
        *e = (BlockEnt){.pos = prev->pos, .kind = BK_NEXT};

        prev->link = e;
    } else
        e = blok_add(sc, BK_NEXT);

    e->sel = o->open;
    e->next = o->open->pos * 4 + TO_NEXT_STMT;

    /* a counter after the first is a part of its own, at its name: the loop before leaves to it */
    const uint32_t part = prev && v ? (uint32_t)(v - sc->body) : sc->off;
    const uint32_t t = blok_target(sc, part, TO_NEXT_STMT);
    o->open->done = t;
    blok_exits(sc, o, t, blok_target(sc, part, TO_STMT));
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

/* FUNCTION p's result AS a TYPE or an array, ours: a parameter past its own, at its name; the
   caller passes the record or the array for it. */
static void blok_resultarg(BlokScan *const sc, Proc *const p)
{
    Param *const params = blok_alloc(sc, (p->npar + 1u) * sizeof *params);

    for (uint16_t k = 0; k < p->npar; k++)
        params[k] = p->params[k];

    params[p->npar++] = (Param){.at = p->at};
    p->params = params;
    p->flags |= PF_RESARG;
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

    Proc *const p = blok_new(sc, &b->procs);
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

    if (kind == PK_FUNC && *q == TK_AS) { /* ours */
        const uint8_t *const after = decl_item(q + 1);
        const bool arr = after[0] == '(' && after[1] == ')';

        if (q[1] == M_VAR && !symb_name(sc->sy, comp_nameof(q + 1))->rec) /* no such TYPE */
            blok_fail(sc, E_SN, 0, defoff + (uint32_t)(q + 1 - s), sc->line, nullptr, 0);
        if (q[1] == M_VAR || arr) { /* its body past the JMP: the result emptied there */
            blok_resultarg(sc, p);
            p->entry = blok_target(sc, defoff, PAST_JMP);
        }

        q = arr ? after + 2 : after;
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

/* DECLARE SUB|FUNCTION n CDECL [ALIAS "x"] (params) at s in a native module's interface: its
   procedure, its code the extension's (link binds it). No body, so its lines are none: it starts at
   the DECLARE's line and ends before it, and blok_procof never finds it. */
static void blok_native(BlokScan *const sc, const uint8_t *const s)
{
    if (!(prog_line(sc->pg, sc->line)->flags & LF_NATIVE) ||
        (s[1] != TK_SUB && s[1] != TK_FUNCTION) || s[2] != M_VAR)
        return;

    const uint8_t *q = decl_item(s + 2);

    if (q[0] != TK_X || q[1] != TK1_CDECL)
        return;

    q += 2;

    if (q[0] == TK_X && q[1] == TK1_ALIAS && q[2] == '"')
        q = decl_item(q + 2);

    Proc *const p = blok_new(sc, &sc->b->procs);
    *p = (Proc){
        .at = (uint32_t)(s + 2 - at(sc->pg->base)),
        .name = comp_nameof(s + 2),
        .kind = s[1] == TK_SUB ? PK_SUB : PK_FUNC,
        .flags = PF_NATIVE,
        .entry = 0, /* none: its code is the extension's */
        .first = sc->line,
        .last = sc->line - 1,
        .skip = UINT32_MAX,
    };

    if (*q != '(')
        return;

    q++;
    p->npar = blok_params(sc, q, nullptr);

    if (p->npar) {
        p->params = blok_alloc(sc, p->npar * sizeof *p->params);
        blok_params(sc, q, p->params);
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
    blok_exits(sc, o, t, blok_target(sc, sc->off, TO_STMT));
}

/* EXIT or CONTINUE (kind: BK_EXIT, BK_CONT; err its error) of the loop whose word is at w, a FOR's
   counter after it or none: on the chain of the innermost such loop. Not a loop's word: nothing
   (EXIT SUB ..). */
static void blok_leave(
    BlokScan *const sc,
    const uint8_t *const w,
    const uint8_t kind,
    const uint8_t err
)
{
    const uint8_t loop = loopkind[*w];
    const uint8_t *const v = loop == BK_FOR && w[1] == M_VAR ? w + 1 : nullptr;

    if (!loop)
        return;

    bool open = false;

    for (uint32_t k = sc->open.n; k-- > 0;) {
        BlokOpen *const o = blok_open(sc, k);

        if (o->kind != loop)
            continue;

        open = true;
        const uint8_t *const f = blok_counter(sc, o);

        if (v && f && !blok_same(f, v))
            continue;

        BlockEnt *const e = blok_add(sc, kind);
        e->aux = loop;
        e->link = o->chain;
        o->chain = e;
        return;
    }

    if (open) /* in such loops, none counting with v: the caret at it */
        blok_fail(
            sc,
            err,
            err == E_EXW ? DG_EXIT_NOT_WITHIN_MATCHING_BLOCK_COUNTER
                         : DG_CONTINUE_NOT_WITHIN_MATCHING_BLOCK_COUNTER,
            (uint32_t)(v - sc->body),
            sc->line,
            nullptr,
            0
        );

    blok_failw(sc, err, 0, words[loop]);
}

/* The statement at s: a block statement's entry and the stack. */
static void blok_stmt(BlokScan *const sc, const uint8_t *s)
{
    sc->off = (uint32_t)(s - sc->body);
    s += s[0] == TK_X && s[1] == TK1_PRIVATE ? 2
                                             : 0; /* PRIVATE SUB, FUNCTION: the statement its own */
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

        for (uint32_t k = 0; k < sc->open.n; k++)
            if (blok_open(sc, k)->kind == BK_SELECT)
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
    case TK_EXIT:
        blok_leave(sc, s + 1, BK_EXIT, E_EXW);
        return;

    case TK_X:
        if (s[1] == TK1_CONTINUE)
            blok_leave(sc, s + 2, BK_CONT, E_CNW);

        return;

    case TK_SUB:
        blok_procstmt(sc, s, PK_SUB);
        return;

    case TK_DECLARE:
        blok_native(sc, s);
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

    const bool label = *s == M_VAR && *comp_skipname(s) == ':';

    if (sc->selcase && (label || prog_line(sc->pg, sc->line)->num != NONUM)) {
        sc->off = 0;
        blok_failw(sc, E_SLC, 0, nullptr);
    }

    if (label) /* its statement after the ':' */
        s = comp_skipname(s) + 1;
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

            sc->b->nnames += c == M_VAR;
            s = decl_item(s);
        }
    }
}

Proc *blok_add_proc(Blok *const b, Fault *const f)
{
    Proc *const p = btab_add(&b->procs, &b->arena);

    if (!p)
        fault_error(f, E_OM);

    return p;
}

void blok_reset(Blok *const b, Fault *const f)
{
    if (!b->arena.chunk)
        arena_init(&b->arena, BLOK_CHUNK);

    arena_reset(&b->arena);
    *b = (Blok){.arena = b->arena};

    btab_init(&b->blocks, sizeof(BlockEnt));
    btab_init(&b->procs, sizeof(Proc));
    blok_add_proc(b, f); /* 0, the module */
}

void blok_pair(Blok *const b, const Prog *const pg, const Symb *const sy, Fault *const f)
{
    blok_reset(b, f);

    BlokScan sc = {
        .b = b,
        .pg = pg,
        .sy = sy,
        .f = f,
    };

    btab_init(&sc.open, sizeof(BlokOpen));

    for (uint32_t ln = PROGBASE; prog_line(pg, ln)->len; ln += prog_line(pg, ln)->len) {
        if (prog_line(pg, ln)->flags & LF_TEXT) /* a text dialect's line: its own */
            continue;

        sc.line = ln;
        blok_scanline(&sc, at(pg->base + ln + HDR));
    }

    uint32_t at_;

    if (sc.proc) { /* a SUB with no END SUB */
        const uint32_t ln = prog_lineof(pg, sc.proc->entry >> 2, &at_);
        blok_fail(&sc, E_PWE, 0, at_, ln, nullptr, 0);
    }

    if (sc.open.n) { /* a block left open: at its opener */
        const BlokOpen *const o = blok_open(&sc, sc.open.n - 1);
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
