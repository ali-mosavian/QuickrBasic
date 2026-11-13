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

static uint8_t index_of(const Chan *c) { return (uint8_t)(c - chnl.ch); }

Chan *chnl_look(int32_t num)
{
    if ((uint32_t)(num - 1) > 254)
        return nullptr;
    for (int i = NCH - 1; i > 0; i--)
        if (chnl.ch[i].num == num)
            return &chnl.ch[i];
    return nullptr;
}

Chan *chnl_find(int32_t num)
{
    Chan *c = chnl_look(num);
    if (!c)
        chnl_error(52); /* Bad file name or number */
    return c;
}

void chnl_error(uint8_t qb)
{
    vm.err.user = qb;
    repl_error(E_USR);
}

void chnl_doserr(uint16_t dos)
{
    /* not found, no path, too many, denied, the handle, the drive */
    static const uint8_t map[][2] = {{2, 53}, {3, 76}, {4, 67}, {5, 70}, {6, 52}, {15, 68}};
    for (size_t i = 0; i < countof(map); i++)
        if (map[i][0] == dos)
            chnl_error(map[i][1]);
    chnl_error(75); /* Path/File access error */
}

/* name a device's (SCRN: CONS: the console, NUL:): its kind, else 0. */
static uint8_t device(const char *name)
{
    static const struct {
        uint8_t kind;
        const char *name;
    } devs[] = {{CK_CON, "SCRN:"}, {CK_CON, "CONS:"}, {CK_NUL, "NUL:"}};
    for (size_t i = 0; i < countof(devs); i++) {
        const char *d = devs[i].name, *s = name;
        for (; *d; d++, s++) {
            char c = *s;
            if (c >= 'a' && c <= 'z')
                c -= 32;
            if (c != *d)
                break;
        }
        if (!*d && !*s)
            return devs[i].kind;
    }
    return 0;
}

Chan *chnl_open(int32_t num, const char *name, uint8_t mode, uint32_t reclen)
{
    if ((uint32_t)(num - 1) > 254)
        chnl_error(52);
    int free = 0;
    for (int i = NCH - 1; i > 0; i--) {
        if (chnl.ch[i].num == num)
            chnl_error(55); /* File already open */
        if (!chnl.ch[i].num)
            free = i;
    }
    if (!free)
        chnl_error(67); /* Too many files */
    if (reclen - 1 >= RECMAX)
        chnl_error(59); /* Bad record length */
    Chan *c = &chnl.ch[free];
    memset(c, 0, sizeof *c);
    c->reclen = (uint16_t)reclen;
    c->mode = mode;
    c->kind = device(name);
    if (!c->kind) {
        c->kind = CK_DISK;
        PfilHandle h;
        uint16_t err = PFIL_NOTFOUND;
        if (!(mode & FM_OUT)) {
            err = pfil_open(name, mode & FM_IN ? PFIL_READ : PFIL_RDWR, &h);
            if (err && (err != PFIL_NOTFOUND || mode & FM_IN)) /* not found: INPUT's error, */
                chnl_doserr(err);                              /* the rest create it */
        }
        if (err) {
            err = pfil_create(name, &h);
            if (err)
                chnl_doserr(err);
            if (!(mode & FM_OUT)) { /* reopened to read and write */
                pfil_close(h);
                err = pfil_open(name, PFIL_RDWR, &h);
                if (err)
                    chnl_doserr(err);
            }
        }
        c->handle = h;
        if (mode & FM_APP) /* at the end */
            pfil_seek(h, 0, PFIL_END, &c->pos);
    }
    c->num = (uint8_t)num;
    return c;
}

/* c's handle at pos. */
static void place(const Chan *c) { pfil_seek(c->handle, (int32_t)c->pos, PFIL_SET, nullptr); }

/* c's read-ahead forgotten, pos where reading was. */
static void drop(Chan *c)
{
    c->pos += c->next;
    c->count = c->next = 0;
}

/* c's written bytes to its file. Errors: 61 Disk full, the file's. */
static void flush(Chan *c)
{
    if (!c->dirty)
        return;
    place(c);
    uint32_t put;
    uint16_t err = pfil_write(c->handle, c->buf, c->count, &put);
    if (err)
        chnl_doserr(err);
    if (put != c->count)
        chnl_error(61);
    c->pos += c->count;
    c->count = 0;
    c->dirty = false;
}

/* io's column to the output channel's. */
static void save_col(void) { chnl.ch[chnl.out].col = cons.col; }

void chnl_close(Chan *c)
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
    for (int i = NCH - 1; i > 0; i--)
        if (chnl.ch[i].num)
            chnl_close(&chnl.ch[i]);
}

void chnl_out(Chan *c)
{
    if (!(c->mode & (FM_OUT | FM_APP | FM_RND | FM_BIN)))
        chnl_error(54); /* Bad file mode */
    save_col();
    chnl.out = index_of(c);
    cons.col = c->col;
}

void chnl_in(Chan *c)
{
    if (!(c->mode & (FM_IN | FM_RND | FM_BIN)))
        chnl_error(54);
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
static void put(Chan *c, uint8_t b)
{
    if (!c->dirty) {
        drop(c);
        c->dirty = true;
    }
    c->buf[c->count++] = b;
    if (c->count >= CBSZ)
        flush(c);
}

void chnl_putc(uint8_t ch)
{
    Chan *c = &chnl.ch[chnl.out];
    if (c->kind == CK_DISK) {
        if (ch == NL) {
            put(c, 13);
            ch = 10;
        }
        put(c, ch);
    } else if (c->kind == CK_CON) {
        cons_out(ch);
    }
}

int chnl_peek(Chan *c)
{
    if (c->kind != CK_DISK)
        return -1;
    flush(c);
    if (c->next >= c->count) { /* the next block */
        drop(c);
        place(c);
        uint32_t got;
        uint16_t err = pfil_read(c->handle, c->buf, CBSZ, &got);
        if (err)
            chnl_doserr(err);
        c->count = (uint16_t)got;
        if (!got)
            return -1;
    }
    uint8_t b = c->buf[c->next];
    if (b == 0x1A && c->mode & FM_IN) /* ^Z: a text file's end */
        return -1;
    return b;
}

int chnl_getb(void)
{
    Chan *c = &chnl.ch[chnl.in];
    int b = chnl_peek(c);
    if (b >= 0)
        c->next++;
    return b;
}

void chnl_skiplf(void)
{
    Chan *c = &chnl.ch[chnl.in];
    if (chnl_peek(c) == 10)
        c->next++;
}

uint32_t chnl_tell(const Chan *c) { return c->pos + (c->dirty ? c->count : c->next); }

void chnl_seek(Chan *c, uint32_t at)
{
    flush(c);
    c->count = c->next = 0;
    c->pos = at;
}

uint32_t chnl_lof(Chan *c)
{
    if (c->kind != CK_DISK)
        return 0;
    flush(c);
    uint32_t end = 0;
    pfil_seek(c->handle, 0, PFIL_END, &end);
    return end;
}

/* n bytes at c's place read (write false) or written; how many. */
static uint32_t transfer(Chan *c, uint8_t *p, uint32_t n, bool write)
{
    if (c->kind != CK_DISK)
        return 0;
    chnl_seek(c, chnl_tell(c));
    place(c);
    uint32_t done;
    uint16_t err = write ? pfil_write(c->handle, p, n, &done) : pfil_read(c->handle, p, n, &done);
    if (err)
        chnl_doserr(err);
    c->pos += done;
    return done;
}

uint32_t chnl_read(Chan *c, uint8_t *p, uint32_t n) { return transfer(c, p, n, false); }
uint32_t chnl_write(Chan *c, const uint8_t *p, uint32_t n) { return transfer(c, (uint8_t *)p, n, true); }
