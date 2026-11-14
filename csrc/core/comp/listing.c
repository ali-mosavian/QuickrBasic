/* LIST, SAVE and VERIFY: a stored body back to text.
 *
 * # How it works
 * Spaced as QBasic's editor: keywords from their tokens, a name from its M_VAR, a whole number from
 * its M_INT; strings, REM and DATA verbatim. A space between words, around an operator but a sign,
 * after , ; : and none inside ( ). The last item's class (LS_*) decides the space before the next.
 * Output goes through io's current channel; list_text spells a body into io's sbuf instead.
 *
 * # Example
 *    10 print x+1:goto 10       10 PRINT x + 1: GOTO 10
 *
 * # asm structure (src/comp/list.asm)
 * | asm | here | |
 * |---|---|---|
 * | ls_range | list_range(from, to) | MIRROR |
 * | ls_text | list_text(body, at) | MIRROR; the column |
 * | ls_body | list_body(body) | MIRROR |
 * | ls_all | | D1 |
 * | ls_line, ls_lead, ls_space | listline, lead; cons_space | MIRROR |
 * | g_lsat, g_lscol | list.at (null none), list.col | MIRROR |
 * | io.lq, LS_* | list.last, the LS_* enum | MIRROR |
 * | rn.lfrom, rn.lto | list_range's parameters | D2 |
 *
 * - D1. Unused: list_range(0, 0xFFFF).
 * - D2. Parameters, not globals.
 */
#include "comp/listing.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "io/console.h"
#include "mem/program.h"

/* The last item: LS_NONE no space follows (the start, '(', a sign), LS_WORD a name, number,
   string or ')', LS_KW a keyword, LS_FN a function's, LS_SEP an operator or , ; :, LS_REL a
   relation. */
enum : uint8_t { LS_NONE, LS_WORD, LS_KW, LS_FN, LS_SEP, LS_REL };

/* ls_lead: the space before a word, as the last item wants. */
static void lead(Comp *const C)
{
    if (C->list->last != LS_NONE)
        cons_space(C->cons);
}

static void listline(Comp *const C, const uint32_t l)
{
    cons_uint(C->cons, prog_line(C->prog, l)->num);
    cons_space(C->cons);
    list_body(C, (const uint8_t *)prog_line(C->prog, l) + HDR);
    cons_nl(C->cons);
}

void list_range(Comp *const C, const uint16_t from, const uint16_t to)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(C->prog, l)->len); l += len) {
        const uint16_t num = prog_line(C->prog, l)->num;

        if (num < from)
            continue;
        if (num > to)
            return;

        listline(C, l);
    }
}

uint32_t list_text(Comp *const C, const uint8_t *const body, const uint32_t at)
{
    C->list->at = body + at;
    C->cons->capture = CAP_SBUF;
    C->cons->sblen = 0;
    list_body(C, body);
    C->cons->capture = 0;
    C->cons->sbuf[C->cons->sblen] = 0;
    const uint32_t col = C->list->at ? C->cons->sblen : C->list->col; /* not reached: the end */
    C->list->at = nullptr;
    return col;
}

/* A number kept as text: one word, its exponent's sign too; p past its first byte c. */
static const uint8_t *number(Comp *const C, const uint8_t *p, uint8_t c)
{
    for (;;) {
        cons_putc(C->cons, c);
        const uint8_t prev = c;
        c = *p;
        const bool more = c == '.' || c == '#' || c == '!' || c == 'E' || c == 'D' || is_digit(c) ||
                          ((c == '+' || c == '-') && (prev == 'E' || prev == 'D'));

        if (!more)
            return p;

        p++;
    }
}

/* An operator: spaced, but a sign. */
static void binop(Comp *const C, const uint8_t t)
{
    const bool relation = t >= TK_GT && t <= TK_LT;

    if ((t == TK_MINUS || t == TK_PLUS) && C->list->last != LS_WORD) { /* a sign */
        if (C->list->last != LS_NONE)
            cons_space(C->cons);

        cons_kw(C->cons, t);
        C->list->last = LS_NONE;
        return;
    }

    if (!(relation && C->list->last == LS_REL) &&
        C->list->last != LS_NONE) /* <= <> >=: one operator */
        cons_space(C->cons);

    cons_kw(C->cons, t);
    C->list->last = relation ? LS_REL : LS_SEP;
}

void list_body(Comp *const C, const uint8_t *p)
{
    C->list->last = LS_NONE;

    for (;;) {
        if (C->list->at && p >= C->list->at) { /* list_text's byte: its column */
            C->list->at = nullptr;
            C->list->col = C->cons->sblen;
        }

        uint8_t c = *p++;

        if (!c)
            return;
        switch (c) {
        case M_INT: {
            lead(C);
            cons_uint(C->cons, load32(p));
            p += 4;
            C->list->last = LS_WORD;
            continue;
        }
        case M_VAR: { /* in its casing as first written; its suffix as here */
            lead(C);
            const uint8_t n = p[0];
            uint8_t name[UINT8_MAX];
            memcpy(name, p + 1, n);
            symb_spell(C->symb, comp_nameof(p - 1), name, n);

            for (uint8_t i = 0; i < n; i++)
                cons_putc(C->cons, name[i]);

            p += n + 3u;
            C->list->last = LS_WORD;
            continue;
        }

        case '"': /* a string: to its quote, as it is */
            lead(C);

            for (;;) {
                cons_putc(C->cons, c);
                c = *p++;

                if (!c)
                    return;
                if (c == '"')
                    break;
            }

            cons_putc(C->cons, c);
            C->list->last = LS_WORD;
            continue;

        case '(': /* after a name or a function, close */
            if (C->list->last >= LS_KW && C->list->last != LS_FN)
                cons_space(C->cons);

            cons_putc(C->cons, c);
            C->list->last = LS_NONE;
            continue;

        case ')':
            cons_putc(C->cons, c);
            C->list->last = LS_WORD;
            continue;

        case ',':
        case ';':
        case ':':
            cons_putc(C->cons, c);
            C->list->last = LS_SEP;
            continue;
        }

        if (c == '.' && *p == M_VAR) { /* a ( .. )'s field: r(i).f close */
            cons_putc(C->cons, c);
            C->list->last = LS_NONE;
            continue;
        }

        if (c < 0x80) {
            lead(C);

            if (c == '.' || is_digit(c))
                p = number(C, p, c);
            else
                cons_putc(C->cons, c); /* any other byte: as a word */
            C->list->last = LS_WORD;
            continue;
        }

        if (c == TK_X) { /* FFh n: a function when '(' follows */
            const uint8_t n = *p++;
            const uint8_t cls = *p == '(' ? LS_FN : LS_KW;
            lead(C);
            cons_kw1(C->cons, n);
            C->list->last = cls;
            continue;
        }

        if ((c >= TK_PLUS && c <= TK_LT) || (c >= TK_MOD && c <= TK_IMP)) {
            binop(C, c);
            continue;
        }

        const uint8_t cls =
            (c >= TK_SGN && c <= TK_MID) || c == TK_TAB || c == TK_SPC || c >= TK_LBOUND ? LS_FN
                                                                                         : LS_KW;
        lead(C);

        if (c == TK_REM && *p == REM_Q) { /* ' as it was written */
            p++;
            cons_putc(C->cons, '\'');
        } else {
            cons_kw(C->cons, c);
            C->list->last = cls;

            if (c == TK_DATA) { /* its text as it is, then its ':' */
                const uint8_t *const end = comp_skipdata(p);

                for (; p < end; p++)
                    cons_putc(C->cons, *p);
                if (!*p)
                    return;

                cons_putc(C->cons, *p++);
                C->list->last = LS_SEP;
                continue;
            }

            if (c != TK_REM)
                continue;
        }

        while ((c = *p++)) /* REM's text: the rest, as it is */
            cons_putc(C->cons, c);

        return;
    }
}
