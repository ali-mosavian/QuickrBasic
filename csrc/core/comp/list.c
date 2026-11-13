/* LIST, SAVE and VERIFY: a stored body back to text (comp/list.asm). */
#include "comp/list.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "io/cons.h"
#include "mem/prog.h"

List list;

/* The last item: LS_NONE no space follows (the start, '(', a sign), LS_WORD a name, number,
   string or ')', LS_KW a keyword, LS_FN a function's, LS_SEP an operator or , ; :, LS_REL a
   relation. */
enum : uint8_t { LS_NONE, LS_WORD, LS_KW, LS_FN, LS_SEP, LS_REL };


/* ls_lead: the space before a word, as the last item wants. */
static void lead(void)
{
    if (list.last != LS_NONE)
        cons_space();
}

static void listline(const uint32_t l)
{
    cons_uint(prog_line(l)->num);
    cons_space();
    list_body((const uint8_t *)prog_line(l) + HDR);
    cons_nl();
}

void list_range(const uint16_t from, const uint16_t to)
{
    for (uint32_t l = PROGBASE, len; (len = prog_line(l)->len); l += len) {
        const uint16_t num = prog_line(l)->num;
        if (num < from)
            continue;
        if (num > to)
            return;
        listline(l);
    }
}

uint32_t list_text(const uint8_t *const body, const uint32_t at)
{
    list.at = body + at;
    cons.capture = CAP_SBUF;
    cons.sblen = 0;
    list_body(body);
    cons.capture = 0;
    cons.sbuf[cons.sblen] = 0;
    const uint32_t col = list.at ? cons.sblen : list.col; /* not reached: the end */
    list.at = nullptr;
    return col;
}

/* A number kept as text: one word, its exponent's sign too; p past its first byte c. */
static const uint8_t *number(const uint8_t *p, uint8_t c)
{
    for (;;) {
        cons_putc(c);
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
static void binop(const uint8_t t)
{
    const bool relation = t >= TK_GT && t <= TK_LT;
    if ((t == TK_MINUS || t == TK_PLUS) && list.last != LS_WORD) { /* a sign */
        if (list.last != LS_NONE)
            cons_space();
        cons_kw(t);
        list.last = LS_NONE;
        return;
    }
    if (!(relation && list.last == LS_REL) && list.last != LS_NONE) /* <= <> >=: one operator */
        cons_space();
    cons_kw(t);
    list.last = relation ? LS_REL : LS_SEP;
}

void list_body(const uint8_t *p)
{
    list.last = LS_NONE;
    for (;;) {
        if (list.at && p >= list.at) { /* list_text's byte: its column */
            list.at = nullptr;
            list.col = cons.sblen;
        }
        uint8_t c = *p++;
        if (!c)
            return;
        switch (c) {
        case M_INT: {
            lead();
            cons_uint(load32(p));
            p += 4;
            list.last = LS_WORD;
            continue;
        }
        case M_VAR:
            lead();
            for (uint8_t n = *p++; n; n--)
                cons_putc(*p++);
            p += 2;
            list.last = LS_WORD;
            continue;
        case '"': /* a string: to its quote, as it is */
            lead();
            for (;;) {
                cons_putc(c);
                c = *p++;
                if (!c)
                    return;
                if (c == '"')
                    break;
            }
            cons_putc(c);
            list.last = LS_WORD;
            continue;
        case '(': /* after a name or a function, close */
            if (list.last >= LS_KW && list.last != LS_FN)
                cons_space();
            cons_putc(c);
            list.last = LS_NONE;
            continue;
        case ')':
            cons_putc(c);
            list.last = LS_WORD;
            continue;
        case ',':
        case ';':
        case ':':
            cons_putc(c);
            list.last = LS_SEP;
            continue;
        }
        if (c < 0x80) {
            lead();
            if (c == '.' || is_digit(c))
                p = number(p, c);
            else
                cons_putc(c); /* any other byte: as a word */
            list.last = LS_WORD;
            continue;
        }
        if (c == TK_X) { /* FFh n: a function when '(' follows */
            const uint8_t n = *p++;
            const uint8_t cls = *p == '(' ? LS_FN : LS_KW;
            lead();
            cons_kw1(n);
            list.last = cls;
            continue;
        }
        if ((c >= TK_PLUS && c <= TK_LT) || (c >= TK_MOD && c <= TK_IMP)) {
            binop(c);
            continue;
        }
        const uint8_t cls = (c >= TK_SGN && c <= TK_MID) || c == TK_TAB || c == TK_SPC || c >= TK_LBOUND
                          ? LS_FN
                          : LS_KW;
        lead();
        if (c == TK_REM && *p == REM_Q) { /* ' as it was written */
            p++;
            cons_putc('\'');
        } else {
            cons_kw(c);
            list.last = cls;
            if (c == TK_DATA) { /* its text as it is, then its ':' */
                const uint8_t *const end = comp_skipdata(p);
                for (; p < end; p++)
                    cons_putc(*p);
                if (!*p)
                    return;
                cons_putc(*p++);
                list.last = LS_SEP;
                continue;
            }
            if (c != TK_REM)
                continue;
        }
        while ((c = *p++)) /* REM's text: the rest, as it is */
            cons_putc(c);
        return;
    }
}
