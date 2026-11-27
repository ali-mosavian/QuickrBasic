/* Quickr's lines' block keywords, as the editor's indent, pairs and block ends read them
 * (Dialect.blocks).
 *
 * # How it works
 * A line is one statement: its first word, past private or public, says what it does to a block
 * (qr_words), end its next word too (qr_ends); an if opens one only when then ends the line. A
 * line ending in a lambda's header (the lexer's qlex_header) opens one more, ended by end fn.
 *
 * # Example
 *   each(xs, fn(x as int)     a block opened at fn: its end end fn
 */
#include "qr/qr.h"
#include "qr/qlex.h"
#include "qr/qr_int.h"

/* Quickr's blocks' kinds. */
enum : uint8_t { QB_FOR, QB_WHILE, QB_DO, QB_IF, QB_SELECT, QB_FN, QB_TYPE, QB_ENUM };

/* What a statement's first word does to a block. */
static const struct {
    QKind word;
    DialBlock ev;
} qr_words[] = {
    {QK_FOR, {.kind = QB_FOR, .what = DBK_OPEN, .end = "next"}},
    {QK_NEXT, {.kind = QB_FOR, .what = DBK_END}},
    {QK_WHILE, {.kind = QB_WHILE, .what = DBK_OPEN, .end = "wend"}},
    {QK_WEND, {.kind = QB_WHILE, .what = DBK_END}},
    {QK_DO, {.kind = QB_DO, .what = DBK_OPEN, .end = "loop"}},
    {QK_LOOP, {.kind = QB_DO, .what = DBK_END}},
    {QK_ELSE, {.kind = QB_IF, .what = DBK_MID}},
    {QK_ELSEIF, {.kind = QB_IF, .what = DBK_MID}},
    {QK_SELECT, {.kind = QB_SELECT, .what = DBK_OPEN, .end = "end select"}},
    {QK_MATCH, {.kind = QB_SELECT, .what = DBK_OPEN, .end = "end select"}},
    {QK_CASE, {.kind = QB_SELECT, .what = DBK_MID, .inset = true}},
    {QK_FUNCTION, {.kind = QB_FN, .what = DBK_OPEN, .end = "end function"}},
    {QK_FN, {.kind = QB_FN, .what = DBK_OPEN, .end = "end fn"}},
    {QK_TEST, {.kind = QB_FN, .what = DBK_OPEN, .end = "end test"}},
    {QK_TYPE, {.kind = QB_TYPE, .what = DBK_OPEN, .end = "end type"}},
    {QK_ENUM, {.kind = QB_ENUM, .what = DBK_OPEN, .end = "end enum"}},
};

/* The block end's next word ends. */
static const struct {
    QKind word;
    uint8_t kind;
} qr_ends[] = {
    {QK_IF, QB_IF},
    {QK_SELECT, QB_SELECT},
    {QK_MATCH, QB_SELECT},
    {QK_FUNCTION, QB_FN},
    {QK_FN, QB_FN},
    {QK_TEST, QB_FN},
    {QK_TYPE, QB_TYPE},
    {QK_ENUM, QB_ENUM},
};

static const DialBlock qr_if = {.kind = QB_IF, .what = DBK_OPEN, .end = "end if"};
static const DialBlock qr_lambda = {.kind = QB_FN, .what = DBK_OPEN, .end = "end fn"};

/* e at tokens a to b: ev's next, of its n; their count. */
static uint32_t qr_add(
    DialBlock *const ev,
    const uint32_t n,
    DialBlock e,
    const QTok *a,
    const QTok *b
)
{
    e.lead = true; /* the line's one statement */
    e.col = (uint16_t)a->col;
    e.len = (uint16_t)(b->col + b->len - a->col);

    if (n < DIAL_NBLOCK)
        ev[n] = e;

    return n < DIAL_NBLOCK ? n + 1 : n;
}

/* What the statement's first word, t of the n tokens toks, does to a block: ev's, of its k. */
static uint32_t qr_word(
    DialBlock *const ev,
    const uint32_t k,
    const QTok *const toks,
    const uint32_t n,
    const uint32_t t
)
{
    const QTok *const w = &toks[t];

    if (w->kind == QK_IF)
        return toks[n - 1].kind == QK_THEN ? qr_add(ev, k, qr_if, w, w) : k;

    if (w->kind == QK_END) {
        for (uint32_t i = 0; t + 1 < n && i < countof(qr_ends); i++)
            if (toks[t + 1].kind == qr_ends[i].word)
                return qr_add(
                    ev,
                    k,
                    (DialBlock){.kind = qr_ends[i].kind, .what = DBK_END},
                    w,
                    &toks[t + 1]
                );

        return k;
    }

    if (w->kind == QK_FN &&
        (t + 1 == n || (toks[t + 1].kind != QK_NAME && toks[t + 1].kind != QK_OPERATOR)))
        return k; /* a lambda: by its header */

    for (uint32_t i = 0; i < countof(qr_words); i++)
        if (w->kind == qr_words[i].word)
            return qr_add(ev, k, qr_words[i].ev, w, w);

    return k;
}

uint32_t qr_blocks(Fault *const f, const char *const text, const uint32_t len, DialBlock *const ev)
{
    QMem m = {.fault = f};

    arena_init(&m.arena, QR_LINE_CHUNK);
    const QTok *toks;
    const uint32_t n = qr_lexed(&m, text, len, &toks);
    const uint32_t t = n && (toks[0].kind == QK_PRIVATE || toks[0].kind == QK_PUBLIC);
    uint32_t k = t < n ? qr_word(ev, 0, toks, n, t) : 0;
    const uint32_t fn = qlex_header(toks, t, n);

    if (fn < n)
        k = qr_add(ev, k, qr_lambda, &toks[fn], &toks[fn]);

    arena_free(&m.arena);
    return k;
}
