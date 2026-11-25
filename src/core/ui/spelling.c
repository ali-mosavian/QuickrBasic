/* Names as the program spells them: the debugger shows a name as the program typed it.
 *
 * # How it works
 * A stored line keeps each name as typed (M_VAR's text); one name id may be spelled many ways. A
 * variable is spelled as the first line of its scope that names it: BASIC declares before it uses.
 * A field is spelled as its TYPE's line for it (its name, its subscripts, AS), which no variable's
 * comes from. spel_read walks a procedure's lines, then its file's module lines, once, keeping the
 * first spelling of each (name, kind) in a hash table, its text in a pool.
 */
#include "ui/spelling.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "ui/linemap.h"
#include "ui/text.h"
#include "vm/inspect.h"
#include "ui/view.h"

enum {
    NSPELL = 1024, /* spellings kept at most: a power of two */
    POOL = 16384,  /* their text's bytes */
};

/* A name's spelling as a kind: its text in the pool. */
typedef struct {
    uint32_t key; /* name << 2 | kind + 1; 0 an empty slot */
    uint8_t len;
    uint16_t off;
} Spelling;

static struct {
    Spelling e[NSPELL];
    uint32_t used; /* the pool's */
    char pool[POOL];
} sp;

static uint32_t key_of(const uint32_t name, const SpelKind kind) { return name << 2 | (kind + 1u); }

/* key's slot: its entry, else the empty one it would take; nullptr the table full. */
static Spelling *slot(const uint32_t key)
{
    uint32_t h = key * 2654435761u;

    for (uint32_t i = 0; i < NSPELL; i++, h++) {
        Spelling *const e = &sp.e[h & (NSPELL - 1)];

        if (!e->key || e->key == key)
            return e;
    }

    return nullptr;
}

/* Name name spelled as text, len bytes, as a kind, unless an earlier one was. */
static void spelled(
    const uint32_t name,
    const uint8_t *const text,
    const uint8_t len,
    const SpelKind kind
)
{
    const uint32_t key = key_of(name, kind);
    Spelling *const e = slot(key);

    if (!e || e->key || sp.used + len > POOL)
        return;

    *e = (Spelling){.key = key, .len = len, .off = (uint16_t)sp.used};

    memcpy(sp.pool + sp.used, text, len);
    sp.used += len;
}

/* The name token at tok spells its name as a kind, unless an earlier one did. */
static void seen(const uint8_t *const tok, const SpelKind kind)
{
    spelled(comp_nameof(tok), tok + 2, (uint8_t)(tok[1] - !!comp_suffixof(tok)), kind);
}

/* A text dialect's line's names: its module's. */
typedef struct {
    const Session *S;
    uint32_t module;
} TextNames;

/* Dialect.names's: a text dialect's line's name, n bytes at w, a name's of its module, else of
   the main's (a text dialect's program's names are all its). */
static void text_name(void *const ctx, const uint8_t *const w, const uint32_t n)
{
    const TextNames *const t = ctx;
    const uint8_t len = (uint8_t)(n < UINT8_MAX ? n : UINT8_MAX);
    uint32_t name = symb_look_name(&t->S->symb, t->module, w, len);

    if (!name && t->module)
        name = symb_look_name(&t->S->symb, 0, w, len);
    if (name)
        spelled(name, w, len, SPEL_MODULE);
}

/* The line at p is a TYPE's field's: its name, its subscripts if any, then AS. */
static bool field_line(const uint8_t *const p)
{
    if (*p != M_VAR)
        return false;

    const uint8_t *q = comp_skipname(p);

    for (int32_t depth = 0; *q == '(' || depth; q = decl_item(q)) {
        if (!*q)
            return false;

        depth += (*q == '(') - (*q == ')');
    }

    return *q == TK_AS;
}

/* View v's lines' names: each a variable's of kind, a TYPE's field line's a field's. */
static void view_names(Session *const S, const uint32_t v, const SpelKind kind)
{
    const View *const w = view_at(v);
    const Dialect *d = nullptr; /* a text file's, from its first line */

    for (uint32_t line = w->first; line < w->first + w->n; line++) {
        const uint8_t *p = S->comp.cbuf;
        prog_body(&S->prog, lmap_off(S, line), S->comp.cbuf);

        if (prog_line(&S->prog, lmap_off(S, line))->flags & LF_TEXT) {
            if (!d)
                d = dial_line(&S->prog, lmap_off(S, line));

            d->names(p, text_name, &(TextNames){S, w->module});

            continue;
        }

        if (field_line(p)) {
            seen(p, SPEL_FIELD);
            continue;
        }

        for (; *p && *p != TK_REM && *p != TK_DATA; p = decl_item(p))
            if (*p == M_VAR)
                seen(p, kind);
    }
}

void spel_read(Session *const S, const uint32_t view)
{
    memset(sp.e, 0, sizeof sp.e);
    sp.used = 0;
    const uint32_t module = view_of_module(view_module(view));

    if (view != module)
        view_names(S, view, SPEL_LOCAL);

    view_names(S, module, SPEL_MODULE);
}

char *spel_name(const Session *const S, char *const d, const uint32_t name, const SpelKind kind)
{
    const Spelling *e = slot(key_of(name, kind));

    if ((!e || !e->key) && (kind == SPEL_LOCAL || (kind == SPEL_FIELD && prog_text(&S->prog))))
        e = slot(key_of(name, SPEL_MODULE));

    if (e && e->key) {
        memcpy(d, sp.pool + e->off, e->len);
        return d + e->len;
    }

    return insp_name(S, d, name);
}

char *spel_var(const Session *const S, char *d, const InspVar *const var)
{
    for (uint32_t i = 0; i < dial_count(); i++) { /* a text dialect's as it typed it */
        const Dialect *const dl = dial_at(i);
        const uint32_t typed = dl->text ? dl->spell(S, var->id, d) : 0;

        if (typed)
            return d + typed;
    }

    d = spel_name(S, d, var->name, var->local ? SPEL_LOCAL : SPEL_MODULE);

    if (var->sfx && dialect_qbasic.suffix) /* no text dialect's: QBasic's */
        *d++ = var->sfx;

    return d;
}
