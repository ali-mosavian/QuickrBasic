/* The code cache: lines compiled to streams, and a run's start.
 *
 * # How it works
 * The cache is mem's HCACHE block in the arena, offsets from its base (mem's cachebase): the direct
 * line's stream at DCODE (1536 bytes), the program lines' from CACHE0 to CLIM. A stream is its
 * statement map, a Stmt per statement then their count:4, then the code; Line.code is the code's
 * offset, 0 none.
 *
 * link_start runs before a run: blocks paired (blok_pair), the procedures' variables declared, the
 * cache empty. link_cache compiles a line into the cache when it is first run. A full cache is
 * flushed: the frames' and CONT's ips become offsets in their lines' code, every line loses its
 * code.
 *
 * # asm structure (src/comp/link.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_link | link_start | MIRROR; D1 |
 * | cm_cache | link_cache(line) | MIRROR; its code's Addr |
 * | cm_xlate | link_xlate(&at) | MIRROR; false: no room |
 * | cm_walk | link_walk(code, each) | MIRROR |
 * | g_opsz | flow_opsize(op) (vm) | MOVED |
 * | g_opfl | | D2 |
 * | cm_comp, cm_flush, cx_mapstmt | link_compile(line), flush, mapstmt | MIRROR |
 * | lnk.cend, elim, eeol, einb | link.end, link.lim, link.eol, link.inbeg (Addrs) | MIRROR |
 * | lnk.emap, ecode, mapnext | link.map, link.code (Addrs), link.mapnext | MIRROR |
 * | lnk.linked, cursor, nfl | link.linked, link.cursor, link.flushes | MIRROR |
 *
 * - D1. The procedures' variables are declared here, after the block scan (link_procvars).
 * - D2. Unused in the asm; dropped.
 */
#include "comp/link.h"
#include "session.h"
#include "comp/blocks.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "comp/fold.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "vm/exec.h"
#include "vm/flow.h"

void link_compile(Session *const S, const uint32_t line)
{
    S->code.buf = S->comp.xbuf;
    S->comp.line = line;
    comp_proc(S, blok_procof(&S->blok, line));
    decl_deftype(&S->decl, &S->symb, line);
    comp_text(S, (const uint8_t *)prog_line(&S->prog, line) + HDR);
}

/* A parameter's variable: its type by AS, as decl_line entered the name. */
static uint16_t link_param(Session *const S, const Param *const pa)
{
    const uint8_t *const pv = at(S->prog.base + pa->at);
    const uint8_t *q = decl_item(pv);

    if (*q == '(')
        q += 2;

    uint8_t et = 0;

    if (*q == TK_AS) {
        if (q[1] == TK_X && q[2] == TK1_SHORT)
            et = ET_SH;
        else if ((uint8_t)(q[1] - TK_INTEGER) <= TK_STRING - TK_INTEGER)
            et = decl_astype[q[1] - TK_INTEGER];
        else { /* AS a TYPE: its record, its array */
            const bool arr = pv[pv[1] + 4] == '(';
            const uint16_t t = scop_rec(&S->scop, comp_nameof(pv), arr);
            et = t ? (uint8_t)(t + ET_REC) : 0;
        }
    }

    S->comp.p = pv;
    return comp_var(S, 0, et);
}

/* Each procedure blok_pair found: its name's, its result's, its parameters' variables, in
   program order. */
static void link_procvars(Session *const S)
{
    for (uint8_t i = 1; i < S->blok.nprocs; i++) {
        Proc *const p = S->blok.procs[i];
        decl_deftype(&S->decl, &S->symb, p->first);
        symb_name(&S->symb, p->name)->proc = i;
        comp_proc(S, i);

        if (p->kind == PK_FUNC) {
            S->comp.p = at(S->prog.base + p->at); /* its result: a scalar of its name */
            p->result = comp_var(S, 1, 0);
        }

        for (uint16_t k = 0; k < p->npar; k++)
            p->params[k].var = link_param(S, &p->params[k]);

        comp_proc(S, 0);
    }
}

/* Every line loses its code. */
static void uncache(Session *const S)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len)
        prog_line(&S->prog, l)->code = 0;
}

/* cm_flush: the frames' and CONT's ips become offsets in their lines' code; the cache empty. */
static void flush(Session *const S)
{
    for (uint32_t i = 0; i < S->vm.nframes; i++) {
        Place *const p = &S->vm.frames[i].ret;

        if (p->line != DLINE && p->code >= CACHE0)
            p->code -= prog_line(&S->prog, p->line)->code;
    }

    if (S->vm.cont.line && S->vm.cont.code >= CACHE0)
        S->vm.cont.code -= prog_line(&S->prog, S->vm.cont.line)->code;

    uncache(S);
    S->link.end = S->allo.cache + CACHE0;
    S->link.flushes++;
}

Addr link_cache(Session *const S, const uint32_t line)
{
    link_compile(S, line);

    for (;;) {
        Addr a = S->link.end;
        S->link.lim = S->allo.cache + CLIM;

        if (link_xlate(S, &a)) {
            S->link.end = a;
            prog_line(&S->prog, line)->code = S->link.code - S->allo.cache;
            return S->link.code;
        }

        if (S->link.end == S->allo.cache + CACHE0)
            repl_error(S, E_OM);

        flush(S);
    }
}

/* cx_mapstmt: the map entries of the statements whose code starts at s; a the stream's place. */
static void mapstmt(Session *const S, const uint8_t *const s, const Addr a)
{
    const Stmt *const compiled = (const Stmt *)S->comp.map; /* down from the top */
    Stmt *const map = (Stmt *)at(S->link.map);

    for (; S->link.mapnext < S->comp.nstmt; S->link.mapnext++) {
        const Stmt *const e = &compiled[-1 - (int32_t)S->link.mapnext];

        if (S->comp.xbuf + e->code != s)
            return;

        map[S->link.mapnext] = (Stmt){.body = e->body, .code = a - S->link.code};
    }
}

/* A stream's head: its map, then the count. */
static uint32_t map_size(const uint32_t n) { return n * sizeof(Stmt) + sizeof(uint32_t); }

bool link_xlate(Session *const S, Addr *where)
{
    S->link.inbeg = 0;
    const uint32_t n = S->comp.nstmt;
    Addr a = *where;

    if (a + map_size(n) > S->link.lim)
        return false;

    S->link.map = a;
    a += map_size(n);
    wr32(a - sizeof n, n);
    S->link.code = a;
    S->link.mapnext = 0;

    for (const uint8_t *s = S->comp.xbuf;;) {
        mapstmt(S, s, a);
        const uint8_t op = *s;
        const uint32_t size = flow_opsize(s);

        if (a + size + XSLK > S->link.lim)
            return false;
        if (op == O_EOL)
            S->link.eol = a;

        uint8_t *const d = at(a);

        if (op == O_CALL) { /* CALL INBEG: 2 bytes more, its INPUT's end */
            const uint8_t k = s[1];
            d[0] = op;
            d[1] = k;
            a += 2;
            s += 2;

            if (k == K_INBEG) {
                S->link.inbeg = a;
                wr16(a, 0);
                a += 2;
            } else if (k == K_INEND && S->link.inbeg) {
                wr16(S->link.inbeg, (uint16_t)(a - S->allo.cache));
                S->link.inbeg = 0;
            }

            continue;
        }

        memcpy(d, s, size);
        a += size;
        s += size;

        if (op == O_GOTO || op == O_GOSUB) { /* its line, found now if it exists */
            LineRef *const r = (LineRef *)at(a - sizeof(LineRef));
            uint32_t line;

            if (prog_find(S, r->num, &line))
                r->line = line;
        }

        if (op == O_EOL)
            break;
    }

    if (S->link.inbeg)
        wr16(S->link.inbeg, (uint16_t)(S->link.eol - S->allo.cache));

    *where = a;
    return true;
}

void link_walk(const uint8_t *code, void (*each)(uint8_t op, const uint8_t *at))
{
    for (; *code != O_EOL; code += flow_opsize(code))
        each(*code, code);
}

void link_start(Session *const S)
{
    S->comp.check = false; /* a check an error left */
    if (S->link.linked)
        return;

    symb_size(&S->symb, &S->prog);
    fold_clear(S);
    blok_pair(&S->blok, &S->prog, &S->symb, &S->fault);
    link_procvars(S);
    S->link.cursor = 0;
    uncache(S);
    S->link.end = S->allo.cache + CACHE0;
    S->link.linked = true;
}
