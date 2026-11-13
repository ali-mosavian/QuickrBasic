/* The code cache (comp/link.asm). */
#include "comp/link.h"
#include "comp/blok.h"
#include "comp/comp.h"
#include "comp/decl.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/prog.h"
#include "mem/symb.h"
#include "vm/exec.h"
#include "vm/flow.h"

Link link;

/* cm_comp: line compiled to comp_xbuf. */
static void compile(const uint32_t line)
{
    comp.buf = comp_xbuf;
    comp.line = line;
    blok_scope(line);
    decl_deftype(line);
    comp_text((const uint8_t *)prog_line(line) + HDR);
}

/* Every line loses its code. */
static void uncache(void)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(l)->len); l += len)
        prog_line(l)->code = 0;
}

/* cm_flush: the frames' and CONT's ips become offsets in their lines' code; the cache empty. */
static void flush(void)
{
    for (uint32_t i = 0; i < vm.nframes; i++) {
        Place *const p = &vm.frames[i].ret;
        if (p->line != DLINE && p->code >= CACHE0)
            p->code -= prog_line(p->line)->code;
    }
    if (vm.cont.line && vm.cont.code >= CACHE0)
        vm.cont.code -= prog_line(vm.cont.line)->code;
    uncache();
    link.end = allo.cache + CACHE0;
    link.flushes++;
}

Addr link_cache(const uint32_t line)
{
    compile(line);
    for (;;) {
        Addr a = link.end;
        link.lim = allo.cache + CLIM;
        if (link_xlate(&a)) {
            link.end = a;
            prog_line(line)->code = link.code - allo.cache;
            return link.code;
        }
        if (link.end == allo.cache + CACHE0)
            repl_error(E_OM);
        flush();
    }
}

/* cx_mapstmt: the map entries of the statements whose code starts at s; a the stream's place. */
static void mapstmt(const uint8_t *const s, const Addr a)
{
    const Stmt *const compiled = (const Stmt *)comp.map; /* down from the top */
    Stmt *const map = (Stmt *)at(link.map);
    for (; link.mapnext < comp.nstmt; link.mapnext++) {
        const Stmt *const e = &compiled[-1 - (int32_t)link.mapnext];
        if (comp_xbuf + e->code != s)
            return;
        map[link.mapnext] = (Stmt){.body = e->body, .code = a - link.code};
    }
}

/* A stream's head: its map, then the count. */
static uint32_t map_size(const uint32_t n) { return n * sizeof(Stmt) + sizeof(uint32_t); }

bool link_xlate(Addr *where)
{
    link.inbeg = 0;
    const uint32_t n = comp.nstmt;
    Addr a = *where;
    if (a + map_size(n) > link.lim)
        return false;
    link.map = a;
    a += map_size(n);
    wr32(a - sizeof n, n);
    link.code = a;
    link.mapnext = 0;
    for (const uint8_t *s = comp_xbuf;;) {
        mapstmt(s, a);
        const uint8_t op = *s;
        const uint32_t size = flow_opsize(s);
        if (a + size + XSLK > link.lim)
            return false;
        if (op == O_EOL)
            link.eol = a;
        uint8_t *const d = at(a);
        if (op == O_CALL) { /* CALL INBEG: 2 bytes more, its INPUT's end */
            const uint8_t k = s[1];
            d[0] = op;
            d[1] = k;
            a += 2;
            s += 2;
            if (k == K_INBEG) {
                link.inbeg = a;
                wr16(a, 0);
                a += 2;
            } else if (k == K_INEND && link.inbeg) {
                wr16(link.inbeg, (uint16_t)(a - allo.cache));
                link.inbeg = 0;
            }
            continue;
        }
        memcpy(d, s, size);
        a += size;
        s += size;
        if (op == O_GOTO || op == O_GOSUB) { /* its line, found now if it exists */
            LineRef *const r = (LineRef *)at(a - sizeof(LineRef));
            uint32_t line;
            if (prog_find(r->num, &line))
                r->line = line;
        }
        if (op == O_EOL)
            break;
    }
    if (link.inbeg)
        wr16(link.inbeg, (uint16_t)(link.eol - allo.cache));
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

void link_start(void)
{
    comp.check = false; /* a check an error left */
    if (link.linked)
        return;
    symb_size();
    blok_pair();
    link.cursor = 0;
    uncache();
    link.end = allo.cache + CACHE0;
    link.linked = true;
}
