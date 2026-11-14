#include "mem/program.h"
#include "session.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/copy.h"
#include "mem/symbols.h"
#include "vm/exec.h"

static void terminate(Session *const S) { prog_line(&S->prog, S->prog.end)->len = 0; }

/* The first line numbered num from l on, before the first numbered more when sorted; else that
   place. */
static bool find_from(
    Session *const S,
    uint32_t l,
    const uint16_t num,
    const bool sorted,
    uint32_t *const line
)
{
    for (const Line *p; (p = prog_line(&S->prog, l))->len && (!sorted || num >= p->num);

         l += p->len)
        if (p->num == num) {
            *line = l;
            return true;
        }

    *line = l;
    return false;
}

bool prog_find(Session *const S, const uint16_t num, uint32_t *const line)
{
    if (num == NONUM) {
        *line = PROGBASE;
        return false;
    }

    if (find_from(S, PROGBASE, num, false, line))
        return true;

    const uint32_t cursor = S->link.cursor;
    const bool from_cursor = cursor && num >= prog_line(&S->prog, cursor)->num;
    return find_from(S, from_cursor ? cursor : PROGBASE, num, true, line);
}

/* The record of size bytes at line: num, add_flags, the body, then the text before it (pre,
   used up). */
static void fill(
    Session *const S,
    const uint32_t line,
    const uint32_t size,
    const uint16_t num,
    const uint8_t *const body,
    const uint32_t len
)
{
    *prog_line(&S->prog, line) = (Line){
        .len = size,
        .num = num,
        .flags = S->prog.add_flags,
        .pre = S->prog.pre_len,
    };

    uint8_t *const d = at(prog_addr(&S->prog, line + HDR));
    memcpy(d, body, len);
    memcpy(d + len, S->prog.pre, S->prog.pre_len);
    S->prog.pre_len = 0;
}

void prog_put(Session *const S, const uint16_t num, const uint8_t *const body, const uint32_t len)
{
    uint32_t line;

    if (prog_find(S, num, &line))
        prog_cut(S, line);
    if (len != 1)
        prog_ins(S, line, num, body, len);

    prog_drop(S);
}

uint32_t prog_cut(Session *const S, const uint32_t line)
{
    const uint32_t size = prog_line(&S->prog, line)->len;
    copy_prog(S, line, line + size, S->prog.end - (line + size));
    S->prog.end -= size;
    terminate(S);
    return size;
}

/* A record's size: its header, the body, the text before it. */
static uint32_t record_size(Session *const S, const uint32_t len)
{
    return HDR + len + S->prog.pre_len;
}

enum { TERMINATOR = 4 };

uint32_t prog_ins(
    Session *const S,
    const uint32_t line,
    const uint16_t num,
    const uint8_t *const body,
    const uint32_t len
)
{
    const uint32_t size = record_size(S, len);
    const uint32_t need = S->prog.end + size + TERMINATOR;
    prog_drop(S);
    prog_room(S, need);
    copy_prog(S, line + size, line, S->prog.end - line);
    S->prog.end += size;
    terminate(S);
    fill(S, line, size, num, body, len);
    return size;
}

uint32_t prog_at(Session *const S) { return S->prog.add_at ? S->prog.add_at : S->prog.end; }

void prog_add(Session *const S, const uint16_t num, const uint8_t *const body, const uint32_t len)
{
    if (S->prog.add_at) {
        S->prog.add_at += prog_ins(S, S->prog.add_at, num, body, len);
        return;
    }

    const uint32_t size = record_size(S, len);
    const uint32_t line = S->prog.end;
    prog_room(S, line + size + TERMINATOR);
    S->prog.end += size;
    fill(S, line, size, num, body, len);
    terminate(S);
}

uint8_t *prog_body(Session *const S, const uint32_t line, uint8_t *const dst)
{
    const Line *const p = prog_line(&S->prog, line);
    const uint32_t n = p->len - p->pre - HDR;
    memcpy(dst, at(prog_addr(&S->prog, line + HDR)), n);
    return dst + n;
}

void prog_drop(Session *const S)
{
    S->vm.cont.line = 0;
    S->link.cursor = 0;
    S->link.linked = false;
    S->vm.nframes = 0;
    terminate(S);
    symb_clear(S);
}

void prog_init(Session *const S)
{
    S->prog.end = PROGBASE;
    symb_init(S);
    prog_drop(S);
}

void prog_room(Session *const S, const uint32_t size)
{
    if (size <= allo_cap(&S->allo, HPROG))
        return;

    symb_clear(S);

    if (!allo_grow(S, HPROG, size))
        repl_error(S, E_OM);
}

void prog_place(Session *const S) { S->prog.base = allo_base(&S->allo, HPROG); }

uint32_t prog_lineof(const Prog *const pg, const uint32_t pos, uint32_t *const off)
{
    uint32_t line = PROGBASE;

    while (pos >= line + prog_line(pg, line)->len)
        line += prog_line(pg, line)->len;

    *off = pos - line - HDR;
    return line;
}
