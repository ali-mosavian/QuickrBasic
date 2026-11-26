/* CHAIN and COMMON: carry named variables' values from one program to the next.
 *
 * # How it works
 * COMMON names its variables in order as a run reaches them (chai_cmn), each once: two files'
 * COMMONs name the same, joined by position (comp/declare.c). CHAIN copies their values
 * into a buffer, loads its file (repl_load) and runs it; each COMMON there takes the next value by
 * position, an array with its bounds. Files stay open. In the editor CHAIN is Advanced feature
 * unavailable.
 *
 * A buffer entry is the kind (Var.info's VI_ARR, SK_DBL, VK_MASK), then a number's 4 or 8 bytes, a
 * string's len:2 and chars; an array's dims:1 (0 none), its counts and lower bounds (words), then
 * its elements so.
 *
 * # Example
 *   main:  COMMON n%, s$ : n% = 5 : s$ = "hi" : CHAIN "NEXT"
 *   NEXT:  COMMON a%, b$ : PRINT a%; b$        ' 5 hi
 *
 * # asm structure (retired/asm32/vm/chain.asm)
 * | asm | here | |
 * |---|---|---|
 * | chn_start | chai_start | MIRROR |
 * | sc_cmn, sc_chain | chai_cmn, chai_chain | MIRROR |
 * | chn_kind, chn_cell, chn_elems, chn_room, chn_sout, chn_sin, chn_save | kind_of, elem_count, put*, get*, save, set_file | MIRROR |
 * | g_chr, g_che, g_chf, g_cmn, g_cmid, g_chb, NCMN, CHBSZ | Chai.next, end, chained, n, id, buf; grown, no NCMN, CHBSZ | DIVERGES |
 */
#include "vm/chain.h"
#include "session.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/array.h"
#include "mem/program.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "vm/flow.h"
#include "vm/stmts.h"
#include "util/grow.h"

/* ---- the kinds ------------------------------------------------------------------------------ */

static const uint8_t KIND_MASK = VI_ARR | SK_DBL | VK_MASK;

static bool is_array(const uint8_t kind) { return kind & VI_ARR; }
static bool is_string(const uint8_t kind) { return (kind & (SK_DBL | VK_MASK)) == VK_STR; }

/* Variable id's kind. Type mismatch: a record. */
static uint8_t kind_of(Symb *const sy, Fault *const f, const uint32_t id)
{
    const Var *const v = symb_var(sy, id);

    if (v->type >= ET_REC)
        fault_error(f, E_TM);

    return v->info & KIND_MASK;
}

/* A number of kind k's bytes. */
static uint32_t number_size(const uint8_t kind) { return kind & SK_DBL ? 8 : 4; }

/* Array id's (header hdr) element count. */
static uint32_t elem_count(const ArrayHdr *const a)
{
    uint32_t n = 1;

    for (uint32_t i = 0; i < a->dims; i++)
        n *= a->count[i];

    return n;
}

/* ---- out: the values to the buffer ---------------------------------------------------------- */

/* n bytes from src after the buffer's len. Out of memory. */
static void put(Chai *const ch, Fault *const f, const void *const src, const uint32_t n)
{
    if (n > UINT32_MAX - ch->len || !GROW(ch->buf, ch->cap, ch->len + n))
        fault_error(f, E_OM);

    memcpy(ch->buf + ch->len, src, n);
    ch->len += n;
}

/* The string of the Desc at desc: len:2, chars. */
static void put_string(Chai *const ch, Fault *const f, const Addr desc)
{
    const Desc s = strg_peek(desc);
    put(ch, f, &s.len, sizeof s.len);
    put(ch, f, at(s.ptr), s.len);
}

static void put_array(Session *const S, const uint32_t id)
{
    const Addr hdr = arry_slot(&S->arry, id)->hdr;
    const uint8_t dims = hdr ? ((const ArrayHdr *)at(hdr))->dims : 0;
    put(&S->chai, &S->fault, &dims, 1);

    if (!hdr)
        return;

    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    put(&S->chai,
        &S->fault,
        a->count,
        dims * 2u * sizeof a->count[0]); /* the counts, the lower bounds */
    const uint32_t n = elem_count(a);

    if (is_string(kind_of(&S->symb, &S->fault, id)))
        for (uint32_t i = 0; i < n; i++)
            put_string(&S->chai, &S->fault, a->data + i * DSZ);
    else
        put(&S->chai, &S->fault, at(a->data), arry_esize(&S->arry, id) * n);
}

/* COMMON's variables' values to the buffer. Out of memory: more than it holds. */
static void save(Session *const S)
{
    S->chai.len = 0;

    for (uint32_t i = 0; i < S->chai.n; i++) {
        const uint32_t id = S->chai.id[i];
        const uint8_t kind = kind_of(&S->symb, &S->fault, id);
        put(&S->chai, &S->fault, &kind, 1);

        if (is_array(kind))
            put_array(S, id);
        else if (is_string(kind))
            put_string(&S->chai, &S->fault, symb_cell(&S->symb, id));
        else
            put(&S->chai, &S->fault, at(symb_cell(&S->symb, id)), number_size(kind));
    }

    S->chai.next = S->chai.buf;
    S->chai.end = S->chai.buf + S->chai.len;
}

/* ---- in: the values from the buffer --------------------------------------------------------- */

/* n bytes from *p to dst. */
static void get(const uint8_t **const p, void *const dst, const uint32_t n)
{
    memcpy(dst, *p, n);
    *p += n;
}

/* The string at *p, len:2 and chars, to the Desc at desc: a block it owns. */
static void get_string(Strg *const sg, const uint8_t **const p, const Addr desc)
{
    uint16_t len;
    get(p, &len, sizeof len);
    Addr chars = 0;

    if (len) {
        chars = strg_alloc(sg, len);
        get(p, at(chars), len);
        strg_own(chars, len, desc);
    }

    const Desc s = {.len = len, .ptr = chars};

    memcpy(at(desc), &s, DSZ);
}

static void get_array(Session *const S, const uint8_t **const p, const uint32_t id)
{
    uint8_t dims;
    get(p, &dims, 1);

    if (!dims)
        return;
    if (arry_slot(&S->arry, id)->hdr)
        arry_free(&S->arry, arry_slot(&S->arry, id));

    get(p, S->arry.counts, dims * sizeof S->arry.counts[0]);
    get(p, S->arry.lbounds, dims * sizeof S->arry.lbounds[0]);
    const Addr hdr = arry_new(&S->arry, id, dims, true);
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    const uint32_t n = elem_count(a);

    if (is_string(kind_of(&S->symb, &S->fault, id)))
        for (uint32_t i = 0; i < n; i++)
            get_string(&S->strg, p, a->data + i * DSZ);
    else
        get(p, at(a->data), arry_esize(&S->arry, id) * n);
}

/* COMMON's variable ( id -- ): named, for a CHAIN; CHAIN's next value, if any, stored in it.
   Named already, another file's COMMON's: nothing. Type mismatch: not the value's kind. Out of
   memory. */
void chai_cmn(Session *const S)
{
    const uint32_t id = exec_pop(&S->vm).u;
    Var *const v = symb_var(&S->symb, id);

    if (v->cmn)
        return;
    if (!GROW(S->chai.id, S->chai.ncap, S->chai.n + 1))
        repl_error(S, E_OM);

    v->cmn = true;
    S->chai.id[S->chai.n++] = id;
    const uint8_t *p = S->chai.next;

    if (!p || p >= S->chai.end)
        return;

    const uint8_t kind = kind_of(&S->symb, &S->fault, id);

    if (kind != *p++)
        repl_error(S, E_TM);
    if (is_array(kind))
        get_array(S, &p, id);
    else if (is_string(kind))
        get_string(&S->strg, &p, symb_cell(&S->symb, id));
    else
        get(&p, at(symb_cell(&S->symb, id)), number_size(kind));

    S->chai.next = p;
}

/* ---- CHAIN ---------------------------------------------------------------------------------- */

/* repl.file: s, with .BAS when it has no extension. */
static void set_file(Session *const S, const Desc s)
{
    bool ext = false;

    for (uint16_t i = 0; i < s.len; i++) {
        const char c = (char)rd8(s.ptr + i);
        S->repl.file[i] = c;

        if (c == '.')
            ext = true;
        else if (c == '\\')
            ext = false;
    }

    char *const end = S->repl.file + s.len;

    if (ext)
        *end = 0;
    else
        memcpy(end, ".BAS", sizeof ".BAS");
}

/* CHAIN ( s -- ): COMMON's values kept, the file s loaded and run. Advanced feature unavailable:
   the front end's chain. Illegal function call: a name past CHAIN_NAME_MAX chars. As repl_load. */
void chai_chain(Session *const S)
{
    if (S->repl.fe->chain)
        S->repl.fe->chain(S);

    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));

    if (s.len > CHAIN_NAME_MAX)
        repl_error(S, E_IQ);

    set_file(S, s);
    save(S);
    S->chai.chained = true;
    session_new(S);
    repl_load(S);
    session_drop(S);
    link_start(&S->comp);

    if (!prog_line(&S->prog, PROGBASE)->len)
        flow_end(S);

    stmt_rungo(S, PROGBASE);
}

void chai_start(Chai *const ch, Symb *const sy)
{
    for (uint32_t i = 0; i < ch->n; i++) /* a CHAIN's: the old program's, the same slots */
        if (ch->id[i] < sy->nvars)
            symb_var(sy, ch->id[i])->cmn = false;

    ch->n = 0;

    if (!ch->chained)
        ch->next = ch->end;

    ch->chained = false;
}
