/* PRINT USING, as llrm's runtime/qb/using.c.
 *
 * # How it works
 * US.BEGIN takes the format (usng_usb); each item goes to the format's next field (usng_usi,
 * usng_usf, usng_usd, usng_uss); US.END writes the text up to the next field (usng_use,
 * usng_usen). The format starts again past its last field.
 *
 * The format is literal text and fields: ! the first character, & all of it, \  \ as wide as
 * that, or a number: # a digit, a point, commas, a leading + or $$ or ** or **$, a trailing + or -,
 * ^^^^ an exponent; _ makes the next character literal.
 *
 * A number is digits: its value is 0.digits * 10^scale, rounded to the field's places, then
 * written with its signs, fill and exponent.
 *
 * # Example
 *   PRINT USING "###.##"; 3.14159       "  3.14"
 *
 * # asm structure (src/vm/using.asm)
 * | asm | here | |
 * |---|---|---|
 * | sc_usb, sc_usi, sc_usf, sc_usd, sc_uss, sc_use | usng_usb, usng_usi, usng_usf, usng_usd, usng_uss, usng_use | MIRROR |
 * | sc_usen | usng_usen | MIRROR |
 * | TUSF | UsngField (using.h) | MIRROR |
 * | us_*, g_ufmt .. g_utc, u_numc | static; Usng (using.h) | MIRROR |
 */
#include "vm/using.h"
#include "session.h"
#include "io/console.h"
#include "io/repl.h"
#include "mem/string.h"
#include "num/double.h"

/* The format's char at d, 0 past it. */
static uint8_t fmt_at(Session *const S, const uint32_t d)
{
    return d < S->usng.fmt_len ? S->usng.fmt[d] : 0;
}

static void out(Session *const S, const uint8_t *const p, const uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        cons_putc(S, p[i]);
}

/* US.BEGIN ( fmt -- ). Illegal function call: over 255 characters. */
void usng_usb(Session *const S)
{
    const Desc s = strg_get(S, exec_popa(&S->vm));

    if (s.len > FMTMAX)
        repl_error(S, E_IQ);

    S->usng.fmt_len = s.len;
    memcpy(S->usng.fmt, at(s.ptr), s.len);
    S->usng.pos = 0;
}

/* us_numf: the numeric field at b in us.f; false none. */
static bool num_field(Session *const S, const uint32_t b)
{
    UsngField *const f = &S->usng.f;
    *f = (UsngField){};

    uint32_t d = b;
    bool digits = false;

    if (fmt_at(S, d) == '+' && d + 1 < S->usng.fmt_len) {
        f->plus_lead = true;
        d++;
    }

    if (d + 1 < S->usng.fmt_len) {
        if (S->usng.fmt[d] == '*' && S->usng.fmt[d + 1] == '*') {
            f->stars = true;
            f->width += 2;
            f->before += 2;
            d += 2;

            if (fmt_at(S, d) == '$') {
                f->dollar = true;
                f->width++;
                d++;
            }
        } else if (S->usng.fmt[d] == '$' && S->usng.fmt[d + 1] == '$') {
            f->dollar = true;
            f->width += 2;
            f->before++;
            d += 2;
        }
    }

    for (;; d++) { /* # and , before the point */
        if (fmt_at(S, d) == '#')
            f->before++;
        else if (fmt_at(S, d) == ',')
            f->commas = true;
        else
            break;

        f->width++;
        digits = true;
    }

    if (fmt_at(S, d) == '.') {
        if (d + 1 < S->usng.fmt_len && S->usng.fmt[d + 1] == '#') {
            f->point = true;
            f->width++;

            for (d++; fmt_at(S, d) == '#'; d++) {
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
    if (d + 3 < S->usng.fmt_len && !memcmp(S->usng.fmt + d, "^^^^", 4)) {
        f->carets = 4;
        d += 4;

        if (fmt_at(S, d) == '^') {
            f->carets = 5;
            d++;
        }
    }

    if (!f->plus_lead && (fmt_at(S, d) == '+' || fmt_at(S, d) == '-'))
        f->trail = S->usng.fmt[d++];

    f->len = d - b;
    return true;
}

/* us_fld: the field at b: its kind, *len its length; a number's in us.f. */
static FieldKind field(Session *const S, const uint32_t b, uint32_t *const len)
{
    const uint8_t c = S->usng.fmt[b];
    *len = 1;

    if (c == '!')
        return UF_FIRST;
    if (c == '&')
        return UF_ALL;
    if (c == '\\') {
        for (uint32_t d = b + 1; d < S->usng.fmt_len; d++) {
            if (S->usng.fmt[d] == '\\') {
                *len = d + 1 - b;
                return UF_WIDE;
            }

            if (S->usng.fmt[d] != ' ')
                break;
        }

        return UF_NONE;
    }

    if (!memchr("#.+$*", c, 5) || !num_field(S, b))
        return UF_NONE;

    *len = S->usng.f.len;
    return UF_NUM;
}

/* us_lit: the literal text from us.pos to the next field written; its kind, UF_NONE the format's
   end. */
static FieldKind literal(Session *const S, uint32_t *const len)
{
    for (;;) {
        if (S->usng.pos >= S->usng.fmt_len)
            return UF_NONE;
        if (S->usng.fmt[S->usng.pos] == '_' && S->usng.pos + 1 < S->usng.fmt_len) {
            cons_putc(S, S->usng.fmt[S->usng.pos + 1]);
            S->usng.pos += 2;
            continue;
        }

        const FieldKind k = field(S, S->usng.pos, len);

        if (k)
            return k;

        cons_putc(S, S->usng.fmt[S->usng.pos++]);
    }
}

/* us_next: to the next field for an item, the text before it written; its kind, *len its
   length. Illegal function call: a format with no field. */
static FieldKind next_field(Session *const S, uint32_t *const len)
{
    const uint32_t start = S->usng.pos;

    if (!literal(S, len)) {
        if (!start)
            repl_error(S, E_IQ);

        S->usng.pos = 0;

        if (!literal(S, len))
            repl_error(S, E_IQ);
    }

    return field(S, S->usng.pos, len);
}

/* US.END: the text up to the next field, a new line; US.ENDN: no new line. */
void usng_use(Session *const S)
{
    uint32_t len;
    literal(S, &len);
    cons_nl(S);
}
void usng_usen(Session *const S)
{
    uint32_t len;
    literal(S, &len);
}

/* US.STR ( s -- ): to a string field. Type mismatch: a numeric field. */
void usng_uss(Session *const S)
{
    uint32_t w;
    const FieldKind k = next_field(S, &w);

    if (k == UF_NUM)
        repl_error(S, E_TM);

    S->usng.pos += w;
    const Desc s = strg_get(S, exec_popa(&S->vm));
    const uint8_t *const p = at(s.ptr);

    if (k == UF_FIRST)
        cons_putc(S, s.len ? *p : ' ');
    else if (k == UF_ALL)
        out(S, p, s.len);
    else {
        uint32_t n = s.len < w ? s.len : w;
        out(S, p, n);

        for (; n < w; n++)
            cons_putc(S, ' ');
    }
}

/* us_ldi: the INT v's digits. */
static void load_int(Session *const S, const int32_t v)
{
    S->usng.sci = false;
    S->usng.neg = v < 0;
    uint32_t u = S->usng.neg ? 0u - (uint32_t)v : (uint32_t)v;
    uint8_t t[10];
    int32_t n = 0;

    do {
        t[n++] = '0' + u % 10;
        u /= 10;
    } while (u);

    S->usng.count = S->usng.scale = n;

    for (int32_t i = 0; i < n; i++)
        S->usng.digits[i] = t[n - 1 - i];
}

/* us_ldr: v's digits to 16 places, a SINGLE's when td is 7, a DOUBLE's 16; us.sci when PRINT,
   rounding it to td digits, would give it an exponent. */
static void load_real(Session *const S, const double v, const int32_t td)
{
    S->usng.neg = v < 0;

    if (v == 0) {
        S->usng.digits[0] = '0';
        S->usng.count = S->usng.scale = 1;
        S->usng.sci = false;
        return;
    }

    uint8_t dig[20];
    int32_t dp;
    const int32_t tc = (int32_t)dubl_digits(S->usng.neg ? -v : v, 16, dig, &dp);
    memcpy(S->usng.digits, dig, tc);
    S->usng.count = tc;
    S->usng.scale = dp + 1;

    if (S->usng.count > td) { /* rounded to the type's digits */
        int32_t b = td;
        S->usng.count = td;

        if (S->usng.digits[b] >= '5') {
            for (;;) {
                if (--b < 0) { /* all 9s: 1, a place more */
                    S->usng.digits[0] = '1';
                    S->usng.count = 1;
                    S->usng.scale++;
                    break;
                }

                if (S->usng.digits[b] != '9') {
                    S->usng.digits[b]++;
                    break;
                }

                S->usng.count = b; /* carried up past the 9s */
            }
        }

        while (S->usng.count > 1 && S->usng.digits[S->usng.count - 1] == '0')
            S->usng.count--;
    }

    if (S->usng.scale > td) /* past the type's places: ni or |ni - nc| over its digits */
        S->usng.sci = true;
    else {
        const int32_t d = S->usng.scale - S->usng.count;
        S->usng.sci = (d < 0 ? -d : d) > td;
    }

    memcpy(S->usng.digits, dig, tc); /* the digits: all 16 of them */
    S->usng.count = tc;
    S->usng.scale = dp + 1;
}

/* us_norm: zeros before the digits so that every place before the point has one: ni not under
   0. */
static void pad_zeros(Session *const S)
{
    int32_t k = -S->usng.scale;

    if (k <= 0)
        return;
    if (k > NDIG)
        k = NDIG;

    memmove(S->usng.digits + k, S->usng.digits, S->usng.count);
    memset(S->usng.digits, '0', k);
    S->usng.count += k;
    S->usng.scale = 0;

    if (S->usng.count > NDIG)
        S->usng.count = NDIG;
}

/* us_round: the digits rounded, half up, to places after the point, zeros added to them. */
static void round_to(Session *const S, const int32_t places)
{
    pad_zeros(S);
    int32_t keep = places + S->usng.scale;

    if (keep > NDIG)
        keep = NDIG;
    if (keep >= S->usng.count) {
        for (; S->usng.count < keep; S->usng.count++)
            S->usng.digits[S->usng.count] = '0';

        return;
    }

    S->usng.count = keep;

    if (S->usng.digits[keep] < '5')
        return;
    for (int32_t i = keep - 1; i >= 0; i--) {
        if (S->usng.digits[i] != '9') {
            S->usng.digits[i]++;
            return;
        }

        S->usng.digits[i] = '0';
    }

    memmove(S->usng.digits + 1, S->usng.digits, S->usng.count); /* all 9s: a 1 before them */
    S->usng.digits[0] = '1';
    S->usng.count++;
    S->usng.scale++;
}

/* us_lead: the sign before the number, 0 none. */
static uint8_t lead_sign(Session *const S)
{
    return S->usng.f.plus_lead ? S->usng.neg ? '-' : '+' : 0;
}

/* us_tsign: the sign after it, 0 none. */
static uint8_t trail_sign(Session *const S)
{
    const uint8_t t = S->usng.f.trail;

    if (!t)
        return 0;

    return S->usng.neg ? '-' : t == '+' ? '+' : ' ';
}

/* us_fits: the body's n chars, its signs and $, past the field's columns? */
static bool overflows(Session *const S, uint32_t n)
{
    if (S->usng.f.plus_lead || (S->usng.neg && !S->usng.f.trail))
        n++;

    return n + S->usng.f.dollar > S->usng.f.width + S->usng.f.plus_lead;
}

/* us_fexp: the number as digits and an exponent of car carets (4: two digits, 5: three) in the
   field's places, to the body; its length. */
static uint32_t with_exponent(Session *const S, const uint32_t carets)
{
    const int32_t whole =
        S->usng.f.before ? (int32_t)S->usng.f.before - 1 : 0; /* less the sign's place */
    const int32_t places = whole + (int32_t)S->usng.f.after;
    pad_zeros(S);
    int32_t first = 0; /* the first digit not 0 */
    while (first < S->usng.count && S->usng.digits[first] == '0')
        first++;

    int32_t shift = 0;

    if (first < S->usng.count) {
        int32_t ex = S->usng.scale - first;
        memmove(S->usng.digits, S->usng.digits + first, S->usng.count - first);
        S->usng.count -= first;
        S->usng.scale = 0;
        round_to(S, places);

        if (S->usng.scale) {
            ex++;
            S->usng.scale = 0;
        }

        shift = ex - whole;
    } else
        memset(S->usng.digits, '0', places); /* 0: zeros in every place */
    uint8_t *d = S->usng.body;
    *d++ = S->usng.neg ? '-' : !whole && S->usng.f.point ? '0' : ' ';

    for (int32_t i = 0; i < places; i++) {
        if (i == whole && S->usng.f.point)
            *d++ = '.';

        uint8_t c = S->usng.digits[i];

        if (i < whole - 1 && c == '0' && first == S->usng.count) /* a lead 0 of 0 a space */
            c = ' ';

        *d++ = c;
    }

    if (whole == places && S->usng.f.point)
        *d++ = '.';

    *d++ = 'E';
    *d++ = shift < 0 ? '-' : '+';
    const uint32_t e = shift < 0 ? 0u - (uint32_t)shift : (uint32_t)shift;

    if (carets == 5)
        *d++ = '0' + e / 100 % 10;

    *d++ = '0' + e % 100 / 10;
    *d++ = '0' + e % 10;
    return (uint32_t)(d - S->usng.body);
}

/* us_wnum: the body's n chars in cols columns: right-aligned, filled with spaces or *s, the signs
   and $ around them; a % first when past them. */
static void write_number(Session *const S, const uint32_t n, const uint32_t cols)
{
    uint8_t *d = S->usng.line;
    const uint8_t before = lead_sign(S);
    const uint8_t after = trail_sign(S);
    const bool minus = S->usng.neg && !before && !after;
    const uint32_t need = n + (before || minus) + S->usng.f.dollar;

    if (need > cols)
        *d++ = '%';
    else
        for (uint32_t i = cols - need; i; i--)
            *d++ = S->usng.f.stars ? '*' : ' ';
    if (before)
        *d++ = before;
    if (minus)
        *d++ = '-';
    if (S->usng.f.dollar)
        *d++ = '$';

    memcpy(d, S->usng.body, n);
    d += n;

    if (after)
        *d++ = after;

    out(S, S->usng.line, (uint32_t)(d - S->usng.line));
}

/* us_plain: a number without an exponent, written. */
static void write_plain(Session *const S)
{
    round_to(S, (int32_t)S->usng.f.after);
    uint8_t *d = S->usng.body; /* the digits before the point, commas */
    int32_t b = 0;

    while (b < S->usng.scale && S->usng.digits[b] == '0')
        b++;

    const int32_t n = S->usng.scale - b;

    if (!n) {
        if (S->usng.f.before)
            *d++ = '0';
    } else
        for (int32_t k = 0; k < n; k++) {
            if (S->usng.f.commas && k && (n - k) % 3 == 0)
                *d++ = ',';

            *d++ = S->usng.digits[b + k];
        }

    if (S->usng.f.point) { /* the point, the places after it */
        *d++ = '.';
        memcpy(d, S->usng.digits + S->usng.scale, S->usng.f.after);
        d += S->usng.f.after;
    }

    const uint32_t len = (uint32_t)(d - S->usng.body);

    if (S->usng.sci && overflows(S, len)) { /* past the field: an exponent */
        cons_putc(S, '%');
        out(S, S->usng.body, with_exponent(S, 4));
        return;
    }

    write_number(S, len, S->usng.f.width + S->usng.f.plus_lead);
}

/* A number to a numeric field. Type mismatch: a string field. */
static void number(Session *const S)
{
    uint32_t w;

    if (next_field(S, &w) != UF_NUM)
        repl_error(S, E_TM);

    S->usng.pos += w;

    if (S->usng.f.carets) {
        uint32_t n = with_exponent(S, S->usng.f.carets);
        const uint8_t t = trail_sign(S); /* us_trail */
        if (t)
            S->usng.body[n++] = t;

        out(S, S->usng.body, n);
    } else
        write_plain(S);

    S->vm.sp++;
}

/* US.INT ( i -- ), US.FLT ( f -- ), US.DBL ( d -- ). */
void usng_usi(Session *const S)
{
    load_int(S, S->vm.sp->i);
    number(S);
}
void usng_usf(Session *const S)
{
    load_real(S, S->vm.sp->f, 7);
    number(S);
}
void usng_usd(Session *const S)
{
    load_real(S, S->vm.sp->d, 16);
    number(S);
}
