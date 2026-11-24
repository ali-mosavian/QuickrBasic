/* Quickr's front end's records: allocation, errors, names for messages.
 *
 * # How it works
 * Everything the front end makes lives in one bump arena (util/arena.h), freed at once after the
 * link. An error is its line, column and text, kept in the order found; qast_sort puts them in
 * the source's order, each once, before they are reported.
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
        [QY_BYTE] = "byte",
        [QY_SHORT] = "short",
        [QY_LONG] = "long",
        [QY_UBYTE] = "ubyte",
        [QY_USHORT] = "ushort",
        [QY_UINT] = "uint",
        [QY_ULONG] = "ulong",
    };

    return t < countof(names) ? names[t] : "a type";
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
        [QK_FSTR] = "an f-string",
        [QK_FPART] = "an f-string",
        [QK_FSPEC] = "a format",
        [QK_FEND] = "the f-string's end",
        [QK_AND] = "and",
        [QK_AS] = "as",
        [QK_BREAK] = "break",
        [QK_CASE] = "case",
        [QK_CONST] = "const",
        [QK_CONTINUE] = "continue",
        [QK_DIV] = "div",
        [QK_DO] = "do",
        [QK_ELSE] = "else",
        [QK_ELSEIF] = "elseif",
        [QK_END] = "end",
        [QK_ENUM] = "enum",
        [QK_FALSE] = "false",
        [QK_FN] = "fn",
        [QK_FOR] = "for",
        [QK_IF] = "if",
        [QK_IN] = "in",
        [QK_IS] = "is",
        [QK_LET] = "let",
        [QK_MOD] = "mod",
        [QK_MUT] = "mut",
        [QK_NONE] = "none",
        [QK_NOT] = "not",
        [QK_OR] = "or",
        [QK_RETURN] = "return",
        [QK_STEP] = "step",
        [QK_THEN] = "then",
        [QK_TRUE] = "true",
        [QK_TYPE] = "type",
        [QK_VAR] = "var",
        [QK_WHILE] = "while",
        [QK_XOR] = "xor",
        [QK_YIELD] = "yield",
        [QK_YIELDS] = "yields",
        [QK_FROM] = "from",
        [QK_IMPORT] = "import",
        [QK_PUBLIC] = "public",
        [QK_NATIVE] = "native",
        [QK_OPERATOR] = "operator",
        [QK_TEST] = "test",
        [QK_ASSERT] = "assert",
        [QK_SELF] = "self",
        [QK_FUNCTION] = "function",
        [QK_LPAREN] = "(",
        [QK_RPAREN] = ")",
        [QK_LBRACK] = "[",
        [QK_RBRACK] = "]",
        [QK_LBRACE] = "{",
        [QK_RBRACE] = "}",
        [QK_COMMA] = ",",
        [QK_COLON] = ":",
        [QK_SEMI] = ";",
        [QK_DOT] = ".",
        [QK_DOTDOT] = "..",
        [QK_QUEST] = "?",
        [QK_ORELSE] = "or else",
        [QK_BANG] = "!",
        [QK_PLUS] = "+",
        [QK_MINUS] = "-",
        [QK_STAR] = "*",
        [QK_SLASH] = "/",
        [QK_POW] = "^",
        [QK_EQ] = "=",
        [QK_NE] = "<>",
        [QK_LT] = "<",
        [QK_LE] = "<=",
        [QK_GT] = ">",
        [QK_GE] = ">=",
        [QK_BAND] = "and",
        [QK_BOR] = "or",
        [QK_BXOR] = "xor",
        [QK_BNOT] = "not",
        [QK_SHL] = "shl",
        [QK_SHR] = "shr",
        [QK_TO] = "to",
        [QK_NEXT] = "next",
        [QK_WEND] = "wend",
        [QK_LOOP] = "loop",
        [QK_UNTIL] = "until",
        [QK_SELECT] = "select",
        [QK_PRINT] = "print",
        [QK_MATCH] = "match",
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

    uint32_t n = 0;

    for (uint32_t i = 0; i < e->n; i++) { /* a fn checked again, as an instance, says it once */
        const QErr *const x = &e->at[i];
        const QErr *const y = n ? &e->at[n - 1] : nullptr;

        if (!y || x->line != y->line || x->col != y->col || x->len != y->len ||
            memcmp(x->text, y->text, x->len))
            e->at[n++] = *x;
    }

    e->n = n;
}

uint32_t qast_parts(const QTy *const y)
{
    switch (y->kind) {
    case QT_REC:
    case QT_TUPLE:
        return y->nfields;

    case QT_LIST:
    case QT_ARRAY:
    case QT_OPT:
    case QT_RES:
        return 1;

    default:
        return 0;
    }
}

QType qast_part(const QTy *const y, const uint32_t k)
{
    return y->kind == QT_LIST || y->kind == QT_ARRAY ? y->elem
           : y->kind == QT_OPT || y->kind == QT_RES  ? y->fields[QF_VAL].type
                                                     : y->fields[k].type;
}

bool qast_drops(const QProg *const p, const QType t)
{
    if (!qast_compound(t))
        return false;

    QTy *const y = qast_ty(p, t);

    if (y->drop)
        return true;
    if (y->seen)
        return false;

    y->seen = true;
    bool any = false;

    for (uint32_t k = 0; !any && k < qast_parts(y); k++)
        any = qast_drops(p, qast_part(y, k));

    y->seen = false;
    return any;
}

uint32_t qast_dropdepth(const QProg *const p, const QType t)
{
    if (!qast_drops(p, t))
        return 0;

    QTy *const y = qast_ty(p, t);

    if (y->seen)
        return 0;

    y->seen = true;
    uint32_t most = 0;

    for (uint32_t k = 0; k < qast_parts(y); k++) {
        const uint32_t d = qast_dropdepth(p, qast_part(y, k));
        most = d > most ? d : most;
    }

    y->seen = false;
    return most + (y->kind == QT_LIST || y->kind == QT_ARRAY);
}
