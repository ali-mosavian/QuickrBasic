/* PRINT USING (src/vm/using.asm, as llrm's runtime/qb/using.c). A number is digits: its value is
   0.digits * 10^ni. */
#include "vm/usng.h"
#include "io/cons.h"
#include "io/repl.h"
#include "mem/strg.h"
#include "num/dubl.h"


Usng usng;

/* The format's char at d, 0 past it. */
static uint8_t fmt_at(const uint32_t d) { return d < usng.fmt_len ? usng.fmt[d] : 0; }

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
    usng.fmt_len = s.len;
    memcpy(usng.fmt, at(s.ptr), s.len);
    usng.pos = 0;
}

/* us_numf: the numeric field at b in us.f; false none. */
static bool num_field(const uint32_t b)
{
    UsngField *const f = &usng.f;
    *f = (UsngField){};
    uint32_t d = b;
    bool digits = false;
    if (fmt_at(d) == '+' && d + 1 < usng.fmt_len) {
        f->plus_lead = true;
        d++;
    }
    if (d + 1 < usng.fmt_len) {
        if (usng.fmt[d] == '*' && usng.fmt[d + 1] == '*') {
            f->stars = true;
            f->width += 2;
            f->before += 2;
            d += 2;
            if (fmt_at(d) == '$') {
                f->dollar = true;
                f->width++;
                d++;
            }
        } else if (usng.fmt[d] == '$' && usng.fmt[d + 1] == '$') {
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
        if (d + 1 < usng.fmt_len && usng.fmt[d + 1] == '#') {
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
    if (d + 3 < usng.fmt_len && !memcmp(usng.fmt + d, "^^^^", 4)) {
        f->carets = 4;
        d += 4;
        if (fmt_at(d) == '^') {
            f->carets = 5;
            d++;
        }
    }
    if (!f->plus_lead && (fmt_at(d) == '+' || fmt_at(d) == '-'))
        f->trail = usng.fmt[d++];
    f->len = d - b;
    return true;
}

/* us_fld: the field at b: its kind, *len its length; a number's in us.f. */
static FieldKind field(const uint32_t b, uint32_t *const len)
{
    const uint8_t c = usng.fmt[b];
    *len = 1;
    if (c == '!')
        return UF_FIRST;
    if (c == '&')
        return UF_ALL;
    if (c == '\\') {
        for (uint32_t d = b + 1; d < usng.fmt_len; d++) {
            if (usng.fmt[d] == '\\') {
                *len = d + 1 - b;
                return UF_WIDE;
            }
            if (usng.fmt[d] != ' ')
                break;
        }
        return UF_NONE;
    }
    if (!memchr("#.+$*", c, 5) || !num_field(b))
        return UF_NONE;
    *len = usng.f.len;
    return UF_NUM;
}

/* us_lit: the literal text from us.pos to the next field written; its kind, UF_NONE the format's
   end. */
static FieldKind literal(uint32_t *const len)
{
    for (;;) {
        if (usng.pos >= usng.fmt_len)
            return UF_NONE;
        if (usng.fmt[usng.pos] == '_' && usng.pos + 1 < usng.fmt_len) {
            cons_putc(usng.fmt[usng.pos + 1]);
            usng.pos += 2;
            continue;
        }
        const FieldKind k = field(usng.pos, len);
        if (k)
            return k;
        cons_putc(usng.fmt[usng.pos++]);
    }
}

/* us_next: to the next field for an item, the text before it written; its kind, *len its
   length. Illegal function call: a format with no field. */
static FieldKind next_field(uint32_t *const len)
{
    const uint32_t start = usng.pos;
    if (!literal(len)) {
        if (!start)
            repl_error(E_IQ);
        usng.pos = 0;
        if (!literal(len))
            repl_error(E_IQ);
    }
    return field(usng.pos, len);
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
    usng.pos += w;
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
    usng.sci = false;
    usng.neg = v < 0;
    uint32_t u = usng.neg ? 0u - (uint32_t)v : (uint32_t)v;
    uint8_t t[10];
    int32_t n = 0;
    do {
        t[n++] = '0' + u % 10;
        u /= 10;
    } while (u);
    usng.count = usng.scale = n;
    for (int32_t i = 0; i < n; i++)
        usng.digits[i] = t[n - 1 - i];
}

/* us_ldr: v's digits to 16 places, a SINGLE's when td is 7, a DOUBLE's 16; us.sci when PRINT,
   rounding it to td digits, would give it an exponent. */
static void load_real(const double v, const int32_t td)
{
    usng.neg = v < 0;
    if (v == 0) {
        usng.digits[0] = '0';
        usng.count = usng.scale = 1;
        usng.sci = false;
        return;
    }
    uint8_t dig[20];
    int32_t dp;
    const int32_t tc = (int32_t)dubl_digits(usng.neg ? -v : v, 16, dig, &dp);
    memcpy(usng.digits, dig, tc);
    usng.count = tc;
    usng.scale = dp + 1;
    if (usng.count > td) { /* rounded to the type's digits */
        int32_t b = td;
        usng.count = td;
        if (usng.digits[b] >= '5') {
            for (;;) {
                if (--b < 0) { /* all 9s: 1, a place more */
                    usng.digits[0] = '1';
                    usng.count = 1;
                    usng.scale++;
                    break;
                }
                if (usng.digits[b] != '9') {
                    usng.digits[b]++;
                    break;
                }
                usng.count = b; /* carried up past the 9s */
            }
        }
        while (usng.count > 1 && usng.digits[usng.count - 1] == '0')
            usng.count--;
    }
    if (usng.scale > td) /* past the type's places: ni or |ni - nc| over its digits */
        usng.sci = true;
    else {
        const int32_t d = usng.scale - usng.count;
        usng.sci = (d < 0 ? -d : d) > td;
    }
    memcpy(usng.digits, dig, tc); /* the digits: all 16 of them */
    usng.count = tc;
    usng.scale = dp + 1;
}

/* us_norm: zeros before the digits so that every place before the point has one: ni not under
   0. */
static void pad_zeros(void)
{
    int32_t k = -usng.scale;
    if (k <= 0)
        return;
    if (k > NDIG)
        k = NDIG;
    memmove(usng.digits + k, usng.digits, usng.count);
    memset(usng.digits, '0', k);
    usng.count += k;
    usng.scale = 0;
    if (usng.count > NDIG)
        usng.count = NDIG;
}

/* us_round: the digits rounded, half up, to places after the point, zeros added to them. */
static void round_to(const int32_t places)
{
    pad_zeros();
    int32_t keep = places + usng.scale;
    if (keep > NDIG)
        keep = NDIG;
    if (keep >= usng.count) {
        for (; usng.count < keep; usng.count++)
            usng.digits[usng.count] = '0';
        return;
    }
    usng.count = keep;
    if (usng.digits[keep] < '5')
        return;
    for (int32_t i = keep - 1; i >= 0; i--) {
        if (usng.digits[i] != '9') {
            usng.digits[i]++;
            return;
        }
        usng.digits[i] = '0';
    }
    memmove(usng.digits + 1, usng.digits, usng.count); /* all 9s: a 1 before them */
    usng.digits[0] = '1';
    usng.count++;
    usng.scale++;
}

/* us_lead: the sign before the number, 0 none. */
static uint8_t lead_sign(void) { return usng.f.plus_lead ? usng.neg ? '-' : '+' : 0; }

/* us_tsign: the sign after it, 0 none. */
static uint8_t trail_sign(void)
{
    const uint8_t t = usng.f.trail;
    if (!t)
        return 0;
    return usng.neg ? '-' : t == '+' ? '+' : ' ';
}

/* us_fits: the body's n chars, its signs and $, past the field's columns? */
static bool overflows(uint32_t n)
{
    if (usng.f.plus_lead || (usng.neg && !usng.f.trail))
        n++;
    return n + usng.f.dollar > usng.f.width + usng.f.plus_lead;
}

/* us_fexp: the number as digits and an exponent of car carets (4: two digits, 5: three) in the
   field's places, to the body; its length. */
static uint32_t with_exponent(const uint32_t carets)
{
    const int32_t whole = usng.f.before ? (int32_t)usng.f.before - 1 : 0; /* less the sign's place */
    const int32_t places = whole + (int32_t)usng.f.after;
    pad_zeros();
    int32_t first = 0; /* the first digit not 0 */
    while (first < usng.count && usng.digits[first] == '0')
        first++;
    int32_t shift = 0;
    if (first < usng.count) {
        int32_t ex = usng.scale - first;
        memmove(usng.digits, usng.digits + first, usng.count - first);
        usng.count -= first;
        usng.scale = 0;
        round_to(places);
        if (usng.scale) {
            ex++;
            usng.scale = 0;
        }
        shift = ex - whole;
    } else
        memset(usng.digits, '0', places); /* 0: zeros in every place */
    uint8_t *d = usng.body;
    *d++ = usng.neg ? '-' : !whole && usng.f.point ? '0' : ' ';
    for (int32_t i = 0; i < places; i++) {
        if (i == whole && usng.f.point)
            *d++ = '.';
        uint8_t c = usng.digits[i];
        if (i < whole - 1 && c == '0' && first == usng.count) /* a lead 0 of 0 a space */
            c = ' ';
        *d++ = c;
    }
    if (whole == places && usng.f.point)
        *d++ = '.';
    *d++ = 'E';
    *d++ = shift < 0 ? '-' : '+';
    const uint32_t e = shift < 0 ? 0u - (uint32_t)shift : (uint32_t)shift;
    if (carets == 5)
        *d++ = '0' + e / 100 % 10;
    *d++ = '0' + e % 100 / 10;
    *d++ = '0' + e % 10;
    return (uint32_t)(d - usng.body);
}

/* us_wnum: the body's n chars in cols columns: right-aligned, filled with spaces or *s, the signs
   and $ around them; a % first when past them. */
static void write_number(const uint32_t n, const uint32_t cols)
{
    uint8_t *d = usng.line;
    const uint8_t before = lead_sign();
    const uint8_t after = trail_sign();
    const bool minus = usng.neg && !before && !after;
    const uint32_t need = n + (before || minus) + usng.f.dollar;
    if (need > cols)
        *d++ = '%';
    else
        for (uint32_t i = cols - need; i; i--)
            *d++ = usng.f.stars ? '*' : ' ';
    if (before)
        *d++ = before;
    if (minus)
        *d++ = '-';
    if (usng.f.dollar)
        *d++ = '$';
    memcpy(d, usng.body, n);
    d += n;
    if (after)
        *d++ = after;
    out(usng.line, (uint32_t)(d - usng.line));
}

/* us_plain: a number without an exponent, written. */
static void write_plain(void)
{
    round_to((int32_t)usng.f.after);
    uint8_t *d = usng.body; /* the digits before the point, commas */
    int32_t b = 0;
    while (b < usng.scale && usng.digits[b] == '0')
        b++;
    const int32_t n = usng.scale - b;
    if (!n) {
        if (usng.f.before)
            *d++ = '0';
    } else
        for (int32_t k = 0; k < n; k++) {
            if (usng.f.commas && k && (n - k) % 3 == 0)
                *d++ = ',';
            *d++ = usng.digits[b + k];
        }
    if (usng.f.point) { /* the point, the places after it */
        *d++ = '.';
        memcpy(d, usng.digits + usng.scale, usng.f.after);
        d += usng.f.after;
    }
    const uint32_t len = (uint32_t)(d - usng.body);
    if (usng.sci && overflows(len)) { /* past the field: an exponent */
        cons_putc('%');
        out(usng.body, with_exponent(4));
        return;
    }
    write_number(len, usng.f.width + usng.f.plus_lead);
}

/* A number to a numeric field. Type mismatch: a string field. */
static void number(Vm *const vm)
{
    uint32_t w;
    if (next_field(&w) != UF_NUM)
        repl_error(E_TM);
    usng.pos += w;
    if (usng.f.carets) {
        uint32_t n = with_exponent(usng.f.carets);
        const uint8_t t = trail_sign(); /* us_trail */
        if (t)
            usng.body[n++] = t;
        out(usng.body, n);
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
