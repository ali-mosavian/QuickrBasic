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
 * QX is the rest, by its operand. A list has room to grow (mem/array.c): a push past it doubles
 * the room (arry_relen), so a push is amortized O(1); a pop frees nothing until the list is
 * empty. Insert and remove move an element by swaps, which keep its strings' and
 * arrays' owners. Either may move the heap, the list's cell with it, so each routine takes the
 * cell afresh. A growth sweeps the dead only now and then (arry_tidy), before it, following the
 * cell. An f-string's spec is parsed when it runs, as Python's format() does.
 *
 * A fn value is its procedure's number: QX.CALL makes the call PCALL makes, into the body past
 * its first line's JMP, found from the procedure when it runs.
 *
 * A map is a record of three lists (qsem_mapof): its keys and its values in the order they came,
 * and its slots, a power of 2 of them, each an entry's index + 1 or 0 free. A key's slot is its
 * hash's, probing on; half the slots at most are in use. A remove empties its slot, moving back
 * the run after it, then moves the last entry into its place. Nothing sweeps between a map's
 * lists' changes, so its address holds till the value's is returned.
 *
 * # Example
 *    a and b     a, I.LD.I32 n|FALSE, CALL QJ, b, I.LD.I32 m|ALWAYS, CALL QJ, I.LD.U8 0
 */
#include "qr/qrt.h"
#include "session.h"
#include "comp/blocks.h"
#include "mem/array.h"
#include "num/double.h"
#include "vm/flow.h"

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
static uint32_t qrt_decimal(uint64_t u, uint8_t *const out)
{
    uint8_t rev[20];
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

/* An integer of kind in v: is it below 0; *mag its magnitude. */
static bool qrt_magnitude(const QrtKind kind, const Cell v, uint64_t *const mag)
{
    const int64_t s = kind == QRT_INT ? v.i : (int64_t)v.raw;
    const bool neg = (kind == QRT_INT || kind == QRT_LONG) && s < 0;
    *mag = kind == QRT_UINT ? v.u : kind == QRT_ULONG || !neg ? (uint64_t)s : 0 - (uint64_t)s;
    return neg;
}

/* Is kind an integer's? */
static bool qrt_integer(const QrtKind kind)
{
    return kind == QRT_INT || kind == QRT_UINT || kind == QRT_LONG || kind == QRT_ULONG;
}

uint32_t qrt_text(const QrtKind kind, const Cell v, uint8_t out[static QRT_TXT])
{
    switch (kind) {
    case QRT_INT:
    case QRT_UINT:
    case QRT_LONG:
    case QRT_ULONG: {
        uint64_t mag;
        const uint32_t neg = qrt_magnitude(kind, v, &mag);
        out[0] = '-';
        return neg + qrt_decimal(mag, out + neg);
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

/* The list at cell: its elements' count, its array's fast; *data their first's address. */
static uint32_t qrt_len(const Addr cell, Addr *const data)
{
    const ArrSlot *const s = (const ArrSlot *)at(cell);

    if (!s->hdr) {
        *data = 0;
        return 0;
    }

    const ArrayHdr *const a = (const ArrayHdr *)at(s->hdr);
    *data = a->data;
    return a->fast;
}

/* The list at cell one longer: within its room, else in twice the room, its elements kept, the
   old array dead till a sweep: nothing moves. */
static void qrt_lengthen(Session *const S, const Addr cell, const uint32_t et)
{
    ArrSlot *const s = (ArrSlot *)at(cell);
    ArrayHdr *const a = s->hdr ? (ArrayHdr *)at(s->hdr) : nullptr;

    if (a && a->fast < a->count[0]) {
        a->fast++;
        return;
    }

    const uint32_t n = a ? a->fast : 0;
    arry_relen(&S->arry, s, et, n ? 2 * n : 4, n + 1);
}

/* The bytes a list at cell of et needs to grow: twice its room. */
static uint32_t qrt_need(Session *const S, const Addr cell, const uint32_t et)
{
    Addr data;
    const uint32_t n = qrt_len(cell, &data);
    return 2 * (n ? n : 2) * arry_et_size(&S->decl, et);
}

/* qrt_lengthen, the dead swept first now and then (arry_tidy): the cell may move with the heap. */
static void qrt_grow(Session *const S, Addr cell, const uint32_t et)
{
    cell = arry_tidy(&S->arry, &S->vm, cell, qrt_need(S, cell, et));
    qrt_lengthen(S, cell, et);
}

/* The list at cell one shorter: its last emptied, its strings let go, its records' arrays dead;
   none left, the array dead till a sweep. */
static void qrt_cut(Session *const S, const Addr cell, const uint32_t et)
{
    ArrSlot *const s = (ArrSlot *)at(cell);
    ArrayHdr *const a = (ArrayHdr *)at(s->hdr);
    const uint32_t size = arry_et_size(&S->decl, et);

    if (a->fast == 1) {
        arry_kill(&S->arry, s->hdr);
        s->hdr = 0;
        return;
    }

    const Addr e = a->data + --a->fast * size;
    const TypeDef *const td = et >= ET_REC ? decl_type(&S->decl, et - ET_REC) : nullptr;

    for (uint32_t i = 0; td && i < td->nslot; i++)
        arry_kill(&S->arry, ((const ArrSlot *)at(e + td->slots[i]))->hdr);

    memset(at(e), 0, size);
}

/* qrt_cut, then the dead swept: the cell may move with the heap. */
static void qrt_shrink(Session *const S, const Addr cell, const uint32_t et)
{
    qrt_cut(S, cell, et);
    arry_sweep(&S->arry, &S->vm);
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

/* A number element of et at e: 8 bytes, 4, or a short's 2 or a byte's 1 as an int. */
static Cell qrt_elem(const uint32_t et, const Addr e)
{
    switch (et) {
    case ET_R8:
        return load_cell(at(e));

    case ET_SH:
        return (Cell){.i = (int16_t)load16(at(e))};

    case ET_BY:
        return (Cell){.u = *(const uint8_t *)at(e)};

    default:
        return (Cell){.u = load32(at(e))};
    }
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
        const Cell v = qrt_elem(et, e);

        qrt_shrink(S, cell, et);
        exec_push(&S->vm, v);
        return;
    }

    const Desc d = strg_peek(e);
    memcpy(S->cons.sbuf, at(d.ptr), d.len); /* copied out: the shrink frees its block */
    qrt_shrink(S, cell, et);
    exec_pusha(&S->vm, strg_text(&S->strg, S->cons.sbuf, d.len));
}

/* Is v among the list's elements of et? raw: 8 bytes compared as they are. */
static bool qrt_has(
    Session *const S,
    const Addr cell,
    const uint32_t et,
    const bool raw,
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
        } else if (raw)
            same = load_cell(at(e)).raw == v.raw;
        else if (et == ET_R8)
            same = load_cell(at(e)).d == v.d;
        else if (et == ET_R4)
            same = (Cell){.u = load32(at(e))}.f == v.f;
        else
            same = qrt_elem(et, e).u == v.u;

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
static uint32_t qrt_radix(uint64_t u, const uint32_t base, const bool upper, char *const out)
{
    const char *const digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char rev[64];
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

    uint64_t u = 0;
    const bool neg = qrt_integer(kind) && qrt_magnitude(kind, v, &u);

    if (qrt_integer(kind) && (!t || strchr("bcdoxXn", t))) {
        if (t == 'c') {
            body[0] = (char)u;
            return 1;
        }

        const uint32_t base = t == 'b' ? 2 : t == 'o' ? 8 : t == 'x' || t == 'X' ? 16 : 10;
        return qrt_radix(u, base, t == 'X', body);
    }

    const double d = qrt_integer(kind)    ? (neg ? -(double)u : (double)u)
                     : kind == QRT_SINGLE ? (double)v.f
                                          : v.d;
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

    uint64_t mag;
    neg = qrt_integer(kind) ? qrt_magnitude(kind, v, &mag) : kind == QRT_SINGLE ? v.f < 0 : v.d < 0;
    len = qrt_digits(&f, kind, v, body);

    if (f.type == '%')
        body[len++] = '%';
    if (f.group)
        len = qrt_group(body, len, f.group);
    if (f.alt && qrt_integer(kind) && f.type && strchr("boxX", f.type))
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

/* ---- slices */

/* A slice's end i within 0 .. n. */
static uint32_t qrt_clamp(const int32_t i, const uint32_t n)
{
    return i < 0 ? 0 : (uint32_t)i > n ? n : (uint32_t)i;
}

/* ---- maps */

enum { QM_KEYS, QM_VALS, QM_SLOTS, QM_FIRST = 8 };

/* Map at map's list k's cell: its three lists are its record's fields, in order (qsem_mapof). */
static Addr qrt_mcell(const Addr map, const uint32_t k)
{
    return map + k * (uint32_t)sizeof(ArrSlot);
}

/* A key's hash: an int's bits or a string's FNV-1a, mixed (murmur3's finalizer). */
static uint32_t qrt_hash(const uint32_t ket, const Cell key, const Desc str)
{
    uint32_t h = key.u;

    if (ket == ET_SD) {
        h = 2166136261u;

        for (uint32_t i = 0; i < str.len; i++)
            h = (h ^ ((const uint8_t *)at(str.ptr))[i]) * 16777619u;
    }

    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    return h ^ (h >> 16);
}

static bool qrt_mkey(const uint32_t ket, const Addr e, const Cell key, const Desc str)
{
    if (ket != ET_SD)
        return load32(at(e)) == key.u;

    const Desc d = strg_peek(e);
    return d.len == str.len && !memcmp(at(d.ptr), at(str.ptr), d.len);
}

static uint32_t qrt_ksize(const uint32_t ket) { return ket == ET_SD ? DSZ : 4; }

/* key's slot in map: *found if it holds key's entry, else the free slot its probe stops at. A
   map with no slots: 0, not found. */
static uint32_t qrt_mslot(
    const Addr map,
    const uint32_t ket,
    const Cell key,
    const Desc str,
    bool *const found
)
{
    Addr slots, keys;
    const uint32_t n = qrt_len(qrt_mcell(map, QM_SLOTS), &slots);
    qrt_len(qrt_mcell(map, QM_KEYS), &keys);
    *found = false;

    if (!n)
        return 0;

    for (uint32_t p = qrt_hash(ket, key, str) & (n - 1);; p = (p + 1) & (n - 1)) {
        const uint32_t i = load32(at(slots + 4 * p));

        if (!i)
            return p;
        if (qrt_mkey(ket, keys + (i - 1) * qrt_ksize(ket), key, str)) {
            *found = true;
            return p;
        }
    }
}

/* Entry i's key's free slot, its probe's first: entry i in it. */
static void qrt_mplace(const Addr map, const uint32_t ket, const uint32_t i)
{
    Addr slots, keys;
    const uint32_t n = qrt_len(qrt_mcell(map, QM_SLOTS), &slots);
    qrt_len(qrt_mcell(map, QM_KEYS), &keys);
    const Addr k = keys + i * qrt_ksize(ket);
    const Cell key = {.u = ket == ET_SD ? 0 : load32(at(k))};

    uint32_t p = qrt_hash(ket, key, ket == ET_SD ? strg_peek(k) : (Desc){}) & (n - 1);

    while (load32(at(slots + 4 * p)))
        p = (p + 1) & (n - 1);

    store32(at(slots + 4 * p), i + 1);
}

/* Map's slots twice as many (QM_FIRST at first), its n entries placed again. Nothing moves. */
static void qrt_mrehash(Session *const S, const Addr map, const uint32_t ket, const uint32_t n)
{
    ArrSlot *const s = (ArrSlot *)at(qrt_mcell(map, QM_SLOTS));
    Addr slots;
    const uint32_t cap = qrt_len(qrt_mcell(map, QM_SLOTS), &slots);

    arry_kill(&S->arry, s->hdr);
    s->hdr = 0;
    arry_relen(&S->arry, s, ET_I4, cap ? 2 * cap : QM_FIRST, cap ? 2 * cap : QM_FIRST);

    for (uint32_t i = 0; i < n; i++)
        qrt_mplace(map, ket, i);
}

/* key's entry made in map, its value empty: the value's address. The map may move first. */
static Addr qrt_minsert(
    Session *const S,
    Addr map,
    const uint32_t ket,
    const uint32_t vet,
    const Cell key
)
{
    const uint32_t need = qrt_need(S, qrt_mcell(map, QM_KEYS), ket) +
                          qrt_need(S, qrt_mcell(map, QM_VALS), vet) +
                          qrt_need(S, qrt_mcell(map, QM_SLOTS), ET_I4);
    map = arry_tidy(&S->arry, &S->vm, map, need);
    Addr data;
    const uint32_t n = qrt_len(qrt_mcell(map, QM_KEYS), &data);
    Addr slots;

    if (2 * (n + 1) > qrt_len(qrt_mcell(map, QM_SLOTS), &slots))
        qrt_mrehash(S, map, ket, n);

    qrt_lengthen(S, qrt_mcell(map, QM_KEYS), ket);
    qrt_lengthen(S, qrt_mcell(map, QM_VALS), vet);
    qrt_len(qrt_mcell(map, QM_KEYS), &data);
    const Addr k = data + n * qrt_ksize(ket);

    if (ket == ET_SD)
        strg_set(&S->strg, key.u, k);
    else
        store32(at(k), key.u);

    qrt_mplace(map, ket, n);
    qrt_len(qrt_mcell(map, QM_VALS), &data);
    return data + n * arry_et_size(&S->decl, vet);
}

/* Slot p emptied, the slots after it in its run moved back so each stays on its probe. */
static void qrt_munslot(const Addr map, const uint32_t ket, uint32_t p)
{
    Addr slots, keys;
    const uint32_t n = qrt_len(qrt_mcell(map, QM_SLOTS), &slots);
    qrt_len(qrt_mcell(map, QM_KEYS), &keys);

    for (uint32_t j = (p + 1) & (n - 1);; j = (j + 1) & (n - 1)) {
        const uint32_t i = load32(at(slots + 4 * j));

        if (!i)
            break;

        const Addr k = keys + (i - 1) * qrt_ksize(ket);
        const Cell key = {.u = ket == ET_SD ? 0 : load32(at(k))};

        const uint32_t home = qrt_hash(ket, key, ket == ET_SD ? strg_peek(k) : (Desc){}) & (n - 1);

        if (((j - home) & (n - 1)) >= ((j - p) & (n - 1))) { /* home at p or before: back to p */
            store32(at(slots + 4 * p), i);
            p = j;
        }
    }

    store32(at(slots + 4 * p), 0);
}

/* The entry at slot p removed: the last entry moved into its place, then the lists cut. */
static void qrt_mremove(
    Session *const S,
    const Addr map,
    const uint32_t ket,
    const uint32_t vet,
    const uint32_t p
)
{
    Addr slots, keys, vals;
    qrt_len(qrt_mcell(map, QM_SLOTS), &slots);
    const uint32_t i = load32(at(slots + 4 * p)) - 1;
    const uint32_t last = qrt_len(qrt_mcell(map, QM_KEYS), &keys) - 1;
    qrt_len(qrt_mcell(map, QM_VALS), &vals);
    const uint32_t vsize = arry_et_size(&S->decl, vet);

    qrt_munslot(map, ket, p);

    if (i != last) {
        bool found;
        const Addr k = keys + last * qrt_ksize(ket);
        const Cell key = {.u = ket == ET_SD ? 0 : load32(at(k))};

        const uint32_t q = qrt_mslot(map, ket, key, ket == ET_SD ? strg_peek(k) : (Desc){}, &found);

        store32(at(slots + 4 * q), i + 1);
        qrt_swap(S, ket, keys + i * qrt_ksize(ket), k);
        qrt_swap(S, vet, vals + i * vsize, vals + last * vsize);
    }

    qrt_cut(S, qrt_mcell(map, QM_KEYS), ket);
    qrt_cut(S, qrt_mcell(map, QM_VALS), vet);
    arry_sweep(&S->arry, &S->vm);
}

/* QX_MAPAT: key's value's address in map, by how; QM_FIND's 0 when key is not there. */
static Addr qrt_mapat(
    Session *const S,
    const Addr map,
    const uint32_t ket,
    const uint32_t vet,
    const Cell key,
    const QrtHow how
)
{
    bool found;
    const Desc str = ket == ET_SD ? strg_peek(key.u) : (Desc){};

    const uint32_t p = qrt_mslot(map, ket, key, str, &found);

    if (!found && how == QM_SET)
        return qrt_minsert(S, map, ket, vet, key);

    if (ket == ET_SD)
        strg_get(&S->strg, key.u); /* its temporary popped */

    if (!found && how == QM_READ)
        arry_bad_sub(&S->arry);
    if (!found)
        return 0;

    Addr slots, vals;
    qrt_len(qrt_mcell(map, QM_SLOTS), &slots);
    qrt_len(qrt_mcell(map, QM_VALS), &vals);
    return vals + (load32(at(slots + 4 * p)) - 1) * arry_et_size(&S->decl, vet);
}

/* QX_MAPDEL: key's entry, if there, removed. */
static void qrt_mapdel(
    Session *const S,
    const Addr map,
    const uint32_t ket,
    const uint32_t vet,
    const Cell key
)
{
    bool found;
    const Desc str = ket == ET_SD ? strg_get(&S->strg, key.u) : (Desc){};

    const uint32_t p = qrt_mslot(map, ket, key, str, &found);

    if (found)
        qrt_mremove(S, map, ket, vet, p);
}

/* ---- integers past the int ops: byte .. ulong */

/* Number n's bits; is it unsigned, a float? */
static uint32_t qrt_bits(const QrtNum n) { return 8u << (n & 3); }
static bool qrt_unsigned(const QrtNum n) { return n >= QN_U8 && n <= QN_U64; }

/* An integer of n in cell v, its bits as a uint64: a signed one's sign extended. */
static uint64_t qrt_get(const QrtNum n, const Cell v)
{
    switch (n) {
    case QN_I64:
    case QN_U64:
        return v.raw;

    case QN_U8:
    case QN_U16:
    case QN_U32:
        return v.u;

    default:
        return (uint64_t)(int64_t)v.i;
    }
}

static Cell qrt_cell(const QrtNum n, const uint64_t x)
{
    return qrt_bits(n) == 64 ? (Cell){.raw = x} : (Cell){.u = (uint32_t)x};
}

/* x, signed when sign, as n: does it fit? */
static bool qrt_fits(const QrtNum n, const uint64_t x, const bool sign)
{
    const uint32_t b = qrt_bits(n);
    const bool neg = sign && (int64_t)x < 0;

    if (qrt_unsigned(n))
        return !neg && (b == 64 || x < (uint64_t)1 << b);
    if (neg)
        return b == 64 || (int64_t)x >= -((int64_t)1 << (b - 1));

    return x <= (b == 64 ? (uint64_t)INT64_MAX : ((uint64_t)1 << (b - 1)) - 1);
}

/* x kept to n's bits: a signed one's sign extended. */
static uint64_t qrt_keep(const QrtNum n, const uint64_t x)
{
    const uint32_t b = qrt_bits(n);

    if (b == 64)
        return x;

    const uint64_t m = ((uint64_t)1 << b) - 1, k = x & m;
    return !qrt_unsigned(n) && k >> (b - 1) ? k | ~m : k;
}

/* a ** b of n: the VM's, 0 for a negative power but of 1 and -1; *over past 64 bits. */
static uint64_t qrt_ipow(const QrtNum n, const uint64_t a, const uint64_t b, bool *const over)
{
    const bool sign = !qrt_unsigned(n);
    const int64_t sa = (int64_t)a, sb = (int64_t)b;

    if (a == 1 || !b)
        return 1;
    if (sign && sa == -1)
        return b & 1 ? a : 1;
    if (!a || (sign && sb < 0))
        return 0;

    uint64_t r = 1;

    for (uint64_t i = 0; i < b && !*over; i++) /* |a| > 1: past 64 bits within 64 */
        *over = sign ? __builtin_mul_overflow((int64_t)r, sa, (int64_t *)&r)
                     : __builtin_mul_overflow(r, a, &r);

    return r;
}

/* a op b of number n, by 64-bit arithmetic: true when it overflows 64 bits. */
static bool qrt_arith(
    Session *const S,
    const QrtIop op,
    const QrtNum n,
    const uint64_t a,
    const uint64_t b,
    uint64_t *const r
)
{
    const bool sign = !qrt_unsigned(n);
    const int64_t sa = (int64_t)a, sb = (int64_t)b;
    int64_t sr = 0;
    bool over = false;

    switch (op) {
    case QI_ADD:
        over = sign ? __builtin_add_overflow(sa, sb, &sr) : __builtin_add_overflow(a, b, r);
        break;

    case QI_SUB:
        over = sign ? __builtin_sub_overflow(sa, sb, &sr) : __builtin_sub_overflow(a, b, r);
        break;

    case QI_MUL:
        over = sign ? __builtin_mul_overflow(sa, sb, &sr) : __builtin_mul_overflow(a, b, r);
        break;

    case QI_DIV:
    case QI_MOD:
        if (!b)
            fault_error(&S->fault, E_DZ);
        if (sign && sb == -1) { /* INT64_MIN / -1 */
            over = op == QI_DIV && sa == INT64_MIN;
            sr = op == QI_DIV ? (int64_t)(0 - a) : 0;
        } else if (sign)
            sr = op == QI_DIV ? sa / sb : sa % sb;
        else
            *r = op == QI_DIV ? a / b : a % b;

        break;

    case QI_POW:
        *r = qrt_ipow(n, a, b, &over);
        return over;

    case QI_NEG:
        over = sign ? __builtin_sub_overflow((int64_t)0, sa, &sr) : a != 0;
        sr = (int64_t)(0 - a);
        break;

    default:
        break;
    }

    if (sign)
        *r = (uint64_t)sr;

    return over;
}

/* QX.INT ( a [b] word -- r ): integer operator op of number n; wrap keeps n's bits, else Overflow
   past n's range. A shift's count past n's bits is Illegal function call. */
static void qrt_int(Session *const S, const uint32_t word)
{
    Vm *const vm = &S->vm;
    const QrtIop op = (QrtIop)(word >> QX_OP & 15);
    const QrtNum n = (QrtNum)(word >> QX_NUM & 15);
    const bool wrap = word >> QX_WRAP & 1;
    const bool unary = op == QI_NEG || op == QI_NOT;
    const bool shift = op == QI_SHL || op == QI_SHR;
    const Cell cb = unary ? (Cell){} : exec_pop(vm);

    const uint64_t b = shift || word >> QX_ICB & 1 ? (uint64_t)(int64_t)cb.i : qrt_get(n, cb);
    const Cell ca = exec_pop(vm);
    const uint64_t a = word >> QX_ICA & 1 ? (uint64_t)(int64_t)ca.i : qrt_get(n, ca);
    const uint32_t bits = qrt_bits(n);
    const bool sign = !qrt_unsigned(n);
    uint64_t r = 0;

    switch (op) {
    case QI_AND:
        r = a & b;
        break;

    case QI_OR:
        r = a | b;
        break;

    case QI_XOR:
        r = a ^ b;
        break;

    case QI_NOT:
        r = qrt_keep(n, ~a);
        break;

    case QI_SHL:
    case QI_SHR:
        if (b >= bits)
            fault_error(&S->fault, E_IQ);

        r = op == QI_SHL ? qrt_keep(n, a << b)
            : sign       ? (uint64_t)((int64_t)a < 0 ? ~(~(int64_t)a >> b) : (int64_t)a >> b)
                         : a >> b;
        break;

    case QI_CMP: {
        const bool lt = sign ? (int64_t)a < (int64_t)b : a < b;
        const uint32_t o = a == b ? DC_EQ : lt ? DC_LT : DC_GT;
        exec_pushi(vm, word >> QX_REL & o ? -1 : 0);
        return;
    }

    default: {
        const bool over = qrt_arith(S, op, n, a, b, &r);

        if (wrap)
            r = qrt_keep(n, r);
        else if (over || !qrt_fits(n, r, sign))
            fault_error(&S->fault, E_OV);
    }
    }

    exec_push(vm, qrt_cell(n, r));
}

/* QX.CONV ( v word -- w ): v of number from as number to: a float truncated toward 0; Overflow when
   it doesn't fit, unless wrap keeps to's bits. */
static void qrt_conv(Session *const S, const uint32_t word)
{
    Vm *const vm = &S->vm;
    const QrtNum from = (QrtNum)(word >> QX_OP & 15), to = (QrtNum)(word >> QX_TO & 15);
    const bool wrap = word >> QX_WRAP & 1;
    const Cell v = exec_pop(vm);

    if (to == QN_F32 || to == QN_F64) {
        const uint64_t x = qrt_get(from, v);
        const double d = qrt_unsigned(from) ? (double)x : (double)(int64_t)x;
        exec_push(vm, to == QN_F32 ? (Cell){.f = (float)d} : (Cell){.d = d});

        return;
    }

    uint64_t x;
    bool sign = !qrt_unsigned(from);

    if (from == QN_F32 || from == QN_F64) {
        const double d = from == QN_F32 ? (double)v.f : v.d, top = 9223372036854775808.0;

        if (d >= top && d < 2 * top)
            x = (uint64_t)d, sign = false;
        else if (d >= -top && d < top)
            x = (uint64_t)(int64_t)d, sign = true;
        else
            fault_error(&S->fault, E_OV);
    } else
        x = qrt_get(from, v);

    if (wrap)
        x = qrt_keep(to, x);
    else if (!qrt_fits(to, x, sign))
        fault_error(&S->fault, E_OV);

    exec_push(vm, qrt_cell(to, x));
}

/* QX ( args.. word -- results ): the routine its low byte names (qr/qrt.h QrtX). */
void qrt_x(Session *const S)
{
    const uint32_t word = exec_pop(&S->vm).u;
    const QrtX op = (QrtX)(word & 0xFF);
    Vm *const vm = &S->vm;

    switch (op) {
    case QX_INT:
        qrt_int(S, word);
        return;

    case QX_CONV:
        qrt_conv(S, word);
        return;

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
        qrt_grow(S, cell, et);
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

        qrt_shrink(S, cell, et);
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
        const uint32_t how = exec_pop(vm).u, et = how & ~QRT_RAW;
        const Addr cell = exec_popa(vm);
        const Desc str = et == ET_SD ? strg_get(&S->strg, v.u) : (Desc){};

        exec_pushi(vm, qrt_has(S, cell, et, how & QRT_RAW, v, str) ? -1 : 0);
        return;
    }

    case QX_MAPAT: {
        const QrtHow how = (QrtHow)exec_popi(vm);
        const Addr map = exec_popa(vm);
        const uint32_t vet = exec_pop(vm).u;
        const uint32_t ket = exec_pop(vm).u;
        exec_pusha(vm, qrt_mapat(S, map, ket, vet, exec_pop(vm), how));
        return;
    }

    case QX_MAPDEL: {
        const Addr map = exec_popa(vm);
        const uint32_t vet = exec_pop(vm).u;
        const uint32_t ket = exec_pop(vm).u;
        qrt_mapdel(S, map, ket, vet, exec_pop(vm));
        return;
    }

    case QX_BYTE: {
        const int32_t i = exec_popi(vm);
        const Desc s = strg_get(&S->strg, exec_popa(vm));

        if ((uint32_t)i >= s.len)
            arry_bad_sub(&S->arry);

        exec_pushi(vm, ((const uint8_t *)at(s.ptr))[i]);
        return;
    }

    case QX_SLICE: {
        const int32_t b = exec_popi(vm);
        const int32_t a = exec_popi(vm);
        const Addr s = exec_popa(vm);
        const uint32_t from = qrt_clamp(a, strg_len(s)), to = qrt_clamp(b, strg_len(s));
        exec_pusha(
            vm,
            strg_sub(&S->strg, s, (uint16_t)from, (uint16_t)(to > from ? to - from : 0))
        );
        return;
    }

    case QX_LSLICE: {
        ArrSlot *const d = (ArrSlot *)at(exec_popa(vm));
        const Addr src = exec_popa(vm);
        const int32_t b = exec_popi(vm);
        const int32_t a = exec_popi(vm);
        Addr data;
        const uint32_t n = qrt_len(src, &data);
        const uint32_t from = qrt_clamp(a, n), to = qrt_clamp(b, n);
        arry_copy_part(&S->arry, vm, d, (const ArrSlot *)at(src), from, to > from ? to - from : 0);
        return;
    }

    case QX_FNCALL: { /* into its fn's body: past its first line's JMP, and a DBG before it */
        const uint32_t proc = exec_pop(vm).u;
        const uint32_t line = blok_proc(&S->blok, proc)->first;
        uint8_t *code = at(S->allo.cache + prog_line(&S->prog, line)->code);

        if (*code == O_DBG)
            code += flow_opsize(code);

        flow_call(S, proc, line, code + JSIZE);
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
