/* IMPORT and FROM statements.
 *
 * # How it works
 *   IMPORT module [AS name] [, module [AS name]]..
 *   FROM [.]..module | .. IMPORT * | name[()] [AS name] [, name[()] [AS name]]..
 * A module is a name token, its parts joined by dots (lexer.c makes `lib.gfx` one token); FROM's
 * leading dots are '.' bytes. One scanner reads them for the compiler, which checks the statement,
 * and for the loader (io/repl.c), which finds the modules a line names.
 *
 * The statement runs each module it names, IMPORT's or FROM's: an IMPORT op (vm/flow.c) runs its
 * code from its first line, the first time only. Its code ends at the END past its lines (LF_TOP,
 * io/repl.c), a MODEND op, which goes back past the IMPORT; with no IMPORT running, it ENDs, as the
 * main's.
 */
#include "comp/import.h"
#include "comp/dfa.h"
#include "io/msg.h"
#include "mem/module.h"

static bool at_end(const uint8_t c) { return !c || c == ':' || c == TK_REM; }

/* The name token at p as a module's: letters and digits, parts joined by single dots. */
static bool module_text(const uint8_t *const p)
{
    const uint8_t len = p[1], *const t = p + 2;

    for (uint32_t i = 0; i < len; i++)
        if (!(is_letter(t[i]) || (is_digit(t[i]) && i) ||
              (t[i] == '.' && i && i + 1 < len && t[i - 1] != '.')))
            return false;

    return len;
}

/* The name token at p as an AS's: a module's with no dot. */
static bool alias_text(const uint8_t *const p)
{
    return module_text(p) && !memchr(p + 2, '.', p[1]);
}

static void give(ImptEach *const each, void *const ctx, const ImptKind kind, const ImptName n)
{
    if (each)
        each(ctx, kind, &n);
}

/* [AS name] at *p, n's: *p past it. false: AS without a name. */
static bool alias(const uint8_t **const p, ImptName *const n)
{
    if (comp_peek(p) != TK_AS)
        return true;

    ++*p;

    if (comp_peek(p) != M_VAR || !alias_text(*p))
        return false;

    n->as = *p + 2;
    n->aslen = (*p)[1];
    *p = comp_skipname(*p);
    return true;
}

/* IMPORT's list at p. */
static const uint8_t *modules(const uint8_t *p, ImptEach *const each, void *const ctx, bool *const ok)
{
    for (;;) {
        if (comp_peek(&p) != M_VAR || !module_text(p))
            return p;

        ImptName n = {.text = p + 2, .len = p[1]};
        p = comp_skipname(p);

        if (!alias(&p, &n))
            return p;

        give(each, ctx, IMPT_MODULE, n);

        if (comp_peek(&p) != ',') {
            *ok = at_end(comp_peek(&p));
            return p;
        }

        p++;
    }
}

/* FROM's names after its IMPORT, at p. */
static const uint8_t *names(const uint8_t *p, ImptEach *const each, void *const ctx, bool *const ok)
{
    if (comp_peek(&p) == TK_MUL) {
        p++;
        *ok = at_end(comp_peek(&p));
        give(each, ctx, IMPT_STAR, (ImptName){});
        return p;
    }

    for (;;) {
        if (comp_peek(&p) != M_VAR || memchr(p + 2, '.', p[1]))
            return p;

        const uint8_t sfx = comp_suffixof(p);
        ImptName n = {.text = p + 2, .len = (uint8_t)(p[1] - !!sfx), .sfx = sfx};
        p = comp_skipname(p);

        if (comp_peek(&p) == '(') {
            p++;

            if (comp_peek(&p) != ')')
                return p;

            p++;
            n.arr = true;
        }

        if (!alias(&p, &n))
            return p;

        give(each, ctx, IMPT_NAME, n);

        if (comp_peek(&p) != ',') {
            *ok = at_end(comp_peek(&p));
            return p;
        }

        p++;
    }
}

/* FROM's module and names at p, past FROM. */
static const uint8_t *from(const uint8_t *p, ImptEach *const each, void *const ctx, bool *const ok)
{
    ImptName m = {};

    while (comp_peek(&p) == '.' && m.up < UINT8_MAX) {
        m.up++;
        p++;
    }

    if (comp_peek(&p) == M_VAR) {
        if (!module_text(p))
            return p;

        m = (ImptName){.text = p + 2, .len = p[1], .up = m.up};
        p = comp_skipname(p);
    }

    if (!m.len && !m.up)
        return p;
    if (comp_peek(&p) != TK_X || p[1] != TK1_IMPORT)
        return p;

    give(each, ctx, IMPT_FROM, m);
    return names(p + 2, each, ctx, ok);
}

const uint8_t *impt_scan(const uint8_t *p, ImptEach *const each, void *const ctx, bool *const ok)
{
    *ok = false;

    if (comp_peek(&p) != TK_X || (p[1] != TK1_IMPORT && p[1] != TK1_FROM))
        return p;

    const bool imports = p[1] == TK1_IMPORT;
    p += 2;
    return imports ? modules(p, each, ctx, ok) : from(p, each, ctx, ok);
}

bool impt_line(const uint8_t *body, ImptEach *const each, void *const ctx)
{
    const uint8_t *const colon = comp_label(body); /* a label: on from its ':' */
    bool ok;

    if (colon)
        body = colon + 1;
    if (comp_peek(&body) != TK_X || (body[1] != TK1_IMPORT && body[1] != TK1_FROM))
        return false;

    impt_scan(body, nullptr, nullptr, &ok); /* the names given only once it is all right */
    return ok && (impt_scan(body, each, ctx, &ok), true);
}

/* IMPORT module line: module's code run from its first line, past its LF_TOP. */
static void run(void *const ctx, const uint32_t module)
{
    Comp *const C = ctx;
    const uint32_t top = modu_top_of(C->modu, module);
    Node *const n = comp_xop(C, X_IMPORT);
    comp_dword(n, module);
    comp_dword(n, top + prog_line(C->prog, top)->len);
}

void impt_compile(Comp *const C)
{
    if (C->line == DLINE && !C->check) /* the editor's check: a line of the program */
        fault_error(C->fault, E_ID);
    if (C->proc) {
        C->dg = DG_ILLEGAL_INSIDE_SUB_OR_FUNCTION;
        fault_error(C->fault, E_SN);
    }
    if (C->nstmt != 1) {
        C->dg = DG_SYNTAX_ERROR_IMPORT;
        fault_error(C->fault, E_SN);
    }

    bool ok;
    const uint8_t *const stmt = C->p;
    C->p = impt_scan(C->p, nullptr, nullptr, &ok);

    if (!ok)
        fault_error(C->fault, E_SN);
    if (!C->check)
        C->impt->find(C->impt, C->line, stmt, run, C);
}
