/* A line's declarations, entered into the scopes as the line is entered.
 *
 * # How it works
 * The declarations go in as QBasic's editor does, so lines compiled before one know it: DEFtype,
 * DIM [SHARED] .. AS, CONST, SUB, FUNCTION, DECLARE, DEF FN and their parameters, SHARED, STATIC,
 * COMMON, TYPE's fields, OPTION BASE and EXPLICIT. n( an array is known in scope from there on.
 *
 * Each goes into the binding of decl.scope: the module's or the procedure being entered's. DEFtype
 * holds from its line to the file's end: each loading line's is an event (DefEvent), and
 * decl_deftype sets the DEFtypes for the line compiled.
 *
 * # Example
 *    DIM a(10) AS INTEGER     a is an INTEGER array from here on
 *
 * # asm structure (retired/asm32/comp/decl.asm)
 * | asm | here | |
 * |---|---|---|
 * | dc_reset | decl_reset | MIRROR |
 * | dc_line | decl_line | MIRROR; D1, D3 |
 * | dc_deft | decl_deftype | MIRROR |
 * | dc_item, dc_tail, dc_step | decl_item, decl_tail, decl_step | MIRROR |
 * | dc_proc | decl_proc(body, &name) | MIRROR; PK_*, 0 |
 * | dc_dfev, dc_redim, dc_shdim, dc_bind, dc_nam, dc_use, dc_arr, dc_ause, dc_bnd, dc_next | decl_defevent, decl_redimscope, decl_shareddim, decl_dim1, decl_name, decl_use, decl_array, decl_arrayuse, decl_bound, decl_next | MIRROR |
 * | dc_warn | decl_warning; repl_declare formats it | D4 |
 * | g_typ, g_fld | Decl.types, TypeDef.fields (TypeDef: T_NAM T_NF T_SIZE, Field: F_NAM F_ET F_SUB F_FIX F_OFF) | D2 |
 * | g_obase, g_oexp, g_obset, g_odim, g_ndev, g_dscope, g_dredim | decl.base, explicit_, baseset, dimmed, ndefev, scope, redim | D2 |
 * | g_dfev, TDEFEV, NDEV, DEVSZ | Decl.defevs of DefEvent {line, deft[26]} | D2 |
 * | g_dast, g_etsz | decl_astype[5], decl_etsize[8] | MIRROR |
 * | g_dft0, g_dett | deft0, deftype | MIRROR |
 * | g_wpre .. g_wtail, g_wbuf | repl_warn (io/repl.c) | MOVED |
 * | lx.dproc, lx.ntyps, lx.nflds, lx.dtype | decl.proc, decl.ntypes, decl.nfields, decl.type | MOVED: lex's state before |
 *
 * - D1. The asm's reads g_cbuf; the C takes the body.
 * - D2. The tables live in Decl's arena: a TYPE holds its own fields; limits as the asm's.
 * - D3. It takes the tables, scopes and symbols it changes, and the line's place and loading
 *   flag; errors raise through Fault.
 * - D4. It names the statement; the REPL writes the warning.
 */
#include "comp/declare.h"
#include <string.h>
#include "comp/comp.h"
#include "comp/dfa.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/symbols.h"

enum { DECL_CHUNK = 0x2000 };

/* decl_line's scan: where declarations go, and the body's cursor. */
typedef struct {
    Decl *d;
    Scop *sc;
    Symb *sy;
    uint32_t line; /* the line entered */
    bool loading;  /* from a file: a DEFtype is an event */
    const uint8_t *at;
} DeclScan;

const uint8_t decl_astype[5] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD};
const uint8_t decl_etsize[8] = {0, 4, 4, 4, 8, 0, 2, 1};

static const uint8_t deft0[NLETTERS] = {
    ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4,
    ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4,
};
static const uint8_t deftype[5] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD}; /* DEFINT .. DEFSTR */

uint8_t decl_xastype(const uint8_t tk1)
{
    return tk1 == TK1_SHORT ? ET_SH : tk1 == TK1_BYTE ? ET_BY : 0;
}

uint8_t decl_xdeftype(const uint8_t tk1)
{
    return tk1 == TK1_DEFSHORT ? ET_SH : tk1 == TK1_DEFBYTE ? ET_BY : 0;
}

const uint8_t *decl_item(const uint8_t *p)
{
    switch (*p) {
    case '"':
        for (p++; *p; p++)
            if (*p == '"')
                return p + 1;

        return p;

    case M_INT:
        return p + INT_TOKEN_SIZE;

    case M_VAR:
        return comp_skipname(p);

    case TK_X:
        return p + 2;

    default:
        return p + 1;
    }
}

/* The name at s (M_VAR), stepped past; *sfx its suffix, 0 none. */
static uint16_t decl_name(DeclScan *const ds, uint8_t *const sfx)
{
    const uint16_t n = comp_nameof(ds->at);

    if (sfx)
        *sfx = comp_suffixof(ds->at);

    ds->at = comp_skipname(ds->at);
    return n;
}

uint8_t decl_proc(const uint8_t *const body, const uint8_t **const name)
{
    const uint8_t *p = body;

    if (*p == M_INT)
        p += INT_TOKEN_SIZE;

    const uint8_t c = *p++;

    if (c == TK_SUB || c == TK_FUNCTION) {
        if (*p != M_VAR)
            return 0;

        *name = p;
        return c == TK_SUB ? PK_SUB : PK_FUNC;
    }

    if (c == TK_END && (*p == TK_SUB || *p == TK_FUNCTION))
        return PK_END;

    return 0;
}

bool decl_tail(const uint8_t **const at)
{
    const uint8_t *p = *at;

    if (*p == M_INT)
        p += INT_TOKEN_SIZE;

    const uint8_t *start = p;
    bool open = false;

    for (;;) {
        uint8_t c = *p;

        if (c == 0)
            break;
        if (c == ':') {
            start = ++p;
            continue;
        }

        if (c == TK_REM) {
            open = true;
            break;
        }

        if (c == '"') { /* a string: open at the end? */
            for (p++; *p && *p != '"'; p++)
                ;
            if (!*p) {
                open = true;
                break;
            }

            p++;
            continue;
        }

        if (c == TK_DATA) {
            p = comp_skipdata(p + 1);

            if (!*p) {
                open = true;
                break;
            }

            start = ++p;
            continue;
        }

        p = decl_item(p);
    }

    *at = start;
    return open;
}

const uint8_t *decl_step(const uint8_t *p)
{
    if (*p != '.' && (uint8_t)(*p - '0') >= 10)
        return decl_item(p);
    for (;;) {
        uint8_t c = *++p;

        if (c == '.')
            continue;
        if (c == 'E' || c == 'D') {
            if (p[1] == '+' || p[1] == '-')
                p++;

            continue;
        }

        if (c == '#' || c == '!')
            return p + 1;
        if ((uint8_t)(c - '0') >= 10)
            return p;
    }
}

[[noreturn]]
static void decl_fail(const DeclScan *const ds, const uint8_t code)
{
    fault_error(ds->d->fault, code);
}

/* Room in the array *t of n used, *cap, each size bytes, for one more. */
static void decl_room(
    DeclScan *const ds,
    void **const t,
    const uint32_t n,
    uint32_t *const cap,
    const size_t size
)
{
    if (n < *cap)
        return;

    const uint32_t cap2 = *cap ? *cap * 2 : 8;
    void *const g = arena_grow(&ds->d->arena, *t, *cap * size, cap2 * size);

    if (!g)
        decl_fail(ds, E_OM);

    *t = g;
    *cap = cap2;
}

void decl_init(Decl *const d, Fault *const f)
{
    arena_init(&d->arena, DECL_CHUNK);
    d->fault = f;
}

const TypeDef *decl_type(const Decl *const d, const uint16_t t) { return d->types[t]; }

void decl_reset(Decl *const d, Scop *const sc, Symb *const sy)
{
    arena_reset(&d->arena);
    *d = (Decl){.arena = d->arena, .fault = d->fault};

    memcpy(sy->deft, deft0, NLETTERS);
    scop_reset(sc);

    for (uint16_t i = sy->nnames; i-- > 1;) {
        Name *const n = symb_name(sy, i);
        n->flags &= NM_FN;
        n->rec = 0;
        n->proc = 0;
    }
}

/* A loading line's DEFtype: the DEFtypes from it on, an event at its place. */
static void decl_defevent(DeclScan *const ds)
{
    Decl *const d = ds->d;

    if (!ds->loading)
        return;

    uint32_t n = d->ndefev;

    if (!n || d->defevs[n - 1].line != ds->line) {
        if (n >= NDEV)
            decl_fail(ds, E_OM);

        decl_room(ds, (void **)&d->defevs, n, &d->defcap, sizeof *d->defevs);
        n = ++d->ndefev;
    }

    d->defevs[n - 1].line = ds->line;
    memcpy(d->defevs[n - 1].deft, ds->sy->deft, NLETTERS);
}

void decl_deftype(const Decl *const d, Symb *const sy, const uint32_t line)
{
    const uint8_t *from = deft0;

    for (uint32_t i = 0; i < d->ndefev && line >= d->defevs[i].line; i++)
        from = d->defevs[i].deft;

    memcpy(sy->deft, from, NLETTERS);
}

/* Name n an array known in scope from here on. */
static void decl_array(DeclScan *const ds, const uint16_t n, const uint16_t scope)
{
    scop_bind(ds->sc, scope | SC_ARR, n);
}

/* A procedure's REDIM of a name the module shares with it: the module's array. */
static void decl_redimscope(DeclScan *const ds, const uint16_t n)
{
    if (!ds->d->redim || !ds->d->scope)
        return;

    const Binding *const own = scop_get(ds->sc, ds->d->scope, n);
    const Binding *const module = scop_get(ds->sc, 0, n);
    const bool shared = (own && own->flags & BF_REF) || (module && module->flags & BF_SHR);

    if (shared)
        ds->d->scope = 0;
}

/* A procedure's DIM of a name the module shares: Duplicate definition (QB45's, PDS's). */
static void decl_shareddim(DeclScan *const ds, const uint16_t n)
{
    if (ds->d->scope && scop_mshr(ds->sc, n))
        decl_fail(ds, E_RD);
}

/* One of DIM's names, a parameter: its binding, ( ) an array's, AS type [* n]. */
static void decl_dim1(DeclScan *const ds, const uint16_t n)
{
    Binding *const b = scop_bind(ds->sc, ds->d->scope, n);
    const bool arr = *ds->at == '(';

    if (arr) {
        decl_array(ds, n, ds->d->scope);
        int32_t depth = 0; /* to the ')' that closes it */
        do {
            const uint8_t c = *ds->at;

            if (!c)
                return;
            if (c == '(')
                depth++;
            if (c == ')')
                depth--;

            ds->at = decl_item(ds->at);
        } while (depth);
    }

    if (*ds->at != TK_AS)
        return;
    if (b->flags == BF_USE) /* used with a suffix before */
        decl_fail(ds, E_ASF);

    const uint8_t c = ds->at[1];

    if (c == M_VAR) { /* AS a TYPE: a record, its array */
        ds->at++;
        b->rec[arr] = symb_name(ds->sy, decl_name(ds, nullptr))->rec;
        return;
    }

    if (c == TK_X) {
        const uint8_t et = decl_xastype(ds->at[2]);

        if (!et)
            return;

        ds->at += 3;
        b->et[arr] = et;
        return;
    }

    const uint8_t i = c - TK_INTEGER;

    if (i > TK_STRING - TK_INTEGER)
        return;

    ds->at += 2;
    b->et[arr] = decl_astype[i];

    if (*ds->at != TK_MUL) /* STRING * n */
        return;

    ds->at++;

    if (*ds->at != M_INT)
        return;

    b->fix[arr] = (uint16_t)load32(ds->at + 1);
    ds->at = decl_item(ds->at);
}

/* ( [BYVAL] n[()] [AS type] {, ..} ): as DIM's. */
static void decl_params(DeclScan *const ds)
{
    if (*ds->at != '(')
        return;

    ds->at++;

    for (;;) {
        if (*ds->at == TK_BYVAL)
            ds->at++;
        if (*ds->at != M_VAR)
            return;

        decl_dim1(ds, decl_name(ds, nullptr));

        if (*ds->at != ',')
            return;

        ds->at++;
    }
}

/* DIM's names, flags their bindings'. */
static void decl_dimlist(DeclScan *const ds, const uint8_t flags)
{
    for (;;) {
        if (*ds->at != M_VAR)
            return;

        const uint16_t n = decl_name(ds, nullptr);

        if (*ds->at == '(')
            ds->d->dimmed = true;

        const uint16_t scope = ds->d->scope;
        decl_redimscope(ds, n);
        decl_shareddim(ds, n);
        scop_bind(ds->sc, ds->d->scope, n)->flags |= flags;
        decl_dim1(ds, n);
        ds->d->scope = scope;

        if (*ds->at != ',')
            return;

        ds->at++;
    }
}

/* TYPE t: its fields from here. */
static void decl_typedef(DeclScan *const ds)
{
    if (ds->at[1] != M_VAR)
        return;

    ds->at++;
    const uint16_t n = decl_name(ds, nullptr);

    Decl *const d = ds->d;

    if (d->ntypes >= NTYPE - 1)
        decl_fail(ds, E_OM);

    decl_room(ds, (void **)&d->types, d->ntypes + 1u, &d->typecap, sizeof *d->types);
    TypeDef *const td = arena_alloc(&d->arena, sizeof *td);

    if (!td)
        decl_fail(ds, E_OM);

    const uint16_t t = ++d->ntypes;
    d->type = t;
    symb_name(ds->sy, n)->rec = (uint8_t)t;
    *td = (TypeDef){.name = n};

    d->types[t] = td;
}

/* In a TYPE: f AS type [* n]. */
static void decl_field(DeclScan *const ds)
{
    const uint16_t t = ds->d->type;

    if (!t)
        return;

    const uint16_t n = decl_name(ds, nullptr);

    if (*ds->at != TK_AS)
        return;
    if (ds->d->nfields >= NFLD)
        decl_fail(ds, E_OM);

    TypeDef *const td = ds->d->types[t];
    decl_room(ds, (void **)&td->fields, td->nfld, &td->cap, sizeof *td->fields);
    ds->d->nfields++;
    Field *const f = &td->fields[td->nfld++];
    *f = (Field){.name = n, .off = td->size}; /* at the TYPE's end */
    uint16_t size;
    const uint8_t c = ds->at[1];

    if (c == M_VAR) { /* another TYPE's */
        ds->at++;
        f->sub = symb_name(ds->sy, decl_name(ds, nullptr))->rec;
        size = ds->d->types[f->sub]->size;
    } else if (c == TK_X) {
        f->et = decl_xastype(ds->at[2]);

        if (!f->et)
            return;

        size = decl_etsize[f->et];
    } else {
        const uint8_t i = c - TK_INTEGER;

        if (i > TK_STRING - TK_INTEGER)
            return;

        f->et = decl_astype[i];
        size = decl_etsize[f->et];
        ds->at += 2;

        if (f->et == ET_SD) {
            size = 0;

            if (*ds->at == TK_MUL && ds->at[1] == M_INT) /* STRING * n */
                size = f->fix = (uint16_t)load32(ds->at + 2);
        }
    }

    td->size += size;
}

/* OPTION BASE n, OPTION EXPLICIT. */
static void decl_option(DeclScan *const ds)
{
    if (ds->at[1] == TK_X && ds->at[2] == TK1_EXPLICIT) {
        ds->d->explicit_ = true;
        return;
    }

    if (ds->at[1] != TK_BASE || ds->at[2] != M_INT)
        return;
    if (load32(ds->at + 3) > 1 || ds->d->baseset) /* 0 or 1, once, before any DIM */
        decl_fail(ds, E_SN);
    if (ds->d->dimmed)
        decl_fail(ds, E_AAD);

    ds->d->base = ds->at[3];
    ds->d->baseset = true;
    ds->at += 2 + INT_TOKEN_SIZE;
}

/* DECLARE SUB|FUNCTION n: its flag; its parameters in its own scope. */
static void decl_declare(DeclScan *const ds)
{
    ds->at++;
    const uint8_t flag = *ds->at == TK_SUB ? NM_SUB : *ds->at == TK_FUNCTION ? NM_FUNC : 0;

    if (!flag)
        return;

    ds->at++;

    if (*ds->at != M_VAR)
        return;

    const uint16_t n = decl_name(ds, nullptr);
    symb_name(ds->sy, n)->flags |= flag;
    ds->d->scope = n;
    decl_params(ds);
}

/* DEF FNn (params): their types; its name its own. */
static void decl_deffn(DeclScan *const ds)
{
    ds->at++;

    if (*ds->at != M_VAR)
        return;

    const uint16_t n = decl_name(ds, nullptr);
    ds->d->scope = n;
    scop_bind(ds->sc, ds->d->scope, n);
    decl_params(ds);
}

void decl_enter(Decl *const d, const uint16_t proc)
{
    d->proc = proc;
    d->scope = proc;
    d->type = 0;
}

/* SUB n, FUNCTION n: in it from here. */
static void decl_subfn(DeclScan *const ds, const uint8_t flag)
{
    ds->at++;

    if (*ds->at != M_VAR)
        return;

    const uint16_t n = decl_name(ds, nullptr);
    symb_name(ds->sy, n)->flags |= flag;
    ds->d->proc = n;
    ds->d->scope = n;
    decl_params(ds);
}

/* SHARED (BF_REF), STATIC (BF_STAT) n[()] [AS t] {, ..} */
static void decl_shared(DeclScan *const ds, const uint8_t flag)
{
    ds->at++;

    for (;;) {
        if (*ds->at != M_VAR)
            return;

        const uint16_t n = decl_name(ds, nullptr);

        if (flag == BF_STAT) /* its ( ), AS type as DIM's */
            decl_dim1(ds, n);
        else if (*ds->at == '(')
            decl_array(ds, n, ds->d->proc);

        scop_bind(ds->sc, ds->d->proc, n)->flags |= flag;

        for (;;) { /* past ( ) and AS type */
            const uint8_t c = *ds->at;

            if (!c || c == ':')
                return;
            if (c == ',')
                break;

            ds->at = decl_item(ds->at);
        }

        ds->at++;
    }
}

/* DEFtype l[-l] {, l[-l]}; DEFSHORT and DEFBYTE ours. */
static void decl_deflist(DeclScan *const ds, const uint8_t et)
{
    for (;;) {
        if (*ds->at != M_VAR)
            return;

        const uint32_t first = upcase(ds->at[2]) - 'A';
        uint32_t last = first;
        ds->at = decl_item(ds->at);

        if (*ds->at == TK_MINUS) {
            ds->at++;

            if (*ds->at != M_VAR)
                return;

            last = upcase(ds->at[2]) - 'A';
            ds->at = decl_item(ds->at);
        }

        if (last >= NLETTERS)
            return;
        for (uint32_t i = first; i <= last; i++)
            ds->sy->deft[i] = et;

        decl_defevent(ds);

        if (*ds->at != ',')
            return;

        ds->at++;
    }
}

/* COMMON [SHARED] [/b/]: as DIM's. */
static void decl_common(DeclScan *const ds)
{
    ds->at += 2;
    ds->d->scope = 0;
    const bool shared = *ds->at == TK_SHARED;

    if (shared)
        ds->at++;
    if (*ds->at == TK_DIV) {
        if (ds->at[1] != M_VAR)
            return;

        ds->at = comp_skipname(ds->at + 1);

        if (*ds->at != TK_DIV)
            return;

        ds->at++;
    }

    decl_dimlist(ds, shared ? BF_SHR : 0);
}

/* DIM [SHARED] n[( )] [AS type] {, ..} */
static void decl_dim(DeclScan *const ds, const bool redim)
{
    ds->d->redim = redim;
    ds->at++;
    const bool shared = *ds->at == TK_SHARED;

    if (shared)
        ds->at++;

    ds->d->scope = shared ? 0 : ds->d->proc;
    decl_dimlist(ds, shared ? BF_SHR : 0);
}

/* CONST n = e {, n = e} */
static void decl_const(DeclScan *const ds)
{
    ds->at++;

    for (;;) {
        if (*ds->at != M_VAR)
            return;

        uint8_t sfx;
        const uint16_t n = decl_name(ds, &sfx);

        if (*ds->at != TK_EQ)
            return;

        ds->at++;
        uint8_t et = ET_I4; /* the value's type: its items' */
        int32_t depth = 0;

        for (;;) {
            const uint8_t c = *ds->at;

            if (!c || c == ':' || (!depth && c == ','))
                break;
            if (c == '(')
                depth++;
            if (c == ')')
                depth--;
            if (c == '"')
                et = ET_SD;
            if ((c == '.' || is_digit(c) || c == TK_DIV) && et != ET_SD)
                et = ET_R4; /* a point, an exponent, a / */
            ds->at = decl_item(ds->at);
        }

        if (ds->d->proc && scop_mshr(ds->sc, n)) /* a procedure's of a DIM SHARED name */
            decl_fail(ds, E_SN);

        Binding *const b = scop_bind(ds->sc, ds->d->proc, n);
        b->flags |= BF_CON;

        if (!ds->d->proc)
            b->flags |= BF_SHR; /* the module's: every procedure's */
        if (sfx)
            et = comp_suffix(sfx);

        b->et[0] = et;

        if (*ds->at != ',')
            return;

        ds->at++;
    }
}

/* The name at p used: with a suffix and no binding in decl.proc's scope, one of BF_USE. */
static void decl_use(DeclScan *const ds, const uint8_t *const p)
{
    const uint16_t n = comp_nameof(p);

    if (!comp_suffixof(p) || scop_get(ds->sc, ds->d->proc, n))
        return;

    scop_bind(ds->sc, ds->d->proc, n)->flags = BF_USE;
}

/* The name at p: followed by (, an array known in decl.proc's scope. */
static void decl_arrayuse(DeclScan *const ds, const uint8_t *const p)
{
    if (*comp_skipname(p) == '(')
        decl_array(ds, comp_nameof(p), ds->d->proc);
}

/* LBOUND or UBOUND at p: of an array known in decl.proc's scope, or one the module shares. */
static void decl_bound(DeclScan *const ds, const uint8_t *const p)
{
    if (p[1] != '(' || p[2] != M_VAR)
        return;

    const uint16_t n = comp_nameof(p + 2);
    const uint16_t scope = ds->d->proc | SC_ARR;

    if (scop_get(ds->sc, scope, n))
        return;
    if (scope != SC_ARR && scop_get(ds->sc, SC_ARR, n)) {
        const Binding *b = scop_get(ds->sc, 0, n);

        if (b && (b->flags & BF_SHR))
            return;
    }

    decl_fail(ds, E_AND);
}

/* To the next statement: past a ':' out of strings; REM's and DATA's text skipped. False: the
   end. */
static bool decl_next(DeclScan *const ds)
{
    for (;;) {
        uint8_t c = *ds->at;

        if (!c || c == TK_REM)
            return false;
        if (c == ':') {
            ds->at++;
            return true;
        }

        if (c == TK_DATA) {
            ds->at = comp_skipdata(ds->at + 1);

            if (!*ds->at)
                return false;

            ds->at++;
            return true;
        }

        if (c == M_VAR) {
            decl_use(ds, ds->at);
            decl_arrayuse(ds, ds->at);
        } else if (c == TK_LBOUND || c == TK_UBOUND)
            decl_bound(ds, ds->at);

        ds->at = decl_item(ds->at);
    }
}

void decl_line(
    Decl *const d,
    Scop *const sc,
    Symb *const sy,
    const uint8_t *const body,
    const uint32_t line,
    const bool loading
)
{
    DeclScan scan = {
        .d = d,
        .sc = sc,
        .sy = sy,
        .line = line,
        .loading = loading,
        .at = body,
    };

    DeclScan *const ds = &scan;

    if (*ds->at == M_INT) /* a line number */
        ds->at += INT_TOKEN_SIZE;
    do {
        const uint8_t c = *ds->at;

        switch (c) {
        case TK_DIM:
            decl_dim(ds, false);
            break;

        case TK_REDIM:
            decl_dim(ds, true);
            break;

        case TK_CONST:
            decl_const(ds);
            break;

        case TK_SUB:
            decl_subfn(ds, NM_SUB);
            break;

        case TK_FUNCTION:
            decl_subfn(ds, NM_FUNC);
            break;

        case TK_DECLARE:
            decl_declare(ds);
            break;

        case TK_DEF:
            decl_deffn(ds);
            break;

        case TK_SHARED:
            decl_shared(ds, BF_REF);
            break;

        case TK_STATIC:
            decl_shared(ds, BF_STAT);
            break;

        case TK_END: /* END SUB, END FUNCTION: the module; END TYPE */
            if (ds->at[1] == TK_TYPE)
                ds->d->type = 0;
            else if (ds->at[1] == TK_SUB || ds->at[1] == TK_FUNCTION)
                ds->d->proc = 0;

            break;

        case TK_OPTION:
            decl_option(ds);
            break;

        case TK_TYPE:
            decl_typedef(ds);
            break;

        case M_VAR:
            decl_field(ds);
            break;

        case TK_X:
            if (ds->at[1] == TK1_COMMON)
                decl_common(ds);
            else if (decl_xdeftype(ds->at[1])) {
                ds->at += 2;
                decl_deflist(ds, decl_xdeftype(ds->at[-1]));
            }

            break;

        default:
            if (c >= TK_DEFINT && c <= TK_DEFSTR) {
                ds->at++;
                decl_deflist(ds, deftype[c - TK_DEFINT]);
            }
        }
    } while (decl_next(ds));
}

void decl_defevents(Decl *const d, Scop *const sc, Symb *const sy, const Prog *const pg)
{
    d->ndefev = 0;
    memcpy(sy->deft, deft0, NLETTERS);

    for (uint32_t rec = PROGBASE, len; (len = prog_line(pg, rec)->len); rec += len) {
        if (prog_line(pg, rec)->flags & LF_ASIS)
            continue;

        DeclScan scan = {
            .d = d,
            .sc = sc,
            .sy = sy,
            .line = rec,
            .loading = true,
        };

        DeclScan *const ds = &scan;
        ds->at = at(prog_addr(pg, rec + HDR));

        if (*ds->at == M_INT)
            ds->at += INT_TOKEN_SIZE;
        do {
            const uint8_t c = *ds->at;

            if (c >= TK_DEFINT && c <= TK_DEFSTR) {
                ds->at++;
                decl_deflist(ds, deftype[c - TK_DEFINT]);
            } else if (c == TK_X && decl_xdeftype(ds->at[1])) {
                ds->at += 2;
                decl_deflist(ds, decl_xdeftype(ds->at[-1]));
            }
        } while (decl_next(ds));
    }
}

const char *decl_warning(const uint8_t **const at)
{
    for (const uint8_t *p = *at; *p && *p != TK_REM && *p != TK_DATA; p = decl_item(p)) {
        const char *what = nullptr;

        if (p[0] == TK_DEF && p[1] == TK_X && p[2] == TK1_SEG)
            what = "DEF SEG";
        else if (p[0] == TK_X && p[1] == TK1_VARSEG)
            what = "VARSEG";
        else if (p[0] == TK_X && p[1] == TK1_SSEG)
            what = "SSEG";

        if (what) {
            *at = decl_item(p);
            return what;
        }
    }

    return nullptr;
}
