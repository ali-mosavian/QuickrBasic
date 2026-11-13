/* CHAIN and COMMON (src/vm/chain.asm). The buffer holds COMMON's values in order: an entry is the
   kind (Var.info's VI_ARR, SK_DBL, VK_MASK), then a number's 4 or 8 bytes, a string's len:2 and
   chars; an array's dims:1 (0 none), its counts and lower bounds (words), then its elements so. */
#include "vm/chai.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "vm/flow.h"
#include "vm/stmt.h"

#if IDE
#include "ui/uimn.h"
#endif

enum { NCMN = 512, CHAIN_BUF_SIZE = 0x10000, CHAIN_NAME_MAX = 120 };

static struct {
    const uint8_t *next;  /* g_chr: the next value to take */
    uint8_t *end;         /* g_che: past the last */
    bool chained;         /* g_chf: CHAIN's run starting */
    uint32_t n;           /* g_cmn: the ids COMMON named so far in this run */
    uint16_t id[NCMN];    /* g_cmid */
    uint8_t buf[CHAIN_BUF_SIZE]; /* g_chb */
} ch;

/* ---- the kinds ------------------------------------------------------------------------------ */

static const uint8_t KIND_MASK = VI_ARR | SK_DBL | VK_MASK;

static bool is_array(const uint8_t kind) { return kind & VI_ARR; }
static bool is_string(const uint8_t kind) { return (kind & (SK_DBL | VK_MASK)) == VK_STR; }

/* Variable id's kind. Type mismatch: a record. */
static uint8_t kind_of(const uint16_t id)
{
    const Var *const v = symb_var(id);
    if (v->type >= ET_REC)
        repl_error(E_TM);
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
static void put(uint8_t **const p, const void *const src, const uint32_t n)
{
    if (*p + n > ch.buf + CHAIN_BUF_SIZE)
        repl_error(E_OM);
    memcpy(*p, src, n);
    *p += n;
}

/* The string of the Desc at desc: len:2, chars. */
static void put_string(uint8_t **const p, const Addr desc)
{
    const Desc s = strg_peek(desc);
    put(p, &s.len, sizeof s.len);
    put(p, at(s.ptr), s.len);
}

static void put_array(uint8_t **const p, const uint16_t id)
{
    const Addr hdr = symb_var(id)->vt;
    const uint8_t dims = hdr ? ((const ArrayHdr *)at(hdr))->dims : 0;
    put(p, &dims, 1);
    if (!hdr)
        return;
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    put(p, a->count, dims * 2u * sizeof a->count[0]); /* the counts, the lower bounds */
    const uint32_t n = elem_count(a);
    if (is_string(kind_of(id)))
        for (uint32_t i = 0; i < n; i++)
            put_string(p, a->data + i * DSZ);
    else
        put(p, at(a->data), arry_esize(id) * n);
}

/* COMMON's variables' values to the buffer. Out of memory: more than it holds. */
static void save(void)
{
    uint8_t *p = ch.buf;
    for (uint32_t i = 0; i < ch.n; i++) {
        const uint16_t id = ch.id[i];
        const uint8_t kind = kind_of(id);
        put(&p, &kind, 1);
        if (is_array(kind))
            put_array(&p, id);
        else if (is_string(kind))
            put_string(&p, symb_cell(id));
        else
            put(&p, at(symb_cell(id)), number_size(kind));
    }
    ch.next = ch.buf;
    ch.end = p;
}

/* ---- in: the values from the buffer --------------------------------------------------------- */

/* n bytes from *p to dst. */
static void get(const uint8_t **const p, void *const dst, const uint32_t n)
{
    memcpy(dst, *p, n);
    *p += n;
}

/* The string at *p, len:2 and chars, to the Desc at desc: a block it owns. */
static void get_string(const uint8_t **const p, const Addr desc)
{
    uint16_t len;
    get(p, &len, sizeof len);
    Addr chars = 0;
    if (len) {
        chars = strg_alloc(len);
        get(p, at(chars), len);
        strg_own(chars, len, desc);
    }
    const Desc s = {.len = len, .ptr = chars};
    memcpy(at(desc), &s, DSZ);
}

static void get_array(const uint8_t **const p, const uint16_t id)
{
    uint8_t dims;
    get(p, &dims, 1);
    if (!dims)
        return;
    if (symb_var(id)->vt)
        arry_free(id, 0);
    get(p, arry.counts, dims * sizeof arry.counts[0]);
    get(p, arry.lbounds, dims * sizeof arry.lbounds[0]);
    const Addr hdr = arry_new(id, dims, true);
    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);
    const uint32_t n = elem_count(a);
    if (is_string(kind_of(id)))
        for (uint32_t i = 0; i < n; i++)
            get_string(p, a->data + i * DSZ);
    else
        get(p, at(a->data), arry_esize(id) * n);
}

/* COMMON's variable ( id -- ): named, for a CHAIN; CHAIN's next value, if any, stored in it.
   Type mismatch: not the value's kind. Out of memory: past NCMN variables. */
void chai_cmn(Vm *const vm)
{
    const uint16_t id = (uint16_t)exec_pop(vm).u;
    if (ch.n >= NCMN)
        repl_error(E_OM);
    ch.id[ch.n++] = id;
    const uint8_t *p = ch.next;
    if (!p || p >= ch.end)
        return;
    const uint8_t kind = kind_of(id);
    if (kind != *p++)
        repl_error(E_TM);
    if (is_array(kind))
        get_array(&p, id);
    else if (is_string(kind))
        get_string(&p, symb_cell(id));
    else
        get(&p, at(symb_cell(id)), number_size(kind));
    ch.next = p;
}

/* ---- CHAIN ---------------------------------------------------------------------------------- */

/* repl.file: s, with .BAS when it has no extension. */
static void set_file(const Desc s)
{
    bool ext = false;
    for (uint16_t i = 0; i < s.len; i++) {
        const char c = (char)rd8(s.ptr + i);
        repl.file[i] = c;
        if (c == '.')
            ext = true;
        else if (c == '\\')
            ext = false;
    }
    char *const end = repl.file + s.len;
    if (ext)
        *end = 0;
    else
        memcpy(end, ".BAS", sizeof ".BAS");
}

/* CHAIN ( s -- ): COMMON's values kept, the file s loaded and run. Advanced feature unavailable:
   in the editor. Illegal function call: a name past CHAIN_NAME_MAX chars. As repl_load. */
void chai_chain(Vm *const vm)
{
#if IDE
    if (uimn.on)
        repl_error(E_AF);
#endif
    const Desc s = strg_get(exec_popa(vm));
    if (s.len > CHAIN_NAME_MAX)
        repl_error(E_IQ);
    set_file(s);
    save();
    ch.chained = true;
    prog_init();
    repl_load();
    prog_drop();
    link_start();
    if (!prog_line(PROGBASE)->len)
        flow_end(vm);
    stmt_rungo(PROGBASE);
}

void chai_start(void)
{
    ch.n = 0;
    if (!ch.chained)
        ch.next = ch.end;
    ch.chained = false;
}
