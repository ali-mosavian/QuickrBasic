#include "mem/prog.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/copy.h"
#include "mem/symb.h"
#include "vm/exec.h"

Prog prog;

static void terminate(void) { prog_line(prog.end)->len = 0; }

bool prog_find(uint16_t num, uint32_t *line)
{
    uint32_t l = PROGBASE;
    if (num != NONUM) {
        for (Line *p; (p = prog_line(l))->len; l += p->len)
            if (p->num == num) {
                *line = l;
                return true;
            }
        l = link.cursor;
        if (!l || num < prog_line(l)->num)
            l = PROGBASE;
        for (Line *p; (p = prog_line(l))->len && num >= p->num; l += p->len)
            if (p->num == num) {
                *line = l;
                return true;
            }
    }
    *line = l;
    return false;
}

/* The record of size bytes at line: num, add_flags, the body, then the text before it (pre,
   used up). */
static void fill(uint32_t line, uint32_t size, uint16_t num, const uint8_t *body, uint32_t len)
{
    Line *p = prog_line(line);
    *p = (Line){.len = size, .num = num, .flags = prog.add_flags, .pre = prog.pre_len};
    uint8_t *d = at(prog_addr(line + HDR));
    memcpy(d, body, len);
    memcpy(d + len, prog.pre, prog.pre_len);
    prog.pre_len = 0;
}

void prog_put(uint16_t num, const uint8_t *body, uint32_t len)
{
    uint32_t line;
    if (prog_find(num, &line))
        prog_cut(line);
    if (len != 1)
        prog_ins(line, num, body, len);
    prog_drop();
}

uint32_t prog_cut(uint32_t line)
{
    uint32_t size = prog_line(line)->len;
    copy_prog(line, line + size, prog.end - (line + size));
    prog.end -= size;
    terminate();
    return size;
}

uint32_t prog_ins(uint32_t line, uint16_t num, const uint8_t *body, uint32_t len)
{
    uint32_t size = len + prog.pre_len + HDR;
    uint32_t need = prog.end + size + 4; /* the terminator */
    prog_drop();
    prog_room(need);
    copy_prog(line + size, line, prog.end - line);
    prog.end += size;
    terminate();
    fill(line, size, num, body, len);
    return size;
}

uint32_t prog_at(void) { return prog.add_at ? prog.add_at : prog.end; }

void prog_add(uint16_t num, const uint8_t *body, uint32_t len)
{
    if (prog.add_at) {
        prog.add_at += prog_ins(prog.add_at, num, body, len);
        return;
    }
    uint32_t size = len + prog.pre_len + HDR;
    uint32_t line = prog.end;
    prog_room(line + size + 4);
    prog.end += size;
    fill(line, size, num, body, len);
    terminate();
}

uint8_t *prog_body(uint32_t line, uint8_t *dst)
{
    const Line *p = prog_line(line);
    uint32_t n = p->len - p->pre - HDR;
    memcpy(dst, at(prog_addr(line + HDR)), n);
    return dst + n;
}

void prog_drop(void)
{
    exec.cont_line = 0;
    link.cursor = 0;
    link.linked = false;
    exec.cstk = allo.cs_top;
    terminate();
    symb_clear();
}

void prog_init(void)
{
    prog.end = PROGBASE;
    symb_init();
    prog_drop();
}

void prog_room(uint32_t size)
{
    if (size <= allo_cap(HPROG))
        return;
    symb_clear();
    if (!allo_grow(HPROG, size))
        repl_error(E_OM);
}

void prog_place(void) { prog.base = allo_base(HPROG); }
