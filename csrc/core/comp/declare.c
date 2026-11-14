/* A line's declarations, into the scopes as it is entered (comp/decl.asm). */
#include "comp/declare.h"
#include "session.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/program.h"
#include "mem/scope.h"
#include "mem/symbols.h"

const uint8_t decl_astype[5] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD};
const uint8_t decl_etsize[7] = {0, 4, 4, 4, 8, 0, 2};


static const uint8_t deft0[NLETTERS] = {
    ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4,
    ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4, ET_R4,
};
static const uint8_t deftype[5] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD}; /* DEFINT .. DEFSTR */


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
static uint16_t name(Session *const S, uint8_t *const sfx)
{
    const uint16_t n = comp_nameof(S->decl.at);
    if (sfx)
        *sfx = comp_suffixof(S->decl.at);
    S->decl.at = comp_skipname(S->decl.at);
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

void decl_reset(Session *const S)
{
    memcpy(S->symb.deft, deft0, NLETTERS);
    S->decl.ndefev = 0;
    S->decl.proc = 0;
    scop_init(S);
    S->decl.base = 0;
    S->decl.baseset = false;
    S->decl.dimmed = false;
    S->decl.explicit_ = false;
    S->decl.ntypes = S->decl.nfields = S->decl.type = 0;
    for (uint16_t i = S->symb.nnames; i-- > 1;) {
        Name *const n = symb_name(&S->symb, i);
        n->flags &= NM_FN;
        n->rec = 0;
        n->proc = 0;
    }
}

/* A loading line's DEFtype: the DEFtypes from it on, an event at its place. */
static void defevent(Session *const S)
{
    if (!S->repl.loading)
        return;
    const uint32_t line = S->prog.entering;
    uint32_t n = S->decl.ndefev;
    if (!n || S->decl.defevs[n - 1].line != line) {
        if (n >= NDEV)
            repl_error(S, E_OM);
        n = ++S->decl.ndefev;
    }
    S->decl.defevs[n - 1].line = line;
    memcpy(S->decl.defevs[n - 1].deft, S->symb.deft, NLETTERS);
}

void decl_deftype(Session *const S, const uint32_t line)
{
    const uint8_t *from = deft0;
    for (uint32_t i = 0; i < S->decl.ndefev && line >= S->decl.defevs[i].line; i++)
        from = S->decl.defevs[i].deft;
    memcpy(S->symb.deft, from, NLETTERS);
}

/* Name n an array known in scope from here on. */
static void array(Session *const S, const uint16_t n, const uint16_t scope) { scop_bind(S, scope | SC_ARR, n); }

/* A procedure's REDIM of a name the module shares with it: the module's array. */
static void redimscope(Session *const S, const uint16_t n)
{
    if (!S->decl.redim || !S->decl.scope)
        return;
    const Binding *const own = scop_get(S, S->decl.scope, n);
    const Binding *const module = scop_get(S, 0, n);
    const bool shared = (own && own->flags & BF_REF) || (module && module->flags & BF_SHR);
    if (shared)
        S->decl.scope = 0;
}

/* A procedure's DIM of a name the module shares: Duplicate definition (QB45's, PDS's). */
static void shareddim(Session *const S, const uint16_t n)
{
    if (S->decl.scope && scop_mshr(S, n))
        repl_error(S, E_RD);
}

/* One of DIM's names, a parameter: its binding, ( ) an array's, AS type [* n]. */
static void dim1(Session *const S, const uint16_t n)
{
    Binding *const b = scop_bind(S, S->decl.scope, n);
    const bool arr = *S->decl.at == '(';
    if (arr) {
        array(S, n, S->decl.scope);
        int32_t depth = 0; /* to the ')' that closes it */
        do {
            const uint8_t c = *S->decl.at;
            if (!c)
                return;
            if (c == '(')
                depth++;
            if (c == ')')
                depth--;
            S->decl.at = decl_item(S->decl.at);
        } while (depth);
    }
    if (*S->decl.at != TK_AS)
        return;
    if (b->flags == BF_USE) /* used with a suffix before */
        repl_error(S, E_ASF);
    const uint8_t c = S->decl.at[1];
    if (c == M_VAR) { /* AS a TYPE: a record, its array */
        S->decl.at++;
        b->rec[arr] = symb_name(&S->symb, name(S, nullptr))->rec;
        return;
    }
    if (c == TK_X) {
        if (S->decl.at[2] != TK1_SHORT)
            return;
        S->decl.at += 3;
        b->et[arr] = ET_SH;
        return;
    }
    const uint8_t i = c - TK_INTEGER;
    if (i > TK_STRING - TK_INTEGER)
        return;
    S->decl.at += 2;
    b->et[arr] = decl_astype[i];
    if (*S->decl.at != TK_MUL) /* STRING * n */
        return;
    S->decl.at++;
    if (*S->decl.at != M_INT)
        return;
    b->fix[arr] = (uint16_t)load32(S->decl.at + 1);
    S->decl.at = decl_item(S->decl.at);
}

/* ( [BYVAL] n[()] [AS type] {, ..} ): as DIM's. */
static void params(Session *const S)
{
    if (*S->decl.at != '(')
        return;
    S->decl.at++;
    for (;;) {
        if (*S->decl.at == TK_BYVAL)
            S->decl.at++;
        if (*S->decl.at != M_VAR)
            return;
        dim1(S, name(S, nullptr));
        if (*S->decl.at != ',')
            return;
        S->decl.at++;
    }
}

/* DIM's names, flags their bindings'. */
static void dimlist(Session *const S, const uint8_t flags)
{
    for (;;) {
        if (*S->decl.at != M_VAR)
            return;
        const uint16_t n = name(S, nullptr);
        if (*S->decl.at == '(')
            S->decl.dimmed = true;
        const uint16_t scope = S->decl.scope;
        redimscope(S, n);
        shareddim(S, n);
        scop_bind(S, S->decl.scope, n)->flags |= flags;
        dim1(S, n);
        S->decl.scope = scope;
        if (*S->decl.at != ',')
            return;
        S->decl.at++;
    }
}

/* TYPE t: its fields from here. */
static void typedef_(Session *const S)
{
    if (S->decl.at[1] != M_VAR)
        return;
    S->decl.at++;
    const uint16_t n = name(S, nullptr);
    if (S->decl.ntypes >= NTYPE - 1)
        repl_error(S, E_OM);
    const uint16_t t = ++S->decl.ntypes;
    S->decl.type = t;
    symb_name(&S->symb, n)->rec = (uint8_t)t;
    S->decl.types[t] = (TypeDef){.name = n, .first = S->decl.nfields};
}

/* In a TYPE: f AS type [* n]. */
static void field(Session *const S)
{
    const uint16_t t = S->decl.type;
    if (!t)
        return;
    const uint16_t n = name(S, nullptr);
    if (*S->decl.at != TK_AS)
        return;
    if (S->decl.nfields >= NFLD)
        repl_error(S, E_OM);
    Field *const f = &S->decl.fields[S->decl.nfields++];
    TypeDef *const td = &S->decl.types[t];
    td->nfld++;
    *f = (Field){.name = n, .off = td->size}; /* at the TYPE's end */
    uint16_t size;
    const uint8_t c = S->decl.at[1];
    if (c == M_VAR) { /* another TYPE's */
        S->decl.at++;
        f->sub = symb_name(&S->symb, name(S, nullptr))->rec;
        size = S->decl.types[f->sub].size;
    } else if (c == TK_X) {
        if (S->decl.at[2] != TK1_SHORT)
            return;
        f->et = ET_SH;
        size = 2;
    } else {
        const uint8_t i = c - TK_INTEGER;
        if (i > TK_STRING - TK_INTEGER)
            return;
        f->et = decl_astype[i];
        size = decl_etsize[f->et];
        S->decl.at += 2;
        if (f->et == ET_SD) {
            size = 0;
            if (*S->decl.at == TK_MUL && S->decl.at[1] == M_INT) /* STRING * n */
                size = f->fix = (uint16_t)load32(S->decl.at + 2);
        }
    }
    td->size += size;
}

/* OPTION BASE n, OPTION EXPLICIT. */
static void option(Session *const S)
{
    if (S->decl.at[1] == TK_X && S->decl.at[2] == TK1_EXPLICIT) {
        S->decl.explicit_ = true;
        return;
    }
    if (S->decl.at[1] != TK_BASE || S->decl.at[2] != M_INT)
        return;
    if (load32(S->decl.at + 3) > 1 || S->decl.baseset) /* 0 or 1, once, before any DIM */
        repl_error(S, E_SN);
    if (S->decl.dimmed)
        repl_error(S, E_AAD);
    S->decl.base = S->decl.at[3];
    S->decl.baseset = true;
    S->decl.at += 2 + INT_TOKEN_SIZE;
}

/* DECLARE SUB|FUNCTION n: its flag; its parameters in its own scope. */
static void declare(Session *const S)
{
    S->decl.at++;
    const uint8_t flag = *S->decl.at == TK_SUB ? NM_SUB : *S->decl.at == TK_FUNCTION ? NM_FUNC : 0;
    if (!flag)
        return;
    S->decl.at++;
    if (*S->decl.at != M_VAR)
        return;
    const uint16_t n = name(S, nullptr);
    symb_name(&S->symb, n)->flags |= flag;
    S->decl.scope = n;
    params(S);
}

/* DEF FNn (params): their types; its name its own. */
static void deffn(Session *const S)
{
    S->decl.at++;
    if (*S->decl.at != M_VAR)
        return;
    const uint16_t n = name(S, nullptr);
    S->decl.scope = n;
    scop_bind(S, S->decl.scope, n);
    params(S);
}

/* SUB n, FUNCTION n: in it from here. */
static void subfn(Session *const S, const uint8_t flag)
{
    S->decl.at++;
    if (*S->decl.at != M_VAR)
        return;
    const uint16_t n = name(S, nullptr);
    symb_name(&S->symb, n)->flags |= flag;
    S->decl.proc = n;
    S->decl.scope = n;
    params(S);
}

/* SHARED (BF_REF), STATIC (BF_STAT) n[()] [AS t] {, ..} */
static void shared(Session *const S, const uint8_t flag)
{
    S->decl.at++;
    for (;;) {
        if (*S->decl.at != M_VAR)
            return;
        const uint16_t n = name(S, nullptr);
        scop_bind(S, S->decl.proc, n)->flags |= flag;
        if (*S->decl.at == '(')
            array(S, n, S->decl.proc);
        for (;;) { /* past ( ) and AS type */
            const uint8_t c = *S->decl.at;
            if (!c || c == ':')
                return;
            if (c == ',')
                break;
            S->decl.at = decl_item(S->decl.at);
        }
        S->decl.at++;
    }
}

/* DEFtype l[-l] {, l[-l]}; DEFSHORT ours. */
static void deflist(Session *const S, const uint8_t et)
{
    for (;;) {
        if (*S->decl.at != M_VAR)
            return;
        const uint32_t first = upcase(S->decl.at[2]) - 'A';
        uint32_t last = first;
        S->decl.at = decl_item(S->decl.at);
        if (*S->decl.at == TK_MINUS) {
            S->decl.at++;
            if (*S->decl.at != M_VAR)
                return;
            last = upcase(S->decl.at[2]) - 'A';
            S->decl.at = decl_item(S->decl.at);
        }
        if (last >= NLETTERS)
            return;
        for (uint32_t i = first; i <= last; i++)
            S->symb.deft[i] = et;
        defevent(S);
        if (*S->decl.at != ',')
            return;
        S->decl.at++;
    }
}

/* COMMON [SHARED] [/b/]: as DIM's. */
static void common(Session *const S)
{
    S->decl.at += 2;
    S->decl.scope = 0;
    const bool shared = *S->decl.at == TK_SHARED;
    if (shared)
        S->decl.at++;
    if (*S->decl.at == TK_DIV) {
        if (S->decl.at[1] != M_VAR)
            return;
        S->decl.at = comp_skipname(S->decl.at + 1);
        if (*S->decl.at != TK_DIV)
            return;
        S->decl.at++;
    }
    dimlist(S, shared ? BF_SHR : 0);
}

/* DIM [SHARED] n[( )] [AS type] {, ..} */
static void dim(Session *const S, const bool redim)
{
    S->decl.redim = redim;
    S->decl.at++;
    const bool shared = *S->decl.at == TK_SHARED;
    if (shared)
        S->decl.at++;
    S->decl.scope = shared ? 0 : S->decl.proc;
    dimlist(S, shared ? BF_SHR : 0);
}

/* CONST n = e {, n = e} */
static void konst(Session *const S)
{
    S->decl.at++;
    for (;;) {
        if (*S->decl.at != M_VAR)
            return;
        uint8_t sfx;
        const uint16_t n = name(S, &sfx);
        if (*S->decl.at != TK_EQ)
            return;
        S->decl.at++;
        uint8_t et = ET_I4; /* the value's type: its items' */
        int32_t depth = 0;
        for (;;) {
            const uint8_t c = *S->decl.at;
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
            S->decl.at = decl_item(S->decl.at);
        }
        if (S->decl.proc && scop_mshr(S, n)) /* a procedure's of a DIM SHARED name */
            repl_error(S, E_SN);
        Binding *const b = scop_bind(S, S->decl.proc, n);
        b->flags |= BF_CON;
        if (!S->decl.proc)
            b->flags |= BF_SHR; /* the module's: every procedure's */
        if (sfx)
            et = comp_suffix(sfx);
        b->et[0] = et;
        if (*S->decl.at != ',')
            return;
        S->decl.at++;
    }
}

/* The name at p used: with a suffix and no binding in decl.proc's scope, one of BF_USE. */
static void use(Session *const S, const uint8_t *const p)
{
    const uint16_t n = comp_nameof(p);
    if (!comp_suffixof(p) || scop_get(S, S->decl.proc, n))
        return;
    scop_bind(S, S->decl.proc, n)->flags = BF_USE;
}

/* The name at p: followed by (, an array known in decl.proc's scope. */
static void arrayuse(Session *const S, const uint8_t *const p)
{
    if (*comp_skipname(p) == '(')
        array(S, comp_nameof(p), S->decl.proc);
}

/* LBOUND or UBOUND at p: of an array known in decl.proc's scope, or one the module shares. */
static void bound(Session *const S, const uint8_t *const p)
{
    if (p[1] != '(' || p[2] != M_VAR)
        return;
    const uint16_t n = comp_nameof(p + 2);
    const uint16_t scope = S->decl.proc | SC_ARR;
    if (scop_get(S, scope, n))
        return;
    if (scope != SC_ARR && scop_get(S, SC_ARR, n)) {
        const Binding *b = scop_get(S, 0, n);
        if (b && (b->flags & BF_SHR))
            return;
    }
    repl_error(S, E_AND);
}

/* To the next statement: past a ':' out of strings; REM's and DATA's text skipped. False: the
   end. */
static bool next(Session *const S)
{
    for (;;) {
        uint8_t c = *S->decl.at;
        if (!c || c == TK_REM)
            return false;
        if (c == ':') {
            S->decl.at++;
            return true;
        }
        if (c == TK_DATA) {
            S->decl.at = comp_skipdata(S->decl.at + 1);
            if (!*S->decl.at)
                return false;
            S->decl.at++;
            return true;
        }
        if (c == M_VAR) {
            use(S, S->decl.at);
            arrayuse(S, S->decl.at);
        } else if (c == TK_LBOUND || c == TK_UBOUND)
            bound(S, S->decl.at);
        S->decl.at = decl_item(S->decl.at);
    }
}

/* To the front end's warn, a warning for each DEF SEG, VARSEG, SSEG: no effect, flat. */
static void warn(Session *const S, const uint8_t *const body)
{
    if (!S->repl.fe->warn)
        return;
    for (const uint8_t *p = body; *p && *p != TK_REM && *p != TK_DATA; p = decl_item(p)) {
        const char *what = nullptr;
        if (p[0] == TK_DEF && p[1] == TK_X && p[2] == TK1_SEG)
            what = "DEF SEG";
        else if (p[0] == TK_X && p[1] == TK1_VARSEG)
            what = "VARSEG";
        else if (p[0] == TK_X && p[1] == TK1_SSEG)
            what = "SSEG";
        if (!what)
            continue;
        char buf[80], *w = buf;
        memcpy(w, "Warning: ", 9);
        w += 9;
        if (S->repl.fline) {
            memcpy(w, "line ", 5);
            w += 5;
            char dig[5];
            int n = 0;
            for (uint32_t v = S->repl.fline; v; v /= 10)
                dig[n++] = (char)('0' + v % 10);
            while (n)
                *w++ = dig[--n];
            *w++ = ':';
            *w++ = ' ';
        }
        static const char tail[] = " has no effect in 32-bit mode\r\n";
        const uint32_t len = (uint32_t)strlen(what);
        memcpy(w, what, len);
        w += len;
        memcpy(w, tail, sizeof tail - 1);
        w += sizeof tail - 1;
        S->repl.fe->warn(S, buf, (uint32_t)(w - buf));
    }
}

void decl_line(Session *const S, const uint8_t *const body)
{
    warn(S, body);
    S->decl.at = body;
    if (*S->decl.at == M_INT) /* a line number */
        S->decl.at += INT_TOKEN_SIZE;
    do {
        const uint8_t c = *S->decl.at;
        switch (c) {
        case TK_DIM:
            dim(S, false);
            break;
        case TK_REDIM:
            dim(S, true);
            break;
        case TK_CONST:
            konst(S);
            break;
        case TK_SUB:
            subfn(S, NM_SUB);
            break;
        case TK_FUNCTION:
            subfn(S, NM_FUNC);
            break;
        case TK_DECLARE:
            declare(S);
            break;
        case TK_DEF:
            deffn(S);
            break;
        case TK_SHARED:
            shared(S, BF_REF);
            break;
        case TK_STATIC:
            shared(S, BF_STAT);
            break;
        case TK_END: /* END SUB, END FUNCTION: the module; END TYPE */
            if (S->decl.at[1] == TK_TYPE)
                S->decl.type = 0;
            else if (S->decl.at[1] == TK_SUB || S->decl.at[1] == TK_FUNCTION)
                S->decl.proc = 0;
            break;
        case TK_OPTION:
            option(S);
            break;
        case TK_TYPE:
            typedef_(S);
            break;
        case M_VAR:
            field(S);
            break;
        case TK_X:
            if (S->decl.at[1] == TK1_COMMON)
                common(S);
            else if (S->decl.at[1] == TK1_DEFSHORT) {
                S->decl.at += 2;
                deflist(S, ET_SH);
            }
            break;
        default:
            if (c >= TK_DEFINT && c <= TK_DEFSTR) {
                S->decl.at++;
                deflist(S, deftype[c - TK_DEFINT]);
            }
        }
    } while (next(S));
}
