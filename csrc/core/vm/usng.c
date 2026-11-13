/* PRINT USING (src/vm/using.asm, as llrm's runtime/qb/using.c). A number is digits: its value is
   0.digits * 10^ni. */
#include "vm/usng.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "num/dubl.h"

enum { FMTMAX = 255, NDIG = 80 };

/* A field's kind (us_fld). */
enum : uint8_t { UF_NONE, UF_FIRST, UF_ALL, UF_WIDE, UF_NUM };

/* A numeric field (TUSF). */
typedef struct {
    uint32_t len;   /* its characters in the format */
    uint32_t wid;   /* its columns, the signs not counted */
    uint32_t lp;    /* a leading + */
    uint32_t tr;    /* a trailing + or -, else 0 */
    uint32_t st;    /* ** */
    uint32_t dol;   /* a $ */
    uint32_t com;   /* commas */
    uint32_t bp;    /* places before the point */
    uint32_t dec;   /* after it */
    uint32_t pt;    /* a point */
    uint32_t car;   /* 4 or 5 for an exponent */
} Field;

static struct {
    uint8_t fmt[FMTMAX];
    uint32_t len, at;      /* the format's length, the place */
    Field f;
    uint8_t nt[NDIG + 20]; /* the number: its digits, their count, places before the point */
    int32_t nc, ni;
    bool neg, sci;         /* its sign; PRINT would give it an exponent */
    uint8_t body[FMTMAX + NDIG];
    uint8_t line[FMTMAX + NDIG + 8];
} us;

/* The format's char at d, 0 past it. */
static uint8_t ch(uint32_t d) { return d < us.len ? us.fmt[d] : 0; }

static void out(const uint8_t *p, uint32_t n)
{
    while (n--)
        cons_putc(*p++);
}

/* US.BEGIN ( fmt -- ). Illegal function call: over 255 characters. */
void usng_usb(void)
{
    Desc s = strg_get(exec_popa());
    if (s.len > FMTMAX)
        repl_error(E_IQ);
    us.len = s.len;
    memcpy(us.fmt, at(s.ptr), s.len);
    us.at = 0;
}

/* us_numf: the numeric field at b in us.f; false none. */
static bool numfield(uint32_t b)
{
    Field *f = &us.f;
    *f = (Field){};
    uint32_t d = b;
    bool digits = false;
    if (ch(d) == '+' && d + 1 < us.len) {
        f->lp = 1;
        d++;
    }
    if (d + 1 < us.len) {
        if (us.fmt[d] == '*' && us.fmt[d + 1] == '*') {
            f->st = 1;
            f->wid += 2;
            f->bp += 2;
            d += 2;
            if (ch(d) == '$') {
                f->dol = 1;
                f->wid++;
                d++;
            }
        } else if (us.fmt[d] == '$' && us.fmt[d + 1] == '$') {
            f->dol = 1;
            f->wid += 2;
            f->bp++;
            d += 2;
        }
    }
    for (;; d++) { /* # and , before the point */
        if (ch(d) == '#')
            f->bp++;
        else if (ch(d) == ',')
            f->com = 1;
        else
            break;
        f->wid++;
        digits = true;
    }
    if (ch(d) == '.') {
        if (d + 1 < us.len && us.fmt[d + 1] == '#') {
            f->pt = 1;
            f->wid++;
            for (d++; ch(d) == '#'; d++) {
                f->dec++;
                f->wid++;
                digits = true;
            }
        } else if (digits) { /* ##. : a point, no places after it */
            f->pt = 1;
            f->wid++;
            d++;
        }
    }
    if (!digits && !f->st && !f->dol)
        return false;
    if (d + 3 < us.len && !memcmp(us.fmt + d, "^^^^", 4)) {
        f->car = 4;
        d += 4;
        if (ch(d) == '^') {
            f->car = 5;
            d++;
        }
    }
    if (!f->lp && (ch(d) == '+' || ch(d) == '-'))
        f->tr = us.fmt[d++];
    f->len = d - b;
    return true;
}

/* us_fld: the field at b: its kind, *len its length; a number's in us.f. */
static uint8_t field(uint32_t b, uint32_t *len)
{
    uint8_t c = us.fmt[b];
    *len = 1;
    if (c == '!')
        return UF_FIRST;
    if (c == '&')
        return UF_ALL;
    if (c == '\\') {
        for (uint32_t d = b + 1; d < us.len; d++) {
            if (us.fmt[d] == '\\') {
                *len = d + 1 - b;
                return UF_WIDE;
            }
            if (us.fmt[d] != ' ')
                break;
        }
        return UF_NONE;
    }
    if (!memchr("#.+$*", c, 5) || !numfield(b))
        return UF_NONE;
    *len = us.f.len;
    return UF_NUM;
}

/* us_lit: the literal text from us.at to the next field written; its kind, UF_NONE the format's
   end. */
static uint8_t literal(uint32_t *len)
{
    for (;;) {
        if (us.at >= us.len)
            return UF_NONE;
        if (us.fmt[us.at] == '_' && us.at + 1 < us.len) {
            cons_putc(us.fmt[us.at + 1]);
            us.at += 2;
            continue;
        }
        uint8_t k = field(us.at, len);
        if (k)
            return k;
        cons_putc(us.fmt[us.at++]);
    }
}

/* us_next: to the next field for an item, the text before it written; its kind, *len its
   length. Illegal function call: a format with no field. */
static uint8_t next(uint32_t *len)
{
    uint32_t start = us.at;
    if (!literal(len)) {
        if (!start)
            repl_error(E_IQ);
        us.at = 0;
        if (!literal(len))
            repl_error(E_IQ);
    }
    return field(us.at, len);
}

/* US.END: the text up to the next field, a new line; US.ENDN: no new line. */
void usng_use(void)
{
    uint32_t len;
    literal(&len);
    cons_nl();
}
void usng_usen(void)
{
    uint32_t len;
    literal(&len);
}

/* US.STR ( s -- ): to a string field. Type mismatch: a numeric field. */
void usng_uss(void)
{
    uint32_t w;
    uint8_t k = next(&w);
    if (k == UF_NUM)
        repl_error(E_TM);
    us.at += w;
    Desc s = strg_get(exec_popa());
    const uint8_t *p = at(s.ptr);
    if (k == UF_FIRST)
        cons_putc(s.len ? *p : ' ');
    else if (k == UF_ALL)
        out(p, s.len);
    else {
        uint32_t n = s.len < w ? s.len : w;
        out(p, n);
        for (; n < w; n++)
            cons_putc(' ');
    }
}

/* us_ldi: the INT v's digits. */
static void load_int(int32_t v)
{
    us.sci = false;
    us.neg = v < 0;
    uint32_t u = us.neg ? 0u - (uint32_t)v : (uint32_t)v;
    uint8_t t[10];
    int32_t n = 0;
    do {
        t[n++] = '0' + u % 10;
        u /= 10;
    } while (u);
    us.nc = us.ni = n;
    for (int32_t i = 0; i < n; i++)
        us.nt[i] = t[n - 1 - i];
}

/* us_ldr: v's digits to 16 places, a SINGLE's when td is 7, a DOUBLE's 16; us.sci when PRINT,
   rounding it to td digits, would give it an exponent. */
static void load_real(double v, int32_t td)
{
    us.neg = v < 0;
    if (v == 0) {
        us.nt[0] = '0';
        us.nc = us.ni = 1;
        us.sci = false;
        return;
    }
    uint8_t dig[20];
    int32_t dp;
    int32_t tc = (int32_t)dubl_digits(us.neg ? -v : v, 16, dig, &dp);
    memcpy(us.nt, dig, tc);
    us.nc = tc;
    us.ni = dp + 1;
    if (us.nc > td) { /* rounded to the type's digits */
        int32_t b = td;
        us.nc = td;
        if (us.nt[b] >= '5') {
            for (;;) {
                if (--b < 0) { /* all 9s: 1, a place more */
                    us.nt[0] = '1';
                    us.nc = 1;
                    us.ni++;
                    break;
                }
                if (us.nt[b] != '9') {
                    us.nt[b]++;
                    break;
                }
                us.nc = b; /* carried up past the 9s */
            }
        }
        while (us.nc > 1 && us.nt[us.nc - 1] == '0')
            us.nc--;
    }
    if (us.ni > td) /* past the type's places: ni or |ni - nc| over its digits */
        us.sci = true;
    else {
        int32_t d = us.ni - us.nc;
        us.sci = (d < 0 ? -d : d) > td;
    }
    memcpy(us.nt, dig, tc); /* the digits: all 16 of them */
    us.nc = tc;
    us.ni = dp + 1;
}

/* us_norm: zeros before the digits so that every place before the point has one: ni not under
   0. */
static void norm(void)
{
    int32_t k = -us.ni;
    if (k <= 0)
        return;
    if (k > NDIG)
        k = NDIG;
    memmove(us.nt + k, us.nt, us.nc);
    memset(us.nt, '0', k);
    us.nc += k;
    us.ni = 0;
    if (us.nc > NDIG)
        us.nc = NDIG;
}

/* us_round: the digits rounded, half up, to places after the point, zeros added to them. */
static void round_to(int32_t places)
{
    norm();
    int32_t keep = places + us.ni;
    if (keep > NDIG)
        keep = NDIG;
    if (keep >= us.nc) {
        for (; us.nc < keep; us.nc++)
            us.nt[us.nc] = '0';
        return;
    }
    us.nc = keep;
    if (us.nt[keep] < '5')
        return;
    for (int32_t i = keep - 1; i >= 0; i--) {
        if (us.nt[i] != '9') {
            us.nt[i]++;
            return;
        }
        us.nt[i] = '0';
    }
    memmove(us.nt + 1, us.nt, us.nc); /* all 9s: a 1 before them */
    us.nt[0] = '1';
    us.nc++;
    us.ni++;
}

/* us_lead: the sign before the number, 0 none. */
static uint8_t lead(void) { return us.f.lp ? us.neg ? '-' : '+' : 0; }

/* us_tsign: the sign after it, 0 none. */
static uint8_t tsign(void)
{
    uint8_t t = (uint8_t)us.f.tr;
    if (!t)
        return 0;
    return us.neg ? '-' : t == '+' ? '+' : ' ';
}

/* us_fits: the body's n chars, its signs and $, past the field's columns? */
static bool past(uint32_t n)
{
    if (us.f.lp || (us.neg && !us.f.tr))
        n++;
    return n + us.f.dol > us.f.wid + us.f.lp;
}

/* us_fexp: the number as digits and an exponent of car carets (4: two digits, 5: three) in the
   field's places, to the body; its length. */
static uint32_t fexp(uint32_t car)
{
    int32_t whole = us.f.bp ? (int32_t)us.f.bp - 1 : 0; /* less the sign's place */
    int32_t places = whole + (int32_t)us.f.dec;
    norm();
    int32_t first = 0; /* the first digit not 0 */
    while (first < us.nc && us.nt[first] == '0')
        first++;
    int32_t shift = 0;
    if (first < us.nc) {
        int32_t ex = us.ni - first;
        memmove(us.nt, us.nt + first, us.nc - first);
        us.nc -= first;
        us.ni = 0;
        round_to(places);
        if (us.ni) {
            ex++;
            us.ni = 0;
        }
        shift = ex - whole;
    } else
        memset(us.nt, '0', places); /* 0: zeros in every place */
    uint8_t *d = us.body;
    *d++ = us.neg ? '-' : !whole && us.f.pt ? '0' : ' ';
    for (int32_t i = 0; i < places; i++) {
        if (i == whole && us.f.pt)
            *d++ = '.';
        uint8_t c = us.nt[i];
        if (i < whole - 1 && c == '0' && first == us.nc) /* a lead 0 of 0 a space */
            c = ' ';
        *d++ = c;
    }
    if (whole == places && us.f.pt)
        *d++ = '.';
    *d++ = 'E';
    *d++ = shift < 0 ? '-' : '+';
    uint32_t e = shift < 0 ? 0u - (uint32_t)shift : (uint32_t)shift;
    if (car == 5)
        *d++ = '0' + e / 100 % 10;
    *d++ = '0' + e % 100 / 10;
    *d++ = '0' + e % 10;
    return (uint32_t)(d - us.body);
}

/* us_wnum: the body's n chars in cols columns: right-aligned, filled with spaces or *s, the signs
   and $ around them; a % first when past them. */
static void wnum(uint32_t n, uint32_t cols)
{
    uint8_t *d = us.line;
    uint8_t before = lead(), after = tsign(), minus = 0;
    if (us.neg && !before && !after)
        minus = '-';
    uint32_t need = n + (before || minus) + us.f.dol;
    if (need > cols)
        *d++ = '%';
    else
        for (uint32_t i = cols - need; i; i--)
            *d++ = us.f.st ? '*' : ' ';
    if (before)
        *d++ = before;
    if (minus)
        *d++ = minus;
    if (us.f.dol)
        *d++ = '$';
    memcpy(d, us.body, n);
    d += n;
    if (after)
        *d++ = after;
    out(us.line, (uint32_t)(d - us.line));
}

/* us_plain: a number without an exponent, written. */
static void plain(void)
{
    round_to((int32_t)us.f.dec);
    uint8_t *d = us.body; /* the digits before the point, commas */
    int32_t b = 0;
    while (b < us.ni && us.nt[b] == '0')
        b++;
    int32_t n = us.ni - b;
    if (!n) {
        if (us.f.bp)
            *d++ = '0';
    } else
        for (int32_t k = 0; k < n; k++) {
            if (us.f.com && k && (n - k) % 3 == 0)
                *d++ = ',';
            *d++ = us.nt[b + k];
        }
    if (us.f.pt) { /* the point, the places after it */
        *d++ = '.';
        memcpy(d, us.nt + us.ni, us.f.dec);
        d += us.f.dec;
    }
    uint32_t len = (uint32_t)(d - us.body);
    if (us.sci && past(len)) { /* past the field: an exponent */
        cons_putc('%');
        out(us.body, fexp(4));
        return;
    }
    wnum(len, us.f.wid + us.f.lp);
}

/* A number to a numeric field. Type mismatch: a string field. */
static void number(void)
{
    uint32_t w;
    if (next(&w) != UF_NUM)
        repl_error(E_TM);
    us.at += w;
    if (us.f.car) {
        uint32_t n = fexp(us.f.car);
        uint8_t t = tsign(); /* us_trail */
        if (t)
            us.body[n++] = t;
        out(us.body, n);
    } else
        plain();
    exec.sp++;
}

/* US.INT ( i -- ), US.FLT ( f -- ), US.DBL ( d -- ). */
void usng_usi(void)
{
    load_int(exec.sp->i);
    number();
}
void usng_usf(void)
{
    float f;
    memcpy(&f, &exec.sp->u, 4);
    load_real(f, 7);
    number();
}
void usng_usd(void)
{
    load_real(exec.sp->d, 16);
    number();
}
