/* Block statements paired across lines, before a run (comp/block.asm). */
#include "comp/blok.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "io/diag.h"
#include "io/repl.h"
#include "mem/prog.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "vm/exec.h"


enum { ELSED = 0x8000 };


/* By BK_*: an opener's word; the error a block of it left open gives. */
static const char *const words[] = {
    [BK_IF] = "IF", [BK_DO] = "DO", [BK_WHILE] = "WHILE", [BK_SELECT] = "SELECT", [BK_FOR] = "FOR",
};
static const uint8_t leftopen[] = {
    [BK_IF] = E_BIF, [BK_DO] = E_DWL, [BK_WHILE] = E_WWW, [BK_SELECT] = E_SWE, [BK_FOR] = E_FWN,
};

/* By the word after END: does it end a procedure? */
static const bool endproc[256] = {[TK_SUB] = true, [TK_FUNCTION] = true, [TK_DEF] = true};

[[noreturn]] static void fail(Session *const S, const uint8_t code, const uint8_t dg, const uint32_t at,
                              const uint32_t line, const char *const other, const uint32_t len)
{
    diag_raise(S, code, dg, (uint16_t)at, line, other, len);
}

[[noreturn]] static void failw(Session *const S, const uint8_t code, const uint8_t dg, const char *const word)
{
    fail(S, code, dg, S->blok.off, S->blok.line, word, word ? (uint32_t)strlen(word) : 0);
}

/* @bk_er: Syntax error at the line scanned. */
[[noreturn]] static void synerr(Session *const S)
{
    S->vm.line = S->blok.line;
    repl_error(S, E_SN);
}

/* The target of the statement at offset at of the line scanned. */
static uint32_t target(Session *const S, const uint32_t at, const JumpMode mode) { return (S->blok.line + at + HDR) * 4 + mode; }

uint32_t blok_lineof(Session *const S, const uint32_t pos, uint32_t *const at)
{
    uint32_t line = PROGBASE;
    while (pos >= line + prog_line(&S->prog, line)->len)
        line += prog_line(&S->prog, line)->len;
    *at = pos - line - HDR;
    return line;
}

int32_t blok_find(Session *const S, const uint32_t line, const uint32_t at)
{
    const uint32_t key = line + at + HDR;
    uint32_t lo = 0;
    uint32_t hi = S->blok.nblocks;
    while (lo < hi) {
        const uint32_t mid = (lo + hi) / 2;
        const uint32_t pos = S->blok.blocks[mid].pos;
        if (pos == key)
            return (int32_t)mid;
        if (pos > key)
            hi = mid;
        else
            lo = mid + 1;
    }
    return -1;
}

/* The procedure line is in, 0 none. */
static uint8_t proc_of(Session *const S, const uint32_t line)
{
    for (uint32_t i = S->blok.nprocs; i-- > 1;) {
        const Proc *const p = &S->blok.procs[i];
        if ((uint16_t)line >= p->first && (uint16_t)line <= p->last)
            return (uint8_t)i;
    }
    return 0;
}

uint8_t blok_scope(Session *const S, const uint32_t line)
{
    const uint8_t in = proc_of(S, line);
    scop_proc(S, in);
    return in;
}

/* A new entry here, of kind; its index. */
static uint16_t add(Session *const S, const uint8_t kind)
{
    if (S->blok.nblocks >= NBLK)
        repl_error(S, E_OM);
    const uint16_t i = S->blok.nblocks++;
    S->blok.blocks[i] = (BlockEnt){.pos = S->blok.line + S->blok.off + HDR, .kind = kind};
    return i;
}

/* Open a block of kind, its opener entry i. */
static void push(Session *const S, const uint8_t kind, const uint16_t i)
{
    if (S->blok.depth >= NBST)
        synerr(S);
    S->blok.open[S->blok.depth++] = (Open){.kind = kind, .open = i, .last = i, .chain = 0};
}

/* The innermost block, which must be of kind; else error code (X without Y), or blocks crossed. */
static Open *top(Session *const S, const uint8_t kind, const uint8_t code)
{
    if (S->blok.depth) {
        Open *const o = &S->blok.open[S->blok.depth - 1];
        if (o->kind == kind)
            return o;
        for (uint32_t i = S->blok.depth; i-- > 0;)
            if (S->blok.open[i].kind == kind) /* open further out: crossed */
                failw(S, code, DG_BLOCKS_CROSSED, words[o->kind]);
    }
    failw(S, code, code == E_NF ? DG_NEXT_WITHOUT_FOR_END : 0, nullptr); /* by BC's name */
}

/* Clause i of block o: the last clause's false test comes here, mode; it joins END's chain. */
static void clause(Session *const S, Open *const o, const uint16_t i, const JumpMode mode)
{
    if (o->last & ELSED) /* a clause after ELSE */
        synerr(S);
    S->blok.blocks[o->last].next = target(S, S->blok.off, mode);
    o->last = i;
    if (S->blok.blocks[i].kind == BK_ELSE)
        o->last |= ELSED;
    S->blok.blocks[i].link = o->chain;
    o->chain = i + 1;
}

/* The block o closes: its chain's clauses, its EXITs leave to t. */
static void exits(Session *const S, const Open *const o, const uint32_t t)
{
    for (uint16_t c = o->chain; c; c = S->blok.blocks[c - 1].link)
        S->blok.blocks[c - 1].done = t;
    S->blok.depth--;
}

/* END IF, END SELECT here: the last clause's false test comes here, every clause leaves here. */
static void close(Session *const S, const Open *const o)
{
    if (!(o->last & ELSED))
        S->blok.blocks[o->last].next = target(S, S->blok.off, TO_STMT);
    exits(S, o, target(S, S->blok.off, TO_STMT));
}

static void end_if(Session *const S)
{
    const Open *const o = top(S, BK_IF, E_EIF);
    add(S, BK_ENDIF);
    close(S, o);
}

/* NEXT closes the innermost FOR, which counts with the variable at v (null any). */
static void next(Session *const S, const uint8_t *const v)
{
    const Open *const o = top(S, BK_FOR, E_NF);
    if (v) {
        const uint8_t *const f = at(S->prog.base + S->blok.blocks[o->open].pos + 1); /* FOR v */
        if (*f == M_VAR) {
            if (comp_nameof(f) != comp_nameof(v) || comp_suffixof(f) != comp_suffixof(v))
                fail(S, E_NF, DG_NEXT_WITHOUT_FOR_MISMATCH, (uint32_t)(v - S->blok.body), S->blok.line,
                     (const char *)f + 2, f[1]); /* the caret at NEXT's, {other} the FOR's */
        }
    }
    const uint32_t t = target(S, S->blok.off, TO_NEXT_STMT);
    S->blok.blocks[o->open].done = t;
    exits(S, o, t);
}

/* The parameter at q, ( [BYVAL] n[()] [AS type] ), to p's; q past it, at its ',' or past the ')'.
   false: the list ended. */
static bool param(Session *const S, Proc *const p, const uint8_t **const at)
{
    const uint8_t *q = *at;
    const bool byval = *q == TK_BYVAL;
    if (byval)
        q++;
    if (*q != M_VAR) {
        *at = q;
        return false;
    }
    const uint8_t *const pv = q;
    q = decl_item(q);
    if (*q == '(')
        q += 2;
    uint8_t et = 0;
    if (*q == TK_AS) {
        if (q[1] == TK_X && q[2] == TK1_SHORT)
            et = ET_SH;
        else if ((uint8_t)(q[1] - TK_INTEGER) <= TK_STRING - TK_INTEGER)
            et = decl_astype[q[1] - TK_INTEGER];
        else { /* AS a TYPE: its record, its array, as decl_line entered the name */
            const bool arr = pv[pv[1] + 4] == '(';
            const uint16_t t = scop_rec(S, comp_nameof(pv), arr);
            et = t ? (uint8_t)(t + ET_REC) : 0;
        }
    }
    S->comp.p = pv;
    const uint16_t id = comp_var(S, 0, et);
    q = S->comp.p;
    if (S->blok.nparams >= NPPAR)
        repl_error(S, E_OM);
    S->blok.params[S->blok.nparams] = id;
    S->blok.byval[S->blok.nparams] = byval;
    S->blok.nparams++;
    p->npar++;
    for (uint32_t depth = 0;; q = decl_item(q)) { /* on to its ',' or the ')' */
        const uint8_t c = *q;
        if (!c) {
            *at = q;
            return false;
        }
        if (c == '(')
            depth++;
        if (c == ')') {
            if (!depth) {
                *at = q + 1;
                return false;
            }
            depth--;
        }
        if (c == ',' && !depth) {
            *at = q + 1;
            return true;
        }
    }
}

/* SUB, FUNCTION, DEF FN at s: a procedure's entry. */
static void procstmt(Session *const S, const uint8_t *const s, const uint8_t kind)
{
    decl_deftype(S, S->blok.line);
    const uint32_t defoff = S->blok.off;
    if (S->blok.proc || s[1] != M_VAR)
        synerr(S);
    if (S->blok.nprocs >= NPROC)
        repl_error(S, E_OM);
    const uint16_t i = S->blok.nprocs++;
    S->blok.proc = i;
    Proc *const p = &S->blok.procs[i];
    *p = (Proc){
        .kind = kind,
        .entry = target(S, S->blok.off, TO_NEXT_STMT), /* its body: the statement after */
        .first = (uint16_t)S->blok.line,
        .last = (uint16_t)S->blok.line,
        .par = S->blok.nparams,
        .skip = UINT32_MAX,
    };
    const uint8_t *q = s + 1;
    const uint16_t n = comp_nameof(q);
    p->name = n;
    symb_name(&S->symb, n)->proc = (uint8_t)i;
    scop_proc(S, (uint8_t)i);
    if (kind == PK_FUNC) {
        S->comp.p = q; /* its result: a scalar of its name */
        p->result = comp_var(S, 1, 0);
    }
    q = decl_item(q);
    if (*q == '(') {
        q++;
        while (param(S, p, &q))
            ;
    }
    if (*q == TK_STATIC)
        p->flags |= PF_STAT;
    if (S->blok.isdef) {
        S->blok.isdef = false; /* DEF FN: its parameters known now; = e: the body is this statement's
                          past its JMP */
        p->flags |= PF_DEF;
        if (*q == TK_EQ) {
            p->entry = target(S, defoff, PAST_JMP);
            p->skip = target(S, defoff, TO_NEXT_STMT);
            S->blok.proc = 0;
        }
    }
    scop_proc(S, 0);
}

/* LOOP, WEND, kind: back to the opener (open), which leaves past it, as its EXITs do. */
static void loopend(Session *const S, const uint8_t open, const uint8_t err, const uint8_t kind)
{
    const Open *const o = top(S, open, err);
    const uint16_t i = add(S, kind);
    S->blok.blocks[i].next = S->blok.blocks[o->open].pos * 4 + TO_STMT;
    const uint32_t t = target(S, S->blok.off, TO_NEXT_STMT);
    S->blok.blocks[o->open].done = t;
    exits(S, o, t);
}

/* The statement at s: a block statement's entry and the stack. */
static void stmt(Session *const S, const uint8_t *const s)
{
    S->blok.off = (uint32_t)(s - S->blok.body);
    Open *o;
    uint16_t i;
    switch (*s) {
    case TK_IF: { /* a block when THEN ends the line */
        const uint8_t *q = s;
        while (*q && *q != TK_THEN)
            q = decl_item(q);
        if (*q && (!q[1] || q[1] == TK_REM))
            push(S, BK_IF, add(S, BK_IF));
        return;
    }
    case TK_ELSEIF: /* the clause before goes here */
        o = top(S, BK_IF, E_ELS);
        clause(S, o, add(S, BK_ELSEIF), PAST_JMP);
        return;
    case TK_ELSE: /* at a statement's start: a block's */
        o = top(S, BK_IF, E_ELS);
        clause(S, o, add(S, BK_ELSE), PAST_JMP);
        return;
    case TK_ENDIF:
        end_if(S);
        return;
    case TK_SELECT: /* its depth, for its value's variable */
        i = add(S, BK_SELECT);
        for (uint32_t k = 0; k < S->blok.depth; k++)
            if (S->blok.open[k].kind == BK_SELECT)
                S->blok.blocks[i].aux++;
        push(S, BK_SELECT, i);
        return;
    case TK_CASE: /* SELECT's or the case before's test goes here */
        o = top(S, BK_SELECT, E_CWS);
        i = add(S, BK_CASE);
        S->blok.blocks[i].aux = o->open;
        clause(S, o, i, PAST_JMP);
        return;
    case TK_DO:
        push(S, BK_DO, add(S, BK_DO));
        return;
    case TK_WHILE:
        push(S, BK_WHILE, add(S, BK_WHILE));
        return;
    case TK_LOOP:
        loopend(S, BK_DO, E_LWD, BK_LOOP);
        return;
    case TK_WEND:
        loopend(S, BK_WHILE, E_WEW, BK_WEND);
        return;
    case TK_FOR: /* past its NEXT when not entered */
        push(S, BK_FOR, add(S, BK_FOR));
        return;
    case TK_NEXT: { /* NEXT a, b: a FOR closed for each, which must count with it */
        const uint8_t *v = nullptr;
        for (const uint8_t *q = s + 1;; q = decl_item(q)) {
            const uint8_t c = *q;
            if (!c || c == ':' || c == TK_ELSE || c == TK_REM)
                break;
            if (c == ',') {
                next(S, v);
                v = nullptr;
            }
            if (c == M_VAR)
                v = q;
        }
        next(S, v);
        return;
    }
    case TK_EXIT: { /* EXIT FOR, EXIT DO: on the chain of the innermost such block */
        const uint8_t kind = s[1] == TK_FOR ? BK_FOR : s[1] == TK_DO ? BK_DO : 0;
        if (!kind)
            return; /* EXIT SUB ..: not a block's */
        for (uint32_t k = S->blok.depth; k-- > 0;)
            if (S->blok.open[k].kind == kind) {
                i = add(S, BK_EXIT);
                S->blok.blocks[i].aux = kind;
                S->blok.blocks[i].link = S->blok.open[k].chain;
                S->blok.open[k].chain = i + 1;
                return;
            }
        failw(S, E_EXW, 0, words[kind]);
    }
    case TK_SUB:
        procstmt(S, s, PK_SUB);
        return;
    case TK_FUNCTION:
        procstmt(S, s, PK_FUNC);
        return;
    case TK_DEF: /* DEF FNn: a FUNCTION of its own */
        if (s[1] != M_VAR)
            return;
        if (!(symb_name(&S->symb, comp_nameof(s + 1))->flags & NM_FN))
            synerr(S);
        S->blok.isdef = true;
        procstmt(S, s, PK_FUNC);
        return;
    case TK_END:
        if (s[1] == TK_IF)
            end_if(S);
        else if (s[1] == TK_SELECT) {
            o = top(S, BK_SELECT, E_ESW);
            add(S, BK_ENDSEL);
            S->blok.blocks[o->open].done = target(S, S->blok.off, TO_STMT);
            close(S, o);
        } else if (endproc[s[1]]) {
            if (!S->blok.proc)
                failw(S, E_EPW, 0, nullptr);
            S->blok.procs[S->blok.proc].last = (uint16_t)S->blok.line;
            S->blok.procs[S->blok.proc].skip = target(S, S->blok.off, TO_NEXT_STMT);
            S->blok.proc = 0;
        }
        return;
    }
}

/* The statements of the body at b, and those after THEN and ELSE. */
static void scanline(Session *const S, const uint8_t *const b)
{
    S->blok.body = b;
    const uint8_t *s = b;
    if (*s == M_VAR && s[s[1] + 4] == ':') /* a label: its statement after the ':' */
        s += s[1] + 5;
    for (;;) {
        while (*s == ':')
            s++;
        if (!*s)
            return;
        stmt(S, s);
        for (;;) {
            uint8_t c = *s;
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

void blok_pair(Session *const S)
{
    S->blok = (Blok){.nprocs = 1};
    for (uint32_t ln = PROGBASE; prog_line(&S->prog, ln)->len; ln += prog_line(&S->prog, ln)->len) {
        S->blok.line = ln;
        scanline(S, at(S->prog.base + ln + HDR));
    }
    uint32_t at_;
    if (S->blok.proc) { /* a SUB with no END SUB */
        const uint32_t ln = blok_lineof(S, S->blok.procs[S->blok.proc].entry >> 2, &at_);
        fail(S, E_PWE, 0, at_, ln, nullptr, 0);
    }
    if (S->blok.depth) { /* a block left open: at its opener */
        const Open *o = &S->blok.open[S->blok.depth - 1];
        const uint32_t ln = blok_lineof(S, S->blok.blocks[o->open].pos, &at_);
        const uint8_t code = leftopen[o->kind];
        if (code == E_FWN) { /* FOR: {other} its variable */
            const uint8_t *const v = at(S->prog.base + ln + HDR + at_ + 1);
            if (*v == M_VAR)
                fail(S, code, 0, at_, ln, (const char *)v + 2, v[1]);
        }
        fail(S, code, 0, at_, ln, nullptr, 0);
    }
}
