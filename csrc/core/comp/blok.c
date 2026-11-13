/* Block statements paired across lines, before a run (comp/block.asm). */
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "comp/dfa.h"
#include "io/diag.h"
#include "io/repl.h"
#include "mem/prog.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "vm/exec.h"

Blok blok;
BlockEnt blok_blocks[NBLK];
Proc blok_procs[NPROC];
uint16_t blok_params[NPPAR];
uint8_t blok_byval[NPPAR];

enum { NBST = 64, ELSED = 0x8000 };

/* An open block: its kind, its opener's entry, its last clause (ELSED: an ELSE, no false test
   after it), its chain's head + 1. */
typedef struct {
    uint8_t kind;
    uint16_t open, last, chain;
} Open;
static Open stack[NBST];

/* By BK_*: an opener's word; the error a block of it left open gives. */
static const char *const words[] = {
    [BK_IF] = "IF", [BK_DO] = "DO", [BK_WHILE] = "WHILE", [BK_SELECT] = "SELECT", [BK_FOR] = "FOR",
};
static const uint8_t leftopen[] = {
    [BK_IF] = E_BIF, [BK_DO] = E_DWL, [BK_WHILE] = E_WWW, [BK_SELECT] = E_SWE, [BK_FOR] = E_FWN,
};

static const uint8_t *body; /* (EBP) the body scanned */
static uint32_t off;        /* (ECX) the statement's offset in it */
static bool isdef;          /* g_bkdef: the procedure is a DEF FN's */
/* llrm-cc miscompiles a switch whose case values share one block: the second value's edge
   skips the block's first load (blok.proc here, blok.depth for WEND, decl.proc for REDIM). So no
   shared case labels in these switches: END's words by a table, LOOP and WEND each their own. */
static const bool endproc[256] = {[TK_SUB] = true, [TK_FUNCTION] = true, [TK_DEF] = true};

[[noreturn]] static void fail(uint8_t code, uint8_t dg, uint32_t at, uint32_t line,
                              const char *other, uint32_t len)
{
    diag_raise(code, dg, (uint16_t)at, line, other, len);
}

[[noreturn]] static void failw(uint8_t code, uint8_t dg, const char *word)
{
    fail(code, dg, off, blok.line, word, word ? (uint32_t)strlen(word) : 0);
}

/* @bk_er: Syntax error at the line scanned. */
[[noreturn]] static void synerr(void)
{
    exec.line = blok.line;
    repl_error(E_SN);
}

/* The target here, mode 0 the statement, 1 past its JMP, 2 the statement after it. */
static uint32_t target(uint32_t at, uint8_t mode) { return (blok.line + at + HDR) * 4 + mode; }

uint32_t blok_lineof(uint32_t pos, uint32_t *at)
{
    uint32_t line = PROGBASE;
    for (;;) {
        uint32_t end = line + prog_line(line)->len;
        if (pos < end)
            break;
        line = end;
    }
    *at = pos - line - HDR;
    return line;
}

int32_t blok_find(uint32_t line, uint32_t at)
{
    uint32_t key = line + at + HDR, lo = 0, hi = blok.nblocks;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        uint32_t pos = blok_blocks[mid].pos;
        if (pos == key)
            return (int32_t)mid;
        if (pos > key)
            hi = mid;
        else
            lo = mid + 1;
    }
    return -1;
}

uint8_t blok_scope(uint32_t line)
{
    uint8_t in = 0;
    for (uint32_t i = blok.nprocs - 1; (int32_t)i > 0; i--) {
        const Proc *p = &blok_procs[i];
        if ((uint16_t)line >= p->first && (uint16_t)line <= p->last) {
            in = (uint8_t)i;
            break;
        }
    }
    scop_proc(in);
    return in;
}

/* A new entry here, of kind; its index. */
static uint16_t add(uint8_t kind)
{
    if (blok.nblocks >= NBLK)
        repl_error(E_OM);
    uint16_t i = blok.nblocks++;
    blok_blocks[i] = (BlockEnt){.pos = blok.line + off + HDR, .kind = kind};
    return i;
}

/* Open a block of kind, its opener entry i. */
static void push(uint8_t kind, uint16_t i)
{
    if (blok.depth >= NBST)
        synerr();
    stack[blok.depth++] = (Open){kind, i, i, 0};
}

/* The innermost block, which must be of kind; else error code (X without Y), or blocks crossed. */
static Open *top(uint8_t kind, uint8_t code)
{
    if (blok.depth) {
        Open *o = &stack[blok.depth - 1];
        if (o->kind == kind)
            return o;
        for (int i = blok.depth - 1; i >= 0; i--)
            if (stack[i].kind == kind) /* open further out: crossed */
                failw(code, DG_BLOCKS_CROSSED, words[o->kind]);
    }
    failw(code, code == E_NF ? DG_NEXT_WITHOUT_FOR_END : 0, nullptr); /* by BC's name */
}

/* Clause i of block o: the last clause's false test comes here, mode; it joins END's chain. */
static void clause(Open *o, uint16_t i, uint8_t mode)
{
    if (o->last & ELSED) /* a clause after ELSE */
        synerr();
    blok_blocks[o->last].next = target(off, mode);
    o->last = i;
    if (blok_blocks[i].kind == BK_ELSE)
        o->last |= ELSED;
    blok_blocks[i].link = o->chain;
    o->chain = i + 1;
}

/* The block o closes: its chain's clauses, its EXITs leave to t. */
static void exits(Open *o, uint32_t t)
{
    for (uint16_t c = o->chain; c; c = blok_blocks[c - 1].link)
        blok_blocks[c - 1].done = t;
    blok.depth--;
}

/* END IF, END SELECT here: the last clause's false test comes here, every clause leaves here. */
static void close(Open *o)
{
    if (!(o->last & ELSED))
        blok_blocks[o->last].next = target(off, 0);
    exits(o, target(off, 0));
}

/* NEXT closes the innermost FOR, which counts with the variable at v (null any). */
static void next(const uint8_t *v)
{
    Open *o = top(BK_FOR, E_NF);
    if (v) {
        const uint8_t *f = at(prog.base + blok_blocks[o->open].pos + 1); /* FOR v */
        if (*f == M_VAR) {
            uint8_t fs = f[f[1] + 1], vs = v[v[1] + 1];
            fs = fs < '.' ? fs : 0;
            vs = vs < '.' ? vs : 0;
            if (comp_rd16(f + f[1] + 2) != comp_rd16(v + v[1] + 2) || fs != vs)
                fail(E_NF, DG_NEXT_WITHOUT_FOR_MISMATCH, (uint32_t)(v - body), blok.line,
                     (const char *)f + 2, f[1]); /* the caret at NEXT's, {other} the FOR's */
        }
    }
    uint32_t t = target(off, 2);
    blok_blocks[o->open].done = t;
    exits(o, t);
}

/* SUB, FUNCTION, DEF FN at s: a procedure's entry. */
static void procstmt(const uint8_t *s, uint8_t kind)
{
    decl_deftype(blok.line);
    uint32_t defoff = off;
    if (blok.proc || s[1] != M_VAR)
        synerr();
    if (blok.nprocs >= NPROC)
        repl_error(E_OM);
    uint16_t i = blok.nprocs++;
    blok.proc = i;
    Proc *p = &blok_procs[i];
    *p = (Proc){
        .kind = kind,
        .entry = target(off, 2), /* its body: the statement after */
        .first = (uint16_t)blok.line,
        .last = (uint16_t)blok.line,
        .par = blok.nparams,
        .skip = 0xFFFFFFFF,
    };
    const uint8_t *q = s + 1;
    uint16_t n = comp_nameof(q);
    p->name = n;
    symb_name(n)->proc = (uint8_t)i;
    scop_proc((uint8_t)i);
    if (kind == PK_FUNC) {
        comp.p = q; /* its result: a scalar of its name */
        p->result = comp_var(1, 0);
    }
    q = decl_item(q);
    if (*q == '(') {
        q++;
        for (;;) { /* [BYVAL] n[()] [AS type] */
            bool byval = *q == TK_BYVAL;
            if (byval)
                q++;
            if (*q != M_VAR)
                break;
            const uint8_t *pv = q;
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
                    bool arr = pv[pv[1] + 4] == '(';
                    uint16_t t = scop_decl(comp_nameof(pv), B_REC, arr);
                    et = t ? (uint8_t)(t + ET_REC) : 0;
                }
            }
            comp.p = pv;
            uint16_t id = comp_var(0, et);
            q = comp.p;
            if (blok.nparams >= NPPAR)
                repl_error(E_OM);
            blok_params[blok.nparams] = id;
            blok_byval[blok.nparams] = byval;
            blok.nparams++;
            p->npar++;
            int depth = 0; /* on to its ',' or the ')' */
            for (;;) {
                uint8_t c = *q;
                if (!c)
                    goto done;
                if (c == '(')
                    depth++;
                if (c == ')') {
                    if (!depth) {
                        q++;
                        goto done;
                    }
                    depth--;
                }
                if (c == ',' && !depth)
                    break;
                q = decl_item(q);
            }
            q++;
        }
    }
done:
    if (*q == TK_STATIC)
        p->flags |= PF_STAT;
    if (isdef) {
        isdef = false; /* DEF FN: its parameters known now; = e: the body is this statement's
                          past its JMP */
        p->flags |= PF_DEF;
        if (*q == TK_EQ) {
            p->entry = target(defoff, 1);
            p->skip = target(defoff, 2);
            blok.proc = 0;
        }
    }
    scop_proc(0);
}

/* LOOP, WEND, kind: back to the opener (open), which leaves past it, as its EXITs do. */
static void loopend(uint8_t open, uint8_t err, uint8_t kind)
{
    Open *o = top(open, err);
    uint16_t i = add(kind);
    blok_blocks[i].next = blok_blocks[o->open].pos * 4;
    uint32_t t = target(off, 2);
    blok_blocks[o->open].done = t;
    exits(o, t);
}

/* The statement at s: a block statement's entry and the stack. */
static void stmt(const uint8_t *s)
{
    off = (uint32_t)(s - body);
    Open *o;
    uint16_t i;
    switch (*s) {
    case TK_IF: { /* a block when THEN ends the line */
        const uint8_t *q = s;
        while (*q && *q != TK_THEN)
            q = decl_item(q);
        if (*q && (!q[1] || q[1] == TK_REM))
            push(BK_IF, add(BK_IF));
        return;
    }
    case TK_ELSEIF: /* the clause before goes here */
        o = top(BK_IF, E_ELS);
        clause(o, add(BK_ELSEIF), 1);
        return;
    case TK_ELSE: /* at a statement's start: a block's */
        o = top(BK_IF, E_ELS);
        clause(o, add(BK_ELSE), 1);
        return;
    case TK_ENDIF:
    endif:
        o = top(BK_IF, E_EIF);
        add(BK_ENDIF);
        close(o);
        return;
    case TK_SELECT: /* its depth, for its value's variable */
        i = add(BK_SELECT);
        for (uint32_t k = 0; k < blok.depth; k++)
            if (stack[k].kind == BK_SELECT)
                blok_blocks[i].aux++;
        push(BK_SELECT, i);
        return;
    case TK_CASE: /* SELECT's or the case before's test goes here */
        o = top(BK_SELECT, E_CWS);
        i = add(BK_CASE);
        blok_blocks[i].aux = o->open;
        clause(o, i, 1);
        return;
    case TK_DO:
        push(BK_DO, add(BK_DO));
        return;
    case TK_WHILE:
        push(BK_WHILE, add(BK_WHILE));
        return;
    case TK_LOOP: /* one case each: see endproc */
        loopend(BK_DO, E_LWD, BK_LOOP);
        return;
    case TK_WEND:
        loopend(BK_WHILE, E_WEW, BK_WEND);
        return;
    case TK_FOR: /* past its NEXT when not entered */
        push(BK_FOR, add(BK_FOR));
        return;
    case TK_NEXT: { /* NEXT a, b: a FOR closed for each, which must count with it */
        const uint8_t *v = nullptr;
        for (const uint8_t *q = s + 1;; q = decl_item(q)) {
            uint8_t c = *q;
            if (!c || c == ':' || c == TK_ELSE || c == TK_REM)
                break;
            if (c == ',') {
                next(v);
                v = nullptr;
            }
            if (c == M_VAR)
                v = q;
        }
        next(v);
        return;
    }
    case TK_EXIT: { /* EXIT FOR, EXIT DO: on the chain of the innermost such block */
        uint8_t kind = s[1] == TK_FOR ? BK_FOR : s[1] == TK_DO ? BK_DO : 0;
        if (!kind)
            return; /* EXIT SUB ..: not a block's */
        for (int k = blok.depth - 1; k >= 0; k--)
            if (stack[k].kind == kind) {
                i = add(BK_EXIT);
                blok_blocks[i].aux = kind;
                blok_blocks[i].link = stack[k].chain;
                stack[k].chain = i + 1;
                return;
            }
        failw(E_EXW, 0, words[kind]);
    }
    case TK_SUB:
        procstmt(s, PK_SUB);
        return;
    case TK_FUNCTION:
        procstmt(s, PK_FUNC);
        return;
    case TK_DEF: /* DEF FNn: a FUNCTION of its own */
        if (s[1] != M_VAR)
            return;
        if (!(symb_name(comp_nameof(s + 1))->flags & NM_FN))
            synerr();
        isdef = true;
        procstmt(s, PK_FUNC);
        return;
    case TK_END:
        if (s[1] == TK_IF)
            goto endif;
        if (s[1] == TK_SELECT) {
            o = top(BK_SELECT, E_ESW);
            add(BK_ENDSEL);
            blok_blocks[o->open].done = target(off, 0);
            close(o);
        } else if (endproc[s[1]]) {
            if (!blok.proc)
                failw(E_EPW, 0, nullptr);
            blok_procs[blok.proc].last = (uint16_t)blok.line;
            blok_procs[blok.proc].skip = target(off, 2);
            blok.proc = 0;
        }
        return;
    }
}

/* The statements of the body at b, and those after THEN and ELSE. */
static void scanline(const uint8_t *b)
{
    body = b;
    const uint8_t *s = b;
    if (*s == M_VAR && s[s[1] + 4] == ':') /* a label: its statement after the ':' */
        s += s[1] + 5;
    for (;;) {
        while (*s == ':')
            s++;
        if (!*s)
            return;
        stmt(s);
        for (;;) {
            uint8_t c = *s;
            if (!c || c == TK_REM)
                return;
            if (c == ':' || c == TK_ELSE || c == TK_THEN) { /* a one-line IF's: a statement after */
                s++;
                break;
            }
            if (c == TK_DATA) { /* DATA's text: to a ':' out of quotes */
                bool q = false;
                for (;;) {
                    c = *++s;
                    if (!c)
                        return;
                    if (c == '"')
                        q = !q;
                    if (!q && c == ':')
                        break;
                }
                continue;
            }
            s = decl_item(s);
        }
    }
}

void blok_pair(void)
{
    blok = (Blok){.nprocs = 1};
    for (uint32_t ln = PROGBASE; prog_line(ln)->len; ln += prog_line(ln)->len) {
        blok.line = ln;
        scanline(at(prog.base + ln + HDR));
    }
    uint32_t at_;
    if (blok.proc) { /* a SUB with no END SUB */
        uint32_t ln = blok_lineof(blok_procs[blok.proc].entry >> 2, &at_);
        fail(E_PWE, 0, at_, ln, nullptr, 0);
    }
    if (blok.depth) { /* a block left open: at its opener */
        const Open *o = &stack[blok.depth - 1];
        uint32_t ln = blok_lineof(blok_blocks[o->open].pos, &at_);
        uint8_t code = leftopen[o->kind];
        if (code == E_FWN) { /* FOR: {other} its variable */
            const uint8_t *v = at(prog.base + ln + HDR + at_ + 1);
            if (*v == M_VAR)
                fail(code, 0, at_, ln, (const char *)v + 2, v[1]);
        }
        fail(code, 0, at_, ln, nullptr, 0);
    }
}
