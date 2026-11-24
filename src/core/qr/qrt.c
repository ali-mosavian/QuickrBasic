/* Quickr's run-time routines: the ops and CALL routines its code needs beyond QBasic's.
 *
 * # How it works
 * The op table is full, so Quickr's own are CALL routines, their operands on the stack. QJ jumps
 * within a line's code. Unlike a line's jump it keeps the string temporaries, which an
 * expression's `and`, `or`, `a if c else b` and chained comparison hold across it. QSHL and QSHR
 * shift an int; a count past 0 .. 31 is Illegal function call.
 *
 * print and string() spell a value with qrt_text, no space before a number: 1 2.5 x true.
 *
 * QX is the rest, by its operand. A list grows and shrinks by REDIM PRESERVE (arry_redim), one
 * element at a time; insert and remove move an element by swaps, which keep its strings' and
 * arrays' owners. Either may move the heap, the list's cell with it, so each routine takes the
 * cell afresh. An f-string's spec is parsed when it runs, as Python's format() does.
 *
 * # Example
 *    a and b     a, I.LD.I32 n|FALSE, CALL QJ, b, I.LD.I32 m|ALWAYS, CALL QJ, I.LD.U8 0
 */
#include "qr/qrt.h"
#include "session.h"
#include "mem/array.h"
#include "num/double.h"

enum { SNGL_DIGITS = 7, DBL_DIGITS = 15, EXP_FROM = -4 };

/* ---- jumps and shifts */

/* QJ ( [flag] how -- ): ip moved by how's offset, always or by the flag. */
void qrt_jump(Session *const S)
{
    const int32_t how = exec_popi(&S->vm);
    const QrtWhen when = (QrtWhen)(how & ((1 << QJ_SHIFT) - 1));
    const bool go = when == QJ_ALWAYS || !exec_popi(&S->vm) == (when == QJ_FALSE);

    if (go)
        S->vm.ip += how >> QJ_SHIFT;
}

/* A shift's count: 0 .. 31, else Illegal function call. */
static uint32_t qrt_count(Session *const S)
{
    const uint32_t n = (uint32_t)exec_popi(&S->vm);

    if (n > 31)
        fault_error(&S->fault, E_IQ);

    return n;
}

void qrt_shl(Session *const S)
{
    const uint32_t n = qrt_count(S);
    S->vm.sp->u <<= n;
}

void qrt_shr(Session *const S)
{
    const uint32_t n = qrt_count(S);
    Cell *const x = S->vm.sp;
    x->u = x->i < 0 ? ~(~x->u >> n) : x->u >> n;
}

/* ---- values as text */

static uint32_t qrt_put(uint8_t *const out, const char *const s)
{
    const uint32_t n = (uint32_t)strlen(s);
    memcpy(out, s, n);
    return n;
}

/* u's decimal digits at out; their count. */
static uint32_t qrt_decimal(uint32_t u, uint8_t *const out)
{
    uint8_t rev[10];
    uint32_t n = 0;

    do {
        rev[n++] = (uint8_t)('0' + u % 10);
        u /= 10;
    } while (u);

    for (uint32_t i = 0; i < n; i++)
        out[i] = rev[n - 1 - i];

    return n;
}

/* v, finite and over 0, to ndig significant digits: positional from 10^-4 to 10^ndig, else
   d.ddde+XX. */
static uint32_t qrt_real(const double v, const uint32_t ndig, uint8_t *const out)
{
    uint8_t dig[DBL_DIGITS + 1];
    int32_t dp;
    const uint32_t n = dubl_digits(v, ndig, dig, &dp);
    uint8_t *o = out;

    if (dp < EXP_FROM || dp >= (int32_t)ndig) {
        *o++ = dig[0];

        if (n > 1) {
            *o++ = '.';

            for (uint32_t i = 1; i < n; i++)
                *o++ = dig[i];
        }

        *o++ = 'e';
        *o++ = dp < 0 ? '-' : '+';
        const uint32_t e = (uint32_t)(dp < 0 ? -dp : dp);

        if (e < 10)
            *o++ = '0';

        o += qrt_decimal(e, o);
        return (uint32_t)(o - out);
    }

    if (dp < 0) {
        *o++ = '0';
        *o++ = '.';

        for (int32_t z = -dp - 1; z > 0; z--)
            *o++ = '0';
        for (uint32_t i = 0; i < n; i++)
            *o++ = dig[i];

        return (uint32_t)(o - out);
    }

    for (int32_t i = 0; i <= dp; i++)
        *o++ = (uint32_t)i < n ? dig[i] : '0';

    *o++ = '.';

    if ((uint32_t)dp + 1 >= n)
        *o++ = '0';

    for (uint32_t i = (uint32_t)dp + 1; i < n; i++)
        *o++ = dig[i];

    return (uint32_t)(o - out);
}

/* A float's text: its sign, then 0.0, inf, nan or its digits. */
static uint32_t qrt_float(const double v, const uint32_t ndig, uint8_t *const out)
{
    const uint64_t bits = (Cell){.d = v}.raw;

    const uint64_t mag = bits & ~(1ull << 63);
    const bool nan = mag > 0x7FF0000000000000ull;
    uint32_t n = 0;

    if (bits >> 63 && !nan)
        out[n++] = '-';
    if (nan)
        return qrt_put(out, "nan");
    if (mag == 0x7FF0000000000000ull)
        return n + qrt_put(out + n, "inf");
    if (!mag)
        return n + qrt_put(out + n, "0.0");

    return n + qrt_real((Cell){.raw = mag}.d, ndig, out + n);
}

uint32_t qrt_text(const QrtKind kind, const Cell v, uint8_t out[static QRT_TXT])
{
    switch (kind) {
    case QRT_INT: {
        const uint32_t neg = v.i < 0;
        out[0] = '-';
        return neg + qrt_decimal(neg ? 0u - v.u : v.u, out + neg);
    }

    case QRT_BOOL:
        return qrt_put(out, v.i ? "true" : "false");

    case QRT_SINGLE:
        return qrt_float((double)v.f, SNGL_DIGITS, out);

    default:
        return qrt_float(v.d, DBL_DIGITS, out);
    }
}

/* ---- print and string() */

/* QPR ( [v] kind -- ): v printed, or a space. */
void qrt_print(Session *const S)
{
    const QrtKind kind = (QrtKind)exec_popi(&S->vm);

    if (kind == QRT_SPACE) {
        cons_space(&S->cons);
        return;
    }

    if (kind == QRT_STRING) {
        cons_putdesc(&S->cons, strg_get(&S->strg, exec_popa(&S->vm)));
        return;
    }

    uint8_t t[QRT_TXT];
    const uint32_t n = qrt_text(kind, exec_pop(&S->vm), t);

    for (uint32_t i = 0; i < n; i++)
        cons_putc(&S->cons, t[i]);
}

/* QSTR ( v kind -- s ): a number's or a bool's text. */
void qrt_str(Session *const S)
{
    const QrtKind kind = (QrtKind)exec_popi(&S->vm);
    const uint32_t n =
        qrt_text(kind, exec_pop(&S->vm), S->cons.sbuf); /* strg_text's in the arena */
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, (uint16_t)n));
}

/* ---- lists */

/* The list at cell: its elements' count; *data their first's address. */
static uint32_t qrt_len(const Addr cell, Addr *const data)
{
    const ArrSlot *const s = (const ArrSlot *)at(cell);

    if (!s->hdr) {
        *data = 0;
        return 0;
    }

    const ArrView v = arry_view(s->hdr);
    *data = v.data;
    return arry_view_count(&v, 0);
}

/* The list at cell made n long, its elements kept (REDIM PRESERVE); none, freed. The cell may
   move with the heap. */
static void qrt_resize(Session *const S, const Addr cell, const uint32_t et, const uint32_t n)
{
    ArrSlot *const s = (ArrSlot *)at(cell);

    if (!n) {
        arry_free(&S->arry, &S->vm, s);
        return;
    }

    S->arry.subs[0] = (int32_t)n - 1;
    S->arry.lbounds[0] = 0;
    arry_redim(&S->arry, s, et, 1, s->hdr != 0);
}

/* Elements a and b of et swapped, a string's and a record's owners following. */
static void qrt_swap(Session *const S, const uint32_t et, const Addr a, const Addr b)
{
    if (et >= ET_REC) {
        arry_swap_rec(&S->arry, a, b, decl_type(&S->decl, et - ET_REC));
        return;
    }

    const uint32_t size = arry_et_size(&S->decl, et);
    uint8_t t[8];
    memcpy(t, at(a), size);
    memcpy(at(a), at(b), size);
    memcpy(at(b), t, size);

    if (et == ET_SD) { /* in place: the blocks' owners follow */
        strg_move(&S->strg, a, a);
        strg_move(&S->strg, b, b);
    }
}

/* Element from moved to to, one swap at a time. */
static void qrt_bubble(
    Session *const S,
    const Addr cell,
    const uint32_t et,
    const int32_t from,
    const int32_t to
)
{
    Addr data;
    const uint32_t n = qrt_len(cell, &data);
    const uint32_t size = arry_et_size(&S->decl, et);

    if ((uint32_t)to >= n || (uint32_t)from >= n)
        arry_bad_sub(&S->arry);

    const int32_t step = to > from ? 1 : -1;

    for (int32_t k = from; k != to; k += step)
        qrt_swap(S, et, data + (uint32_t)k * size, data + (uint32_t)(k + step) * size);
}

/* A list's last element's value pushed: a number, or a new string of its text. */
static void qrt_pop(Session *const S, const Addr cell, const uint32_t et)
{
    Addr data;
    const uint32_t n = qrt_len(cell, &data);

    if (!n)
        arry_bad_sub(&S->arry);

    const Addr e = data + (n - 1) * arry_et_size(&S->decl, et);

    if (et != ET_SD) {
        const Cell v = et == ET_R8 ? load_cell(at(e)) : (Cell){.u = load32(at(e))};

        qrt_resize(S, cell, et, n - 1);
        exec_push(&S->vm, v);
        return;
    }

    const Desc d = strg_peek(e);
    memcpy(S->cons.sbuf, at(d.ptr), d.len); /* copied out: the shrink frees its block */
    qrt_resize(S, cell, et, n - 1);
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, d.len));
}

/* Is v among the list's elements of et? */
static bool qrt_has(
    Session *const S,
    const Addr cell,
    const uint32_t et,
    const Cell v,
    const Desc str
)
{
    Addr data;
    const uint32_t n = qrt_len(cell, &data);
    const uint32_t size = arry_et_size(&S->decl, et);

    for (uint32_t k = 0; k < n; k++) {
        const Addr e = data + k * size;
        bool same;

        if (et == ET_SD) {
            const Desc d = strg_peek(e);
            same = d.len == str.len && !memcmp(at(d.ptr), at(str.ptr), d.len);
        } else if (et == ET_R8)
            same = load_cell(at(e)).d == v.d;
        else if (et == ET_R4)
            same = (Cell){.u = load32(at(e))}.f == v.f;
        else
            same = load32(at(e)) == v.u;

        if (same)
            return true;
    }

    return false;
}

/* ---- f-strings: Python's format spec */

typedef struct {
    char fill, align, sign, type;
    bool alt, zero;
    char group;
    int32_t width, prec; /* -1 none */
} QrtSpec;

static QrtSpec qrt_spec(const uint8_t *p, const uint8_t *const end)
{
    QrtSpec f = {.fill = ' ', .width = -1, .prec = -1};

    if (end - p >= 2 && strchr("<>^=", p[1])) {
        f.fill = (char)p[0];
        f.align = (char)p[1];
        p += 2;
    } else if (p < end && strchr("<>^=", *p))
        f.align = (char)*p++;

    if (p < end && strchr("+- ", *p))
        f.sign = (char)*p++;
    if (p < end && *p == '#') {
        f.alt = true;
        p++;
    }

    if (p < end && *p == '0') {
        f.zero = true;
        p++;
    }

    for (; p < end && *p >= '0' && *p <= '9'; p++)
        f.width = (f.width < 0 ? 0 : f.width * 10) + (*p - '0');

    if (p < end && (*p == ',' || *p == '_'))
        f.group = (char)*p++;

    if (p < end && *p == '.')
        for (f.prec = 0, p++; p < end && *p >= '0' && *p <= '9'; p++)
            f.prec = f.prec * 10 + (*p - '0');

    if (p < end)
        f.type = (char)*p;

    if (f.zero && !f.align) {
        f.fill = '0';
        f.align = '=';
    }

    return f;
}

/* digits' runs of three from the right joined by g, in place in the buffer of n at b. */
static uint32_t qrt_group(char *const b, const uint32_t n, const char g)
{
    uint32_t int_end = 0;

    while (int_end < n && b[int_end] >= '0' && b[int_end] <= '9')
        int_end++;

    const uint32_t commas = int_end ? (int_end - 1) / 3 : 0;

    if (!commas)
        return n;

    memmove(b + int_end + commas, b + int_end, n - int_end);

    for (uint32_t i = int_end, o = int_end + commas, run = 0; i-- > 0;) {
        b[--o] = b[i];

        if (++run == 3 && i) {
            b[--o] = g;
            run = 0;
        }
    }

    return n + commas;
}

/* u in base 2, 8, 10 or 16 to out; its length. */
static uint32_t qrt_radix(uint32_t u, const uint32_t base, const bool upper, char *const out)
{
    const char *const digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char rev[32];
    uint32_t n = 0;

    do {
        rev[n++] = digits[u % base];
        u /= base;
    } while (u);

    for (uint32_t i = 0; i < n; i++)
        out[i] = rev[n - 1 - i];

    return n;
}

enum { QRT_SIG = 17, QRT_PREC = 100 }; /* a double's digits known; a precision at most */

/* m, finite and not below 0, to n significant digits, zeros kept: dig, and the first's power of
   ten. */
static void qrt_sig(const double m, const uint32_t n, uint8_t *const dig, int32_t *const dp)
{
    memset(dig, '0', n);
    *dp = 0;

    if (m > 0)
        dubl_digits(m, n < QRT_SIG ? n : QRT_SIG, dig, dp);
}

/* dig[0..n) (its first's power dp) with prec digits after the point, zeros past n. */
static uint32_t qrt_fixed(
    const uint8_t *const dig,
    const uint32_t n,
    const int32_t dp,
    const int32_t prec,
    const bool point,
    char *const out
)
{
    uint32_t o = 0;

    for (int32_t p = dp > 0 ? dp : 0; p >= -prec; p--) {
        const int32_t i = dp - p;
        out[o++] = i >= 0 && (uint32_t)i < n ? (char)dig[i] : '0';

        if (!p && (prec || point))
            out[o++] = '.';
    }

    return o;
}

/* dig[0..n) as d.ddde+XX, n - 1 digits after the point. */
static uint32_t qrt_expo(
    const uint8_t *const dig,
    const uint32_t n,
    const int32_t dp,
    const bool point,
    const char e,
    char *const out
)
{
    uint32_t o = 0;
    out[o++] = (char)dig[0];

    if (n > 1 || point)
        out[o++] = '.';

    for (uint32_t i = 1; i < n; i++)
        out[o++] = (char)dig[i];

    out[o++] = e;
    out[o++] = dp < 0 ? '-' : '+';
    const uint32_t x = (uint32_t)(dp < 0 ? -dp : dp);

    if (x < 10)
        out[o++] = '0';

    return o + qrt_radix(x, 10, false, out + o);
}

/* m, finite and not below 0, by type e, f or g and its precision (Python's, # alt). */
static uint32_t qrt_real_as(
    const double m,
    const char type,
    const int32_t prec,
    const bool alt,
    char *const out
)
{
    uint8_t dig[QRT_PREC + 400];
    int32_t dp;
    const char t = (char)(type | 0x20);
    const char e = type == t ? 'e' : 'E';

    if (t == 'e') {
        qrt_sig(m, (uint32_t)prec + 1, dig, &dp);
        return qrt_expo(dig, (uint32_t)prec + 1, dp, alt, e, out);
    }

    if (t == 'f') {
        qrt_sig(m, QRT_SIG, dig, &dp);
        const int32_t n = dp + 1 + prec;

        if (n < 1) { /* below the last place shown: 0, or its 1 when over half of it */
            bool up = n == 0 && dig[0] > '5';

            for (uint32_t i = 1; n == 0 && dig[0] == '5' && i < QRT_SIG && !up; i++)
                up = dig[i] != '0';

            const uint8_t one = '1';
            return qrt_fixed(&one, up, -prec, prec, alt, out);
        }

        qrt_sig(m, (uint32_t)n, dig, &dp);
        return qrt_fixed(dig, (uint32_t)n, dp, prec, alt, out);
    }

    const int32_t p = prec ? prec : 1; /* g */
    qrt_sig(m, (uint32_t)p, dig, &dp);
    uint32_t n = (uint32_t)p;

    while (!alt && n > 1 && dig[n - 1] == '0')
        n--;

    if (dp < -4 || dp >= p)
        return qrt_expo(dig, n, dp, alt, e, out);

    const int32_t after = alt ? p - 1 - dp : (int32_t)n - 1 - dp;
    return qrt_fixed(dig, n, dp, after > 0 ? after : 0, alt, out);
}

/* A number's digits, no sign, by the spec's type: to body; its length. */
static uint32_t qrt_digits(
    const QrtSpec *const f,
    const QrtKind kind,
    const Cell v,
    char *const body
)
{
    const char t = f->type;

    if (kind == QRT_INT && (!t || strchr("bcdoxXn", t))) {
        const uint32_t u = v.i < 0 ? 0u - v.u : v.u;

        if (t == 'c') {
            body[0] = (char)u;
            return 1;
        }

        const uint32_t base = t == 'b' ? 2 : t == 'o' ? 8 : t == 'x' || t == 'X' ? 16 : 10;
        return qrt_radix(u, base, t == 'X', body);
    }

    const double d = kind == QRT_INT ? (double)v.i : kind == QRT_SINGLE ? (double)v.f : v.d;
    const double m = d < 0 ? -d : d;
    const bool upper = t && t != '%' && t != 'n' && !(t & 0x20);

    if (m != m || m > 1.7976931348623157e308) {
        const char *const s = m != m ? (upper ? "NAN" : "nan") : upper ? "INF" : "inf";
        memcpy(body, s, 3);
        return 3;
    }

    if (!t && f->prec < 0) { /* as print spells it */
        const Cell mag = kind == QRT_SINGLE ? (Cell){.f = (float)m} : (Cell){.d = m};

        return qrt_text(kind == QRT_SINGLE ? QRT_SINGLE : QRT_DOUBLE, mag, (uint8_t *)body);
    }

    const int32_t prec = f->prec < 0 ? 6 : f->prec > QRT_PREC ? QRT_PREC : f->prec;
    const char type = !t || t == 'n' ? 'g' : t == '%' ? 'f' : t;
    return qrt_real_as(t == '%' ? m * 100 : m, type, prec, f->alt, body);
}

uint32_t qrt_format(
    const QrtKind kind,
    const Cell v,
    const Desc str,
    const uint8_t *const spec,
    const uint32_t n,
    uint8_t *const out
)
{
    const QrtSpec f = qrt_spec(spec, spec + n);
    char body[QRT_PREC + 512];
    uint32_t len;
    const char *pre = "";
    bool neg = false;

    if (kind == QRT_STRING || kind == QRT_BOOL) {
        const uint8_t *const text =
            kind == QRT_BOOL ? (const uint8_t *)(v.i ? "true" : "false") : at(str.ptr);
        len = kind == QRT_BOOL ? (v.i ? 4 : 5) : str.len;

        if (f.prec >= 0 && (uint32_t)f.prec < len)
            len = (uint32_t)f.prec;

        const uint32_t pad = f.width > (int32_t)len ? (uint32_t)f.width - len : 0;
        const char al = f.align ? f.align : '<';
        const uint32_t left = al == '>' ? pad : al == '^' ? pad / 2 : 0;

        memset(out, f.fill, left);
        memcpy(out + left, text, len);
        memset(out + left + len, f.fill, pad - left);
        return len + pad;
    }

    neg = kind == QRT_INT ? v.i < 0 : kind == QRT_SINGLE ? v.f < 0 : v.d < 0;
    len = qrt_digits(&f, kind, v, body);

    if (f.type == '%')
        body[len++] = '%';
    if (f.group)
        len = qrt_group(body, len, f.group);
    if (f.alt && kind == QRT_INT && f.type && strchr("boxX", f.type))
        pre = f.type == 'b' ? "0b" : f.type == 'o' ? "0o" : f.type == 'x' ? "0x" : "0X";

    char sign[4] = {0};

    if (neg)
        sign[0] = '-';
    else if (f.sign == '+' || f.sign == ' ')
        sign[0] = f.sign;

    const uint32_t ns = (uint32_t)strlen(sign), np = (uint32_t)strlen(pre);
    const uint32_t all = ns + np + len;
    const uint32_t pad = f.width > (int32_t)all ? (uint32_t)f.width - all : 0;
    const char al = f.align ? f.align : '>';
    const uint32_t left = al == '>' ? pad : al == '^' ? pad / 2 : 0;
    uint32_t o = 0;

    if (al != '=') {
        memset(out, f.fill, left);
        o = left;
    }

    memcpy(out + o, sign, ns);
    memcpy(out + o + ns, pre, np);
    o += ns + np;

    if (al == '=') {
        memset(out + o, f.fill, pad);
        o += pad;
    }

    memcpy(out + o, body, len);
    o += len;

    if (al == '<' || al == '^') {
        memset(out + o, f.fill, pad - left);
        o += pad - left;
    }

    return o;
}

/* QX ( args.. op -- results ): the routine op names (qr/qrt.h QrtX). */
void qrt_x(Session *const S)
{
    const QrtX op = (QrtX)exec_popi(&S->vm);
    Vm *const vm = &S->vm;

    switch (op) {
    case QX_LEN: {
        Addr data;
        exec_pushi(vm, (int32_t)qrt_len(exec_popa(vm), &data));
        return;
    }

    case QX_PUSH: {
        const uint32_t et = exec_pop(vm).u;
        const Addr cell = exec_popa(vm);
        Addr data;
        const uint32_t n = qrt_len(cell, &data);
        qrt_resize(S, cell, et, n + 1);
        exec_pushi(vm, (int32_t)n);
        return;
    }

    case QX_POP: {
        const uint32_t et = exec_pop(vm).u;
        qrt_pop(S, exec_popa(vm), et);
        return;
    }

    case QX_LAST: {
        Addr data;
        const Addr cell = exec_popa(vm);
        const uint32_t n = qrt_len(cell, &data);

        if (!n)
            arry_bad_sub(&S->arry);

        const ArrSlot *const s = (const ArrSlot *)at(cell);
        exec_pusha(vm, data + (n - 1) * arry_et_size(&S->decl, arry_view(s->hdr).et));
        return;
    }

    case QX_DROP: {
        const uint32_t et = exec_pop(vm).u;
        const Addr cell = exec_popa(vm);
        Addr data;
        const uint32_t n = qrt_len(cell, &data);

        if (!n)
            arry_bad_sub(&S->arry);

        qrt_resize(S, cell, et, n - 1);
        return;
    }

    case QX_SINK:
    case QX_RAISE: {
        const int32_t i = exec_popi(vm);
        const uint32_t et = exec_pop(vm).u;
        const Addr cell = exec_popa(vm);
        Addr data;
        const int32_t last = (int32_t)qrt_len(cell, &data) - 1;

        if (op == QX_SINK)
            qrt_bubble(S, cell, et, last, i);
        else
            qrt_bubble(S, cell, et, i, last);

        return;
    }

    case QX_HAS: {
        const Cell v = exec_pop(vm);
        const uint32_t et = exec_pop(vm).u;
        const Addr cell = exec_popa(vm);
        const Desc str = et == ET_SD ? strg_get(&S->strg, v.u) : (Desc){};

        exec_pushi(vm, qrt_has(S, cell, et, v, str) ? -1 : 0);
        return;
    }

    case QX_FMT: {
        const QrtKind kind = (QrtKind)exec_popi(vm);
        const Desc spec = strg_get(&S->strg, exec_popa(vm));
        const Cell v = exec_pop(vm);
        const Desc str = kind == QRT_STRING ? strg_get(&S->strg, v.u) : (Desc){};

        uint8_t sp[64];
        const uint32_t ns = spec.len < sizeof sp ? spec.len : sizeof sp;
        memcpy(sp, at(spec.ptr), ns);
        const uint32_t n = qrt_format(kind, v, str, sp, ns, S->cons.sbuf);
        exec_pusha(vm, strg_text(&S->strg, S->cons.sbuf, (uint16_t)n));
        return;
    }
    }
}
