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
 * # asm structure (src/io/chan.asm)
 * | asm | here | |
 * |---|---|---|
 * | ch_init | chnl_init | MIRROR |
 * | ch_look, ch_find | chnl_look, chnl_find | MIRROR; null for none |
 * | ch_err | repl_qberr (repl.c) | MOVED |
 * | ch_doserr | chnl_doserr | MIRROR |
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
#include "io/repl.h"
#include "vm/exec.h"
#include "plat/pfil.h"

void chnl_init(Chnl *const cl, Cons *const co, Fault *const f)
{
    cl->cons = co;
    cl->fault = f;
    memset(cl->ch, 0, sizeof cl->ch);
    cl->ch[0].kind = CK_CON;
    cl->ch[0].mode = FM_OUT | FM_IN;
    cl->out = cl->in = 0;
}

enum { CTRL_Z = 0x1A }; /* a text file's end */

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

Chan *chnl_find(Chnl *const cl, const int32_t num)
{
    Chan *const c = chnl_look(cl, num);

    if (!c)
        fault_qberr(cl->fault, QB_BAD_FILE_NUMBER);

    return c;
}

/* Errors with a QBasic number of their own; the rest are Path/File access errors. */
void chnl_doserr(Chnl *const cl, const uint16_t dos)
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
            fault_qberr(cl->fault, map[i].qb);

    fault_qberr(cl->fault, QB_PATH_ACCESS);
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
    } devs[] = {{CK_CON, "SCRN:"}, {CK_CON, "CONS:"}, {CK_NUL, "NUL:"}};

    for (size_t i = 0; i < countof(devs); i++)
        if (chnl_names_device(name, devs[i].name))
            return devs[i].kind;

    return 0;
}

/* A free channel for file number num. Errors: already open, none free. */
static Chan *chnl_free_channel(Chnl *const cl, const int32_t num)
{
    if (!chnl_valid_number(num))
        fault_qberr(cl->fault, QB_BAD_FILE_NUMBER);

    uint32_t slot = 0;

    for (uint32_t i = NCH - 1; i > 0; i--) {
        if (cl->ch[i].num == num)
            fault_qberr(cl->fault, QB_FILE_OPEN);
        if (!cl->ch[i].num)
            slot = i;
    }

    if (!slot)
        fault_qberr(cl->fault, QB_TOO_MANY_FILES);

    return &cl->ch[slot];
}

/* File name opened for mode: read when it exists, else created (INPUT's: File not found); not
   OUTPUT's then read and write. */
static PfilHandle chnl_open_file(Chnl *const cl, const char *const name, const uint8_t mode)
{
    PfilHandle h;
    uint16_t err = PFIL_NOTFOUND;

    if (!(mode & FM_OUT)) {
        err = pfil_open(name, mode & FM_IN ? PFIL_READ : PFIL_RDWR, &h);

        if (err && (err != PFIL_NOTFOUND || mode & FM_IN))
            chnl_doserr(cl, err);
    }

    if (!err)
        return h;

    err = pfil_create(name, &h);

    if (err)
        chnl_doserr(cl, err);
    if (!(mode & FM_OUT)) { /* reopened to read and write */
        pfil_close(h);
        err = pfil_open(name, PFIL_RDWR, &h);

        if (err)
            chnl_doserr(cl, err);
    }

    return h;
}

Chan *chnl_open(
    Chnl *const cl,
    const int32_t num,
    const char *const name,
    const uint8_t mode,
    const uint32_t reclen
)
{
    Chan *const c = chnl_free_channel(cl, num);

    if (reclen - 1 >= RECMAX)
        fault_qberr(cl->fault, QB_BAD_RECORD_LENGTH);

    memset(c, 0, sizeof *c);
    c->reclen = (uint16_t)reclen;
    c->mode = mode;
    c->kind = chnl_device(name);

    if (!c->kind) {
        c->kind = CK_DISK;
        c->handle = chnl_open_file(cl, name, mode);

        if (mode & FM_APP) /* at the end */
            pfil_seek(c->handle, 0, PFIL_END, &c->pos);
    }

    c->num = (uint8_t)num;
    return c;
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
static void chnl_flush(Chnl *const cl, Chan *const c)
{
    if (!c->dirty)
        return;

    place(c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, c->buf, c->count, &put);

    if (err)
        chnl_doserr(cl, err);
    if (put != c->count)
        fault_qberr(cl->fault, QB_DISK_FULL);

    c->pos += c->count;
    c->count = 0;
    c->dirty = false;
}

/* io's column to the output channel's. */
static void chnl_save_col(Chnl *const cl) { cl->ch[cl->out].col = cl->cons->col; }

void chnl_close(Chnl *const cl, Chan *const c)
{
    if (c == cl->ch)
        return;

    chnl_flush(cl, c);

    if (c->kind == CK_DISK)
        pfil_close(c->handle);

    c->num = 0;

    if (cl->out == chnl_index_of(cl, c))
        chnl_clear(cl);
}

void chnl_closeall(Chnl *const cl)
{
    chnl_clear(cl);

    for (uint32_t i = 1; i < NCH; i++)
        if (cl->ch[i].num)
            chnl_close(cl, &cl->ch[i]);
}

void chnl_out(Chnl *const cl, Chan *const c)
{
    if (!(c->mode & (FM_OUT | FM_APP | FM_RND | FM_BIN)))
        fault_qberr(cl->fault, QB_BAD_FILE_MODE);

    chnl_save_col(cl);
    cl->out = chnl_index_of(cl, c);
    cl->cons->col = c->col;
}

void chnl_in(Chnl *const cl, Chan *const c)
{
    if (!(c->mode & (FM_IN | FM_RND | FM_BIN)))
        fault_qberr(cl->fault, QB_BAD_FILE_MODE);

    cl->in = chnl_index_of(cl, c);
}

void chnl_clear(Chnl *const cl)
{
    chnl_save_col(cl);
    cl->out = cl->in = 0;
    cl->cons->from_file = false;
    cl->cons->col = cl->ch[0].col;
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
        chnl_flush(cl, c);
}

void chnl_putc(Chnl *const cl, const uint8_t ch)
{
    Chan *const c = &cl->ch[cl->out];

    if (c->kind == CK_DISK) {
        if (ch == NL) {
            chnl_put(cl, c, '\r');
            chnl_put(cl, c, '\n');
        } else
            chnl_put(cl, c, ch);
    } else if (c->kind == CK_CON) {
        cons_out(cl->cons, ch);
    }
}

int chnl_peek(Chnl *const cl, Chan *const c)
{
    if (c->kind != CK_DISK)
        return -1;

    chnl_flush(cl, c);

    if (c->next >= c->count) { /* the next block */
        drop(c);
        place(c);
        uint32_t got;
        const uint16_t err = pfil_read(c->handle, c->buf, CBSZ, &got);

        if (err)
            chnl_doserr(cl, err);

        c->count = (uint16_t)got;

        if (!got)
            return -1;
    }

    const uint8_t b = c->buf[c->next];

    if (b == CTRL_Z && c->mode & FM_IN)
        return -1;

    return b;
}

int chnl_getb(Chnl *const cl)
{
    Chan *const c = &cl->ch[cl->in];
    const int b = chnl_peek(cl, c);

    if (b >= 0)
        c->next++;

    return b;
}

void chnl_skiplf(Chnl *const cl)
{
    Chan *const c = &cl->ch[cl->in];

    if (chnl_peek(cl, c) == '\n')
        c->next++;
}

uint32_t chnl_tell(const Chan *const c) { return c->pos + (c->dirty ? c->count : c->next); }

void chnl_seek(Chnl *const cl, Chan *const c, const uint32_t at)
{
    chnl_flush(cl, c);
    c->count = c->next = 0;
    c->pos = at;
}

uint32_t chnl_lof(Chnl *const cl, Chan *const c)
{
    if (c->kind != CK_DISK)
        return 0;

    chnl_flush(cl, c);
    uint32_t end = 0;
    pfil_seek(c->handle, 0, PFIL_END, &end);
    return end;
}

/* c's buffer written and dropped, its handle at its place, for a transfer. */
static void chnl_unbuffer(Chnl *const cl, Chan *const c)
{
    chnl_seek(cl, c, chnl_tell(c));
    place(c);
}

/* A transfer of done bytes, err its error: c's place past them. */
static uint32_t chnl_moved(Chnl *const cl, Chan *const c, const uint16_t err, const uint32_t done)
{
    if (err)
        chnl_doserr(cl, err);

    c->pos += done;
    return done;
}

uint32_t chnl_read(Chnl *const cl, Chan *const c, uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;

    chnl_unbuffer(cl, c);
    uint32_t got;
    const uint16_t err = pfil_read(c->handle, p, n, &got);
    return chnl_moved(cl, c, err, got);
}

uint32_t chnl_write(Chnl *const cl, Chan *const c, const uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;

    chnl_unbuffer(cl, c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, p, n, &put);
    return chnl_moved(cl, c, err, put);
}
