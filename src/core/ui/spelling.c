/* Names as the program spells them: the debugger shows a name as the program typed it.
 *
 * # How it works
 * A stored line keeps each name as typed (M_VAR's text); one name id may be spelled many ways. A
 * variable is spelled as the first line of its scope that names it: BASIC declares before it uses.
 * A field is spelled as its TYPE's line for it (its name, then AS), which no variable's spelling
 * comes from. spel_read walks a procedure's lines, then its file's module lines, once, keeping the
 * first spelling of each (name, kind) in a hash table, its text in a pool.
 */
#include "ui/spelling.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/declare.h"
#include "comp/dfa.h"
#include "ui/linemap.h"
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

/* The name token at tok spells its name as a kind, unless an earlier one did. */
static void seen(const uint8_t *const tok, const SpelKind kind)
{
    const uint32_t key = key_of(comp_nameof(tok), kind);
    Spelling *const e = slot(key);
    const uint8_t len = (uint8_t)(tok[1] - !!comp_suffixof(tok));

    if (!e || e->key || sp.used + len > POOL)
        return;

    *e = (Spelling){.key = key, .len = len, .off = (uint16_t)sp.used};

    memcpy(sp.pool + sp.used, tok + 2, len);
    sp.used += len;
}

/* View v's lines' names: each a variable's of kind, a TYPE's field line's a field's. */
static void view_names(Session *const S, const uint32_t v, const SpelKind kind)
{
    const View *const w = view_at(v);

    for (uint32_t line = w->first; line < w->first + w->n; line++) {
        const uint8_t *p = S->comp.cbuf;
        prog_body(&S->prog, lmap_off(S, line), S->comp.cbuf);

        if (*p == M_VAR && *comp_skipname(p) == TK_AS) {
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

    if ((!e || !e->key) && kind == SPEL_LOCAL)
        e = slot(key_of(name, SPEL_MODULE));

    if (e && e->key) {
        memcpy(d, sp.pool + e->off, e->len);
        return d + e->len;
    }

    const uint8_t *const t = symb_text(&S->symb, name);
    memcpy(d, t + 1, t[0]);
    return d + t[0];
}
