#include "mem/prog.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/copy.h"
#include "mem/symb.h"
#include "vm/exec.h"

Prog prog;

static void terminate(void) { prog_line(prog.end)->len = 0; }

/* The first line numbered num from l on, before the first numbered more when sorted; else that
   place. */
static bool find_from(uint32_t l, const uint16_t num, const bool sorted, uint32_t *const line)
{
    for (const Line *p; (p = prog_line(l))->len && (!sorted || num >= p->num); l += p->len)
        if (p->num == num) {
            *line = l;
            return true;
        }
    *line = l;
    return false;
}

bool prog_find(const uint16_t num, uint32_t *const line)
{
    if (num == NONUM) {
        *line = PROGBASE;
        return false;
    }
    if (find_from(PROGBASE, num, false, line))
        return true;
    const uint32_t cursor = link.cursor;
    const bool from_cursor = cursor && num >= prog_line(cursor)->num;
    return find_from(from_cursor ? cursor : PROGBASE, num, true, line);
}

/* The record of size bytes at line: num, add_flags, the body, then the text before it (pre,
   used up). */
static void fill(const uint32_t line, const uint32_t size, const uint16_t num,
                 const uint8_t *const body, const uint32_t len)
{
    *prog_line(line) = (Line){.len = size, .num = num, .flags = prog.add_flags, .pre = prog.pre_len};
    uint8_t *const d = at(prog_addr(line + HDR));
    memcpy(d, body, len);
    memcpy(d + len, prog.pre, prog.pre_len);
    prog.pre_len = 0;
}

void prog_put(const uint16_t num, const uint8_t *const body, const uint32_t len)
{
    uint32_t line;
    if (prog_find(num, &line))
        prog_cut(line);
    if (len != 1)
        prog_ins(line, num, body, len);
    prog_drop();
}

uint32_t prog_cut(const uint32_t line)
{
    const uint32_t size = prog_line(line)->len;
    copy_prog(line, line + size, prog.end - (line + size));
    prog.end -= size;
    terminate();
    return size;
}

/* A record's size: its header, the body, the text before it. */
static uint32_t record_size(const uint32_t len) { return HDR + len + prog.pre_len; }

enum { TERMINATOR = 4 };

uint32_t prog_ins(const uint32_t line, const uint16_t num, const uint8_t *const body,
                  const uint32_t len)
{
    const uint32_t size = record_size(len);
    const uint32_t need = prog.end + size + TERMINATOR;
    prog_drop();
    prog_room(need);
    copy_prog(line + size, line, prog.end - line);
    prog.end += size;
    terminate();
    fill(line, size, num, body, len);
    return size;
}

uint32_t prog_at(void) { return prog.add_at ? prog.add_at : prog.end; }

void prog_add(const uint16_t num, const uint8_t *const body, const uint32_t len)
{
    if (prog.add_at) {
        prog.add_at += prog_ins(prog.add_at, num, body, len);
        return;
    }
    const uint32_t size = record_size(len);
    const uint32_t line = prog.end;
    prog_room(line + size + TERMINATOR);
    prog.end += size;
    fill(line, size, num, body, len);
    terminate();
}

uint8_t *prog_body(const uint32_t line, uint8_t *const dst)
{
    const Line *const p = prog_line(line);
    const uint32_t n = p->len - p->pre - HDR;
    memcpy(dst, at(prog_addr(line + HDR)), n);
    return dst + n;
}

void prog_drop(void)
{
    vm.cont.line = 0;
    link.cursor = 0;
    link.linked = false;
    vm.nframes = 0;
    terminate();
    symb_clear();
}

void prog_init(void)
{
    prog.end = PROGBASE;
    symb_init();
    prog_drop();
}

void prog_room(const uint32_t size)
{
    if (size <= allo_cap(HPROG))
        return;
    symb_clear();
    if (!allo_grow(HPROG, size))
        repl_error(E_OM);
}

void prog_place(void) { prog.base = allo_base(HPROG); }
