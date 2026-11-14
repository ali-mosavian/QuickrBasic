/* The code cache: lines compiled to streams, and a run's start.
 *
 * # How it works
 * The cache is mem's HCACHE block in the arena, offsets from its base (mem's cachebase): the direct
 * line's stream at DCODE (1536 bytes), the program lines' from CACHE0 on. A stream is its
 * statement map, a Stmt per statement then their count:4, then the code; Line.code is the code's
 * offset, 0 none.
 *
 * link_start runs before a run: blocks paired (blok_pair), the procedures' variables declared, then
 * every line compiled into the cache in program order, the cache grown to fit; then link_resolve
 * sets each jump's and line reference's target and each variable op's cell. A target not found goes
 * on the ledger, whose first error link_check raises before the run starts. A run compiles and
 * resolves nothing but the direct line.
 *
 * # asm structure (src/comp/link.asm)
 * | asm | here | |
 * |---|---|---|
 * | cm_link | link_start | MIRROR; D1 |
 * | cm_cache | link_line(line) (static) | D3 |
 * | cm_xlate | link_xlate(&at) | MIRROR; false: no room |
 * | cm_walk | link_walk(code, each) | MIRROR |
 * | g_opsz | flow_opsize(op) (vm) | MOVED |
 * | g_opfl | | D2 |
 * | cm_comp, cm_flush, cx_mapstmt | link_compile(line), , mapstmt | MIRROR; D3 |
 * | lnk.cend, elim, eeol, einb | link.end, link.lim, link.eol, link.inbeg (Addrs) | MIRROR |
 * | lnk.emap, ecode, mapnext | link.map, link.code (Addrs), link.mapnext | MIRROR |
 * | lnk.linked, cursor, nfl | link.linked, link.cursor | MIRROR; D3 |
 *
 * - D1. The procedures' variables are declared here, after the block scan (link_procvars).
 * - D2. Unused in the asm; dropped.
 * - D3. The asm compiles a line when first run, into a fixed cache it flushes when full. Here
 *   link_start compiles all of them and the cache grows: a run resolves nothing.
 */
#include "comp/link.h"
#include "comp/blocks.h"
#include "comp/comp.h"
#include "comp/control.h"
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

void link_compile(Comp *const C, const uint32_t line)
{
    C->code->buf = C->xbuf;
    C->line = line;
    comp_proc(C, blok_procof(C->blok, line));
    decl_deftype(C->decl, C->symb, line);
    comp_text(C, (const uint8_t *)prog_line(C->prog, line) + HDR);
}

/* A parameter's variable: its type by AS, as decl_line entered the name. */
static uint16_t link_param(Comp *const C, const Param *const pa)
{
    const uint8_t *const pv = at(C->prog->base + pa->at);
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
            const uint16_t t = scop_rec(C->scop, comp_nameof(pv), arr);
            et = t ? (uint8_t)(t + ET_REC) : 0;
        }
    }

    C->p = pv;
    return comp_var(C, 0, et);
}

/* Each procedure blok_pair found: its name's, its result's, its parameters' variables, in
   program order. */
static void link_procvars(Comp *const C)
{
    for (uint8_t i = 1; i < C->blok->nprocs; i++) {
        Proc *const p = C->blok->procs[i];
        decl_deftype(C->decl, C->symb, p->first);
        symb_name(C->symb, p->name)->proc = i;
        comp_proc(C, i);

        if (p->kind == PK_FUNC) {
            C->p = at(C->prog->base + p->at); /* its result: a scalar of its name */
            p->result = comp_var(C, 1, 0);
        }

        for (uint16_t k = 0; k < p->npar; k++)
            p->params[k].var = link_param(C, &p->params[k]);

        comp_proc(C, 0);
    }
}

/* Every line loses its code. */
static void uncache(Comp *const C)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(C->prog, l)->len); l += len)
        prog_line(C->prog, l)->code = 0;
}

/* Line compiled into the cache at link.end, the cache grown should it not fit. Out of memory. */
static void link_line(Comp *const C, const uint32_t line)
{
    link_compile(C, line);

    for (;;) {
        Addr a = C->link->end;
        C->link->lim = C->allo->cache + allo_cap(C->allo, HCACHE);

        if (link_xlate(C, &a)) {
            C->link->end = a;
            prog_line(C->prog, line)->code = C->link->code - C->allo->cache;
            return;
        }

        const uint32_t need = C->link->end - C->allo->cache + XLIM + XSLK;

        if (!allo_grow(C->allo, HCACHE, need))
            fault_error(C->fault, E_OM);
    }
}

/* cx_mapstmt: the map entries of the statements whose code starts at s; a the stream's place. */
static void mapstmt(Comp *const C, const uint8_t *const s, const Addr a)
{
    const Stmt *const compiled = (const Stmt *)C->map; /* down from the top */
    Stmt *const map = (Stmt *)at(C->link->map);

    for (; C->link->mapnext < C->nstmt; C->link->mapnext++) {
        const Stmt *const e = &compiled[-1 - (int32_t)C->link->mapnext];

        if (C->xbuf + e->code != s)
            return;

        map[C->link->mapnext] = (Stmt){.body = e->body, .code = a - C->link->code};
    }
}

/* A stream's head: its map, then the count. */
static uint32_t map_size(const uint32_t n) { return n * sizeof(Stmt) + sizeof(uint32_t); }

bool link_xlate(Comp *const C, Addr *where)
{
    C->link->inbeg = 0;
    const uint32_t n = C->nstmt;
    Addr a = *where;

    if (a + map_size(n) > C->link->lim)
        return false;

    C->link->map = a;
    a += map_size(n);
    wr32(a - sizeof n, n);
    C->link->code = a;
    C->link->mapnext = 0;

    for (const uint8_t *s = C->xbuf;;) {
        mapstmt(C, s, a);
        const uint8_t op = *s;
        const uint32_t size = flow_opsize(s);

        if (a + size + XSLK > C->link->lim)
            return false;
        if (op == O_EOL)
            C->link->eol = a;

        uint8_t *const d = at(a);

        if (op == O_CALL) { /* CALL INBEG: 2 bytes more, its INPUT's end */
            const uint8_t k = s[1];
            d[0] = op;
            d[1] = k;
            a += 2;
            s += 2;

            if (k == K_INBEG) {
                C->link->inbeg = a;
                wr16(a, 0);
                a += 2;
            } else if (k == K_INEND && C->link->inbeg) {
                wr16(C->link->inbeg, (uint16_t)(a - C->allo->cache));
                C->link->inbeg = 0;
            }

            continue;
        }

        memcpy(d, s, size);
        a += size;
        s += size;

        if (op == O_EOL)
            break;
    }

    if (C->link->inbeg)
        wr16(C->link->inbeg, (uint16_t)(C->link->eol - C->allo->cache));

    *where = a;
    return true;
}

/* A stream's map: its entries before its code, their count just before the code. */
static Stmt *map_of(const Addr code, uint32_t *const count)
{
    *count = rd32(code - 4);
    return (Stmt *)at(code - 4 - *count * sizeof(Stmt));
}

/* Line's code: an offset in the cache; the direct line's is the stream link_xlate made last. */
static uint32_t code_of(const Comp *const C, const uint32_t line)
{
    return line == DLINE ? C->link->code - C->allo->cache : prog_line(C->prog, line)->code;
}

/* The ledger gets code at body offset at of line. */
static void note(Comp *const C, const uint32_t line, const uint32_t at, const uint8_t code)
{
    if (C->link->nmiss < LINK_MISSES)
        C->link->miss[C->link->nmiss] = (LinkMiss){.line = line, .at = at, .code = code};

    C->link->nmiss++;
}

/* The ledger gets code at the statement of op, off bytes into line's code at base. */
static void miss(
    Comp *const C,
    const uint32_t line,
    const Addr base,
    const uint32_t off,
    const uint8_t code
)
{
    uint32_t n;
    const Stmt *s = map_of(base, &n);

    for (; n > 1 && s[1].code <= off; s++)
        n--;

    note(C, line, s->body, code);
}

/* Jump j's target: its statement's code in its line, by its mode; false not found. */
static bool jump_dest(const Comp *const C, Jump *const j)
{
    if (j->line == NOTARGET)
        return false;

    const uint32_t code = code_of(C, j->line);
    uint32_t left;
    const Stmt *s = map_of(C->allo->cache + code, &left);

    for (; left && s->body != j->body; s++, left--)
        ;
    if (!left || (j->dest == TO_NEXT_STMT && left == 1))
        return false;

    j->dest =
        code + (j->dest == TO_NEXT_STMT ? s[1].code : s->code + (j->dest == PAST_JMP ? JSIZE : 0));

    /* Jump threading: a target that only ends its line is the next line's start. A jump does the
       line end's STOP check and drops the temporaries itself. */
    while (j->line != DLINE && rd8(C->allo->cache + j->dest) == O_EOL) {
        const uint32_t next = j->line + prog_line(C->prog, j->line)->len;

        if (!prog_line(C->prog, next)->len) /* the program's end: END's */
            break;

        j->line = next;
        j->dest = prog_line(C->prog, next)->code;
    }

    return true;
}

/* LineRef r's line: the one the compiler found, else any numbered num; false none, *at its label's
   body offset. */
static bool ref_line(const Comp *const C, LineRef *const r, uint32_t *const at)
{
    uint32_t line = r->line;

    if (!(line & LREF_AT))
        return true;

    *at = line & ~LREF_AT;

    if (!prog_find(C->prog, 0, r->num, &line))
        return false;

    r->line = line;
    return true;
}

/* The jumps and references of the op at op. */
static void resolve_op(Comp *const C, const uint32_t line, const Addr base, uint8_t *const op)
{
    const uint32_t off = (uint32_t)(addr_of(op) - base);
    LineRef *refs = nullptr;
    uint32_t nrefs = 1;
    Jump *j = nullptr;

    switch (op[0]) {
    case O_JMP:
    case O_I_JF:
    case O_F_JF:
    case O_I_JT:
    case O_F_JT:
        j = (Jump *)(op + 1);
        break;

    case O_I_FOR:
    case O_F_FOR:
    case O_I_FOR1:
    case O_F_FOR1:
    case O_I_NEXT:
    case O_F_NEXT:
        j = (Jump *)(op + 7); /* past the counter's, limit's and step's ids */
        break;

    case O_PCALL:
        j = (Jump *)(op + 2);
        break;

    case O_GOTO:
    case O_GOSUB:
        refs = (LineRef *)(op + 1);
        break;

    case O_ONERR:
    case O_ONLERR:
        refs = ((LineRef *)(op + 1))->num ? (LineRef *)(op + 1) : nullptr; /* GOTO 0: none */
        break;

    case O_ONEV:
        refs = ((LineRef *)(op + 2))->num ? (LineRef *)(op + 2) : nullptr;
        break;

    case O_RESUME:
        refs = op[1] == RESUME_LINE ? (LineRef *)(op + 2) : nullptr;
        break;

    case O_ON:
        refs = (LineRef *)(op + 2);
        nrefs = op[1] & ON_COUNT;
        break;

    default:
        if (exec_flags(op[0]) & FV)
            store32(op + 1, symb_cell(C->symb, (uint16_t)load32(op + 1)));

        return;
    }

    if (j && !jump_dest(C, j) && line != DLINE)
        miss(C, line, base, off, E_SN);

    for (uint32_t i = 0; refs && i < nrefs; i++) {
        uint32_t at;

        if (!ref_line(C, &refs[i], &at))
            note(C, line, at, E_US);
    }
}

/* The size in a cache stream of the op at op: INPUT's start has its end's offset after it. */
static uint32_t stream_opsize(const uint8_t *const op)
{
    return op[0] == O_CALL && op[1] == K_INBEG ? 4 : flow_opsize(op);
}

void link_resolve(Comp *const C, const uint32_t line, const uint32_t code)
{
    const Addr base = C->allo->cache + code;

    for (uint8_t *op = at(base); *op != O_EOL; op += stream_opsize(op))
        resolve_op(C, line, base, op);
}

void link_check(Comp *const C)
{
    if (!C->link->nmiss)
        return;

    const LinkMiss m = C->link->miss[0];
    C->link->nmiss = 0;
    fault_raise(C->fault, (Failure){.code = m.code, .line = m.line, .at = m.at});
}

void link_walk(const uint8_t *code, void (*each)(uint8_t op, const uint8_t *at))
{
    for (; *code != O_EOL; code += flow_opsize(code))
        each(*code, code);
}

void link_start(Comp *const C)
{
    C->check = false; /* a check an error left */
    if (C->link->linked)
        return;

    symb_size(C->symb, C->prog);
    fold_clear(C);
    blok_pair(C->blok, C->prog, C->symb, C->fault);
    link_procvars(C);
    C->link->cursor = 0;
    uncache(C);
    C->link->end = C->allo->cache + CACHE0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(C->prog, l)->len); l += len)
        link_line(C, l);

    C->link->nmiss = 0;

    for (uint32_t l = PROGBASE, len; (len = prog_line(C->prog, l)->len); l += len)
        link_resolve(C, l, prog_line(C->prog, l)->code);

    link_check(C); /* not linked: the next run reports it again */
    C->link->linked = true;
}
