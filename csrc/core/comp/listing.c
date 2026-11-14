/* LIST, SAVE and VERIFY: a stored body back to text (comp/list.asm). */
#include "comp/listing.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "io/console.h"
#include "mem/program.h"


/* The last item: LS_NONE no space follows (the start, '(', a sign), LS_WORD a name, number,
   string or ')', LS_KW a keyword, LS_FN a function's, LS_SEP an operator or , ; :, LS_REL a
   relation. */
enum : uint8_t { LS_NONE, LS_WORD, LS_KW, LS_FN, LS_SEP, LS_REL };


/* ls_lead: the space before a word, as the last item wants. */
static void lead(Session *const S)
{
    if (S->list.last != LS_NONE)
        cons_space(S);
}

static void listline(Session *const S, const uint32_t l)
{
    cons_uint(S, prog_line(&S->prog, l)->num);
    cons_space(S);
    list_body(S, (const uint8_t *)prog_line(&S->prog, l) + HDR);
    cons_nl(S);
}

void list_range(Session *const S, const uint16_t from, const uint16_t to)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(&S->prog, l)->len); l += len) {
        const uint16_t num = prog_line(&S->prog, l)->num;
        if (num < from)
            continue;
        if (num > to)
            return;
        listline(S, l);
    }
}

uint32_t list_text(Session *const S, const uint8_t *const body, const uint32_t at)
{
    S->list.at = body + at;
    S->cons.capture = CAP_SBUF;
    S->cons.sblen = 0;
    list_body(S, body);
    S->cons.capture = 0;
    S->cons.sbuf[S->cons.sblen] = 0;
    const uint32_t col = S->list.at ? S->cons.sblen : S->list.col; /* not reached: the end */
    S->list.at = nullptr;
    return col;
}

/* A number kept as text: one word, its exponent's sign too; p past its first byte c. */
static const uint8_t *number(Session *const S, const uint8_t *p, uint8_t c)
{
    for (;;) {
        cons_putc(S, c);
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
static void binop(Session *const S, const uint8_t t)
{
    const bool relation = t >= TK_GT && t <= TK_LT;
    if ((t == TK_MINUS || t == TK_PLUS) && S->list.last != LS_WORD) { /* a sign */
        if (S->list.last != LS_NONE)
            cons_space(S);
        cons_kw(S, t);
        S->list.last = LS_NONE;
        return;
    }
    if (!(relation && S->list.last == LS_REL) && S->list.last != LS_NONE) /* <= <> >=: one operator */
        cons_space(S);
    cons_kw(S, t);
    S->list.last = relation ? LS_REL : LS_SEP;
}

void list_body(Session *const S, const uint8_t *p)
{
    S->list.last = LS_NONE;
    for (;;) {
        if (S->list.at && p >= S->list.at) { /* list_text's byte: its column */
            S->list.at = nullptr;
            S->list.col = S->cons.sblen;
        }
        uint8_t c = *p++;
        if (!c)
            return;
        switch (c) {
        case M_INT: {
            lead(S);
            cons_uint(S, load32(p));
            p += 4;
            S->list.last = LS_WORD;
            continue;
        }
        case M_VAR:
            lead(S);
            for (uint8_t n = *p++; n; n--)
                cons_putc(S, *p++);
            p += 2;
            S->list.last = LS_WORD;
            continue;
        case '"': /* a string: to its quote, as it is */
            lead(S);
            for (;;) {
                cons_putc(S, c);
                c = *p++;
                if (!c)
                    return;
                if (c == '"')
                    break;
            }
            cons_putc(S, c);
            S->list.last = LS_WORD;
            continue;
        case '(': /* after a name or a function, close */
            if (S->list.last >= LS_KW && S->list.last != LS_FN)
                cons_space(S);
            cons_putc(S, c);
            S->list.last = LS_NONE;
            continue;
        case ')':
            cons_putc(S, c);
            S->list.last = LS_WORD;
            continue;
        case ',':
        case ';':
        case ':':
            cons_putc(S, c);
            S->list.last = LS_SEP;
            continue;
        }
        if (c < 0x80) {
            lead(S);
            if (c == '.' || is_digit(c))
                p = number(S, p, c);
            else
                cons_putc(S, c); /* any other byte: as a word */
            S->list.last = LS_WORD;
            continue;
        }
        if (c == TK_X) { /* FFh n: a function when '(' follows */
            const uint8_t n = *p++;
            const uint8_t cls = *p == '(' ? LS_FN : LS_KW;
            lead(S);
            cons_kw1(S, n);
            S->list.last = cls;
            continue;
        }
        if ((c >= TK_PLUS && c <= TK_LT) || (c >= TK_MOD && c <= TK_IMP)) {
            binop(S, c);
            continue;
        }
        const uint8_t cls = (c >= TK_SGN && c <= TK_MID) || c == TK_TAB || c == TK_SPC || c >= TK_LBOUND
                          ? LS_FN
                          : LS_KW;
        lead(S);
        if (c == TK_REM && *p == REM_Q) { /* ' as it was written */
            p++;
            cons_putc(S, '\'');
        } else {
            cons_kw(S, c);
            S->list.last = cls;
            if (c == TK_DATA) { /* its text as it is, then its ':' */
                const uint8_t *const end = comp_skipdata(p);
                for (; p < end; p++)
                    cons_putc(S, *p);
                if (!*p)
                    return;
                cons_putc(S, *p++);
                S->list.last = LS_SEP;
                continue;
            }
            if (c != TK_REM)
                continue;
        }
        while ((c = *p++)) /* REM's text: the rest, as it is */
            cons_putc(S, c);
        return;
    }
}
