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
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "vm/exec.h"
#include "plat/pfil.h"

void chnl_init(Session *const S)
{
    memset(S->chnl.ch, 0, sizeof S->chnl.ch);
    S->chnl.ch[0].kind = CK_CON;
    S->chnl.ch[0].mode = FM_OUT | FM_IN;
    S->chnl.out = S->chnl.in = 0;
}

enum { CTRL_Z = 0x1A }; /* a text file's end */

static uint8_t index_of(Session *const S, const Chan *const c) { return (uint8_t)(c - S->chnl.ch); }

static bool valid_number(const int32_t num) { return (uint32_t)(num - 1) < FILENO_MAX; }

Chan *chnl_look(Session *const S, const int32_t num)
{
    if (!valid_number(num))
        return nullptr;
    for (uint32_t i = 1; i < NCH; i++)
        if (S->chnl.ch[i].num == num)
            return &S->chnl.ch[i];

    return nullptr;
}

Chan *chnl_find(Session *const S, const int32_t num)
{
    Chan *const c = chnl_look(S, num);

    if (!c)
        repl_qberr(S, QB_BAD_FILE_NUMBER);

    return c;
}

/* Errors with a QBasic number of their own; the rest are Path/File access errors. */
void chnl_doserr(Session *const S, const uint16_t dos)
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
            repl_qberr(S, map[i].qb);

    repl_qberr(S, QB_PATH_ACCESS);
}

/* Whether name is device, any case. */
static bool names_device(const char *name, const char *device)
{
    for (; *device; device++, name++)
        if (upcase((uint8_t)*name) != (uint8_t)*device)
            return false;

    return !*name;
}

/* name a device's (SCRN: CONS: the console, NUL:): its kind, else 0. */
static uint8_t device(const char *const name)
{
    static const struct {
        uint8_t kind;
        const char *name;
    } devs[] = {{CK_CON, "SCRN:"}, {CK_CON, "CONS:"}, {CK_NUL, "NUL:"}};

    for (size_t i = 0; i < countof(devs); i++)
        if (names_device(name, devs[i].name))
            return devs[i].kind;

    return 0;
}

/* A free channel for file number num. Errors: already open, none free. */
static Chan *free_channel(Session *const S, const int32_t num)
{
    if (!valid_number(num))
        repl_qberr(S, QB_BAD_FILE_NUMBER);

    uint32_t slot = 0;

    for (uint32_t i = NCH - 1; i > 0; i--) {
        if (S->chnl.ch[i].num == num)
            repl_qberr(S, QB_FILE_OPEN);
        if (!S->chnl.ch[i].num)
            slot = i;
    }

    if (!slot)
        repl_qberr(S, QB_TOO_MANY_FILES);

    return &S->chnl.ch[slot];
}

/* File name opened for mode: read when it exists, else created (INPUT's: File not found); not
   OUTPUT's then read and write. */
static PfilHandle open_file(Session *const S, const char *const name, const uint8_t mode)
{
    PfilHandle h;
    uint16_t err = PFIL_NOTFOUND;

    if (!(mode & FM_OUT)) {
        err = pfil_open(name, mode & FM_IN ? PFIL_READ : PFIL_RDWR, &h);

        if (err && (err != PFIL_NOTFOUND || mode & FM_IN))
            chnl_doserr(S, err);
    }

    if (!err)
        return h;

    err = pfil_create(name, &h);

    if (err)
        chnl_doserr(S, err);
    if (!(mode & FM_OUT)) { /* reopened to read and write */
        pfil_close(h);
        err = pfil_open(name, PFIL_RDWR, &h);

        if (err)
            chnl_doserr(S, err);
    }

    return h;
}

Chan *chnl_open(
    Session *const S,
    const int32_t num,
    const char *const name,
    const uint8_t mode,
    const uint32_t reclen
)
{
    Chan *const c = free_channel(S, num);

    if (reclen - 1 >= RECMAX)
        repl_qberr(S, QB_BAD_RECORD_LENGTH);

    memset(c, 0, sizeof *c);
    c->reclen = (uint16_t)reclen;
    c->mode = mode;
    c->kind = device(name);

    if (!c->kind) {
        c->kind = CK_DISK;
        c->handle = open_file(S, name, mode);

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
static void flush(Session *const S, Chan *const c)
{
    if (!c->dirty)
        return;

    place(c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, c->buf, c->count, &put);

    if (err)
        chnl_doserr(S, err);
    if (put != c->count)
        repl_qberr(S, QB_DISK_FULL);

    c->pos += c->count;
    c->count = 0;
    c->dirty = false;
}

/* io's column to the output channel's. */
static void save_col(Session *const S) { S->chnl.ch[S->chnl.out].col = S->cons.col; }

void chnl_close(Session *const S, Chan *const c)
{
    if (c == S->chnl.ch)
        return;

    flush(S, c);

    if (c->kind == CK_DISK)
        pfil_close(c->handle);

    c->num = 0;

    if (S->chnl.out == index_of(S, c))
        chnl_clear(S);
}

void chnl_closeall(Session *const S)
{
    chnl_clear(S);

    for (uint32_t i = 1; i < NCH; i++)
        if (S->chnl.ch[i].num)
            chnl_close(S, &S->chnl.ch[i]);
}

void chnl_out(Session *const S, Chan *const c)
{
    if (!(c->mode & (FM_OUT | FM_APP | FM_RND | FM_BIN)))
        repl_qberr(S, QB_BAD_FILE_MODE);

    save_col(S);
    S->chnl.out = index_of(S, c);
    S->cons.col = c->col;
}

void chnl_in(Session *const S, Chan *const c)
{
    if (!(c->mode & (FM_IN | FM_RND | FM_BIN)))
        repl_qberr(S, QB_BAD_FILE_MODE);

    S->chnl.in = index_of(S, c);
}

void chnl_clear(Session *const S)
{
    save_col(S);
    S->chnl.out = S->chnl.in = 0;
    S->cons.from_file = false;
    S->cons.col = S->chnl.ch[0].col;
}

/* b into c's buffer, written when full; a read-ahead dropped first, its place kept. */
static void put(Session *const S, Chan *const c, const uint8_t b)
{
    if (!c->dirty) {
        drop(c);
        c->dirty = true;
    }

    c->buf[c->count++] = b;

    if (c->count >= CBSZ)
        flush(S, c);
}

void chnl_putc(Session *const S, const uint8_t ch)
{
    Chan *const c = &S->chnl.ch[S->chnl.out];

    if (c->kind == CK_DISK) {
        if (ch == NL) {
            put(S, c, '\r');
            put(S, c, '\n');
        } else
            put(S, c, ch);
    } else if (c->kind == CK_CON) {
        cons_out(S, ch);
    }
}

int chnl_peek(Session *const S, Chan *const c)
{
    if (c->kind != CK_DISK)
        return -1;

    flush(S, c);

    if (c->next >= c->count) { /* the next block */
        drop(c);
        place(c);
        uint32_t got;
        const uint16_t err = pfil_read(c->handle, c->buf, CBSZ, &got);

        if (err)
            chnl_doserr(S, err);

        c->count = (uint16_t)got;

        if (!got)
            return -1;
    }

    const uint8_t b = c->buf[c->next];

    if (b == CTRL_Z && c->mode & FM_IN)
        return -1;

    return b;
}

int chnl_getb(Session *const S)
{
    Chan *const c = &S->chnl.ch[S->chnl.in];
    const int b = chnl_peek(S, c);

    if (b >= 0)
        c->next++;

    return b;
}

void chnl_skiplf(Session *const S)
{
    Chan *const c = &S->chnl.ch[S->chnl.in];

    if (chnl_peek(S, c) == '\n')
        c->next++;
}

uint32_t chnl_tell(const Chan *const c) { return c->pos + (c->dirty ? c->count : c->next); }

void chnl_seek(Session *const S, Chan *const c, const uint32_t at)
{
    flush(S, c);
    c->count = c->next = 0;
    c->pos = at;
}

uint32_t chnl_lof(Session *const S, Chan *const c)
{
    if (c->kind != CK_DISK)
        return 0;

    flush(S, c);
    uint32_t end = 0;
    pfil_seek(c->handle, 0, PFIL_END, &end);
    return end;
}

/* c's buffer written and dropped, its handle at its place, for a transfer. */
static void unbuffer(Session *const S, Chan *const c)
{
    chnl_seek(S, c, chnl_tell(c));
    place(c);
}

/* A transfer of done bytes, err its error: c's place past them. */
static uint32_t moved(Session *const S, Chan *const c, const uint16_t err, const uint32_t done)
{
    if (err)
        chnl_doserr(S, err);

    c->pos += done;
    return done;
}

uint32_t chnl_read(Session *const S, Chan *const c, uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;

    unbuffer(S, c);
    uint32_t got;
    const uint16_t err = pfil_read(c->handle, p, n, &got);
    return moved(S, c, err, got);
}

uint32_t chnl_write(Session *const S, Chan *const c, const uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;

    unbuffer(S, c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, p, n, &put);
    return moved(S, c, err, put);
}
