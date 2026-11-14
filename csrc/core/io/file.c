/* QBasic's file statements and functions (io/fio.asm). */
#include "io/file.h"
#include "session.h"
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



enum { STRING_IO_MAX = 255 }; /* a string GET reads, at most */

void file_init(Session *const S) { S->file.recs = allo_carve(S, NCH * RECMAX); }

/* An INT kept in 2 bytes, widened. */
static uint32_t widen16(const uint16_t v) { return (uint32_t)(int32_t)(int16_t)v; }

/* String value s as a 0-ended name. Error: Bad file name, empty. */
static const char *name_of(Session *const S, const Addr s)
{
    const Desc d = strg_get(S, s);
    if (!d.len)
        repl_qberr(S, QB_BAD_FILE_NAME);
    return (const char *)strg_near(S, d);
}

/* name_of, .BAS added when its last part has no '.'. */
static const char *bas_name(Session *const S, const Addr s)
{
    const char *const n = name_of(S, s);
    const size_t len = strlen(n);
    memcpy(S->file.bname, n, len + 1);
    bool dot = false;
    for (const char *p = S->file.bname; *p; p++) {
        if (*p == '.')
            dot = true;
        if (*p == '\\' || *p == ':')
            dot = false;
    }
    if (!dot)
        memcpy(S->file.bname + len, ".BAS", 5);
    return S->file.bname;
}

/* c's record buffer. */
static Addr record(Session *const S, const Chan *const c) { return S->file.recs + (uint32_t)(c - S->chnl.ch) * RECMAX; }

void file_opendef(Session *const S)
{
    S->file.mode = FM_RND;
    S->file.reclen = RECLEN_DEFAULT;
}
void file_mode_in(Session *const S) { S->file.mode = FM_IN; }
void file_mode_out(Session *const S) { S->file.mode = FM_OUT; }
void file_mode_app(Session *const S) { S->file.mode = FM_APP; }
void file_mode_rnd(Session *const S) { S->file.mode = FM_RND; }
void file_mode_bin(Session *const S) { S->file.mode = FM_BIN; }

void file_reclen(Session *const S)
{
    const uint32_t n = (uint32_t)exec_popi(&S->vm);
    if (n - 1 > RECMAX - 1)
        repl_qberr(S, QB_BAD_RECORD_LENGTH);
    S->file.reclen = n;
}

void file_open(Session *const S)
{
    const int32_t num = exec_popi(&S->vm);
    const char *const name = name_of(S, exec_popa(&S->vm));
    chnl_open(S, num, name, S->file.mode, S->file.reclen);
}

/* OPEN's mode string's first letter as FM_*; 0 none. */
static uint8_t mode_of(const uint8_t letter)
{
    static const struct {
        char letter;
        uint8_t mode;
    } modes[] = {{'I', FM_IN}, {'O', FM_OUT}, {'A', FM_APP}, {'R', FM_RND}, {'B', FM_BIN}};
    for (size_t i = 0; i < countof(modes); i++)
        if (upcase(letter) == modes[i].letter)
            return modes[i].mode;
    return 0;
}

void file_open2(Session *const S)
{
    const char *const name = name_of(S, exec_popa(&S->vm));
    const int32_t num = exec_popi(&S->vm);
    const Desc m = strg_get(S, exec_popa(&S->vm));
    const uint8_t mode = m.len ? mode_of(rd8(m.ptr)) : 0;
    if (!mode)
        repl_qberr(S, QB_BAD_FILE_MODE);
    S->file.mode = mode;
    chnl_open(S, num, name, S->file.mode, S->file.reclen);
}

void file_close(Session *const S)
{
    Chan *const c = chnl_look(S, exec_popi(&S->vm));
    if (c)
        chnl_close(S, c);
}

void file_closeall(Session *const S) { chnl_closeall(S); }
void file_out(Session *const S) { chnl_out(S, chnl_find(S, exec_popi(&S->vm))); }
void file_in(Session *const S) { chnl_in(S, chnl_find(S, exec_popi(&S->vm))); }
void file_console(Session *const S) { chnl_clear(S); }

void file_lineinput(Session *const S)
{
    const uint16_t len = (uint16_t)S->cons.inlen;
    const Addr chars = strg_alloc(S, len);
    exec_pusha(&S->vm, strg_tods(S, chars, addr_of(S->cons.inbuf), len));
}

/* Output to sbuf, from its start. */
static void capture(Session *const S)
{
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
}

/* sbuf's text, its spaces left out, to the output. */
static void write_out(Session *const S)
{
    S->cons.capture = 0;
    for (uint32_t i = 0; i < S->cons.sblen; i++)
        if (S->cons.sbuf[i] != ' ')
            cons_putc(S, S->cons.sbuf[i]);
}

void file_write_int(Session *const S)
{
    const int32_t v = exec_popi(&S->vm);
    capture(S);
    cons_int(S, v);
    write_out(S);
}

void file_write_flt(Session *const S)
{
    const Sngl v = exec_pop(&S->vm).u;
    capture(S);
    sngl_print(S, v);
    write_out(S);
}

void file_write_dbl(Session *const S)
{
    uint8_t text[DUBL_TXT];
    const uint32_t n = dubl_fmt(exec_popd(&S->vm), text);
    capture(S);
    for (uint32_t i = 0; i < n; i++)
        cons_putc(S, text[i]);
    write_out(S);
}

void file_write_str(Session *const S)
{
    const Desc d = strg_get(S, exec_popa(&S->vm));
    cons_putc(S, '"');
    strg_print(S, d);
    cons_putc(S, '"');
}

void file_write_comma(Session *const S) { cons_putc(S, ','); }

void file_eof(Session *const S)
{
    Chan *const c = chnl_find(S, exec_popi(&S->vm));
    int32_t r;
    if (c->mode & (FM_RND | FM_BIN))
        r = c->eof ? -1 : 0;
    else if (c->mode & FM_IN)
        r = chnl_peek(S, c) < 0 ? -1 : 0;
    else
        r = -1; /* written: at its end */
    exec_pushi(&S->vm, r);
}

void file_lof(Session *const S) { exec_pushi(&S->vm, (int32_t)chnl_lof(S, chnl_find(S, exec_popi(&S->vm)))); }

void file_loc(Session *const S)
{
    const Chan *const c = chnl_find(S, exec_popi(&S->vm));
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t = c->last;
    else if (!(c->mode & FM_BIN)) /* a text file's: in 128-byte records */
        t = (t + RECLEN_DEFAULT - 1) / RECLEN_DEFAULT;
    exec_pushi(&S->vm, (int32_t)t);
}

void file_seekpos(Session *const S)
{
    const Chan *const c = chnl_find(S, exec_popi(&S->vm));
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t /= c->reclen;
    exec_pushi(&S->vm, (int32_t)(t + 1));
}

/* Record or byte p of c, from 1, as an offset. Error: Bad record number, under 1. */
static uint32_t offset_of(Session *const S, const Chan *const c, const uint32_t p)
{
    if ((int32_t)(p - 1) < 0)
        repl_qberr(S, QB_BAD_RECORD_NUMBER);
    return c->mode & FM_RND ? (p - 1) * c->reclen : p - 1;
}

void file_seek(Session *const S)
{
    Chan *const c = chnl_find(S, S->vm.sp[1].i);
    const uint32_t p = S->vm.sp[0].u;
    S->vm.sp += 2;
    chnl_seek(S, c, offset_of(S, c, p));
    c->eof = false;
}

void file_freefile(Session *const S)
{
    int32_t n = 1;
    while (n <= FILENO_MAX && chnl_look(S, n))
        n++;
    exec_pushi(&S->vm, n);
}

void file_input_str(Session *const S)
{
    Chan *c = nullptr; /* nullptr: the keyboard */
    if (exec_popi(&S->vm) != 1)
        c = chnl_find(S, exec_popi(&S->vm));
    const uint32_t n = (uint32_t)exec_popi(&S->vm);
    if (n - 1 > INMAX - 1)
        repl_error(S, E_IQ);
    const Addr chars = strg_alloc(S, (uint16_t)n);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b;
        if (c) {
            const int v = chnl_peek(S, c);
            if (v < 0)
                repl_qberr(S, QB_PAST_END);
            c->next++;
            b = (uint8_t)v;
        } else {
            cons_flush(S);
            b = (uint8_t)pkey_wait();
        }
        wr8(chars + i, b);
    }
    exec_pusha(&S->vm, strg_new(S, chars, (uint16_t)n));
}

/* FILEATTR's mode numbers: INPUT OUTPUT RANDOM APPEND BINARY. */
static int32_t attr_mode(const uint8_t mode)
{
    static const struct {
        uint8_t mode;
        int32_t attr;
    } attrs[] = {{FM_IN, 1}, {FM_OUT, 2}, {FM_RND, 4}, {FM_APP, 8}};
    for (size_t i = 0; i < countof(attrs); i++)
        if (mode & attrs[i].mode)
            return attrs[i].attr;
    return 32;
}

void file_attr(Session *const S)
{
    enum { ATTR_MODE = 1, ATTR_HANDLE = 2 };
    const int32_t a = exec_popi(&S->vm);
    const Chan *const c = chnl_find(S, exec_popi(&S->vm));
    if (a == ATTR_HANDLE)
        exec_pushi(&S->vm, c->handle);
    else if (a == ATTR_MODE)
        exec_pushi(&S->vm, attr_mode(c->mode));
    else
        repl_error(S, E_IQ);
}

static void dos(Session *const S, const uint16_t err)
{
    if (err)
        chnl_doserr(S, err);
}

void file_kill(Session *const S) { dos(S, pfil_delete(name_of(S, exec_popa(&S->vm)))); }
void file_mkdir(Session *const S) { dos(S, pfil_mkdir(name_of(S, exec_popa(&S->vm)))); }
void file_rmdir(Session *const S) { dos(S, pfil_rmdir(name_of(S, exec_popa(&S->vm)))); }
void file_chdir(Session *const S) { dos(S, pfil_chdir(name_of(S, exec_popa(&S->vm)))); }

void file_rename(Session *const S)
{
    const char *const to = name_of(S, exec_popa(&S->vm));
    memcpy(S->file.name2, to, strlen(to) + 1);
    const uint16_t err = pfil_rename(name_of(S, exec_popa(&S->vm)), S->file.name2);
    if (err == PFIL_DENIED)
        repl_qberr(S, QB_FILE_EXISTS);
    dos(S, err);
}

/* FILES' entry: NAME.EXT as NAME    .EXT, <DIR> after a directory's, in ENTRY_WIDTH columns. */
static void files_entry(Session *const S, const PfilFind *const f)
{
    enum { NAME_WIDTH = 8, ENTRY_WIDTH = 18, DIR_MARK = 5 };
    const char *s = f->name;
    uint32_t n = 0;
    for (; s[n] && s[n] != '.'; n++)
        cons_putc(S, (uint8_t)s[n]);
    for (uint32_t i = n; i < NAME_WIDTH; i++)
        cons_space(S);
    s += n + (s[n] != 0);
    cons_putc(S, '.');
    cons_puts(S, s);
    uint32_t used = NAME_WIDTH + 1 + (uint32_t)strlen(s);
    if (f->attr & PFIL_ADIR) {
        cons_puts(S, "<DIR>");
        used += DIR_MARK;
    }
    for (uint32_t i = used; i < ENTRY_WIDTH - 1; i++)
        cons_space(S);
    cons_space(S); /* at least one */
}

void file_files(Session *const S)
{
    enum { LAST_COL = 62 }; /* past it: the next line */
    static char pattern[264];
    memcpy(pattern, "*.*", 4);
    if (exec_popi(&S->vm)) {
        const char *n = name_of(S, exec_popa(&S->vm));
        memcpy(pattern, n, strlen(n) + 1);
    }
    cons_putc(S, '\\'); /* the current directory */
    if (pfil_curdir(0, S->file.name2, sizeof S->file.name2))
        S->file.name2[0] = 0;
    cons_puts(S, S->file.name2);
    cons_nl(S);
    PfilFind f;
    if (pfil_first(pattern, PFIL_ADIR, &f))
        repl_qberr(S, QB_FILE_NOT_FOUND);
    do {
        files_entry(S, &f);
        if (S->cons.col >= LAST_COL)
            cons_nl(S);
    } while (!pfil_next(&f));
    cons_fresh(S);
    uint32_t free = 0;
    pfil_free(0, &free);
    cons_int(S, (int32_t)free);
    cons_puts(S, " Bytes free");
    cons_nl(S);
}

void file_novar(Session *const S)
{
    exec_pushi(&S->vm, 0);
    exec_pushi(&S->vm, 0);
}

/* The bytes of the variable at addr of kind: its place, *n its length; a Desc's chars. */
static Addr var_bytes(const Addr addr, const uint32_t kind, uint32_t *const n)
{
    *n = va_n(kind);
    if (va_kind(kind) != VA_DESC)
        return addr;
    const Desc *const d = (const Desc *)at(addr);
    *n = d->len;
    return d->ptr;
}

/* GET's string of the Desc at desc: its length of bytes (STRING_IO_MAX at most) read, its new
   value. */
static void get_string(Session *const S, Chan *const c, const Addr desc)
{
    uint32_t n = ((const Desc *)at(desc))->len;
    if (n > STRING_IO_MAX)
        n = STRING_IO_MAX;
    if (chnl_read(S, c, S->cons.sbuf, n) < n)
        c->eof = true;
    strg_set(S, strg_buftmp(S, (uint16_t)n), desc);
}

/* PUT: the variable into the record at its start; a variable-length string its length first.
   Error: Bad record length, longer than the record. */
static void to_record(Session *const S, const Chan *const c, const Addr addr, const uint32_t kind)
{
    uint32_t n;
    const Addr src = var_bytes(addr, kind, &n);
    Addr rec = record(S, c);
    if (va_kind(kind) == VA_DESC && !va_n(kind)) {
        if ((uint16_t)(n + 2) > c->reclen)
            repl_qberr(S, QB_BAD_RECORD_LENGTH);
        wr16(rec, (uint16_t)n);
        rec += 2;
    } else {
        if (va_kind(kind) == VA_I2)
            n = 2;
        if ((uint16_t)n > c->reclen)
            repl_qberr(S, QB_BAD_RECORD_LENGTH);
    }
    memmove(at(rec), at(src), n);
}

/* GET: the record into the variable. */
static void from_record(Session *const S, const Chan *const c, const Addr addr, const uint32_t kind)
{
    Addr rec = record(S, c);
    uint32_t n = va_n(kind);
    switch (va_kind(kind)) {
    case VA_I2:
        wr32(addr, widen16(rd16(rec)));
        return;
    case VA_DESC:
        if (!n) { /* a length, then the chars: a new value */
            n = rd16(rec);
            rec += 2;
        }
        if (n > STRING_IO_MAX)
            n = STRING_IO_MAX;
        memcpy(S->cons.sbuf, at(rec), n);
        strg_set(S, strg_buftmp(S, (uint16_t)n), addr);
        return;
    default:
        memmove(at(addr), at(rec), n);
    }
}

/* BINARY: the variable's bytes to or from the file; an INT as 2 bytes. */
static void binary(Session *const S, Chan *const c, const Addr addr, const uint32_t kind, const bool put)
{
    static uint8_t i2[2];
    uint32_t n;
    const Addr p = var_bytes(addr, kind, &n);
    if (va_kind(kind) == VA_I2) {
        if (put) {
            chnl_write(S, c, at(p), 2);
            return;
        }
        if (chnl_read(S, c, i2, 2) < 2) /* GET an INT: 2 bytes, widened */
            c->eof = true;
        wr32(p, widen16(load16(i2)));
        return;
    }
    if (put)
        chnl_write(S, c, at(p), n);
    else if (chnl_read(S, c, at(p), n) < n)
        c->eof = true;
}

/* GET, PUT #n [, p] [, v] ( n p addr kind -- ). RANDOM: record p (0 the next) through the
   record; BINARY: v's bytes at byte p, a string read its own length of them. */
static void get_put(Session *const S, const bool put)
{
    const uint32_t kind = S->vm.sp[0].u;
    const Addr addr = S->vm.sp[1].a;
    uint32_t p = S->vm.sp[2].u;
    Chan *const c = chnl_find(S, S->vm.sp[3].i);
    if (!(c->mode & (FM_RND | FM_BIN)))
        repl_qberr(S, QB_BAD_FILE_MODE);
    uint32_t off;
    if (!p && !(c->mode & FM_RND)) {
        off = chnl_tell(c);
    } else {
        if (!p) /* the next: RANDOM's, a whole record on */
            p = (chnl_tell(c) + c->reclen - 1) / c->reclen + 1;
        c->last = p;
        off = offset_of(S, c, p);
    }
    chnl_seek(S, c, off);
    if (!(c->mode & FM_RND)) {
        if (!put && va_kind(kind) == VA_DESC)
            get_string(S, c, addr);
        else
            binary(S, c, addr, kind, put);
    } else if (put) {
        if (kind)
            to_record(S, c, addr, kind);
        chnl_write(S, c, at(record(S, c)), c->reclen);
    } else {
        const uint32_t got = chnl_read(S, c, at(record(S, c)), c->reclen);
        if (got < c->reclen) { /* past the end: the rest 0, EOF */
            c->eof = true;
            memset(at(record(S, c) + got), 0, c->reclen - got);
        }
        if (kind)
            from_record(S, c, addr, kind);
    }
    S->vm.sp += 4;
}

void file_get(Session *const S) { get_put(S, false); }
void file_put(Session *const S) { get_put(S, true); }

void file_field(Session *const S)
{
    S->file.field_chan = chnl_find(S, exec_popi(&S->vm));
    S->file.field_chan->field = 0;
}

void file_fielditem(Session *const S)
{
    const uint32_t kind = S->vm.sp++->u;
    if (va_kind(kind) != VA_DESC)
        repl_error(S, E_TM);
    const Addr desc = exec_popa(&S->vm);
    const uint32_t w = S->vm.sp++->u;
    Chan *const c = S->file.field_chan;
    if ((uint16_t)(c->field + w) > c->reclen)
        repl_qberr(S, QB_FIELD_OVERFLOW);
    Desc *const d = (Desc *)at(desc);
    d->ptr = record(S, c) + c->field;
    d->len = (uint16_t)w;
    c->field += (uint16_t)w;
}

/* n chars at src into len at dst, left or right, spaces around them. */
static void justify(uint8_t *const dst, const uint32_t len, const uint8_t *const src,
                    const uint32_t count, const bool right)
{
    const uint32_t n = count < len ? count : len;
    const uint32_t before = right ? len - n : 0;
    memset(dst, ' ', before);
    memmove(dst + before, src, n);
    memset(dst + before + n, ' ', len - n - before);
}

/* LSET, RSET v = s ( addr kind s -- ): a FIELD's string changed where it is; another string
   gets a new value; a record's STRING * n its bytes. */
static void set(Session *const S, const bool right)
{
    const Desc s = strg_get(S, S->vm.sp[0].a);
    const uint32_t kind = S->vm.sp[1].u;
    const Addr addr = S->vm.sp[2].a;
    const uint8_t *const src = at(s.ptr);
    if (va_kind(kind) != VA_DESC) {
        justify(at(addr), va_n(kind), src, s.len, right);
    } else {
        const Desc *const d = (const Desc *)at(addr);
        if (d->ptr >= S->file.recs && d->ptr < S->file.recs + NCH * RECMAX) {
            justify(at(d->ptr), d->len, src, s.len, right);
        } else { /* built in sbuf, then stored */
            const uint16_t len = d->len;
            justify(S->cons.sbuf, len, src, s.len, right);
            strg_set(S, strg_buftmp(S, len), addr);
        }
    }
    S->vm.sp += 3;
}

void file_lset(Session *const S) { set(S, false); }
void file_rset(Session *const S) { set(S, true); }

/* BSAVE's header: FDh, the address as segment:offset (its words swapped), the length. */
enum { BSAVE_MAGIC = 0xFD, BSAVE_ADDR = 1, BSAVE_LEN = 5, BSAVE_HDR = 7 };
static uint32_t swap_words(const uint32_t v) { return v << 16 | v >> 16; }

void file_bsave(Session *const S)
{
    const char *const name = bas_name(S, S->vm.sp[2].a);
    const Addr from = S->vm.sp[1].a;
    const uint16_t n = (uint16_t)S->vm.sp[0].u;
    PfilHandle h;
    dos(S, pfil_create(name, &h));
    uint8_t hdr[BSAVE_HDR] = {BSAVE_MAGIC};
    store32(hdr + BSAVE_ADDR, swap_words(from));
    store16(hdr + BSAVE_LEN, n);
    uint32_t put;
    pfil_write(h, hdr, BSAVE_HDR, &put);
    pfil_write(h, at(from), n, &put);
    pfil_close(h);
    S->vm.sp += 3;
}

void file_bload(Session *const S)
{
    const uint32_t flag = S->vm.sp++->u;
    const Addr given = flag ? exec_popa(&S->vm) : 0;
    const char *const name = bas_name(S, exec_popa(&S->vm));
    PfilHandle h;
    dos(S, pfil_open(name, PFIL_READ, &h));
    uint8_t hdr[BSAVE_HDR] = {0};
    uint32_t got;
    pfil_read(h, hdr, BSAVE_HDR, &got);
    if (hdr[0] != BSAVE_MAGIC) {
        pfil_close(h);
        repl_qberr(S, QB_BAD_FILE_MODE);
    }
    const Addr to = (uint8_t)flag ? given : swap_words(load32(hdr + BSAVE_ADDR)); /* an address given */
    const uint16_t n = load16(hdr + BSAVE_LEN);
    pfil_read(h, at(to), n, &got);
    pfil_close(h);
}

void file_lock(Session *const S) { chnl_find(S, exec_popi(&S->vm)); }
void file_drop(Session *const S) { S->vm.sp++; }
void file_noadv(Session *const S) { repl_qberr(S, QB_NO_ADVANCED); }
