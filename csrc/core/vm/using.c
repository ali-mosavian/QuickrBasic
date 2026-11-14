/* PRINT USING, as llrm's runtime/qb/using.c.
 *
 * # How it works
 * US.BEGIN takes the format (usng_begin); each item goes to the format's next field (usng_int,
 * usng_real, usng_string); US.END writes the text up to the next field (usng_end). The format
 * starts again past its last field. Text goes to a UsngOut; errors through Fault. The VM's
 * handlers (stmt_us*, vm/stmts.c) pop the operands and call these.
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
 * | sc_usb, sc_usi, sc_usf, sc_usd, sc_uss, sc_use, sc_usen | usng_begin, usng_int, usng_real, usng_string, usng_end | D1 |
 * | TUSF | UsngField (using.h) | MIRROR |
 * | us_*, g_ufmt .. g_utc, u_numc | usng_* statics; Usng (using.h) | MIRROR |
 *
 * - D1. The operands come as parameters; the VM's handlers (stmt_usb .. stmt_usen) pop them.
 */
#include "vm/using.h"
#include <string.h>
#include "num/double.h"

/* The format's char at d, 0 past it. */
static uint8_t usng_fmt_at(Usng *const u, const uint32_t d)
{
    return d < u->fmt_len ? u->fmt[d] : 0;
}

static void usng_out(Usng *const u, const uint8_t *const p, const uint32_t n)
{
    u->out->put(u->out, p, n);
}

static void usng_putc(Usng *const u, const uint8_t c) { usng_out(u, &c, 1); }

[[noreturn]]
static void usng_fail(const Usng *const u, const uint8_t code)
{
    fault_error(u->fault, code);
}

void usng_init(Usng *const u, Fault *const f, UsngOut *const out)
{
    u->fault = f;
    u->out = out;
}

/* US.BEGIN: the format. Illegal function call: over 255 characters. */
void usng_begin(Usng *const u, const uint8_t *const fmt, const uint32_t len)
{
    if (len > FMTMAX)
        usng_fail(u, E_IQ);

    u->fmt_len = len;
    memcpy(u->fmt, fmt, len);
    u->pos = 0;
}

/* us_numf: the numeric field at b in us.f; false none. */
static bool usng_num_field(Usng *const u, const uint32_t b)
{
    UsngField *const f = &u->f;
    *f = (UsngField){};

    uint32_t d = b;
    bool digits = false;

    if (usng_fmt_at(u, d) == '+' && d + 1 < u->fmt_len) {
        f->plus_lead = true;
        d++;
    }

    if (d + 1 < u->fmt_len) {
        if (u->fmt[d] == '*' && u->fmt[d + 1] == '*') {
            f->stars = true;
            f->width += 2;
            f->before += 2;
            d += 2;

            if (usng_fmt_at(u, d) == '$') {
                f->dollar = true;
                f->width++;
                d++;
            }
        } else if (u->fmt[d] == '$' && u->fmt[d + 1] == '$') {
            f->dollar = true;
            f->width += 2;
            f->before++;
            d += 2;
        }
    }

    for (;; d++) { /* # and , before the point */
        if (usng_fmt_at(u, d) == '#')
            f->before++;
        else if (usng_fmt_at(u, d) == ',')
            f->commas = true;
        else
            break;

        f->width++;
        digits = true;
    }

    if (usng_fmt_at(u, d) == '.') {
        if (d + 1 < u->fmt_len && u->fmt[d + 1] == '#') {
            f->point = true;
            f->width++;

            for (d++; usng_fmt_at(u, d) == '#'; d++) {
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
    if (d + 3 < u->fmt_len && !memcmp(u->fmt + d, "^^^^", 4)) {
        f->carets = 4;
        d += 4;

        if (usng_fmt_at(u, d) == '^') {
            f->carets = 5;
            d++;
        }
    }

    if (!f->plus_lead && (usng_fmt_at(u, d) == '+' || usng_fmt_at(u, d) == '-'))
        f->trail = u->fmt[d++];

    f->len = d - b;
    return true;
}

/* us_fld: the field at b: its kind, *len its length; a number's in us.f. */
static FieldKind usng_field(Usng *const u, const uint32_t b, uint32_t *const len)
{
    const uint8_t c = u->fmt[b];
    *len = 1;

    if (c == '!')
        return UF_FIRST;
    if (c == '&')
        return UF_ALL;
    if (c == '\\') {
        for (uint32_t d = b + 1; d < u->fmt_len; d++) {
            if (u->fmt[d] == '\\') {
                *len = d + 1 - b;
                return UF_WIDE;
            }

            if (u->fmt[d] != ' ')
                break;
        }

        return UF_NONE;
    }

    if (!memchr("#.+$*", c, 5) || !usng_num_field(u, b))
        return UF_NONE;

    *len = u->f.len;
    return UF_NUM;
}

/* us_lit: the literal text from us.pos to the next field written; its kind, UF_NONE the format's
   end. */
static FieldKind usng_literal(Usng *const u, uint32_t *const len)
{
    for (;;) {
        if (u->pos >= u->fmt_len)
            return UF_NONE;
        if (u->fmt[u->pos] == '_' && u->pos + 1 < u->fmt_len) {
            usng_putc(u, u->fmt[u->pos + 1]);
            u->pos += 2;
            continue;
        }

        const FieldKind k = usng_field(u, u->pos, len);

        if (k)
            return k;

        usng_putc(u, u->fmt[u->pos++]);
    }
}

/* us_next: to the next field for an item, the text before it written; its kind, *len its
   length. Illegal function call: a format with no field. */
static FieldKind usng_next_field(Usng *const u, uint32_t *const len)
{
    const uint32_t start = u->pos;

    if (!usng_literal(u, len)) {
        if (!start)
            usng_fail(u, E_IQ);

        u->pos = 0;

        if (!usng_literal(u, len))
            usng_fail(u, E_IQ);
    }

    return usng_field(u, u->pos, len);
}

/* US.END: the text up to the next field, then a new line when nl. */
void usng_end(Usng *const u, const bool nl)
{
    uint32_t len;
    usng_literal(u, &len);

    if (nl)
        u->out->nl(u->out);
}

/* US.STR: s (len bytes at p) to a string field. Type mismatch: a numeric field. */
void usng_string(Usng *const u, const uint8_t *const p, const uint32_t len)
{
    uint32_t w;
    const FieldKind k = usng_next_field(u, &w);

    if (k == UF_NUM)
        usng_fail(u, E_TM);

    u->pos += w;

    if (k == UF_FIRST)
        usng_putc(u, len ? *p : ' ');
    else if (k == UF_ALL)
        usng_out(u, p, len);
    else {
        uint32_t n = len < w ? len : w;
        usng_out(u, p, n);

        for (; n < w; n++)
            usng_putc(u, ' ');
    }
}

/* us_ldi: the INT v's digits. */
static void usng_load_int(Usng *const u, const int32_t v)
{
    u->sci = false;
    u->neg = v < 0;
    uint32_t mag = u->neg ? 0u - (uint32_t)v : (uint32_t)v;
    uint8_t t[10];
    int32_t n = 0;

    do {
        t[n++] = '0' + mag % 10;
        mag /= 10;
    } while (mag);

    u->count = u->scale = n;

    for (int32_t i = 0; i < n; i++)
        u->digits[i] = t[n - 1 - i];
}

/* us_ldr: v's digits to 16 places, a SINGLE's when td is 7, a DOUBLE's 16; us.sci when PRINT,
   rounding it to td digits, would give it an exponent. */
static void usng_load_real(Usng *const u, const double v, const int32_t td)
{
    u->neg = v < 0;

    if (v == 0) {
        u->digits[0] = '0';
        u->count = u->scale = 1;
        u->sci = false;
        return;
    }

    uint8_t dig[20];
    int32_t dp;
    const int32_t tc = (int32_t)dubl_digits(u->neg ? -v : v, 16, dig, &dp);
    memcpy(u->digits, dig, tc);
    u->count = tc;
    u->scale = dp + 1;

    if (u->count > td) { /* rounded to the type's digits */
        int32_t b = td;
        u->count = td;

        if (u->digits[b] >= '5') {
            for (;;) {
                if (--b < 0) { /* all 9s: 1, a place more */
                    u->digits[0] = '1';
                    u->count = 1;
                    u->scale++;
                    break;
                }

                if (u->digits[b] != '9') {
                    u->digits[b]++;
                    break;
                }

                u->count = b; /* carried up past the 9s */
            }
        }

        while (u->count > 1 && u->digits[u->count - 1] == '0')
            u->count--;
    }

    if (u->scale > td) /* past the type's places: ni or |ni - nc| over its digits */
        u->sci = true;
    else {
        const int32_t d = u->scale - u->count;
        u->sci = (d < 0 ? -d : d) > td;
    }

    memcpy(u->digits, dig, tc); /* the digits: all 16 of them */
    u->count = tc;
    u->scale = dp + 1;
}

/* us_norm: zeros before the digits so that every place before the point has one: ni not under
   0. */
static void usng_pad_zeros(Usng *const u)
{
    int32_t k = -u->scale;

    if (k <= 0)
        return;
    if (k > NDIG)
        k = NDIG;

    memmove(u->digits + k, u->digits, u->count);
    memset(u->digits, '0', k);
    u->count += k;
    u->scale = 0;

    if (u->count > NDIG)
        u->count = NDIG;
}

/* us_round: the digits rounded, half up, to places after the point, zeros added to them. */
static void usng_round_to(Usng *const u, const int32_t places)
{
    usng_pad_zeros(u);
    int32_t keep = places + u->scale;

    if (keep > NDIG)
        keep = NDIG;
    if (keep >= u->count) {
        for (; u->count < keep; u->count++)
            u->digits[u->count] = '0';

        return;
    }

    u->count = keep;

    if (u->digits[keep] < '5')
        return;
    for (int32_t i = keep - 1; i >= 0; i--) {
        if (u->digits[i] != '9') {
            u->digits[i]++;
            return;
        }

        u->digits[i] = '0';
    }

    memmove(u->digits + 1, u->digits, u->count); /* all 9s: a 1 before them */
    u->digits[0] = '1';
    u->count++;
    u->scale++;
}

/* us_lead: the sign before the number, 0 none. */
static uint8_t usng_lead_sign(Usng *const u) { return u->f.plus_lead ? u->neg ? '-' : '+' : 0; }

/* us_tsign: the sign after it, 0 none. */
static uint8_t usng_trail_sign(Usng *const u)
{
    const uint8_t t = u->f.trail;

    if (!t)
        return 0;

    return u->neg ? '-' : t == '+' ? '+' : ' ';
}

/* us_fits: the body's n chars, its signs and $, past the field's columns? */
static bool usng_overflows(Usng *const u, uint32_t n)
{
    if (u->f.plus_lead || (u->neg && !u->f.trail))
        n++;

    return n + u->f.dollar > u->f.width + u->f.plus_lead;
}

/* us_fexp: the number as digits and an exponent of car carets (4: two digits, 5: three) in the
   field's places, to the body; its length. */
static uint32_t usng_with_exponent(Usng *const u, const uint32_t carets)
{
    const int32_t whole = u->f.before ? (int32_t)u->f.before - 1 : 0; /* less the sign's place */
    const int32_t places = whole + (int32_t)u->f.after;
    usng_pad_zeros(u);
    int32_t first = 0; /* the first digit not 0 */
    while (first < u->count && u->digits[first] == '0')
        first++;

    int32_t shift = 0;

    if (first < u->count) {
        int32_t ex = u->scale - first;
        memmove(u->digits, u->digits + first, u->count - first);
        u->count -= first;
        u->scale = 0;
        usng_round_to(u, places);

        if (u->scale) {
            ex++;
            u->scale = 0;
        }

        shift = ex - whole;
    } else
        memset(u->digits, '0', places); /* 0: zeros in every place */
    uint8_t *d = u->body;
    *d++ = u->neg ? '-' : !whole && u->f.point ? '0' : ' ';

    for (int32_t i = 0; i < places; i++) {
        if (i == whole && u->f.point)
            *d++ = '.';

        uint8_t c = u->digits[i];

        if (i < whole - 1 && c == '0' && first == u->count) /* a lead 0 of 0 a space */
            c = ' ';

        *d++ = c;
    }

    if (whole == places && u->f.point)
        *d++ = '.';

    *d++ = 'E';
    *d++ = shift < 0 ? '-' : '+';
    const uint32_t e = shift < 0 ? 0u - (uint32_t)shift : (uint32_t)shift;

    if (carets == 5)
        *d++ = '0' + e / 100 % 10;

    *d++ = '0' + e % 100 / 10;
    *d++ = '0' + e % 10;
    return (uint32_t)(d - u->body);
}

/* us_wnum: the body's n chars in cols columns: right-aligned, filled with spaces or *s, the signs
   and $ around them; a % first when past them. */
static void usng_write_number(Usng *const u, const uint32_t n, const uint32_t cols)
{
    uint8_t *d = u->line;
    const uint8_t before = usng_lead_sign(u);
    const uint8_t after = usng_trail_sign(u);
    const bool minus = u->neg && !before && !after;
    const uint32_t need = n + (before || minus) + u->f.dollar;

    if (need > cols)
        *d++ = '%';
    else
        for (uint32_t i = cols - need; i; i--)
            *d++ = u->f.stars ? '*' : ' ';
    if (before)
        *d++ = before;
    if (minus)
        *d++ = '-';
    if (u->f.dollar)
        *d++ = '$';

    memcpy(d, u->body, n);
    d += n;

    if (after)
        *d++ = after;

    usng_out(u, u->line, (uint32_t)(d - u->line));
}

/* us_plain: a number without an exponent, written. */
static void usng_write_plain(Usng *const u)
{
    usng_round_to(u, (int32_t)u->f.after);
    uint8_t *d = u->body; /* the digits before the point, commas */
    int32_t b = 0;

    while (b < u->scale && u->digits[b] == '0')
        b++;

    const int32_t n = u->scale - b;

    if (!n) {
        if (u->f.before)
            *d++ = '0';
    } else
        for (int32_t k = 0; k < n; k++) {
            if (u->f.commas && k && (n - k) % 3 == 0)
                *d++ = ',';

            *d++ = u->digits[b + k];
        }

    if (u->f.point) { /* the point, the places after it */
        *d++ = '.';
        memcpy(d, u->digits + u->scale, u->f.after);
        d += u->f.after;
    }

    const uint32_t len = (uint32_t)(d - u->body);

    if (u->sci && usng_overflows(u, len)) { /* past the field: an exponent */
        usng_putc(u, '%');
        usng_out(u, u->body, usng_with_exponent(u, 4));
        return;
    }

    usng_write_number(u, len, u->f.width + u->f.plus_lead);
}

/* A number to a numeric field. Type mismatch: a string field. */
static void usng_number(Usng *const u)
{
    uint32_t w;

    if (usng_next_field(u, &w) != UF_NUM)
        usng_fail(u, E_TM);

    u->pos += w;

    if (u->f.carets) {
        uint32_t n = usng_with_exponent(u, u->f.carets);
        const uint8_t t = usng_trail_sign(u); /* us_trail */
        if (t)
            u->body[n++] = t;

        usng_out(u, u->body, n);
    } else
        usng_write_plain(u);
}

/* US.INT, US.FLT, US.DBL: v, of digits significant digits, to a numeric field. */
void usng_int(Usng *const u, const int32_t v)
{
    usng_load_int(u, v);
    usng_number(u);
}

void usng_real(Usng *const u, const double v, const int32_t digits)
{
    usng_load_real(u, v, digits);
    usng_number(u);
}
