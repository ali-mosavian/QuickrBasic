/* CHAIN and COMMON (src/vm/chain.asm). The buffer holds COMMON's values in order: an entry is the
   kind (Var.info's VI_ARR, SK_DBL, VK_MASK), then a number's 4 or 8 bytes, a string's len:2 and
   chars; an array's dims:1 (0 none), its counts and lower bounds (words), then its elements so. */
#include "vm/chai.h"
#include "session.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "vm/flow.h"
#include "vm/stmt.h"




/* ---- the kinds ------------------------------------------------------------------------------ */

static const uint8_t KIND_MASK = VI_ARR | SK_DBL | VK_MASK;

static bool is_array(const uint8_t kind) { return kind & VI_ARR; }
static bool is_string(const uint8_t kind) { return (kind & (SK_DBL | VK_MASK)) == VK_STR; }

/* Variable id's kind. Type mismatch: a record. */
static uint8_t kind_of(Session *const S, const uint16_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    if (v->type >= ET_REC)
        repl_error(S, E_TM);
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

/* n bytes from src to *p. Out of memory: past the buffer. */
static void put(Session *const S, uint8_t **const p, const void *const src, const uint32_t n)
{
    if (*p + n > S->chai.buf + CHAIN_BUF_SIZE)
        repl_error(S, E_OM);
    memcpy(*p, src, n);
    *p += n;
}

/* The string of the Desc at desc: len:2, chars. */
static void put_string(Session *const S, uint8_t **const p, const Addr desc)
{
    const Desc s = strg_peek(desc);
    put(S, p, &s.len, sizeof s.len);
    put(S, p, at(s.ptr), s.len);
}

static void put_array(Session *const S, uint8_t **const p, const uint16_t id)
{
    const Addr hdr = symb_var(&S->symb, id)->vt;
    const uint8_t dims = hdr ? ((const ArrayHdr *)at(hdr))->dims : 0;
    put(S, p, &dims, 1);
    if (!hdr)
        return;
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    put(S, p, a->count, dims * 2u * sizeof a->count[0]); /* the counts, the lower bounds */
    const uint32_t n = elem_count(a);
    if (is_string(kind_of(S, id)))
        for (uint32_t i = 0; i < n; i++)
            put_string(S, p, a->data + i * DSZ);
    else
        put(S, p, at(a->data), arry_esize(S, id) * n);
}

/* COMMON's variables' values to the buffer. Out of memory: more than it holds. */
static void save(Session *const S)
{
    uint8_t *p = S->chai.buf;
    for (uint32_t i = 0; i < S->chai.n; i++) {
        const uint16_t id = S->chai.id[i];
        const uint8_t kind = kind_of(S, id);
        put(S, &p, &kind, 1);
        if (is_array(kind))
            put_array(S, &p, id);
        else if (is_string(kind))
            put_string(S, &p, symb_cell(&S->symb, id));
        else
            put(S, &p, at(symb_cell(&S->symb, id)), number_size(kind));
    }
    S->chai.next = S->chai.buf;
    S->chai.end = p;
}

/* ---- in: the values from the buffer --------------------------------------------------------- */

/* n bytes from *p to dst. */
static void get(const uint8_t **const p, void *const dst, const uint32_t n)
{
    memcpy(dst, *p, n);
    *p += n;
}

/* The string at *p, len:2 and chars, to the Desc at desc: a block it owns. */
static void get_string(Session *const S, const uint8_t **const p, const Addr desc)
{
    uint16_t len;
    get(p, &len, sizeof len);
    Addr chars = 0;
    if (len) {
        chars = strg_alloc(S, len);
        get(p, at(chars), len);
        strg_own(chars, len, desc);
    }
    const Desc s = {.len = len, .ptr = chars};
    memcpy(at(desc), &s, DSZ);
}

static void get_array(Session *const S, const uint8_t **const p, const uint16_t id)
{
    uint8_t dims;
    get(p, &dims, 1);
    if (!dims)
        return;
    if (symb_var(&S->symb, id)->vt)
        arry_free(S, id, 0);
    get(p, S->arry.counts, dims * sizeof S->arry.counts[0]);
    get(p, S->arry.lbounds, dims * sizeof S->arry.lbounds[0]);
    const Addr hdr = arry_new(S, id, dims, true);
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    const uint32_t n = elem_count(a);
    if (is_string(kind_of(S, id)))
        for (uint32_t i = 0; i < n; i++)
            get_string(S, p, a->data + i * DSZ);
    else
        get(p, at(a->data), arry_esize(S, id) * n);
}

/* COMMON's variable ( id -- ): named, for a CHAIN; CHAIN's next value, if any, stored in it.
   Type mismatch: not the value's kind. Out of memory: past NCMN variables. */
void chai_cmn(Session *const S)
{
    const uint16_t id = (uint16_t)exec_pop(&S->vm).u;
    if (S->chai.n >= NCMN)
        repl_error(S, E_OM);
    S->chai.id[S->chai.n++] = id;
    const uint8_t *p = S->chai.next;
    if (!p || p >= S->chai.end)
        return;
    const uint8_t kind = kind_of(S, id);
    if (kind != *p++)
        repl_error(S, E_TM);
    if (is_array(kind))
        get_array(S, &p, id);
    else if (is_string(kind))
        get_string(S, &p, symb_cell(&S->symb, id));
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
    const Desc s = strg_get(S, exec_popa(&S->vm));
    if (s.len > CHAIN_NAME_MAX)
        repl_error(S, E_IQ);
    set_file(S, s);
    save(S);
    S->chai.chained = true;
    prog_init(S);
    repl_load(S);
    prog_drop(S);
    link_start(S);
    if (!prog_line(&S->prog, PROGBASE)->len)
        flow_end(S);
    stmt_rungo(S, PROGBASE);
}

void chai_start(Session *const S)
{
    S->chai.n = 0;
    if (!S->chai.chained)
        S->chai.next = S->chai.end;
    S->chai.chained = false;
}
