/* Closing pairs as they are typed: ( and " get their ) and " after the cursor.
 *
 * # How it works
 * In insert mode, ( or " typed where a space, a ) or the line's end follows puts its closing one
 * in too, the cursor between. A ) or " typed where the same one follows steps over it; a " only
 * inside a string, which it then ends. Backspace between ( and ), or between the quotes of an
 * empty string, takes both out. In a comment, and for ( in a string, nothing is added.
 *
 * Whether the cursor is in a string or a comment is the highlighter's (synt_cols): a comment is
 * its slot, a string an odd count of quotes before the cursor outside one.
 */
#include "ui/closers.h"
#include "ui/edit.h"
#include "ui/highlight.h"
#include "ui/text.h"

enum : uint8_t { IN_CODE, IN_STRING, IN_COMMENT };

/* What column col of the cursor's line is in. */
static uint8_t place(Session *const S, const uint16_t col)
{
    const LineBuf *const b = &uimn.edline;
    const uint16_t end = col < b->len ? col : b->len;

    if (!end)
        return IN_CODE;

    static uint8_t cols[SYNT_KEEP];
    const uint32_t n = end < SYNT_KEEP ? end : SYNT_KEEP;
    synt_cols(S, text_dialect_at(S, uimn.text.first + uimn.ed.line), b->text, n, cols);

    if (cols[n - 1] == SL_COM)
        return IN_COMMENT;

    uint32_t quotes = 0;

    for (uint16_t i = 0; i < end; i++)
        quotes += b->text[i] == '"';

    return quotes % 2 ? IN_STRING : IN_CODE;
}

bool clos_type(Session *const S, const uint8_t ch)
{
    LineBuf *const b = &uimn.edline;
    const uint16_t col = b->col;
    const uint8_t next = col < b->len ? b->text[col] : 0;

    if (!uimn.ed.insert || (ch != '(' && ch != ')' && ch != '"'))
        return false;

    const uint8_t at = place(S, col);

    if (next == ch && ch != '(' && (ch == ')' ? at == IN_CODE : at == IN_STRING)) {
        b->col++; /* over the closing one */
        return true;
    }

    if (ch == ')' || at != IN_CODE || (next && next != ' ' && next != ')') || b->len + 2 > LNMAX)
        return false;

    edit_lb_type(b, ch, LNMAX);
    edit_lb_type(b, ch == '(' ? ')' : '"', LNMAX);
    b->col--;
    return true;
}

bool clos_bs(Session *const S)
{
    if (edit_refused(S))
        return true;

    LineBuf *const b = &uimn.edline;
    const uint16_t col = b->col;

    if (!col || col >= b->len)
        return false;

    const uint8_t open = b->text[col - 1], close = b->text[col];
    const uint8_t at = place(S, col);
    const bool pair = (open == '(' && close == ')' && at == IN_CODE) ||
                      (open == '"' && close == '"' && at == IN_STRING);

    if (!pair)
        return false;

    edit_lb_cut(b, col - 1, col + 1);
    return true;
}
