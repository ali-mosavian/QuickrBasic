/* The source dialects: each one's table, and which one a file, the program or a line is.
 *
 * # How it works
 * A dialect is a table of operations (dialect.h). QBasic's is the core's own: its lines are
 * stored as tokens, and the loader, the editor and the REPL's own paths are its. Every other
 * dialect is a text dialect: its lines are stored as typed (LF_TEXT) and the core asks it, by its
 * hooks, whatever it would do itself for QBasic's.
 *
 * A text dialect's file says so by its first statement, its mark: before it, only lines that are
 * none to it (Dialect.blank). So does the store: each file of a text dialect's lines has its mark
 * among its first records, and the program's first file says what the program is. A dialect's
 * state in a session (Dial) is made on first use, from the C side's heap, so the session holds
 * none of its types.
 *
 * A new dialect is its table and its name in DIALECTS.
 */
#include "io/dialect.h"
#include "session.h"
#include "plat/pmem.h"

/* Each dialect's table, dialect_NAME: QBasic's first. */
#define DIALECTS(X) X(qbasic) X(quickr)

#define DIAL_EXTERN(n) extern const Dialect dialect_##n;
#define DIAL_ENTRY(n) &dialect_##n,
DIALECTS(DIAL_EXTERN)
static const Dialect *const dialects[] = {DIALECTS(DIAL_ENTRY)};
static_assert(countof(dialects) <= DIAL_MAX);

uint32_t dial_count(void) { return countof(dialects); }
const Dialect *dial_at(const uint32_t i) { return dialects[i]; }

const Dialect *dial_of_file(const char *const path)
{
    for (uint32_t i = 0; i < countof(dialects); i++)
        if (dialects[i]->text && dialects[i]->is_file(path))
            return dialects[i];

    return &dialect_qbasic;
}

/* The text dialect file file's lines from record rec, its first, mark: the first that is a
   statement to each (Dialect.blank), the mark one's; the first text dialect when none. */
static const Dialect *marked(const Prog *const pg, const uint32_t rec)
{
    const uint16_t file = prog_line(pg, rec)->file;
    const Dialect *first = nullptr;

    for (uint32_t i = 0; i < countof(dialects) && !first; i++)
        first = dialects[i]->text ? dialects[i] : nullptr;

    for (uint32_t r = rec; prog_line(pg, r)->len; r += prog_line(pg, r)->len) {
        const Line *const l = prog_line(pg, r);

        if ((l->flags & (LF_TEXT | LF_INC)) != LF_TEXT || l->file != file)
            continue;

        const char *const text = (const char *)at(prog_addr(pg, r) + HDR);
        const uint32_t len = (uint32_t)strlen(text);
        bool blank = false; /* to a dialect: the mark may come after it */

        for (uint32_t i = 0; i < countof(dialects); i++) {
            if (!dialects[i]->text)
                continue;
            if (dialects[i]->marker(text, len))
                return dialects[i];

            blank |= dialects[i]->blank(text, len);
        }

        if (!blank)
            break;
    }

    return first;
}

const Dialect *dial_program(const Prog *const pg)
{
    return prog_text(pg) ? marked(pg, PROGBASE) : &dialect_qbasic;
}

const Dialect *dial_line(const Prog *const pg, const uint32_t rec)
{
    const Line *const l = prog_line(pg, rec);

    if (!l->len) /* the end: a line typed there the program's */
        return dial_program(pg);
    if (!(l->flags & LF_TEXT))
        return &dialect_qbasic;

    for (uint32_t r = PROGBASE; prog_line(pg, r)->len; r += prog_line(pg, r)->len) {
        const Line *const f = prog_line(pg, r);

        if ((f->flags & (LF_TEXT | LF_INC)) == LF_TEXT && f->file == l->file)
            return marked(pg, r);
    }

    return marked(pg, rec);
}

const Dialect *dial_file(const Prog *const pg, const uint32_t file)
{
    for (uint32_t r = PROGBASE; prog_line(pg, r)->len; r += prog_line(pg, r)->len) {
        const Line *const l = prog_line(pg, r);

        if (l->file == file && !(l->flags & (LF_INC | LF_TOP)))
            return l->flags & LF_TEXT ? marked(pg, r) : &dialect_qbasic;
    }

    return &dialect_qbasic;
}

uint32_t dial_index(const Dialect *const d)
{
    uint32_t i = 0;

    while (dialects[i] != d)
        i++;

    return i;
}

void *dial_state(Dial *const dl, const Dialect *const d)
{
    void **const s = &dl->of[dial_index(d)];

    if (!*s && (*s = pmem_resize(nullptr, d->size)))
        memset(*s, 0, d->size);

    return *s;
}

void *dial_kept(const Dial *const dl, const Dialect *const d) { return dl->of[dial_index(d)]; }

void dial_forget(Session *const S)
{
    for (uint32_t i = 0; i < countof(dialects); i++)
        if (dialects[i]->forget)
            dialects[i]->forget(S);
}

const char *dial_errtext(const Session *const S, const uint8_t code)
{
    for (uint32_t i = 0; i < countof(dialects); i++) {
        const char *const t = dialects[i]->errtext ? dialects[i]->errtext(S, code) : nullptr;

        if (t)
            return t;
    }

    return nullptr;
}
