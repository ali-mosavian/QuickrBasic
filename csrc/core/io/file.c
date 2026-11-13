/* QBasic's file statements and functions (io/fio.asm). */
#include "io/file.h"
#include "io/chnl.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/allo.h"
#include "mem/strg.h"
#include "num/dubl.h"
#include "num/sngl.h"
#include "plat/pfil.h"
#include "plat/pkey.h"
#include "vm/exec.h"

File file;

static Chan *field_chan; /* FIELD's channel */
static char name2[128];  /* NAME's new name; FILES's directory */
static char bname[264];  /* BSAVE's, BLOAD's name, .BAS added */

void file_init(void) { file.recs = allo_carve(NCH * RECMAX); }

/* A variable's place's kind (VA_*) and its n. */
static uint8_t va_kind(uint32_t kind) { return (uint8_t)(kind >> 24); }
static uint16_t va_n(uint32_t kind) { return (uint16_t)kind; }

/* String value s as a 0-ended name. Error: 64 Bad file name, empty. */
static const char *name_of(Addr s)
{
    Desc d = strg_get(s);
    if (!d.len)
        chnl_error(64);
    return (const char *)strg_near(d);
}

/* name_of, .BAS added when its last part has no '.'. */
static const char *bas_name(Addr s)
{
    const char *n = name_of(s);
    size_t len = strlen(n);
    memcpy(bname, n, len + 1);
    bool dot = false;
    for (const char *p = bname; *p; p++) {
        if (*p == '.')
            dot = true;
        if (*p == '\\' || *p == ':')
            dot = false;
    }
    if (!dot)
        memcpy(bname + len, ".BAS", 5);
    return bname;
}

/* c's record buffer. */
static Addr record(const Chan *c) { return file.recs + (uint32_t)(c - chnl.ch) * RECMAX; }

void file_opendef(void)
{
    file.mode = FM_RND;
    file.reclen = 128;
}
void file_mode_in(void) { file.mode = FM_IN; }
void file_mode_out(void) { file.mode = FM_OUT; }
void file_mode_app(void) { file.mode = FM_APP; }
void file_mode_rnd(void) { file.mode = FM_RND; }
void file_mode_bin(void) { file.mode = FM_BIN; }

void file_reclen(void)
{
    uint32_t n = (uint32_t)exec_popi();
    if (n - 1 > RECMAX - 1)
        chnl_error(59); /* Bad record length */
    file.reclen = n;
}

void file_open(void)
{
    int32_t num = exec_popi();
    const char *name = name_of(exec_popa());
    chnl_open(num, name, file.mode, file.reclen);
}

void file_open2(void)
{
    const char *name = name_of(exec_popa());
    int32_t num = exec_popi();
    Desc m = strg_get(exec_popa());
    static const char modes[] = {'I', FM_IN, 'O', FM_OUT, 'A', FM_APP, 'R', FM_RND, 'B', FM_BIN};
    if (m.len) {
        uint8_t c = rd8(m.ptr);
        if (c >= 'a' && c <= 'z')
            c -= 32;
        for (size_t i = 0; i < sizeof modes; i += 2)
            if (c == modes[i]) {
                file.mode = (uint8_t)modes[i + 1];
                chnl_open(num, name, file.mode, file.reclen);
                return;
            }
    }
    chnl_error(54); /* Bad file mode */
}

void file_close(void)
{
    Chan *c = chnl_look(exec_popi());
    if (c)
        chnl_close(c);
}

void file_closeall(void) { chnl_closeall(); }
void file_out(void) { chnl_out(chnl_find(exec_popi())); }
void file_in(void) { chnl_in(chnl_find(exec_popi())); }
void file_console(void) { chnl_clear(); }

void file_lineinput(void)
{
    uint16_t len = (uint16_t)cons.inlen;
    Addr chars = strg_alloc(len);
    exec_pusha(strg_tods(chars, addr_of(cons.inbuf), len));
}

/* Output to sbuf, from its start. */
static void capture(void)
{
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
}

/* sbuf's text, its spaces left out, to the output. */
static void write_out(void)
{
    cons.capture = 0;
    for (uint32_t i = 0; i < cons.sblen; i++)
        if (cons.sbuf[i] != ' ')
            cons_putc(cons.sbuf[i]);
}

void file_write_int(void)
{
    int32_t v = exec_popi();
    capture();
    cons_int(v);
    write_out();
}

void file_write_flt(void)
{
    Sngl v = exec_pop().u;
    capture();
    sngl_print(v);
    write_out();
}

void file_write_dbl(void)
{
    uint8_t text[DUBL_TXT];
    uint32_t n = dubl_fmt(exec_popd(), text);
    capture();
    for (uint32_t i = 0; i < n; i++)
        cons_putc(text[i]);
    write_out();
}

void file_write_str(void)
{
    Desc d = strg_get(exec_popa());
    cons_putc('"');
    strg_print(d);
    cons_putc('"');
}

void file_write_comma(void) { cons_putc(','); }

void file_eof(void)
{
    Chan *c = chnl_find(exec_popi());
    int32_t r;
    if (c->mode & (FM_RND | FM_BIN))
        r = c->eof ? -1 : 0;
    else if (c->mode & FM_IN)
        r = chnl_peek(c) < 0 ? -1 : 0;
    else
        r = -1; /* written: at its end */
    exec_pushi(r);
}

void file_lof(void) { exec_pushi((int32_t)chnl_lof(chnl_find(exec_popi()))); }

void file_loc(void)
{
    Chan *c = chnl_find(exec_popi());
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t = c->last;
    else if (!(c->mode & FM_BIN))
        t = (t + 127) >> 7;
    exec_pushi((int32_t)t);
}

void file_seekpos(void)
{
    Chan *c = chnl_find(exec_popi());
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t /= c->reclen;
    exec_pushi((int32_t)(t + 1));
}

/* Record or byte p of c, from 1, as an offset. Error: 63 Bad record number, under 1. */
static uint32_t offset_of(const Chan *c, uint32_t p)
{
    p--;
    if ((int32_t)p < 0)
        chnl_error(63);
    return c->mode & FM_RND ? p * c->reclen : p;
}

void file_seek(void)
{
    Chan *c = chnl_find(exec.sp[1].i);
    uint32_t p = exec.sp[0].u;
    exec.sp += 2;
    chnl_seek(c, offset_of(c, p));
    c->eof = false;
}

void file_freefile(void)
{
    int32_t n = 1;
    while (n <= 255 && chnl_look(n))
        n++;
    exec_pushi(n);
}

void file_input_str(void)
{
    Chan *c = nullptr; /* nullptr: the keyboard */
    if (exec_popi() != 1)
        c = chnl_find(exec_popi());
    uint32_t n = (uint32_t)exec_popi();
    if (n - 1 > INMAX - 1)
        repl_error(E_IQ);
    Addr chars = strg_alloc((uint16_t)n);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b;
        if (c) {
            int v = chnl_peek(c);
            if (v < 0)
                chnl_error(62); /* Input past end of file */
            c->next++;
            b = (uint8_t)v;
        } else {
            cons_flush();
            b = (uint8_t)pkey_wait();
        }
        wr8(chars + i, b);
    }
    exec_pusha(strg_new(chars, (uint16_t)n));
}

void file_attr(void)
{
    int32_t a = exec_popi();
    Chan *c = chnl_find(exec_popi());
    int32_t r = c->handle;
    if (a != 2) {
        if (a != 1)
            repl_error(E_IQ);
        /* 1 2 4 8 32: INPUT OUTPUT RANDOM APPEND BINARY */
        r = c->mode & FM_IN ? 1 : c->mode & FM_OUT ? 2 : c->mode & FM_RND ? 4 : c->mode & FM_APP ? 8 : 32;
    }
    exec_pushi(r);
}

static void dos(uint16_t err)
{
    if (err)
        chnl_doserr(err);
}

void file_kill(void) { dos(pfil_delete(name_of(exec_popa()))); }
void file_mkdir(void) { dos(pfil_mkdir(name_of(exec_popa()))); }
void file_rmdir(void) { dos(pfil_rmdir(name_of(exec_popa()))); }
void file_chdir(void) { dos(pfil_chdir(name_of(exec_popa()))); }

void file_rename(void)
{
    const char *to = name_of(exec_popa());
    size_t n = strlen(to);
    memcpy(name2, to, n + 1);
    uint16_t err = pfil_rename(name_of(exec_popa()), name2);
    if (err == PFIL_DENIED)
        chnl_error(58); /* File already exists */
    dos(err);
}

void file_files(void)
{
    static char pattern[264];
    memcpy(pattern, "*.*", 4);
    if (exec_popi()) {
        const char *n = name_of(exec_popa());
        memcpy(pattern, n, strlen(n) + 1);
    }
    cons_putc('\\'); /* the current directory */
    if (pfil_curdir(0, name2, sizeof name2))
        name2[0] = 0;
    cons_puts(name2);
    cons_nl();
    PfilFind f;
    if (pfil_first(pattern, PFIL_ADIR, &f))
        chnl_error(53); /* File not found */
    do {
        const char *s = f.name; /* NAME.EXT as NAME    .EXT */
        uint32_t n = 0;
        for (; *s && *s != '.'; s++, n++)
            cons_putc((uint8_t)*s);
        for (; n < 8; n++)
            cons_space();
        if (*s)
            s++;
        cons_putc('.');
        int32_t pad = 3;
        for (; *s; s++, pad--)
            cons_putc((uint8_t)*s);
        if (f.attr & PFIL_ADIR) { /* <DIR> */
            cons_putc('<');
            cons_puts("DIR>");
            pad -= 5;
        }
        pad += 6; /* 18 columns a name */
        do cons_space();
        while (--pad > 0);
        if (cons.col >= 62)
            cons_nl();
    } while (!pfil_next(&f));
    cons_fresh();
    uint32_t free = 0;
    pfil_free(0, &free);
    cons_int((int32_t)free);
    cons_puts(" Bytes free");
    cons_nl();
}

void file_novar(void)
{
    exec_pushi(0);
    exec_pushi(0);
}

/* The bytes of the variable at addr of kind: its place, *n its length; a Desc's chars. */
static Addr var_bytes(Addr addr, uint32_t kind, uint32_t *n)
{
    *n = va_n(kind);
    if (va_kind(kind) != VA_DESC)
        return addr;
    Desc *d = (Desc *)at(addr);
    *n = d->len;
    return d->ptr;
}

/* GET's string of the Desc at desc: its length of bytes (255 at most) read, its new value. */
static void get_string(Chan *c, Addr desc)
{
    uint32_t n = ((Desc *)at(desc))->len;
    if (n > 255)
        n = 255;
    if (chnl_read(c, cons.sbuf, n) < n)
        c->eof = true;
    strg_set(strg_buftmp((uint16_t)n), desc);
}

/* PUT: the variable into the record at its start; a variable-length string its length first.
   Error: 59 Bad record length, longer than the record. */
static void to_record(const Chan *c, Addr addr, uint32_t kind)
{
    uint32_t n;
    Addr src = var_bytes(addr, kind, &n);
    Addr rec = record(c);
    if (va_kind(kind) == VA_DESC && !va_n(kind)) {
        if ((uint16_t)(n + 2) > c->reclen)
            chnl_error(59);
        wr16(rec, (uint16_t)n);
        rec += 2;
    } else {
        if (va_kind(kind) == VA_I2)
            n = 2;
        if ((uint16_t)n > c->reclen)
            chnl_error(59);
    }
    memmove(at(rec), at(src), n);
}

/* GET: the record into the variable. */
static void from_record(const Chan *c, Addr addr, uint32_t kind)
{
    Addr rec = record(c);
    uint32_t n = va_n(kind);
    switch (va_kind(kind)) {
    case VA_I2:
        wr32(addr, (uint32_t)(int32_t)(int16_t)rd16(rec));
        return;
    case VA_DESC:
        if (!n) { /* a length, then the chars: a new value */
            n = rd16(rec);
            rec += 2;
        }
        if (n > 255)
            n = 255;
        memcpy(cons.sbuf, at(rec), n);
        strg_set(strg_buftmp((uint16_t)n), addr);
        return;
    default:
        memmove(at(addr), at(rec), n);
    }
}

/* BINARY: the variable's bytes to or from the file; an INT as 2 bytes. */
static void binary(Chan *c, Addr addr, uint32_t kind, bool put)
{
    static uint8_t i2[2];
    uint32_t n;
    Addr p = var_bytes(addr, kind, &n);
    if (va_kind(kind) == VA_I2) {
        if (put) {
            chnl_write(c, at(p), 2);
            return;
        }
        if (chnl_read(c, i2, 2) < 2) /* GET an INT: 2 bytes, widened */
            c->eof = true;
        wr32(p, (uint32_t)(int32_t)(int16_t)(i2[0] | i2[1] << 8));
        return;
    }
    if (put)
        chnl_write(c, at(p), n);
    else if (chnl_read(c, at(p), n) < n)
        c->eof = true;
}

/* GET, PUT #n [, p] [, v] ( n p addr kind -- ). RANDOM: record p (0 the next) through the
   record; BINARY: v's bytes at byte p, a string read its own length of them. */
static void get_put(bool put)
{
    uint32_t kind = exec.sp[0].u;
    Addr addr = exec.sp[1].a;
    uint32_t p = exec.sp[2].u;
    Chan *c = chnl_find(exec.sp[3].i);
    if (!(c->mode & (FM_RND | FM_BIN)))
        chnl_error(54);
    uint32_t off;
    if (!p && !(c->mode & FM_RND)) {
        off = chnl_tell(c);
    } else {
        if (!p) /* the next: RANDOM's, a whole record on */
            p = (chnl_tell(c) + c->reclen - 1) / c->reclen + 1;
        c->last = p;
        off = offset_of(c, p);
    }
    chnl_seek(c, off);
    if (!(c->mode & FM_RND)) {
        if (!put && va_kind(kind) == VA_DESC)
            get_string(c, addr);
        else
            binary(c, addr, kind, put);
    } else if (put) {
        if (kind)
            to_record(c, addr, kind);
        chnl_write(c, at(record(c)), c->reclen);
    } else {
        uint32_t got = chnl_read(c, at(record(c)), c->reclen);
        if (got < c->reclen) { /* past the end: the rest 0, EOF */
            c->eof = true;
            memset(at(record(c) + got), 0, c->reclen - got);
        }
        if (kind)
            from_record(c, addr, kind);
    }
    exec.sp += 4;
}

void file_get(void) { get_put(false); }
void file_put(void) { get_put(true); }

void file_field(void)
{
    field_chan = chnl_find(exec_popi());
    field_chan->field = 0;
}

void file_fielditem(void)
{
    uint32_t kind = exec.sp++->u;
    if (va_kind(kind) != VA_DESC)
        repl_error(E_TM);
    Addr desc = exec_popa();
    uint32_t w = exec.sp++->u;
    Chan *c = field_chan;
    if ((uint16_t)(c->field + w) > c->reclen)
        chnl_error(50); /* FIELD overflow */
    Desc *d = (Desc *)at(desc);
    d->ptr = record(c) + c->field;
    d->len = (uint16_t)w;
    c->field += (uint16_t)w;
}

/* n chars at src into len at dst, left or right, spaces around them. */
static void justify(uint8_t *dst, uint32_t len, const uint8_t *src, uint32_t n, bool right)
{
    if (n > len)
        n = len;
    uint32_t spaces = len - n;
    if (right) {
        memset(dst, ' ', spaces);
        dst += spaces;
        spaces = 0;
    }
    memmove(dst, src, n);
    memset(dst + n, ' ', spaces);
}

/* LSET, RSET v = s ( addr kind s -- ): a FIELD's string changed where it is; another string
   gets a new value; a record's STRING * n its bytes. */
static void set(bool right)
{
    Desc s = strg_get(exec.sp[0].a);
    uint32_t kind = exec.sp[1].u;
    Addr addr = exec.sp[2].a;
    const uint8_t *src = at(s.ptr);
    if (va_kind(kind) != VA_DESC) {
        justify(at(addr), va_n(kind), src, s.len, right);
    } else {
        Desc *d = (Desc *)at(addr);
        if (d->ptr >= file.recs && d->ptr < file.recs + NCH * RECMAX) {
            justify(at(d->ptr), d->len, src, s.len, right);
        } else { /* built in sbuf, then stored */
            uint16_t len = d->len;
            justify(cons.sbuf, len, src, s.len, right);
            strg_set(strg_buftmp(len), addr);
        }
    }
    exec.sp += 3;
}

void file_lset(void) { set(false); }
void file_rset(void) { set(true); }

void file_bsave(void)
{
    const char *name = bas_name(exec.sp[2].a);
    Addr from = exec.sp[1].a;
    uint16_t n = (uint16_t)exec.sp[0].u;
    PfilHandle h;
    dos(pfil_create(name, &h));
    uint8_t hdr[7] = {0xFD}; /* FDh, the address's words swapped, n */
    uint32_t seg = from << 16 | from >> 16;
    memcpy(hdr + 1, &seg, 4);
    memcpy(hdr + 5, &n, 2);
    uint32_t put;
    pfil_write(h, hdr, 7, &put);
    pfil_write(h, at(from), n, &put);
    pfil_close(h);
    exec.sp += 3;
}

void file_bload(void)
{
    uint32_t flag = exec.sp++->u;
    Addr to = flag ? exec_popa() : 0;
    bool given = (uint8_t)flag; /* an address given */
    const char *name = bas_name(exec_popa());
    PfilHandle h;
    dos(pfil_open(name, PFIL_READ, &h));
    uint8_t hdr[7] = {0};
    uint32_t got;
    pfil_read(h, hdr, 7, &got);
    if (hdr[0] != 0xFD) {
        pfil_close(h);
        chnl_error(54);
    }
    if (!given) {
        uint32_t seg;
        memcpy(&seg, hdr + 1, 4);
        to = seg << 16 | seg >> 16;
    }
    uint16_t n;
    memcpy(&n, hdr + 5, 2);
    pfil_read(h, at(to), n, &got);
    pfil_close(h);
}

void file_lock(void) { chnl_find(exec_popi()); }
void file_drop(void) { exec.sp++; }
void file_noadv(void) { chnl_error(73); }
