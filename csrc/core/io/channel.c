/* Channels: the files OPEN makes, by number, and where output and input go.
 *
 * # How it works
 * Channel 0 is the console, always open; the rest are free (num 0) or hold file num. A channel's
 * kind (CK_*) says where its bytes go: the console, a file handle, nowhere. Its buffer holds the
 * file's bytes from pos: read ahead (count of them, next is the next read) or written (count,
 * dirty).
 *
 * chnl.out and chnl.in are the channels output and input use now, 0 the console (standard output,
 * standard input). cons.col is out's column, kept in the channel's col while another is used.
 *
 * A channel is a Chan *; the asm's index is c - chnl.ch.
 *
 * Errors are Status values. The byte paths (chnl_putc, chnl_getb, chnl_skiplf) keep the first in
 * chnl.err instead, a write dropping its bytes from there on; the statement takes it (chnl_take)
 * when it ends.
 *
 * # asm structure (src/io/chan.asm)
 * | asm | here | |
 * |---|---|---|
 * | ch_init | chnl_init | MIRROR |
 * | ch_look, ch_find | chnl_look, chnl_find | MIRROR; null for none |
 * | ch_err | repl_qberr (repl.c) | MOVED |
 * | ch_doserr | chnl_doserr | MIRROR; a Status |
 * | ch_open, ch_close, ch_clall | chnl_open, chnl_close, chnl_closeall | MIRROR |
 * | ch_out, ch_in, ch_clr | chnl_out, chnl_in, chnl_clear | MIRROR |
 * | ch_putc, ch_getb, ch_skiplf | chnl_putc, chnl_getb, chnl_skiplf | MIRROR; getb -1 at the end |
 * | ch_peek, ch_tell, ch_seek, ch_lof | chnl_peek, chnl_tell, chnl_seek, chnl_lof | MIRROR; peek -1 for none |
 * | ch_rw AH=3Fh / 40h | chnl_read, chnl_write | MIRROR |
 * | ch_ptr, ch_dev, ch_swcol, ch_wb, ch_flush, ch_drop, ch_at, ch_e52 | static | D1 |
 * | g_ch, g_och, g_ich | chnl.ch, chnl.out, chnl.in | MIRROR |
 * | g_dose, g_devs | static | D1 |
 * | TCHAN: cnum, ckind, cmode, ccol, chnd, creclen, cbn, cbp, cdirty, ceof, cpos, clast, cfld, cbuf | Chan: num, kind, mode, col, handle, reclen, count, next, dirty, eof, pos, last, field, buf | MIRROR |
 *
 * - D1. Helpers and tables are static; the names differ.
 */
#include "io/channel.h"
#include "io/console.h"
#include "vm/exec.h"
#include "plat/pfil.h"
#include "plat/pmem.h"
#include "qbrt/qbrt.h"

void chnl_init(Chnl *const cl, Cons *const co)
{
    cl->cons = co;
    cl->err = ST_OK;
    memset(cl->ch, 0, sizeof cl->ch);
    cl->ch[0].kind = CK_CON;
    cl->ch[0].mode = FM_OUT | FM_IN;
    cl->ch[CH_LPT].kind = CK_LPT;
    cl->ch[CH_LPT].mode = FM_OUT;
    cl->out = cl->in = 0;
    cl->lpt_col = 0;
    cl->lpt_width = LPT_WIDTH;
}

static uint8_t chnl_index_of(const Chnl *const cl, const Chan *const c)
{
    return (uint8_t)(c - cl->ch);
}

static bool chnl_valid_number(const int32_t num) { return (uint32_t)(num - 1) < FILENO_MAX; }

Chan *chnl_look(Chnl *const cl, const int32_t num)
{
    if (!chnl_valid_number(num))
        return nullptr;
    for (uint32_t i = 1; i < NCH; i++)
        if (cl->ch[i].num == num)
            return &cl->ch[i];

    return nullptr;
}

Status chnl_find(Chnl *const cl, const int32_t num, Chan **const c)
{
    *c = chnl_look(cl, num);
    return *c ? ST_OK : st_qb(QB_BAD_FILE_NUMBER);
}

/* Errors with a QBasic number of their own; the rest are Path/File access errors. */
Status chnl_doserr(const uint16_t dos)
{
    static const struct {
        uint16_t dos;
        QbErr qb;
    } map[] = {
        {PFIL_NOTFOUND, QB_FILE_NOT_FOUND},
        {PFIL_NOPATH, QB_PATH_NOT_FOUND},
        {PFIL_TOOMANY, QB_TOO_MANY_FILES},
        {PFIL_DENIED, QB_PERMISSION_DENIED},
        {PFIL_BADHANDLE, QB_BAD_FILE_NUMBER},
        {PFIL_BADDRIVE, QB_DEVICE_UNAVAILABLE},
    };

    for (size_t i = 0; i < countof(map); i++)
        if (map[i].dos == dos)
            return st_qb(map[i].qb);

    return st_qb(QB_PATH_ACCESS);
}

/* Whether name is device, any case. */
static bool chnl_names_device(const char *name, const char *device)
{
    for (; *device; device++, name++)
        if (upcase((uint8_t)*name) != (uint8_t)*device)
            return false;

    return !*name;
}

/* name a device's (SCRN: CONS: the console, NUL:): its kind, else 0. */
static uint8_t chnl_device(const char *const name)
{
    static const struct {
        uint8_t kind;
        const char *name;
    } devs[] = {{CK_CON, "SCRN:"}, {CK_CON, "CONS:"}, {CK_NUL, "NUL:"}, {CK_LPT, "LPT1:"}};

    for (size_t i = 0; i < countof(devs); i++)
        if (chnl_names_device(name, devs[i].name))
            return devs[i].kind;

    return 0;
}

/* A free channel for file number num, *c. Errors: already open, none free. */
static Status chnl_free_channel(Chnl *const cl, const int32_t num, Chan **const c)
{
    if (!chnl_valid_number(num))
        return st_qb(QB_BAD_FILE_NUMBER);

    uint32_t slot = 0;

    for (uint32_t i = NCH - 1; i > 0; i--) {
        if (cl->ch[i].num == num)
            return st_qb(QB_FILE_OPEN);
        if (!cl->ch[i].num)
            slot = i;
    }

    if (!slot)
        return st_qb(QB_TOO_MANY_FILES);

    *c = &cl->ch[slot];
    return ST_OK;
}

/* The printer's handle, opened once. Device unavailable: not ready. */
static Status chnl_printer(Chnl *const cl)
{
    if (!qbrt_printer_ready())
        return st_qb(QB_DEVICE_UNAVAILABLE);
    if (!cl->lpt_open) {
        const uint16_t err = pfil_printer(&cl->lpt_handle);

        if (err)
            return chnl_doserr(err);

        cl->lpt_open = true;
    }

    return ST_OK;
}

/* File name opened for mode, *h: read when it exists, else created (INPUT's: File not found);
   not OUTPUT's then read and write. */
static Status chnl_open_file(const char *const name, const uint8_t mode, PfilHandle *const h)
{
    uint16_t err = PFIL_NOTFOUND;

    if (!(mode & FM_OUT)) {
        err = pfil_open(name, mode & FM_IN ? PFIL_READ : PFIL_RDWR, h);

        if (err && (err != PFIL_NOTFOUND || mode & FM_IN))
            return chnl_doserr(err);
    }

    if (!err)
        return ST_OK;

    err = pfil_create(name, h);

    if (err)
        return chnl_doserr(err);
    if (!(mode & FM_OUT)) { /* reopened to read and write */
        pfil_close(*h);
        err = pfil_open(name, PFIL_RDWR, h);

        if (err)
            return chnl_doserr(err);
    }

    return ST_OK;
}

Status chnl_open(
    Chnl *const cl,
    const int32_t num,
    const char *const name,
    const uint8_t mode,
    const uint32_t reclen,
    Chan **const out
)
{
    Chan *c;
    Status st = chnl_free_channel(cl, num, &c);

    if (st != ST_OK)
        return st;
    if (reclen - 1 >= RECMAX)
        return st_qb(QB_BAD_RECORD_LENGTH);

    uint8_t *const buf = c->buf ? c->buf : pmem_resize(nullptr, CBSZ); /* kept for the slot */

    if (!buf)
        return E_OM;

    memset(c, 0, sizeof *c);
    c->buf = buf;
    c->reclen = (uint16_t)reclen;
    c->mode = mode;
    c->kind = chnl_device(name);

    if (c->kind == CK_LPT) {
        st = chnl_printer(cl);

        if (st != ST_OK)
            return st;

        cl->lpt_col = 0;
    }

    if (!c->kind) {
        c->kind = CK_DISK;
        st = chnl_open_file(name, mode, &c->handle);

        if (st != ST_OK)
            return st;
        if (mode & FM_APP) /* at the end */
            pfil_seek(c->handle, 0, PFIL_END, &c->pos);
    }

    c->num = (uint8_t)num;
    *out = c;
    return ST_OK;
}

/* c's handle at pos. */
static void place(const Chan *const c) { pfil_seek(c->handle, (int32_t)c->pos, PFIL_SET, nullptr); }

/* c's read-ahead forgotten, pos where reading was. */
static void drop(Chan *const c)
{
    c->pos += c->next;
    c->count = c->next = 0;
}

/* c's written bytes to its file. Errors: Disk full, the file's. */
static Status chnl_flush(Chan *const c)
{
    if (!c->dirty)
        return ST_OK;

    place(c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, c->buf, c->count, &put);

    if (err)
        return chnl_doserr(err);
    if (put != c->count)
        return st_qb(QB_DISK_FULL);

    c->pos += c->count;
    c->count = 0;
    c->dirty = false;
    return ST_OK;
}

/* Channel c's column: LPT1's, shared by its channels. */
static uint8_t *chnl_col(Chnl *const cl, Chan *const c)
{
    return c->kind == CK_LPT ? &cl->lpt_col : &c->col;
}

/* io's column to the output channel's. */
static void chnl_save_col(Chnl *const cl) { *chnl_col(cl, &cl->ch[cl->out]) = cl->cons->col; }

Status chnl_close(Chnl *const cl, Chan *const c)
{
    if (c == cl->ch)
        return ST_OK;

    const Status st = chnl_flush(c);

    if (st != ST_OK)
        return st;
    if (c->kind == CK_DISK)
        pfil_close(c->handle); /* LPT1's handle stays the printer's */

    c->num = 0;

    if (cl->out == chnl_index_of(cl, c))
        chnl_clear(cl);

    return ST_OK;
}

Status chnl_closeall(Chnl *const cl)
{
    chnl_clear(cl);

    for (uint32_t i = 1; i < NCH; i++) {
        if (!cl->ch[i].num)
            continue;

        const Status st = chnl_close(cl, &cl->ch[i]);

        if (st != ST_OK)
            return st;
    }

    return ST_OK;
}

Status chnl_out(Chnl *const cl, Chan *const c)
{
    if (!(c->mode & (FM_OUT | FM_APP | FM_RND | FM_BIN)))
        return st_qb(QB_BAD_FILE_MODE);

    chnl_save_col(cl);
    cl->out = chnl_index_of(cl, c);
    cl->cons->col = *chnl_col(cl, c);
    return ST_OK;
}

Status chnl_lprint(Chnl *const cl)
{
    const Status st = chnl_printer(cl);
    return st != ST_OK ? st : chnl_out(cl, &cl->ch[CH_LPT]);
}

uint8_t chnl_lpos(const Chnl *const cl)
{
    return cl->ch[cl->out].kind == CK_LPT ? cl->cons->col : cl->lpt_col;
}

bool chnl_wraps(const Chnl *const cl, const uint8_t col)
{
    return cl->ch[cl->out].kind == CK_LPT && cl->lpt_width != LPT_NOWRAP && col >= cl->lpt_width;
}

Status chnl_in(Chnl *const cl, Chan *const c)
{
    if (!(c->mode & (FM_IN | FM_RND | FM_BIN)))
        return st_qb(QB_BAD_FILE_MODE);

    cl->in = chnl_index_of(cl, c);
    return ST_OK;
}

void chnl_clear(Chnl *const cl)
{
    chnl_save_col(cl);
    cl->out = cl->in = 0;
    cl->err = ST_OK;
    cl->cons->from_file = false;
    cl->cons->col = cl->ch[0].col;
}

Status chnl_take(Chnl *const cl)
{
    const Status st = cl->err;
    cl->err = ST_OK;
    return st;
}

/* st kept as the byte path's error, the first only. */
static void keep(Chnl *const cl, const Status st)
{
    if (cl->err == ST_OK)
        cl->err = st;
}

/* b into c's buffer, written when full; a read-ahead dropped first, its place kept. */
static void chnl_put(Chnl *const cl, Chan *const c, const uint8_t b)
{
    if (!c->dirty) {
        drop(c);
        c->dirty = true;
    }

    c->buf[c->count++] = b;

    if (c->count >= CBSZ)
        keep(cl, chnl_flush(c));
}

void chnl_putc(Chnl *const cl, const uint8_t ch)
{
    Chan *const c = &cl->ch[cl->out];

    if (c->kind == CK_DISK) {
        if (cl->err != ST_OK) /* a write failed: the statement's bytes from there on dropped */
            return;
        if (ch == NL) {
            chnl_put(cl, c, '\r');
            chnl_put(cl, c, '\n');
        } else
            chnl_put(cl, c, ch);
    } else if (c->kind == CK_CON) {
        cons_out(cl->cons, ch);
    } else if (c->kind == CK_LPT && cl->err == ST_OK) {
        static const uint8_t crlf[] = {'\r', '\n'};

        uint32_t put;
        const uint16_t err = ch == NL ? pfil_write(cl->lpt_handle, crlf, 2, &put)
                                      : pfil_write(cl->lpt_handle, &ch, 1, &put);

        if (err)
            keep(cl, chnl_doserr(err));
    }
}

Status chnl_peek(Chan *const c, int *const b)
{
    *b = -1;

    if (c->kind != CK_DISK)
        return ST_OK;

    const Status st = chnl_flush(c);

    if (st != ST_OK)
        return st;
    if (c->next >= c->count) { /* the next block */
        drop(c);
        place(c);
        uint32_t got;
        const uint16_t err = pfil_read(c->handle, c->buf, CBSZ, &got);

        if (err)
            return chnl_doserr(err);

        c->count = (uint16_t)got;

        if (!got)
            return ST_OK;
    }

    const uint8_t v = c->buf[c->next];

    if (!(v == CTRL_Z && c->mode & FM_IN))
        *b = v;

    return ST_OK;
}

/* The input channel's next byte, not read; -1 none or an error, kept. */
static int peek_in(Chnl *const cl)
{
    int b;
    keep(cl, chnl_peek(&cl->ch[cl->in], &b));
    return b;
}

const uint8_t *chnl_span(Chnl *const cl, uint32_t *const n)
{
    Chan *const c = &cl->ch[cl->in];
    *n = 0;

    if (peek_in(cl) < 0)
        return nullptr;

    *n = (uint32_t)(c->count - c->next);
    return c->buf + c->next;
}

int chnl_getb_slow(Chnl *const cl)
{
    const int b = peek_in(cl);

    if (b >= 0)
        cl->ch[cl->in].next++;

    return b;
}

void chnl_skiplf(Chnl *const cl)
{
    if (peek_in(cl) == '\n')
        cl->ch[cl->in].next++;
}

uint32_t chnl_tell(const Chan *const c) { return c->pos + (c->dirty ? c->count : c->next); }

Status chnl_seek(Chan *const c, const uint32_t at)
{
    const Status st = chnl_flush(c);

    if (st != ST_OK)
        return st;

    c->count = c->next = 0;
    c->pos = at;
    return ST_OK;
}

Status chnl_lof(Chan *const c, uint32_t *const len)
{
    *len = 0;

    if (c->kind != CK_DISK)
        return ST_OK;

    const Status st = chnl_flush(c);

    if (st == ST_OK)
        pfil_seek(c->handle, 0, PFIL_END, len);

    return st;
}

/* c's buffer written and dropped, its handle at its place, for a transfer. */
static Status chnl_unbuffer(Chan *const c)
{
    const Status st = chnl_seek(c, chnl_tell(c));

    if (st == ST_OK)
        place(c);

    return st;
}

/* A transfer of done bytes, err its error: c's place past them. */
static Status chnl_moved(Chan *const c, const uint16_t err, const uint32_t done)
{
    if (err)
        return chnl_doserr(err);

    c->pos += done;
    return ST_OK;
}

Status chnl_read(Chan *const c, uint8_t *const p, const uint32_t n, uint32_t *const got)
{
    *got = 0;

    if (c->kind != CK_DISK)
        return ST_OK;

    const Status st = chnl_unbuffer(c);

    if (st != ST_OK)
        return st;

    const uint16_t err = pfil_read(c->handle, p, n, got);
    return chnl_moved(c, err, *got);
}

Status chnl_write(Chan *const c, const uint8_t *const p, const uint32_t n, uint32_t *const put)
{
    *put = 0;

    if (c->kind != CK_DISK)
        return ST_OK;

    const Status st = chnl_unbuffer(c);

    if (st != ST_OK)
        return st;

    const uint16_t err = pfil_write(c->handle, p, n, put);
    return chnl_moved(c, err, *put);
}
