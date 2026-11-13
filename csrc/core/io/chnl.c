/* Channels (io/chan.asm). */
#include "io/chnl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "vm/exec.h"
#include "plat/pfil.h"

Chnl chnl;

void chnl_init(void)
{
    memset(chnl.ch, 0, sizeof chnl.ch);
    chnl.ch[0].kind = CK_CON;
    chnl.ch[0].mode = FM_OUT | FM_IN;
    chnl.out = chnl.in = 0;
}

enum { CTRL_Z = 0x1A }; /* a text file's end */

static uint8_t index_of(const Chan *const c) { return (uint8_t)(c - chnl.ch); }

static bool valid_number(const int32_t num) { return (uint32_t)(num - 1) < FILENO_MAX; }

Chan *chnl_look(const int32_t num)
{
    if (!valid_number(num))
        return nullptr;
    for (uint32_t i = 1; i < NCH; i++)
        if (chnl.ch[i].num == num)
            return &chnl.ch[i];
    return nullptr;
}

Chan *chnl_find(const int32_t num)
{
    Chan *const c = chnl_look(num);
    if (!c)
        repl_qberr(QB_BAD_FILE_NUMBER);
    return c;
}

/* Errors with a QBasic number of their own; the rest are Path/File access errors. */
void chnl_doserr(const uint16_t dos)
{
    static const struct {
        uint16_t dos;
        QbErr qb;
    } map[] = {
        {PFIL_NOTFOUND, QB_FILE_NOT_FOUND}, {PFIL_NOPATH, QB_PATH_NOT_FOUND},
        {PFIL_TOOMANY, QB_TOO_MANY_FILES},  {PFIL_DENIED, QB_PERMISSION_DENIED},
        {PFIL_BADHANDLE, QB_BAD_FILE_NUMBER}, {PFIL_BADDRIVE, QB_DEVICE_UNAVAILABLE},
    };
    for (size_t i = 0; i < countof(map); i++)
        if (map[i].dos == dos)
            repl_qberr(map[i].qb);
    repl_qberr(QB_PATH_ACCESS);
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
static Chan *free_channel(const int32_t num)
{
    if (!valid_number(num))
        repl_qberr(QB_BAD_FILE_NUMBER);
    uint32_t slot = 0;
    for (uint32_t i = NCH - 1; i > 0; i--) {
        if (chnl.ch[i].num == num)
            repl_qberr(QB_FILE_OPEN);
        if (!chnl.ch[i].num)
            slot = i;
    }
    if (!slot)
        repl_qberr(QB_TOO_MANY_FILES);
    return &chnl.ch[slot];
}

/* File name opened for mode: read when it exists, else created (INPUT's: File not found); not
   OUTPUT's then read and write. */
static PfilHandle open_file(const char *const name, const uint8_t mode)
{
    PfilHandle h;
    uint16_t err = PFIL_NOTFOUND;
    if (!(mode & FM_OUT)) {
        err = pfil_open(name, mode & FM_IN ? PFIL_READ : PFIL_RDWR, &h);
        if (err && (err != PFIL_NOTFOUND || mode & FM_IN))
            chnl_doserr(err);
    }
    if (!err)
        return h;
    err = pfil_create(name, &h);
    if (err)
        chnl_doserr(err);
    if (!(mode & FM_OUT)) { /* reopened to read and write */
        pfil_close(h);
        err = pfil_open(name, PFIL_RDWR, &h);
        if (err)
            chnl_doserr(err);
    }
    return h;
}

Chan *chnl_open(const int32_t num, const char *const name, const uint8_t mode, const uint32_t reclen)
{
    Chan *const c = free_channel(num);
    if (reclen - 1 >= RECMAX)
        repl_qberr(QB_BAD_RECORD_LENGTH);
    memset(c, 0, sizeof *c);
    c->reclen = (uint16_t)reclen;
    c->mode = mode;
    c->kind = device(name);
    if (!c->kind) {
        c->kind = CK_DISK;
        c->handle = open_file(name, mode);
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
static void flush(Chan *const c)
{
    if (!c->dirty)
        return;
    place(c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, c->buf, c->count, &put);
    if (err)
        chnl_doserr(err);
    if (put != c->count)
        repl_qberr(QB_DISK_FULL);
    c->pos += c->count;
    c->count = 0;
    c->dirty = false;
}

/* io's column to the output channel's. */
static void save_col(void) { chnl.ch[chnl.out].col = cons.col; }

void chnl_close(Chan *const c)
{
    if (c == chnl.ch)
        return;
    flush(c);
    if (c->kind == CK_DISK)
        pfil_close(c->handle);
    c->num = 0;
    if (chnl.out == index_of(c))
        chnl_clear();
}

void chnl_closeall(void)
{
    chnl_clear();
    for (uint32_t i = 1; i < NCH; i++)
        if (chnl.ch[i].num)
            chnl_close(&chnl.ch[i]);
}

void chnl_out(Chan *const c)
{
    if (!(c->mode & (FM_OUT | FM_APP | FM_RND | FM_BIN)))
        repl_qberr(QB_BAD_FILE_MODE);
    save_col();
    chnl.out = index_of(c);
    cons.col = c->col;
}

void chnl_in(Chan *const c)
{
    if (!(c->mode & (FM_IN | FM_RND | FM_BIN)))
        repl_qberr(QB_BAD_FILE_MODE);
    chnl.in = index_of(c);
}

void chnl_clear(void)
{
    save_col();
    chnl.out = chnl.in = 0;
    cons.from_file = false;
    cons.col = chnl.ch[0].col;
}

/* b into c's buffer, written when full; a read-ahead dropped first, its place kept. */
static void put(Chan *const c, const uint8_t b)
{
    if (!c->dirty) {
        drop(c);
        c->dirty = true;
    }
    c->buf[c->count++] = b;
    if (c->count >= CBSZ)
        flush(c);
}

void chnl_putc(const uint8_t ch)
{
    Chan *const c = &chnl.ch[chnl.out];
    if (c->kind == CK_DISK) {
        if (ch == NL) {
            put(c, '\r');
            put(c, '\n');
        } else
            put(c, ch);
    } else if (c->kind == CK_CON) {
        cons_out(ch);
    }
}

int chnl_peek(Chan *const c)
{
    if (c->kind != CK_DISK)
        return -1;
    flush(c);
    if (c->next >= c->count) { /* the next block */
        drop(c);
        place(c);
        uint32_t got;
        const uint16_t err = pfil_read(c->handle, c->buf, CBSZ, &got);
        if (err)
            chnl_doserr(err);
        c->count = (uint16_t)got;
        if (!got)
            return -1;
    }
    const uint8_t b = c->buf[c->next];
    if (b == CTRL_Z && c->mode & FM_IN)
        return -1;
    return b;
}

int chnl_getb(void)
{
    Chan *const c = &chnl.ch[chnl.in];
    const int b = chnl_peek(c);
    if (b >= 0)
        c->next++;
    return b;
}

void chnl_skiplf(void)
{
    Chan *const c = &chnl.ch[chnl.in];
    if (chnl_peek(c) == '\n')
        c->next++;
}

uint32_t chnl_tell(const Chan *const c) { return c->pos + (c->dirty ? c->count : c->next); }

void chnl_seek(Chan *const c, const uint32_t at)
{
    flush(c);
    c->count = c->next = 0;
    c->pos = at;
}

uint32_t chnl_lof(Chan *const c)
{
    if (c->kind != CK_DISK)
        return 0;
    flush(c);
    uint32_t end = 0;
    pfil_seek(c->handle, 0, PFIL_END, &end);
    return end;
}

/* c's buffer written and dropped, its handle at its place, for a transfer. */
static void unbuffer(Chan *const c)
{
    chnl_seek(c, chnl_tell(c));
    place(c);
}

/* A transfer of done bytes, err its error: c's place past them. */
static uint32_t moved(Chan *const c, const uint16_t err, const uint32_t done)
{
    if (err)
        chnl_doserr(err);
    c->pos += done;
    return done;
}

uint32_t chnl_read(Chan *const c, uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;
    unbuffer(c);
    uint32_t got;
    const uint16_t err = pfil_read(c->handle, p, n, &got);
    return moved(c, err, got);
}

uint32_t chnl_write(Chan *const c, const uint8_t *const p, const uint32_t n)
{
    if (c->kind != CK_DISK)
        return 0;
    unbuffer(c);
    uint32_t put;
    const uint16_t err = pfil_write(c->handle, p, n, &put);
    return moved(c, err, put);
}
