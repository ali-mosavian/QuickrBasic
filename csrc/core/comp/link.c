/* The code cache (comp/link.asm). */
#include "comp/link.h"
#include "session.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/symb.h"
#include "vm/exec.h"
#include "vm/flow.h"


/* cm_comp: line compiled to comp_xbuf. */
static void compile(Session *const S, const uint32_t line)
{
    S->comp.buf = S->comp.xbuf;
    S->comp.line = line;
    blok_scope(S, line);
    decl_deftype(S, line);
    comp_text(S, (const uint8_t *)prog_line(&S->prog, line) + HDR);
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
    compile(S, line);
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

#if IDE
void link_walk(const uint8_t *code, void (*each)(uint8_t op, const uint8_t *at))
{
    for (; *code != O_EOL; code += flow_opsize(code))
        each(*code, code);
}
#endif

void link_start(Session *const S)
{
    S->comp.check = false; /* a check an error left */
    if (S->link.linked)
        return;
    symb_size(S);
    blok_pair(S);
    S->link.cursor = 0;
    uncache(S);
    S->link.end = S->allo.cache + CACHE0;
    S->link.linked = true;
}
