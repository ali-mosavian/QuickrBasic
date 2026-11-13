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

enum { STRING_IO_MAX = 255 }; /* a string GET reads, at most */

void file_init(void) { file.recs = allo_carve(NCH * RECMAX); }

/* An INT kept in 2 bytes, widened. */
static uint32_t widen16(const uint16_t v) { return (uint32_t)(int32_t)(int16_t)v; }

/* String value s as a 0-ended name. Error: Bad file name, empty. */
static const char *name_of(const Addr s)
{
    const Desc d = strg_get(s);
    if (!d.len)
        repl_qberr(QB_BAD_FILE_NAME);
    return (const char *)strg_near(d);
}

/* name_of, .BAS added when its last part has no '.'. */
static const char *bas_name(const Addr s)
{
    const char *const n = name_of(s);
    const size_t len = strlen(n);
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
static Addr record(const Chan *const c) { return file.recs + (uint32_t)(c - chnl.ch) * RECMAX; }

void file_opendef(Vm *vm)
{
    file.mode = FM_RND;
    file.reclen = RECLEN_DEFAULT;
}
void file_mode_in(Vm *vm) { file.mode = FM_IN; }
void file_mode_out(Vm *vm) { file.mode = FM_OUT; }
void file_mode_app(Vm *vm) { file.mode = FM_APP; }
void file_mode_rnd(Vm *vm) { file.mode = FM_RND; }
void file_mode_bin(Vm *vm) { file.mode = FM_BIN; }

void file_reclen(Vm *vm)
{
    const uint32_t n = (uint32_t)exec_popi(vm);
    if (n - 1 > RECMAX - 1)
        repl_qberr(QB_BAD_RECORD_LENGTH);
    file.reclen = n;
}

void file_open(Vm *vm)
{
    const int32_t num = exec_popi(vm);
    const char *const name = name_of(exec_popa(vm));
    chnl_open(num, name, file.mode, file.reclen);
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

void file_open2(Vm *vm)
{
    const char *const name = name_of(exec_popa(vm));
    const int32_t num = exec_popi(vm);
    const Desc m = strg_get(exec_popa(vm));
    const uint8_t mode = m.len ? mode_of(rd8(m.ptr)) : 0;
    if (!mode)
        repl_qberr(QB_BAD_FILE_MODE);
    file.mode = mode;
    chnl_open(num, name, file.mode, file.reclen);
}

void file_close(Vm *vm)
{
    Chan *const c = chnl_look(exec_popi(vm));
    if (c)
        chnl_close(c);
}

void file_closeall(Vm *vm) { chnl_closeall(); }
void file_out(Vm *vm) { chnl_out(chnl_find(exec_popi(vm))); }
void file_in(Vm *vm) { chnl_in(chnl_find(exec_popi(vm))); }
void file_console(Vm *vm) { chnl_clear(); }

void file_lineinput(Vm *vm)
{
    const uint16_t len = (uint16_t)cons.inlen;
    const Addr chars = strg_alloc(len);
    exec_pusha(vm, strg_tods(chars, addr_of(cons.inbuf), len));
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

void file_write_int(Vm *vm)
{
    const int32_t v = exec_popi(vm);
    capture();
    cons_int(v);
    write_out();
}

void file_write_flt(Vm *vm)
{
    const Sngl v = exec_pop(vm).u;
    capture();
    sngl_print(v);
    write_out();
}

void file_write_dbl(Vm *vm)
{
    uint8_t text[DUBL_TXT];
    const uint32_t n = dubl_fmt(exec_popd(vm), text);
    capture();
    for (uint32_t i = 0; i < n; i++)
        cons_putc(text[i]);
    write_out();
}

void file_write_str(Vm *vm)
{
    const Desc d = strg_get(exec_popa(vm));
    cons_putc('"');
    strg_print(d);
    cons_putc('"');
}

void file_write_comma(Vm *vm) { cons_putc(','); }

void file_eof(Vm *vm)
{
    Chan *const c = chnl_find(exec_popi(vm));
    int32_t r;
    if (c->mode & (FM_RND | FM_BIN))
        r = c->eof ? -1 : 0;
    else if (c->mode & FM_IN)
        r = chnl_peek(c) < 0 ? -1 : 0;
    else
        r = -1; /* written: at its end */
    exec_pushi(vm, r);
}

void file_lof(Vm *vm) { exec_pushi(vm, (int32_t)chnl_lof(chnl_find(exec_popi(vm)))); }

void file_loc(Vm *vm)
{
    const Chan *const c = chnl_find(exec_popi(vm));
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t = c->last;
    else if (!(c->mode & FM_BIN)) /* a text file's: in 128-byte records */
        t = (t + RECLEN_DEFAULT - 1) / RECLEN_DEFAULT;
    exec_pushi(vm, (int32_t)t);
}

void file_seekpos(Vm *vm)
{
    const Chan *const c = chnl_find(exec_popi(vm));
    uint32_t t = chnl_tell(c);
    if (c->mode & FM_RND)
        t /= c->reclen;
    exec_pushi(vm, (int32_t)(t + 1));
}

/* Record or byte p of c, from 1, as an offset. Error: Bad record number, under 1. */
static uint32_t offset_of(const Chan *const c, const uint32_t p)
{
    if ((int32_t)(p - 1) < 0)
        repl_qberr(QB_BAD_RECORD_NUMBER);
    return c->mode & FM_RND ? (p - 1) * c->reclen : p - 1;
}

void file_seek(Vm *vm)
{
    Chan *const c = chnl_find(vm->sp[1].i);
    const uint32_t p = vm->sp[0].u;
    vm->sp += 2;
    chnl_seek(c, offset_of(c, p));
    c->eof = false;
}

void file_freefile(Vm *vm)
{
    int32_t n = 1;
    while (n <= FILENO_MAX && chnl_look(n))
        n++;
    exec_pushi(vm, n);
}

void file_input_str(Vm *vm)
{
    Chan *c = nullptr; /* nullptr: the keyboard */
    if (exec_popi(vm) != 1)
        c = chnl_find(exec_popi(vm));
    const uint32_t n = (uint32_t)exec_popi(vm);
    if (n - 1 > INMAX - 1)
        repl_error(E_IQ);
    const Addr chars = strg_alloc((uint16_t)n);
    for (uint32_t i = 0; i < n; i++) {
        uint8_t b;
        if (c) {
            const int v = chnl_peek(c);
            if (v < 0)
                repl_qberr(QB_PAST_END);
            c->next++;
            b = (uint8_t)v;
        } else {
            cons_flush();
            b = (uint8_t)pkey_wait();
        }
        wr8(chars + i, b);
    }
    exec_pusha(vm, strg_new(chars, (uint16_t)n));
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

void file_attr(Vm *vm)
{
    enum { ATTR_MODE = 1, ATTR_HANDLE = 2 };
    const int32_t a = exec_popi(vm);
    const Chan *const c = chnl_find(exec_popi(vm));
    if (a == ATTR_HANDLE)
        exec_pushi(vm, c->handle);
    else if (a == ATTR_MODE)
        exec_pushi(vm, attr_mode(c->mode));
    else
        repl_error(E_IQ);
}

static void dos(const uint16_t err)
{
    if (err)
        chnl_doserr(err);
}

void file_kill(Vm *vm) { dos(pfil_delete(name_of(exec_popa(vm)))); }
void file_mkdir(Vm *vm) { dos(pfil_mkdir(name_of(exec_popa(vm)))); }
void file_rmdir(Vm *vm) { dos(pfil_rmdir(name_of(exec_popa(vm)))); }
void file_chdir(Vm *vm) { dos(pfil_chdir(name_of(exec_popa(vm)))); }

void file_rename(Vm *vm)
{
    const char *const to = name_of(exec_popa(vm));
    memcpy(name2, to, strlen(to) + 1);
    const uint16_t err = pfil_rename(name_of(exec_popa(vm)), name2);
    if (err == PFIL_DENIED)
        repl_qberr(QB_FILE_EXISTS);
    dos(err);
}

/* FILES' entry: NAME.EXT as NAME    .EXT, <DIR> after a directory's, in ENTRY_WIDTH columns. */
static void files_entry(const PfilFind *const f)
{
    enum { NAME_WIDTH = 8, ENTRY_WIDTH = 18, DIR_MARK = 5 };
    const char *s = f->name;
    uint32_t n = 0;
    for (; s[n] && s[n] != '.'; n++)
        cons_putc((uint8_t)s[n]);
    for (uint32_t i = n; i < NAME_WIDTH; i++)
        cons_space();
    s += n + (s[n] != 0);
    cons_putc('.');
    cons_puts(s);
    uint32_t used = NAME_WIDTH + 1 + (uint32_t)strlen(s);
    if (f->attr & PFIL_ADIR) {
        cons_puts("<DIR>");
        used += DIR_MARK;
    }
    for (uint32_t i = used; i < ENTRY_WIDTH - 1; i++)
        cons_space();
    cons_space(); /* at least one */
}

void file_files(Vm *vm)
{
    enum { LAST_COL = 62 }; /* past it: the next line */
    static char pattern[264];
    memcpy(pattern, "*.*", 4);
    if (exec_popi(vm)) {
        const char *n = name_of(exec_popa(vm));
        memcpy(pattern, n, strlen(n) + 1);
    }
    cons_putc('\\'); /* the current directory */
    if (pfil_curdir(0, name2, sizeof name2))
        name2[0] = 0;
    cons_puts(name2);
    cons_nl();
    PfilFind f;
    if (pfil_first(pattern, PFIL_ADIR, &f))
        repl_qberr(QB_FILE_NOT_FOUND);
    do {
        files_entry(&f);
        if (cons.col >= LAST_COL)
            cons_nl();
    } while (!pfil_next(&f));
    cons_fresh();
    uint32_t free = 0;
    pfil_free(0, &free);
    cons_int((int32_t)free);
    cons_puts(" Bytes free");
    cons_nl();
}

void file_novar(Vm *vm)
{
    exec_pushi(vm, 0);
    exec_pushi(vm, 0);
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
static void get_string(Chan *const c, const Addr desc)
{
    uint32_t n = ((const Desc *)at(desc))->len;
    if (n > STRING_IO_MAX)
        n = STRING_IO_MAX;
    if (chnl_read(c, cons.sbuf, n) < n)
        c->eof = true;
    strg_set(strg_buftmp((uint16_t)n), desc);
}

/* PUT: the variable into the record at its start; a variable-length string its length first.
   Error: Bad record length, longer than the record. */
static void to_record(const Chan *const c, const Addr addr, const uint32_t kind)
{
    uint32_t n;
    const Addr src = var_bytes(addr, kind, &n);
    Addr rec = record(c);
    if (va_kind(kind) == VA_DESC && !va_n(kind)) {
        if ((uint16_t)(n + 2) > c->reclen)
            repl_qberr(QB_BAD_RECORD_LENGTH);
        wr16(rec, (uint16_t)n);
        rec += 2;
    } else {
        if (va_kind(kind) == VA_I2)
            n = 2;
        if ((uint16_t)n > c->reclen)
            repl_qberr(QB_BAD_RECORD_LENGTH);
    }
    memmove(at(rec), at(src), n);
}

/* GET: the record into the variable. */
static void from_record(const Chan *const c, const Addr addr, const uint32_t kind)
{
    Addr rec = record(c);
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
        memcpy(cons.sbuf, at(rec), n);
        strg_set(strg_buftmp((uint16_t)n), addr);
        return;
    default:
        memmove(at(addr), at(rec), n);
    }
}

/* BINARY: the variable's bytes to or from the file; an INT as 2 bytes. */
static void binary(Chan *const c, const Addr addr, const uint32_t kind, const bool put)
{
    static uint8_t i2[2];
    uint32_t n;
    const Addr p = var_bytes(addr, kind, &n);
    if (va_kind(kind) == VA_I2) {
        if (put) {
            chnl_write(c, at(p), 2);
            return;
        }
        if (chnl_read(c, i2, 2) < 2) /* GET an INT: 2 bytes, widened */
            c->eof = true;
        wr32(p, widen16(load16(i2)));
        return;
    }
    if (put)
        chnl_write(c, at(p), n);
    else if (chnl_read(c, at(p), n) < n)
        c->eof = true;
}

/* GET, PUT #n [, p] [, v] ( n p addr kind -- ). RANDOM: record p (0 the next) through the
   record; BINARY: v's bytes at byte p, a string read its own length of them. */
static void get_put(Vm *const vm, const bool put)
{
    const uint32_t kind = vm->sp[0].u;
    const Addr addr = vm->sp[1].a;
    uint32_t p = vm->sp[2].u;
    Chan *const c = chnl_find(vm->sp[3].i);
    if (!(c->mode & (FM_RND | FM_BIN)))
        repl_qberr(QB_BAD_FILE_MODE);
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
        const uint32_t got = chnl_read(c, at(record(c)), c->reclen);
        if (got < c->reclen) { /* past the end: the rest 0, EOF */
            c->eof = true;
            memset(at(record(c) + got), 0, c->reclen - got);
        }
        if (kind)
            from_record(c, addr, kind);
    }
    vm->sp += 4;
}

void file_get(Vm *vm) { get_put(vm, false); }
void file_put(Vm *vm) { get_put(vm, true); }

void file_field(Vm *vm)
{
    field_chan = chnl_find(exec_popi(vm));
    field_chan->field = 0;
}

void file_fielditem(Vm *vm)
{
    const uint32_t kind = vm->sp++->u;
    if (va_kind(kind) != VA_DESC)
        repl_error(E_TM);
    const Addr desc = exec_popa(vm);
    const uint32_t w = vm->sp++->u;
    Chan *const c = field_chan;
    if ((uint16_t)(c->field + w) > c->reclen)
        repl_qberr(QB_FIELD_OVERFLOW);
    Desc *const d = (Desc *)at(desc);
    d->ptr = record(c) + c->field;
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
static void set(Vm *const vm, const bool right)
{
    const Desc s = strg_get(vm->sp[0].a);
    const uint32_t kind = vm->sp[1].u;
    const Addr addr = vm->sp[2].a;
    const uint8_t *const src = at(s.ptr);
    if (va_kind(kind) != VA_DESC) {
        justify(at(addr), va_n(kind), src, s.len, right);
    } else {
        const Desc *const d = (const Desc *)at(addr);
        if (d->ptr >= file.recs && d->ptr < file.recs + NCH * RECMAX) {
            justify(at(d->ptr), d->len, src, s.len, right);
        } else { /* built in sbuf, then stored */
            const uint16_t len = d->len;
            justify(cons.sbuf, len, src, s.len, right);
            strg_set(strg_buftmp(len), addr);
        }
    }
    vm->sp += 3;
}

void file_lset(Vm *vm) { set(vm, false); }
void file_rset(Vm *vm) { set(vm, true); }

/* BSAVE's header: FDh, the address as segment:offset (its words swapped), the length. */
enum { BSAVE_MAGIC = 0xFD, BSAVE_ADDR = 1, BSAVE_LEN = 5, BSAVE_HDR = 7 };
static uint32_t swap_words(const uint32_t v) { return v << 16 | v >> 16; }

void file_bsave(Vm *vm)
{
    const char *const name = bas_name(vm->sp[2].a);
    const Addr from = vm->sp[1].a;
    const uint16_t n = (uint16_t)vm->sp[0].u;
    PfilHandle h;
    dos(pfil_create(name, &h));
    uint8_t hdr[BSAVE_HDR] = {BSAVE_MAGIC};
    store32(hdr + BSAVE_ADDR, swap_words(from));
    store16(hdr + BSAVE_LEN, n);
    uint32_t put;
    pfil_write(h, hdr, BSAVE_HDR, &put);
    pfil_write(h, at(from), n, &put);
    pfil_close(h);
    vm->sp += 3;
}

void file_bload(Vm *vm)
{
    const uint32_t flag = vm->sp++->u;
    const Addr given = flag ? exec_popa(vm) : 0;
    const char *const name = bas_name(exec_popa(vm));
    PfilHandle h;
    dos(pfil_open(name, PFIL_READ, &h));
    uint8_t hdr[BSAVE_HDR] = {0};
    uint32_t got;
    pfil_read(h, hdr, BSAVE_HDR, &got);
    if (hdr[0] != BSAVE_MAGIC) {
        pfil_close(h);
        repl_qberr(QB_BAD_FILE_MODE);
    }
    const Addr to = (uint8_t)flag ? given : swap_words(load32(hdr + BSAVE_ADDR)); /* an address given */
    const uint16_t n = load16(hdr + BSAVE_LEN);
    pfil_read(h, at(to), n, &got);
    pfil_close(h);
}

void file_lock(Vm *vm) { chnl_find(exec_popi(vm)); }
void file_drop(Vm *vm) { vm->sp++; }
void file_noadv(Vm *vm) { repl_qberr(QB_NO_ADVANCED); }
