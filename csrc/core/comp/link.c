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
static void compile(uint32_t line)
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
    for (Addr f = exec.cstk; f < allo.cs_top; f += ((Frame *)at(f))->mark) {
        Frame *fr = (Frame *)at(f);
        if (fr->line != DLINE && fr->resume >= CACHE0)
            fr->resume -= prog_line(fr->line)->code;
    }
    if (exec.cont_line && exec.cont_code >= CACHE0)
        exec.cont_code -= prog_line(exec.cont_line)->code;
    uncache();
    link.end = allo.cache + CACHE0;
    link.flushes++;
}

Addr link_cache(uint32_t line)
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
static void mapstmt(const uint8_t *s, Addr a)
{
    for (; link.mapnext < comp.nstmt; link.mapnext++) {
        const Stmt *e = (const Stmt *)(comp.map - (link.mapnext * 8 + sizeof(Stmt)));
        if (comp_xbuf + e->code != s)
            return;
        Stmt *m = (Stmt *)at(link.map) + link.mapnext;
        m->body = e->body;
        m->code = a - link.code;
    }
}

static void put16(Addr where, uint16_t v) { memcpy(at(where), &v, 2); }

bool link_xlate(Addr *where)
{
    link.inbeg = 0;
    uint32_t n = comp.nstmt;
    Addr a = *where;
    if (a + n * 8 + 4 > link.lim)
        return false;
    link.map = a;
    a += n * 8 + 4;
    memcpy(at(a - 4), &n, 4);
    link.code = a;
    link.mapnext = 0;
    for (const uint8_t *s = comp_xbuf;;) {
        mapstmt(s, a);
        uint8_t op = *s;
        uint32_t size = flow_opsize(s);
        if (a + size + XSLK > link.lim)
            return false;
        if (op == O_EOL)
            link.eol = a;
        uint8_t *d = at(a);
        if (op == O_CALL) { /* CALL INBEG: 2 bytes more, its INPUT's end */
            uint8_t k = s[1];
            d[0] = op;
            d[1] = k;
            a += 2;
            s += 2;
            if (k == K_INBEG) {
                link.inbeg = a;
                put16(a, 0);
                a += 2;
            } else if (k == K_INEND && link.inbeg) {
                put16(link.inbeg, (uint16_t)(a - allo.cache));
                link.inbeg = 0;
            }
            continue;
        }
        memcpy(d, s, size);
        a += size;
        s += size;
        if (op == O_GOTO || op == O_GOSUB) { /* its line, found now if it exists */
            LineRef *r = (LineRef *)at(a - sizeof(LineRef));
            uint32_t line;
            if (prog_find(r->num, &line))
                r->line = line;
        }
        if (op == O_EOL)
            break;
    }
    if (link.inbeg)
        put16(link.inbeg, (uint16_t)(link.eol - allo.cache));
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
