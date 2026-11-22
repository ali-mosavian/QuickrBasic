/* The program store: numbered lines kept in a block, found, stored, replaced and deleted.
 *
 * # How it works
 * The store is a block (HPROG) of Line records, each its body (the lexer's tokens and their 0) then
 * pre bytes of its text before the body (the editor's indent, the number as typed). The direct line
 * is at DLINE, the program from PROGBASE; a len of 0 ends it, at end. A line is named by its
 * offset in the store, which stays when the block moves.
 *
 * prog_find looks from the link's cursor when that is not above the number. Any edit ends with
 * prog_drop: CONT and the link's cursor end, the frames drop and the variables are cleared, so the
 * next link empties the code cache.
 *
 * # asm structure (retired/asm32/mem/prog.asm)
 * | asm | here | |
 * |---|---|---|
 * | mm.progbase, mm.progend, mm.enterat | Prog.base, Prog.end, Prog.entering | MIRROR |
 * | mm.preptr, mm.prelen | Prog.pre, Prog.pre_len | MIRROR |
 * | mm.addfl, mm.addat | Prog.add_flags, Prog.add_at | MIRROR |
 * | mm.progfirst, mm.progsel | | D1 |
 * | mm.bodyptr, mm.bodylen | prog_ins's body, len | D2 |
 * | prg_findw (rn.lnum, lnk.cursor -> EBX, CF) | prog_find(num, &line) | MIRROR; D3 |
 * | prg_put, prg_add (lnum, ESI, CX) | prog_put, prog_add (num, body, len) | MIRROR |
 * | prg_at | prog_at | MIRROR |
 * | prg_body (EBX, EDI -> EDI) | prog_body(line, dst) | MIRROR; returns past it |
 * | prg_cut, prg_ins | prog_cut, prog_ins | MIRROR |
 * | prg_drop, prg_init, prg_place | prog_drop, prog_init, prog_new (its owner) | MIRROR |
 * | prg_room (EDX) | prog_room(size) | MIRROR |
 * | prg_fill, prg_size | fill, record_size | MIRROR; static |
 * | bk_lineof (comp/block.asm) | prog_lineof | MOVED: here |
 *
 * - D1. No selectors: the first line is Prog.base + PROGBASE.
 * - D2. The body and its length are parameters, not globals.
 * - D3. Returns true when found, where the asm's CF is the flag.
 */
#include "mem/program.h"

void prog_copy(Prog *const pg, const uint32_t dst, const uint32_t src, const uint32_t len)
{
    memmove(at(pg->base + dst), at(pg->base + src), len);
}

void prog_terminate(Prog *const pg) { prog_line(pg, pg->end)->len = 0; }

/* The first line numbered num from l on, before the first numbered more when sorted; else that
   place. */
static bool prog_find_from(
    const Prog *const pg,
    uint32_t l,
    const uint16_t num,
    const bool sorted,
    uint32_t *const line
)
{
    for (const Line *p; (p = prog_line(pg, l))->len && (!sorted || num >= p->num); l += p->len)
        if (p->num == num) {
            *line = l;
            return true;
        }

    *line = l;
    return false;
}

bool prog_find(
    const Prog *const pg,
    const uint32_t cursor,
    const uint16_t num,
    uint32_t *const line
)
{
    if (num == NONUM) {
        *line = PROGBASE;
        return false;
    }

    if (prog_find_from(pg, PROGBASE, num, false, line))
        return true;

    const bool from_cursor = cursor && num >= prog_line(pg, cursor)->num;
    return prog_find_from(pg, from_cursor ? cursor : PROGBASE, num, true, line);
}

/* The record of size bytes at line: num, add_flags, add_src, add_file, the body, then the text before it (pre,
   used up). */
static void prog_fill(
    Prog *const pg,
    const uint32_t line,
    const uint32_t size,
    const uint16_t num,
    const uint8_t *const body,
    const uint32_t len
)
{
    *prog_line(pg, line) = (Line){
        .len = size,
        .num = num,
        .flags = pg->add_flags,
        .pre = pg->pre_len,
        .src = pg->add_src,
        .file = pg->add_file,
    };

    uint8_t *const d = at(prog_addr(pg, line + HDR));
    memcpy(d, body, len);
    memcpy(d + len, pg->pre, pg->pre_len);
    pg->pre_len = 0;
}

void prog_put(
    Prog *const pg,
    const uint32_t cursor,
    const uint16_t num,
    const uint8_t *const body,
    const uint32_t len
)
{
    uint32_t line;

    if (prog_find(pg, cursor, num, &line))
        prog_cut(pg, line);
    if (len != 1)
        prog_ins(pg, line, num, body, len);

    pg->user->edited(pg->user);
}

uint32_t prog_modules(const Prog *const pg)
{
    uint32_t tops = 0;

    for (uint32_t line = PROGBASE; prog_line(pg, line)->len; line += prog_line(pg, line)->len)
        tops += prog_line(pg, line)->flags & LF_TOP ? 1 : 0;

    return tops ? tops : 1;
}

void prog_cut_to(Prog *const pg, const uint32_t line, const uint32_t end)
{
    prog_copy(pg, line, end, pg->end - end);
    pg->end -= end - line;
    prog_terminate(pg);
}

uint32_t prog_cut(Prog *const pg, const uint32_t line)
{
    const uint32_t size = prog_line(pg, line)->len;
    prog_cut_to(pg, line, line + size);
    return size;
}

/* A record's size: its header, the body, the text before it. */
static uint32_t prog_record_size(const Prog *const pg, const uint32_t len)
{
    return HDR + len + pg->pre_len;
}

enum { TERMINATOR = 4 };

uint32_t prog_ins(
    Prog *const pg,
    const uint32_t line,
    const uint16_t num,
    const uint8_t *const body,
    const uint32_t len
)
{
    const uint32_t size = prog_record_size(pg, len);
    const uint32_t need = pg->end + size + TERMINATOR;
    pg->user->edited(pg->user);
    prog_room(pg, need);
    prog_copy(pg, line + size, line, pg->end - line);
    pg->end += size;
    prog_terminate(pg);
    prog_fill(pg, line, size, num, body, len);
    return size;
}

uint32_t prog_at(const Prog *const pg) { return pg->add_at ? pg->add_at : pg->end; }

void prog_add(Prog *const pg, const uint16_t num, const uint8_t *const body, const uint32_t len)
{
    if (pg->add_at) {
        pg->add_at += prog_ins(pg, pg->add_at, num, body, len);
        return;
    }

    const uint32_t size = prog_record_size(pg, len);
    const uint32_t line = pg->end;
    prog_room(pg, line + size + TERMINATOR);
    pg->end += size;
    prog_fill(pg, line, size, num, body, len);
    prog_terminate(pg);
}

uint8_t *prog_body(const Prog *const pg, const uint32_t line, uint8_t *const dst)
{
    const Line *const p = prog_line(pg, line);
    const uint32_t n = p->len - p->pre - HDR;
    memcpy(dst, at(prog_addr(pg, line + HDR)), n);
    return dst + n;
}

void prog_room(Prog *const pg, const uint32_t size)
{
    if (size <= allo_cap(pg->allo, HPROG))
        return;

    pg->user->growing(pg->user);

    if (!allo_grow(pg->allo, HPROG, size))
        fault_error(pg->fault, E_OM);
}

static void prog_moved(AlloOwner *const o)
{
    Prog *const pg = container_of(o, Prog, owner);
    pg->base = allo_base(pg->allo, HPROG);
}

void prog_new(Prog *const pg, Allo *const al, Fault *const f, ProgUser *const user)
{
    pg->owner.moved = prog_moved;
    pg->allo = al;
    pg->fault = f;
    pg->user = user;
    allo_new(al, PROG_FIRST, &pg->owner);
    prog_moved(&pg->owner);
}

uint32_t prog_lineof(const Prog *const pg, const uint32_t pos, uint32_t *const off)
{
    return prog_lineof_from(pg, PROGBASE, pos, off);
}

uint32_t prog_lineof_from(
    const Prog *const pg,
    const uint32_t from,
    const uint32_t pos,
    uint32_t *const off
)
{
    uint32_t line = from;

    while (pos >= line + prog_line(pg, line)->len)
        line += prog_line(pg, line)->len;

    *off = pos - line - HDR;
    return line;
}
