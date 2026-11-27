/* Quickr's completion: what fits at the word being typed (qr_suggest).
 *
 * # How it works
 * After a '.', the members of what is before it: a module's public names, a value's fields and
 * methods, a type's values. Else the file's names and the keywords that fit. The parser judges
 * a keyword: the file's lines before the cursor's parsed (qpar_upto), then the line so far and the
 * keyword parsed as one statement there (qpar_fits). One that fits closes when it ends the block
 * open there or adds its clause (next, else, case); one that wants a keyword after it (end) closes
 * when such a pair does.
 */
#include "qr/qr.h"
#include "qr/qr_int.h"
#include "session.h"
#include "comp/bind.h"
#include "comp/export.h"
#include "qr/qast.h"
#include "qr/qlex.h"
#include "qr/qpar.h"
#include "qr/qsem.h"

static void qr_no_token(void *const ctx, const QTok *const t) {}

typedef struct {
    Session *S;
    QMem *m;
    DialSuggFn *fn;
    void *ctx;
} QrSugg;

/* File file's next text line past rec, 0 its first; 0 none. */
static uint32_t qr_next_line(const Prog *const pg, const uint32_t file, const uint32_t rec)
{
    uint32_t r = rec ? rec + prog_line(pg, rec)->len : PROGBASE;

    for (; prog_line(pg, r)->len; r += prog_line(pg, r)->len)
        if (qr_text(prog_line(pg, r), file))
            return r;

    return 0;
}

/* Line rec's tokens to *toks: their count. */
static uint32_t qr_line_toks(const QrSugg *const g, const uint32_t rec, const QTok **const toks)
{
    const char *const body = (const char *)at(prog_addr(&g->S->prog, rec) + HDR);
    return qr_lexed(g->m, body, (uint32_t)strlen(body), toks);
}

/* Token t is the name text, n bytes. */
static bool qr_named(const QTok *const t, const char *const text, const uint32_t n)
{
    return t->kind == QK_NAME && t->len == n && !memcmp(t->text, text, n);
}

/* The module named name, n bytes: its file's name less its extension; modu_count none. */
static uint32_t qr_module_named(const Session *const S, const char *const name, const uint32_t n)
{
    for (uint32_t k = 0; k < modu_count(&S->modu); k++) {
        const char *base = modu_module_path(&S->modu, k);

        for (const char *c = base; *c; c++)
            if (*c == '/' || *c == '\\')
                base = c + 1;

        uint32_t i = 0;

        while (i < n && base[i] && upcase((uint8_t)base[i]) == upcase((uint8_t)name[i]))
            i++;

        if (i == n && base[n] == '.')
            return k;
    }

    return modu_count(&S->modu);
}

/* The file module k's lines are: a QBasic program's Quickr module's its own, else k. */
static uint32_t qr_file_of(Session *const S, const uint32_t k)
{
    const ModuMod *const mod = modu_mod(&S->modu, k);
    return mod->dialect ? mod->file : k;
}

/* File file's lines parsed, p's: its module; null not Quickr's. */
static const QMod *qr_parsed(const QrSugg *const g, const uint32_t file, QProg *const p)
{
    if (dial_file(&g->S->prog, file) != &dialect_quickr)
        return nullptr;

    *p = qr_loose(g->m);
    qr_lines(g->S, p, g->m, file);
    return &p->mods[0];
}

/* Name t, kind k, of statement s of p's to g. */
static void qr_offer(
    const QrSugg *const g,
    const QProg *const p,
    const QStmt *const s,
    const QTok *const t,
    const DialSuggKind k
)
{
    g->fn(g->ctx, t->text, t->len, k, p->src.lines[s->line].at);
}

/* Module mod's public names, of p's: its functions, types, enums, consts, vars and lets. */
static void qr_publics(const QrSugg *const g, const QProg *const p, const QMod *const mod)
{
    for (QStmt *s = mod->head; s; s = qsem_nextin(mod, s)) {
        const QFn *const f = s->kind == QS_FN ? s->fn : nullptr;

        if (f && !f->priv && !f->in && !f->op && !f->test)
            qr_offer(g, p, s, f->name, DSG_FN);
        else if ((s->kind == QS_TYPE || s->kind == QS_ENUM) && !s->priv)
            qr_offer(g, p, s, s->decls->name, DSG_NAME);
        else if (s->kind == QS_DECL && !s->priv)
            for (const QDecl *d = s->decls; d; d = d->next)
                qr_offer(g, p, s, d->name, DSG_NAME);
    }
}

/* A QBasic module's export t, kinds k, as written, to the QrSugg ctx: one Quickr can use. */
static void qr_qbexport(void *const ctx, const uint32_t t, const uint8_t k)
{
    const QrSugg *const g = ctx;

    if (!expo_usable(&g->S->comp, t))
        return;

    const uint8_t *const text = symb_text(&g->S->symb, t);
    g->fn(g->ctx, (const char *)text + 1 + text[0], text[0], k & BIND_PROC ? DSG_FN : DSG_NAME, 0);
}

/* Module k's public names: a Quickr module's; a QBasic module's SUBs, FUNCTIONs and TYPEs. */
static void qr_members(const QrSugg *const g, const uint32_t k)
{
    QProg p;
    const QMod *const mod = qr_parsed(g, qr_file_of(g->S, k), &p);

    if (mod)
        qr_publics(g, &p, mod);
    else
        bind_each_export(
            &g->S->scop,
            &g->S->symb,
            k,
            BIND_PROC | BIND_TYPE,
            qr_qbexport,
            (void *)g
        );
}

/* A value's type as declared: a record's or an enum's name; else what built-in methods it has
   (QM_*). */
typedef struct {
    const QTok *rec;
    uint8_t on;
} QrValue;

/* The value of type tx: a list's, an array's, a dict's, a string's, a type's by name. */
static QrValue qr_typed(const QTypeEx *const tx)
{
    if (!tx)
        return (QrValue){};

    switch (tx->kind) {
    case QX_LIST:
        return (QrValue){.on = QM_LIST};

    case QX_ARRAY:
        return (QrValue){.on = QM_ARRAY};

    case QX_DICT:
        return (QrValue){.on = QM_DICT};

    case QX_GEN: /* Name[T]: Name's */
        return qr_typed(tx->elem);

    case QX_NAME:
        return tx->tok->len == 6 && !memcmp(tx->tok->text, "string", 6) ? (QrValue){.on = QM_STRING}

                                                                        : (QrValue){.rec = tx->tok};

    default:
        return (QrValue){};
    }
}

/* The value e gives, as written: a string's, a call's of [m.]T(..). */
static QrValue qr_given(const QExpr *const e)
{
    if (e && (e->kind == QE_FSTR || (e->kind == QE_CONST && e->type == QY_STRING)))
        return (QrValue){.on = QM_STRING};

    return e && e->kind == QE_CALL ? (QrValue){.rec = e->tok} : (QrValue){};
}

/* Variable name's (n bytes) value as file declares it: x as T, let x = T(..), a parameter x as
   T; none found, neither part set. */
static QrValue qr_value_of(
    const QrSugg *const g,
    const uint32_t file,
    const char *const name,
    const uint32_t n
)
{
    QProg p;
    const QMod *const mod = qr_parsed(g, file, &p);

    for (const QStmt *s = mod ? mod->head : nullptr; s; s = s == mod->tail ? nullptr : s->next) {
        for (const QDecl *d = s->kind == QS_DECL ? s->decls : nullptr; d; d = d->next)
            if (qr_named(d->name, name, n))
                return d->type ? qr_typed(d->type) : qr_given(d->value);

        for (uint32_t i = 0; s->kind == QS_FN && i < s->fn->npar; i++)
            if (qr_named(s->fn->params[i].name, name, n))
                return qr_typed(s->fn->params[i].type);
    }

    return (QrValue){};
}

/* A built-in method's name to the QrSugg ctx. */
static void qr_method(void *const ctx, const char *const name)
{
    const QrSugg *const g = ctx;
    g->fn(g->ctx, name, (uint32_t)strlen(name), DSG_FN, 0);
}

/* Is token t the name text, n bytes, case aside? */
static bool qr_named_nocase(const uint8_t *const t, const uint8_t n, const char *const text)
{
    uint32_t i = 0;

    while (i < n && t[i] == upcase((uint8_t)text[i]))
        i++;

    return i == n;
}

/* A QBasic module's TYPE: the name sought, the QrSugg its fields go to. */
typedef struct {
    const QrSugg *g;
    const char *name;
    uint32_t n;
} QrQbType;

/* Export t, a TYPE: its fields to the QrQbType ctx when it is the one sought. */
static void qr_qbfields(void *const ctx, const uint32_t t, const uint8_t k)
{
    const QrQbType *const q = ctx;
    const Session *const S = q->g->S;
    const uint8_t *const text = symb_text(&S->symb, t);

    if (text[0] != q->n || !qr_named_nocase(text + 1, text[0], q->name))
        return;

    const TypeDef *const td = decl_type(&S->decl, symb_name(&S->symb, t)->rec);

    for (uint32_t f = 0; f < td->nfld; f++) {
        const uint8_t *const ft = symb_text(&S->symb, td->fields[f].name);
        q->g->fn(q->g->ctx, (const char *)ft + 1 + ft[0], ft[0], DSG_NAME, 0);
    }
}

/* Type t's (tn bytes) members, of module mod of p's, a private one there only when own: on a
   value its fields and its methods with self; on the type its methods without self and an
   enum's values. */
static void qr_members_of(
    const QrSugg *const g,
    const QProg *const p,
    const QMod *const mod,
    const char *const t,
    const uint32_t tn,
    const bool value,
    const bool own
)
{
    const QStmt *in = nullptr; /* type t's statement, while in its lines */

    for (QStmt *s = mod->head; s; s = qsem_nextin(mod, s)) {
        const QFn *const f = s->kind == QS_FN ? s->fn : nullptr;

        if (s->kind == QS_TYPE || s->kind == QS_ENUM)
            in = s->decls->name->len == tn && !memcmp(s->decls->name->text, t, tn) ? s : nullptr;
        else if (s->kind == QS_ENDTYPE || s->kind == QS_ENDENUM)
            in = nullptr;
        else if (!in)
            continue;
        else if (f && f->in == in && f->self == value && (own || !f->priv))
            qr_offer(g, p, s, f->name, DSG_FN);
        else if (s->kind == QS_FIELD && value && (own || !s->priv))
            qr_offer(g, p, s, s->decls->name, DSG_NAME);
        else if (s->kind == QS_VARIANT && !value)
            qr_offer(g, p, s, s->decls->name, DSG_NAME);
    }
}

/* Type t's (tn bytes) members as any module declares them, a private one file's alone: a Quickr
   type's qr_members_of; a QBasic TYPE's fields on a value. */
static void qr_fields(
    const QrSugg *const g,
    const uint32_t file,
    const char *const t,
    const uint32_t tn,
    const bool value
)
{
    for (uint32_t k = 0; k < modu_count(&g->S->modu); k++) {
        const uint32_t kf = qr_file_of(g->S, k);
        QProg p;
        const QMod *const mod = qr_parsed(g, kf, &p);
        QrQbType q = {g, t, tn};

        if (mod)
            qr_members_of(g, &p, mod, t, tn, value, kf == file);
        else if (value)
            bind_each_export(&g->S->scop, &g->S->symb, k, BIND_TYPE, qr_qbfields, &q);
    }
}

/* Keyword k at col after the line's np tokens, then a token of kind k2 (QK_EOF none): whether
   the last fits where a leaves off, and closes. */
static QFit qr_probe(
    QParAt *const a,
    QTok *const line,
    const uint32_t np,
    const uint32_t col,
    const QKind k,
    const QKind k2
)
{
    const char *const w = qlex_keyword(k);
    QTok *t = &line[np];
    *t = (QTok){
        .kind = k,
        .col = col,
        .len = (uint32_t)strlen(w),
        .text = w,
    };

    if (k2 != QK_EOF) {
        const char *const w2 = k2 == QK_NAME ? "x" : qlex_keyword(k2);
        t[1] = (QTok){
            .kind = k2,
            .col = col + t->len + 1,
            .len = (uint32_t)strlen(w2),
            .text = w2,
        };

        t++;
    }

    t[1] = (QTok){.kind = QK_NL, .col = t->col + t->len};

    t[2] = (QTok){.kind = QK_EOF, .col = t->col + t->len};

    return qpar_fits(a, line, t);
}

/* Keyword k at col after the line's np tokens: whether it fits, and closes, alone or with the
   keyword it wants after it (end if). */
static QFit qr_fit(
    QParAt *const a,
    QTok *const line,
    const uint32_t np,
    const uint32_t col,
    const QKind k
)
{
    const QFit alone = qr_probe(a, line, np, col, k, QK_EOF);

    if (alone != QFIT_YES || qr_probe(a, line, np, col, k, QK_NAME) != QFIT_NO)
        return alone;

    for (QKind w = QK_AND; w <= QK_FUNCTION; w++)
        if (qr_probe(a, line, np, col, k, w) == QFIT_CLOSES)
            return QFIT_CLOSES;

    return QFIT_YES;
}

/* The keywords that fit after text, the line's first n bytes, at line rec of file: a closer of
   the block open there DSG_CLOSE, one starting the line DSG_STMT. */
static void qr_keywords(
    const QrSugg *const g,
    const uint32_t file,
    const uint32_t rec,
    const char *const text,
    const uint32_t n
)
{
    QProg p;
    QParAt *const a = qr_upto(g->S, &p, g->m, file, rec);
    const QTok *before;
    const uint32_t np = qr_lexed(g->m, text, n, &before);
    QTok *const line = qast_alloc(g->m, (np + 4) * sizeof *line); /* a probe's 2 words, NL, EOF */
    memcpy(line, before, np * sizeof *line);

    for (QKind k = QK_AND; k <= QK_FUNCTION; k++) {
        const QFit f = qr_fit(a, line, np, n, k);
        const DialSuggKind kind = f == QFIT_CLOSES ? DSG_CLOSE : np ? DSG_KW : DSG_STMT;

        if (f != QFIT_NO)
            g->fn(g->ctx, qlex_keyword(k), (uint32_t)strlen(qlex_keyword(k)), kind, 0);
    }
}

/* File file's names, not a field's after '.'. */
static void qr_names_of(const QrSugg *const g, const uint32_t file)
{
    for (uint32_t rec = qr_next_line(&g->S->prog, file, 0); rec;

         rec = qr_next_line(&g->S->prog, file, rec)) {
        const QTok *toks;
        const uint32_t n = qr_line_toks(g, rec, &toks);

        for (uint32_t i = 0; i < n; i++)
            if (toks[i].kind == QK_NAME && (!i || toks[i - 1].kind != QK_DOT))
                g->fn(
                    g->ctx,
                    toks[i].text,
                    toks[i].len,
                    i && toks[i - 1].kind == QK_FUNCTION ? DSG_FN : DSG_NAME,
                    rec
                );
    }
}

void qr_suggest(
    Session *const S,
    const uint32_t rec,
    const char *const text,
    const uint32_t n,
    DialSuggFn *const fn,
    void *const ctx
)
{
    uint32_t quotes = 0;

    for (uint32_t i = 0; i < n; i++)
        quotes += text[i] == '"';

    if (quotes % 2 ||
        qr_tokens(&S->fault, text, n, qr_no_token, nullptr) < n) /* a string's, a comment's */
        return;

    QMem m = {.fault = &S->fault};

    arena_init(&m.arena, QR_LINE_CHUNK);
    const QrSugg g = {S, &m, fn, ctx};

    const uint32_t file = prog_line(&S->prog, rec)->len ? prog_line(&S->prog, rec)->file : 0;
    /* the name's end before a '.' at n - 1, past its [..]: Stack[int]. */
    uint32_t e = n ? n - 1 : 0;

    for (uint32_t depth = 0; e && (depth || text[e - 1] == ']');) {
        depth += text[e - 1] == ']';
        depth -= text[e - 1] == '[';
        e--;
    }

    uint32_t b = e;

    while (b && qr_namec(text[b - 1]))
        b--;

    if (!n || text[n - 1] != '.') {
        qr_keywords(&g, file, rec, text, n);
        qr_names_of(&g, file);
    } else if (b < e && !is_digit((uint8_t)text[b]) && b && text[b - 1] == '.') {
        uint32_t a = b - 1; /* m.T.: T module m's type */

        while (a && qr_namec(text[a - 1]))
            a--;

        if (a < b - 1 && qr_module_named(S, text + a, b - 1 - a) < modu_count(&S->modu))
            qr_fields(&g, file, text + b, e - b, false);
    } else if (b < e && !is_digit((uint8_t)text[b])) {
        const uint32_t k = qr_module_named(S, text + b, e - b);
        const QrValue v = qr_value_of(&g, file, text + b, e - b);

        if (k < modu_count(&S->modu) && e == n - 1)
            qr_members(&g, k);
        else if (v.rec)
            qr_fields(&g, file, v.rec->text, v.rec->len, true);
        else if (v.on)
            qsem_methods(v.on, qr_method, (void *)&g);
        else
            qr_fields(&g, file, text + b, e - b, false);
    }

    arena_reset(&m.arena);
}
