/* CHAIN and COMMON (src/vm/chain.asm). chb holds COMMON's values in order: an entry is the kind
   (Var.info's VI_ARR, SK_DBL, VK_MASK), then a number's 4 or 8 bytes, a string's len:2 and chars;
   an array's dims:1 (0 none), its counts and lower bounds (words), then its elements so. */
#include "vm/chai.h"
#include "comp/link.h"
#include "io/repl.h"
#include "mem/arry.h"
#include "mem/prog.h"
#include "mem/strg.h"
#include "mem/symb.h"
#include "vm/flow.h"
#include "vm/stmt.h"

enum { NCMN = 512, CHBSZ = 0x10000 };

#if IDE
#include "ui/uimn.h"
#endif

static struct {
    uint8_t *next, *end;  /* g_chr, g_che: chb's next value, its end */
    bool chained;         /* g_chf: CHAIN's run starting */
    uint32_t n;           /* g_cmn: the ids COMMON named so far in this run */
    uint16_t id[NCMN];    /* g_cmid */
    uint8_t buf[CHBSZ];   /* g_chb */
} ch;

/* chn_kind: variable id's kind. Type mismatch: a record. */
static uint8_t kind(uint16_t id)
{
    const Var *v = symb_var(id);
    if (v->type >= ET_REC)
        repl_error(E_TM);
    return v->info & (VI_ARR | SK_DBL | VK_MASK);
}

/* chn_cell: a scalar of kind k's bytes; 0 a string. */
static uint32_t bytes(uint8_t k)
{
    if ((k & (SK_DBL | VK_MASK)) == VK_STR)
        return 0;
    return k & SK_DBL ? 8 : 4;
}

/* chn_elems: array id's (header h) elements; *size their bytes, 0 strings. */
static uint32_t elems(Addr h, uint16_t id, uint32_t *size)
{
    const ArrayHdr *a = (const ArrayHdr *)at(h);
    uint32_t n = 1;
    for (uint32_t i = 0; i < a->dims; i++)
        n *= a->count[i];
    uint8_t k = symb_var(id)->info & (SK_DBL | VK_MASK);
    *size = k == VK_STR ? 0 : arry_esize(id) * n;
    return n;
}

/* chn_room: n more bytes fit at p. Out of memory. */
static void room(const uint8_t *p, uint32_t n)
{
    if (p + n > ch.buf + CHBSZ)
        repl_error(E_OM);
}

/* chn_sout: the string of the Desc at d to *p: len:2, chars. */
static void str_out(uint8_t **p, Addr d)
{
    uint16_t len = rd16(d);
    room(*p, len + 2u);
    memcpy(*p, &len, 2);
    memcpy(*p + 2, at(rd32(d + 2)), len);
    *p += len + 2;
}

/* chn_sin: the string at *p, len:2 and chars, to the Desc at d: a block it owns. */
static void str_in(const uint8_t **p, Addr d)
{
    uint16_t len;
    memcpy(&len, *p, 2);
    *p += 2;
    Addr chars = 0;
    if (len) {
        chars = strg_alloc(len);
        memcpy(at(chars), *p, len);
        strg_own(chars, len, d);
        *p += len;
    }
    wr16(d, len);
    wr32(d + 2, chars);
}

/* chn_save: COMMON's variables' values to chb. Out of memory: more than it holds. */
static void save(void)
{
    uint8_t *p = ch.buf;
    for (uint32_t i = 0; i < ch.n; i++) {
        uint16_t id = ch.id[i];
        uint8_t k = kind(id);
        room(p, 1);
        *p++ = k;
        if (!(k & VI_ARR)) {
            uint32_t n = bytes(k);
            if (!n)
                str_out(&p, symb_cell(id));
            else {
                room(p, n);
                memcpy(p, at(symb_cell(id)), n);
                p += n;
            }
            continue;
        }
        Addr h = symb_var(id)->vt; /* dims, the counts, the lower bounds; 0 none */
        const ArrayHdr *a = (const ArrayHdr *)at(h);
        uint32_t dims = h ? a->dims : 0;
        room(p, dims * 4 + 1);
        *p++ = (uint8_t)dims;
        if (!h)
            continue;
        memcpy(p, a->count, dims * 4);
        p += dims * 4;
        uint32_t size, n = elems(h, id, &size);
        Addr d = a->data;
        if (size) {
            room(p, size);
            memcpy(p, at(d), size);
            p += size;
        } else
            for (; n; n--, d += DSZ) /* strings */
                str_out(&p, d);
    }
    ch.next = ch.buf;
    ch.end = p;
}

/* COMMON's variable ( id -- ): named, for a CHAIN; CHAIN's next value, if any, stored in it.
   Type mismatch: not the value's kind. Out of memory: past NCMN variables. */
void chai_cmn(void)
{
    uint16_t id = (uint16_t)exec_pop().u;
    if (ch.n >= NCMN)
        repl_error(E_OM);
    ch.id[ch.n++] = id;
    const uint8_t *p = ch.next;
    if (!p || p >= ch.end)
        return;
    uint8_t k = kind(id);
    if (k != *p++)
        repl_error(E_TM);
    if (!(k & VI_ARR)) {
        uint32_t n = bytes(k);
        if (!n)
            str_in(&p, symb_cell(id));
        else {
            memcpy(at(symb_cell(id)), p, n);
            p += n;
        }
    } else {
        uint8_t dims = *p++;
        if (dims) {
            if (symb_var(id)->vt)
                arry_free(id, 0);
            memcpy(arry.counts, p, dims * 2u);
            memcpy(arry.lbounds, p + dims * 2, dims * 2u);
            p += dims * 4;
            Addr h = arry_new(id, dims, true);
            uint32_t size, n = elems(h, id, &size);
            Addr d = ((ArrayHdr *)at(h))->data;
            if (size) {
                memcpy(at(d), p, size);
                p += size;
            } else
                for (; n; n--, d += DSZ) /* strings */
                    str_in(&p, d);
        }
    }
    ch.next = (uint8_t *)p;
}

/* CHAIN ( s -- ): COMMON's values kept, the file s (.BAS when it has no extension) loaded and
   run. Advanced feature unavailable: in the editor. Illegal function call: a name past 120
   chars. As repl_load. */
void chai_chain(void)
{
#if IDE
    if (uimn.on)
        repl_error(E_AF);
#endif
    Desc s = strg_get(exec_popa());
    if (s.len > 120)
        repl_error(E_IQ);
    bool ext = false;
    for (uint16_t i = 0; i < s.len; i++) {
        char c = (char)rd8(s.ptr + i);
        repl.file[i] = c;
        if (c == '.')
            ext = true;
        else if (c == '\\')
            ext = false;
    }
    char *e = repl.file + s.len;
    if (!ext) {
        memcpy(e, ".BAS", 4);
        e += 4;
    }
    *e = 0;
    save();
    ch.chained = true;
    prog_init();
    repl_load();
    prog_drop();
    link_start();
    if (!prog_line(PROGBASE)->len)
        flow_end();
    stmt_rungo(PROGBASE);
}

void chai_start(void)
{
    ch.n = 0;
    if (!ch.chained)
        ch.next = ch.end;
    ch.chained = false;
}
