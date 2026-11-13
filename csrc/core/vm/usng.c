/* PRINT USING (src/vm/using.asm, as llrm's runtime/qb/using.c). A number is digits: its value is
   0.digits * 10^ni. */
#include "vm/usng.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "num/dubl.h"

enum { FMTMAX = 255, NDIG = 80 };

/* A field's kind (us_fld). */
typedef enum : uint8_t { UF_NONE, UF_FIRST, UF_ALL, UF_WIDE, UF_NUM } FieldKind;

/* A numeric field (TUSF). */
typedef struct {
    uint32_t len;     /* its characters in the format */
    uint32_t width;   /* its columns, the signs not counted */
    bool plus_lead;   /* a leading + */
    uint8_t trail;    /* a trailing + or -, else 0 */
    bool stars;       /* ** */
    bool dollar;      /* a $ */
    bool commas;
    uint32_t before;  /* places before the point */
    uint32_t after;   /* after it */
    bool point;
    uint32_t carets;  /* 4 or 5 for an exponent, else 0 */
} Field;

static struct {
    uint8_t fmt[FMTMAX];
    uint32_t fmt_len, pos;     /* the format's length, the place */
    Field f;
    uint8_t digits[NDIG + 20]; /* the number: 0.digits * 10^scale */
    int32_t count, scale;
    bool neg, sci;             /* its sign; PRINT would give it an exponent */
    uint8_t body[FMTMAX + NDIG];
    uint8_t line[FMTMAX + NDIG + 8];
} us;

/* The format's char at d, 0 past it. */
static uint8_t fmt_at(const uint32_t d) { return d < us.fmt_len ? us.fmt[d] : 0; }

static void out(const uint8_t *const p, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        cons_putc(p[i]);
}

/* US.BEGIN ( fmt -- ). Illegal function call: over 255 characters. */
void usng_usb(Vm *const vm)
{
    const Desc s = strg_get(exec_popa(vm));
    if (s.len > FMTMAX)
        repl_error(E_IQ);
    us.fmt_len = s.len;
    memcpy(us.fmt, at(s.ptr), s.len);
    us.pos = 0;
}

/* us_numf: the numeric field at b in us.f; false none. */
static bool num_field(const uint32_t b)
{
    Field *const f = &us.f;
    *f = (Field){};
    uint32_t d = b;
    bool digits = false;
    if (fmt_at(d) == '+' && d + 1 < us.fmt_len) {
        f->plus_lead = true;
        d++;
    }
    if (d + 1 < us.fmt_len) {
        if (us.fmt[d] == '*' && us.fmt[d + 1] == '*') {
            f->stars = true;
            f->width += 2;
            f->before += 2;
            d += 2;
            if (fmt_at(d) == '$') {
                f->dollar = true;
                f->width++;
                d++;
            }
        } else if (us.fmt[d] == '$' && us.fmt[d + 1] == '$') {
            f->dollar = true;
            f->width += 2;
            f->before++;
            d += 2;
        }
    }
    for (;; d++) { /* # and , before the point */
        if (fmt_at(d) == '#')
            f->before++;
        else if (fmt_at(d) == ',')
            f->commas = true;
        else
            break;
        f->width++;
        digits = true;
    }
    if (fmt_at(d) == '.') {
        if (d + 1 < us.fmt_len && us.fmt[d + 1] == '#') {
            f->point = true;
            f->width++;
            for (d++; fmt_at(d) == '#'; d++) {
                f->after++;
                f->width++;
                digits = true;
            }
        } else if (digits) { /* ##. : a point, no places after it */
            f->point = true;
            f->width++;
            d++;
        }
    }
    if (!digits && !f->stars && !f->dollar)
        return false;
    if (d + 3 < us.fmt_len && !memcmp(us.fmt + d, "^^^^", 4)) {
        f->carets = 4;
        d += 4;
        if (fmt_at(d) == '^') {
            f->carets = 5;
            d++;
        }
    }
    if (!f->plus_lead && (fmt_at(d) == '+' || fmt_at(d) == '-'))
        f->trail = us.fmt[d++];
    f->len = d - b;
    return true;
}

/* us_fld: the field at b: its kind, *len its length; a number's in us.f. */
static FieldKind field(const uint32_t b, uint32_t *const len)
{
    const uint8_t c = us.fmt[b];
    *len = 1;
    if (c == '!')
        return UF_FIRST;
    if (c == '&')
        return UF_ALL;
    if (c == '\\') {
        for (uint32_t d = b + 1; d < us.fmt_len; d++) {
            if (us.fmt[d] == '\\') {
                *len = d + 1 - b;
                return UF_WIDE;
            }
            if (us.fmt[d] != ' ')
                break;
        }
        return UF_NONE;
    }
    if (!memchr("#.+$*", c, 5) || !num_field(b))
        return UF_NONE;
    *len = us.f.len;
    return UF_NUM;
}

/* us_lit: the literal text from us.pos to the next field written; its kind, UF_NONE the format's
   end. */
static FieldKind literal(uint32_t *const len)
{
    for (;;) {
        if (us.pos >= us.fmt_len)
            return UF_NONE;
        if (us.fmt[us.pos] == '_' && us.pos + 1 < us.fmt_len) {
            cons_putc(us.fmt[us.pos + 1]);
            us.pos += 2;
            continue;
        }
        const FieldKind k = field(us.pos, len);
        if (k)
            return k;
        cons_putc(us.fmt[us.pos++]);
    }
}

/* us_next: to the next field for an item, the text before it written; its kind, *len its
   length. Illegal function call: a format with no field. */
static FieldKind next_field(uint32_t *const len)
{
    const uint32_t start = us.pos;
    if (!literal(len)) {
        if (!start)
            repl_error(E_IQ);
        us.pos = 0;
        if (!literal(len))
            repl_error(E_IQ);
    }
    return field(us.pos, len);
}

/* US.END: the text up to the next field, a new line; US.ENDN: no new line. */
void usng_use(Vm *const)
{
    uint32_t len;
    literal(&len);
    cons_nl();
}
void usng_usen(Vm *const)
{
    uint32_t len;
    literal(&len);
}

/* US.STR ( s -- ): to a string field. Type mismatch: a numeric field. */
void usng_uss(Vm *const vm)
{
    uint32_t w;
    const FieldKind k = next_field(&w);
    if (k == UF_NUM)
        repl_error(E_TM);
    us.pos += w;
    const Desc s = strg_get(exec_popa(vm));
    const uint8_t *const p = at(s.ptr);
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
static void load_int(const int32_t v)
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
    us.count = us.scale = n;
    for (int32_t i = 0; i < n; i++)
        us.digits[i] = t[n - 1 - i];
}

/* us_ldr: v's digits to 16 places, a SINGLE's when td is 7, a DOUBLE's 16; us.sci when PRINT,
   rounding it to td digits, would give it an exponent. */
static void load_real(const double v, const int32_t td)
{
    us.neg = v < 0;
    if (v == 0) {
        us.digits[0] = '0';
        us.count = us.scale = 1;
        us.sci = false;
        return;
    }
    uint8_t dig[20];
    int32_t dp;
    const int32_t tc = (int32_t)dubl_digits(us.neg ? -v : v, 16, dig, &dp);
    memcpy(us.digits, dig, tc);
    us.count = tc;
    us.scale = dp + 1;
    if (us.count > td) { /* rounded to the type's digits */
        int32_t b = td;
        us.count = td;
        if (us.digits[b] >= '5') {
            for (;;) {
                if (--b < 0) { /* all 9s: 1, a place more */
                    us.digits[0] = '1';
                    us.count = 1;
                    us.scale++;
                    break;
                }
                if (us.digits[b] != '9') {
                    us.digits[b]++;
                    break;
                }
                us.count = b; /* carried up past the 9s */
            }
        }
        while (us.count > 1 && us.digits[us.count - 1] == '0')
            us.count--;
    }
    if (us.scale > td) /* past the type's places: ni or |ni - nc| over its digits */
        us.sci = true;
    else {
        const int32_t d = us.scale - us.count;
        us.sci = (d < 0 ? -d : d) > td;
    }
    memcpy(us.digits, dig, tc); /* the digits: all 16 of them */
    us.count = tc;
    us.scale = dp + 1;
}

/* us_norm: zeros before the digits so that every place before the point has one: ni not under
   0. */
static void pad_zeros(void)
{
    int32_t k = -us.scale;
    if (k <= 0)
        return;
    if (k > NDIG)
        k = NDIG;
    memmove(us.digits + k, us.digits, us.count);
    memset(us.digits, '0', k);
    us.count += k;
    us.scale = 0;
    if (us.count > NDIG)
        us.count = NDIG;
}

/* us_round: the digits rounded, half up, to places after the point, zeros added to them. */
static void round_to(const int32_t places)
{
    pad_zeros();
    int32_t keep = places + us.scale;
    if (keep > NDIG)
        keep = NDIG;
    if (keep >= us.count) {
        for (; us.count < keep; us.count++)
            us.digits[us.count] = '0';
        return;
    }
    us.count = keep;
    if (us.digits[keep] < '5')
        return;
    for (int32_t i = keep - 1; i >= 0; i--) {
        if (us.digits[i] != '9') {
            us.digits[i]++;
            return;
        }
        us.digits[i] = '0';
    }
    memmove(us.digits + 1, us.digits, us.count); /* all 9s: a 1 before them */
    us.digits[0] = '1';
    us.count++;
    us.scale++;
}

/* us_lead: the sign before the number, 0 none. */
static uint8_t lead_sign(void) { return us.f.plus_lead ? us.neg ? '-' : '+' : 0; }

/* us_tsign: the sign after it, 0 none. */
static uint8_t trail_sign(void)
{
    const uint8_t t = us.f.trail;
    if (!t)
        return 0;
    return us.neg ? '-' : t == '+' ? '+' : ' ';
}

/* us_fits: the body's n chars, its signs and $, past the field's columns? */
static bool overflows(uint32_t n)
{
    if (us.f.plus_lead || (us.neg && !us.f.trail))
        n++;
    return n + us.f.dollar > us.f.width + us.f.plus_lead;
}

/* us_fexp: the number as digits and an exponent of car carets (4: two digits, 5: three) in the
   field's places, to the body; its length. */
static uint32_t with_exponent(const uint32_t carets)
{
    const int32_t whole = us.f.before ? (int32_t)us.f.before - 1 : 0; /* less the sign's place */
    const int32_t places = whole + (int32_t)us.f.after;
    pad_zeros();
    int32_t first = 0; /* the first digit not 0 */
    while (first < us.count && us.digits[first] == '0')
        first++;
    int32_t shift = 0;
    if (first < us.count) {
        int32_t ex = us.scale - first;
        memmove(us.digits, us.digits + first, us.count - first);
        us.count -= first;
        us.scale = 0;
        round_to(places);
        if (us.scale) {
            ex++;
            us.scale = 0;
        }
        shift = ex - whole;
    } else
        memset(us.digits, '0', places); /* 0: zeros in every place */
    uint8_t *d = us.body;
    *d++ = us.neg ? '-' : !whole && us.f.point ? '0' : ' ';
    for (int32_t i = 0; i < places; i++) {
        if (i == whole && us.f.point)
            *d++ = '.';
        uint8_t c = us.digits[i];
        if (i < whole - 1 && c == '0' && first == us.count) /* a lead 0 of 0 a space */
            c = ' ';
        *d++ = c;
    }
    if (whole == places && us.f.point)
        *d++ = '.';
    *d++ = 'E';
    *d++ = shift < 0 ? '-' : '+';
    const uint32_t e = shift < 0 ? 0u - (uint32_t)shift : (uint32_t)shift;
    if (carets == 5)
        *d++ = '0' + e / 100 % 10;
    *d++ = '0' + e % 100 / 10;
    *d++ = '0' + e % 10;
    return (uint32_t)(d - us.body);
}

/* us_wnum: the body's n chars in cols columns: right-aligned, filled with spaces or *s, the signs
   and $ around them; a % first when past them. */
static void write_number(const uint32_t n, const uint32_t cols)
{
    uint8_t *d = us.line;
    const uint8_t before = lead_sign();
    const uint8_t after = trail_sign();
    const bool minus = us.neg && !before && !after;
    const uint32_t need = n + (before || minus) + us.f.dollar;
    if (need > cols)
        *d++ = '%';
    else
        for (uint32_t i = cols - need; i; i--)
            *d++ = us.f.stars ? '*' : ' ';
    if (before)
        *d++ = before;
    if (minus)
        *d++ = '-';
    if (us.f.dollar)
        *d++ = '$';
    memcpy(d, us.body, n);
    d += n;
    if (after)
        *d++ = after;
    out(us.line, (uint32_t)(d - us.line));
}

/* us_plain: a number without an exponent, written. */
static void write_plain(void)
{
    round_to((int32_t)us.f.after);
    uint8_t *d = us.body; /* the digits before the point, commas */
    int32_t b = 0;
    while (b < us.scale && us.digits[b] == '0')
        b++;
    const int32_t n = us.scale - b;
    if (!n) {
        if (us.f.before)
            *d++ = '0';
    } else
        for (int32_t k = 0; k < n; k++) {
            if (us.f.commas && k && (n - k) % 3 == 0)
                *d++ = ',';
            *d++ = us.digits[b + k];
        }
    if (us.f.point) { /* the point, the places after it */
        *d++ = '.';
        memcpy(d, us.digits + us.scale, us.f.after);
        d += us.f.after;
    }
    const uint32_t len = (uint32_t)(d - us.body);
    if (us.sci && overflows(len)) { /* past the field: an exponent */
        cons_putc('%');
        out(us.body, with_exponent(4));
        return;
    }
    write_number(len, us.f.width + us.f.plus_lead);
}

/* A number to a numeric field. Type mismatch: a string field. */
static void number(Vm *const vm)
{
    uint32_t w;
    if (next_field(&w) != UF_NUM)
        repl_error(E_TM);
    us.pos += w;
    if (us.f.carets) {
        uint32_t n = with_exponent(us.f.carets);
        const uint8_t t = trail_sign(); /* us_trail */
        if (t)
            us.body[n++] = t;
        out(us.body, n);
    } else
        write_plain();
    vm->sp++;
}

/* US.INT ( i -- ), US.FLT ( f -- ), US.DBL ( d -- ). */
void usng_usi(Vm *const vm)
{
    load_int(vm->sp->i);
    number(vm);
}
void usng_usf(Vm *const vm)
{
    load_real(vm->sp->f, 7);
    number(vm);
}
void usng_usd(Vm *const vm)
{
    load_real(vm->sp->d, 16);
    number(vm);
}
