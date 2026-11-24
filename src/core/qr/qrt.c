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
 * # Example
 *    a and b     a, I.LD.I32 n|FALSE, CALL QJ, b, I.LD.I32 m|ALWAYS, CALL QJ, I.LD.U8 0
 */
#include "qr/qrt.h"
#include "session.h"
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
