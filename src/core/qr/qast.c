/* Quickr's front end's records: allocation, errors, names for messages.
 *
 * # How it works
 * Everything the front end makes lives in one bump arena (util/arena.h), freed at once after the
 * link. An error is its line, column and text, kept in the order found; qast_sort puts them in
 * the source's order before they are reported.
 */
#include "qr/qast.h"

void *qast_alloc(QMem *const m, const size_t size)
{
    void *const p = arena_alloc(&m->arena, size);

    if (!p)
        fault_error(m->fault, E_OM);

    return p;
}

void *qast_room(
    QMem *const m,
    void *const at,
    const uint32_t n,
    uint32_t *const cap,
    const size_t size
)
{
    if (n < *cap)
        return at;

    const uint32_t more = *cap < 8 ? 8 : *cap + *cap / 2;
    void *const p = at ? arena_grow(&m->arena, at, *cap * size, more * size)
                       : arena_alloc(&m->arena, more * size);

    if (!p)
        fault_error(m->fault, E_OM);

    *cap = more;
    return p;
}

QExpr *qast_expr(QMem *const m, const QExprKind kind, const QTok *const tok)
{
    QExpr *const e = qast_alloc(m, sizeof *e);
    e->kind = kind;
    e->tok = tok;
    return e;
}

const char *qast_typename(const QType t)
{
    static const char *const names[] = {
        [QY_VOID] = "nothing",
        [QY_INT] = "int",
        [QY_SINGLE] = "single",
        [QY_DOUBLE] = "double",
        [QY_STRING] = "string",
        [QY_BOOL] = "bool",
        [QY_ERROR] = "an error",
    };

    return names[t];
}

const char *qast_spelling(const QKind k)
{
    static const char *const spell[] = {
        [QK_EOF] = "the end",
        [QK_NL] = "the line's end",
        [QK_NAME] = "a name",
        [QK_INT] = "a number",
        [QK_REAL] = "a number",
        [QK_STR] = "a string",
        [QK_AND] = "and",
        [QK_AS] = "as",
        [QK_BREAK] = "break",
        [QK_CONST] = "const",
        [QK_CONTINUE] = "continue",
        [QK_DIV] = "div",
        [QK_DO] = "do",
        [QK_ELSE] = "else",
        [QK_ELSEIF] = "elseif",
        [QK_END] = "end",
        [QK_FALSE] = "false",
        [QK_FN] = "fn",
        [QK_FOR] = "for",
        [QK_IF] = "if",
        [QK_IN] = "in",
        [QK_LET] = "let",
        [QK_MOD] = "mod",
        [QK_MUT] = "mut",
        [QK_NOT] = "not",
        [QK_OR] = "or",
        [QK_RETURN] = "return",
        [QK_STEP] = "step",
        [QK_THEN] = "then",
        [QK_TRUE] = "true",
        [QK_VAR] = "var",
        [QK_WHILE] = "while",
        [QK_XOR] = "xor",
        [QK_LATER] = "a keyword",
        [QK_LPAREN] = "(",
        [QK_RPAREN] = ")",
        [QK_LBRACK] = "[",
        [QK_RBRACK] = "]",
        [QK_LBRACE] = "{",
        [QK_RBRACE] = "}",
        [QK_COMMA] = ",",
        [QK_COLON] = ":",
        [QK_DOT] = ".",
        [QK_DOTDOT] = "..",
        [QK_QUEST] = "?",
        [QK_BANG] = "!",
        [QK_PLUS] = "+",
        [QK_MINUS] = "-",
        [QK_STAR] = "*",
        [QK_SLASH] = "/",
        [QK_POW] = "**",
        [QK_EQ] = "=",
        [QK_NE] = "<>",
        [QK_LT] = "<",
        [QK_LE] = "<=",
        [QK_GT] = ">",
        [QK_GE] = ">=",
        [QK_AMP] = "&",
        [QK_BAR] = "|",
        [QK_CARET] = "^",
        [QK_TILDE] = "~",
        [QK_SHL] = "<<",
        [QK_SHR] = ">>",
        [QK_ADDEQ] = "+=",
        [QK_SUBEQ] = "-=",
        [QK_MULEQ] = "*=",
        [QK_DIVEQ] = "/=",
        [QK_BAD] = "?",
    };

    return spell[k];
}

void qast_error(
    QErrs *const e,
    const QTok *const t,
    const char *const msg,
    const char *const name,
    const uint32_t len
)
{
    const char *const hole = strchr(msg, '{'); /* "{}" */
    const uint32_t head = hole ? (uint32_t)(hole - msg) : (uint32_t)strlen(msg);
    const uint32_t tail = hole ? (uint32_t)strlen(hole + 2) : 0;
    const uint32_t n = head + (hole ? len : 0) + tail;
    char *const text = qast_alloc(e->mem, n + 1);

    memcpy(text, msg, head);

    if (hole) {
        memcpy(text + head, name, len);
        memcpy(text + head + len, hole + 2, tail);
    }

    e->at = qast_room(e->mem, e->at, e->n, &e->cap, sizeof *e->at);
    e->at[e->n++] = (QErr){
        .line = t->line,
        .col = t->col,
        .text = text,
        .len = n,
    };
}

/* a before b in the source. */
static bool qast_before(const QErr *const a, const QErr *const b)
{
    return a->line < b->line || (a->line == b->line && a->col < b->col);
}

void qast_sort(QErrs *const e)
{
    for (uint32_t i = 1; i < e->n; i++) {
        const QErr x = e->at[i];
        uint32_t j = i;

        for (; j > 0 && qast_before(&x, &e->at[j - 1]); j--)
            e->at[j] = e->at[j - 1];

        e->at[j] = x;
    }
}
