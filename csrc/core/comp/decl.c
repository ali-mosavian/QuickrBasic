/* A line's declarations, into the scopes as it is entered (comp/decl.asm). */
#include "comp/decl.h"
#include "comp/comp.h"
#include "comp/dfa.h"
#include "io/repl.h"
#include "mem/prog.h"
#include "mem/scop.h"
#include "mem/symb.h"
#include "plat/pfil.h"

Decl decl;
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
static uint16_t name(uint8_t *const sfx)
{
    const uint16_t n = comp_nameof(decl.at);
    if (sfx)
        *sfx = comp_suffixof(decl.at);
    decl.at = comp_skipname(decl.at);
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

#if AC
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
#endif

void decl_reset(void)
{
    memcpy(symb.deft, deft0, NLETTERS);
    decl.ndefev = 0;
    decl.proc = 0;
    scop_init();
    decl.base = 0;
    decl.baseset = false;
    decl.dimmed = false;
    decl.explicit_ = false;
    decl.ntypes = decl.nfields = decl.type = 0;
    for (uint16_t i = symb.nnames; i-- > 1;) {
        Name *const n = symb_name(i);
        n->flags &= NM_FN;
        n->rec = 0;
        n->proc = 0;
    }
}

/* A loading line's DEFtype: the DEFtypes from it on, an event at its place. */
static void defevent(void)
{
    if (!repl.loading)
        return;
    const uint32_t line = prog.entering;
    uint32_t n = decl.ndefev;
    if (!n || decl.defevs[n - 1].line != line) {
        if (n >= NDEV)
            repl_error(E_OM);
        n = ++decl.ndefev;
    }
    decl.defevs[n - 1].line = line;
    memcpy(decl.defevs[n - 1].deft, symb.deft, NLETTERS);
}

void decl_deftype(const uint32_t line)
{
    const uint8_t *from = deft0;
    for (uint32_t i = 0; i < decl.ndefev && line >= decl.defevs[i].line; i++)
        from = decl.defevs[i].deft;
    memcpy(symb.deft, from, NLETTERS);
}

/* Name n an array known in scope from here on. */
static void array(const uint16_t n, const uint16_t scope) { scop_bind(scope | SC_ARR, n); }

/* A procedure's REDIM of a name the module shares with it: the module's array. */
static void redimscope(const uint16_t n)
{
    if (!decl.redim || !decl.scope)
        return;
    const Binding *const own = scop_get(decl.scope, n);
    const Binding *const module = scop_get(0, n);
    const bool shared = (own && own->flags & BF_REF) || (module && module->flags & BF_SHR);
    if (shared)
        decl.scope = 0;
}

/* A procedure's DIM of a name the module shares: Duplicate definition (QB45's, PDS's). */
static void shareddim(const uint16_t n)
{
    if (decl.scope && scop_mshr(n))
        repl_error(E_RD);
}

/* One of DIM's names, a parameter: its binding, ( ) an array's, AS type [* n]. */
static void dim1(const uint16_t n)
{
    Binding *const b = scop_bind(decl.scope, n);
    const bool arr = *decl.at == '(';
    if (arr) {
        array(n, decl.scope);
        int32_t depth = 0; /* to the ')' that closes it */
        do {
            const uint8_t c = *decl.at;
            if (!c)
                return;
            if (c == '(')
                depth++;
            if (c == ')')
                depth--;
            decl.at = decl_item(decl.at);
        } while (depth);
    }
    if (*decl.at != TK_AS)
        return;
    if (b->flags == BF_USE) /* used with a suffix before */
        repl_error(E_ASF);
    const uint8_t c = decl.at[1];
    if (c == M_VAR) { /* AS a TYPE: a record, its array */
        decl.at++;
        b->rec[arr] = symb_name(name(nullptr))->rec;
        return;
    }
    if (c == TK_X) {
        if (decl.at[2] != TK1_SHORT)
            return;
        decl.at += 3;
        b->et[arr] = ET_SH;
        return;
    }
    const uint8_t i = c - TK_INTEGER;
    if (i > TK_STRING - TK_INTEGER)
        return;
    decl.at += 2;
    b->et[arr] = decl_astype[i];
    if (*decl.at != TK_MUL) /* STRING * n */
        return;
    decl.at++;
    if (*decl.at != M_INT)
        return;
    b->fix[arr] = (uint16_t)load32(decl.at + 1);
    decl.at = decl_item(decl.at);
}

/* ( [BYVAL] n[()] [AS type] {, ..} ): as DIM's. */
static void params(void)
{
    if (*decl.at != '(')
        return;
    decl.at++;
    for (;;) {
        if (*decl.at == TK_BYVAL)
            decl.at++;
        if (*decl.at != M_VAR)
            return;
        dim1(name(nullptr));
        if (*decl.at != ',')
            return;
        decl.at++;
    }
}

/* DIM's names, flags their bindings'. */
static void dimlist(const uint8_t flags)
{
    for (;;) {
        if (*decl.at != M_VAR)
            return;
        const uint16_t n = name(nullptr);
        if (*decl.at == '(')
            decl.dimmed = true;
        const uint16_t scope = decl.scope;
        redimscope(n);
        shareddim(n);
        scop_bind(decl.scope, n)->flags |= flags;
        dim1(n);
        decl.scope = scope;
        if (*decl.at != ',')
            return;
        decl.at++;
    }
}

/* TYPE t: its fields from here. */
static void typedef_(void)
{
    if (decl.at[1] != M_VAR)
        return;
    decl.at++;
    const uint16_t n = name(nullptr);
    if (decl.ntypes >= NTYPE - 1)
        repl_error(E_OM);
    const uint16_t t = ++decl.ntypes;
    decl.type = t;
    symb_name(n)->rec = (uint8_t)t;
    decl.types[t] = (TypeDef){.name = n, .first = decl.nfields};
}

/* In a TYPE: f AS type [* n]. */
static void field(void)
{
    const uint16_t t = decl.type;
    if (!t)
        return;
    const uint16_t n = name(nullptr);
    if (*decl.at != TK_AS)
        return;
    if (decl.nfields >= NFLD)
        repl_error(E_OM);
    Field *const f = &decl.fields[decl.nfields++];
    TypeDef *const td = &decl.types[t];
    td->nfld++;
    *f = (Field){.name = n, .off = td->size}; /* at the TYPE's end */
    uint16_t size;
    const uint8_t c = decl.at[1];
    if (c == M_VAR) { /* another TYPE's */
        decl.at++;
        f->sub = symb_name(name(nullptr))->rec;
        size = decl.types[f->sub].size;
    } else if (c == TK_X) {
        if (decl.at[2] != TK1_SHORT)
            return;
        f->et = ET_SH;
        size = 2;
    } else {
        const uint8_t i = c - TK_INTEGER;
        if (i > TK_STRING - TK_INTEGER)
            return;
        f->et = decl_astype[i];
        size = decl_etsize[f->et];
        decl.at += 2;
        if (f->et == ET_SD) {
            size = 0;
            if (*decl.at == TK_MUL && decl.at[1] == M_INT) /* STRING * n */
                size = f->fix = (uint16_t)load32(decl.at + 2);
        }
    }
    td->size += size;
}

/* OPTION BASE n, OPTION EXPLICIT. */
static void option(void)
{
    if (decl.at[1] == TK_X && decl.at[2] == TK1_EXPLICIT) {
        decl.explicit_ = true;
        return;
    }
    if (decl.at[1] != TK_BASE || decl.at[2] != M_INT)
        return;
    if (load32(decl.at + 3) > 1 || decl.baseset) /* 0 or 1, once, before any DIM */
        repl_error(E_SN);
    if (decl.dimmed)
        repl_error(E_AAD);
    decl.base = decl.at[3];
    decl.baseset = true;
    decl.at += 2 + INT_TOKEN_SIZE;
}

/* DECLARE SUB|FUNCTION n: its flag; its parameters in its own scope. */
static void declare(void)
{
    decl.at++;
    const uint8_t flag = *decl.at == TK_SUB ? NM_SUB : *decl.at == TK_FUNCTION ? NM_FUNC : 0;
    if (!flag)
        return;
    decl.at++;
    if (*decl.at != M_VAR)
        return;
    const uint16_t n = name(nullptr);
    symb_name(n)->flags |= flag;
    decl.scope = n;
    params();
}

/* DEF FNn (params): their types; its name its own. */
static void deffn(void)
{
    decl.at++;
    if (*decl.at != M_VAR)
        return;
    const uint16_t n = name(nullptr);
    decl.scope = n;
    scop_bind(decl.scope, n);
    params();
}

/* SUB n, FUNCTION n: in it from here. */
static void subfn(const uint8_t flag)
{
    decl.at++;
    if (*decl.at != M_VAR)
        return;
    const uint16_t n = name(nullptr);
    symb_name(n)->flags |= flag;
    decl.proc = n;
    decl.scope = n;
    params();
}

/* SHARED (BF_REF), STATIC (BF_STAT) n[()] [AS t] {, ..} */
static void shared(const uint8_t flag)
{
    decl.at++;
    for (;;) {
        if (*decl.at != M_VAR)
            return;
        const uint16_t n = name(nullptr);
        scop_bind(decl.proc, n)->flags |= flag;
        if (*decl.at == '(')
            array(n, decl.proc);
        for (;;) { /* past ( ) and AS type */
            const uint8_t c = *decl.at;
            if (!c || c == ':')
                return;
            if (c == ',')
                break;
            decl.at = decl_item(decl.at);
        }
        decl.at++;
    }
}

/* DEFtype l[-l] {, l[-l]}; DEFSHORT ours. */
static void deflist(const uint8_t et)
{
    for (;;) {
        if (*decl.at != M_VAR)
            return;
        const uint32_t first = upcase(decl.at[2]) - 'A';
        uint32_t last = first;
        decl.at = decl_item(decl.at);
        if (*decl.at == TK_MINUS) {
            decl.at++;
            if (*decl.at != M_VAR)
                return;
            last = upcase(decl.at[2]) - 'A';
            decl.at = decl_item(decl.at);
        }
        if (last >= NLETTERS)
            return;
        for (uint32_t i = first; i <= last; i++)
            symb.deft[i] = et;
        defevent();
        if (*decl.at != ',')
            return;
        decl.at++;
    }
}

/* COMMON [SHARED] [/b/]: as DIM's. */
static void common(void)
{
    decl.at += 2;
    decl.scope = 0;
    const bool shared = *decl.at == TK_SHARED;
    if (shared)
        decl.at++;
    if (*decl.at == TK_DIV) {
        if (decl.at[1] != M_VAR)
            return;
        decl.at = comp_skipname(decl.at + 1);
        if (*decl.at != TK_DIV)
            return;
        decl.at++;
    }
    dimlist(shared ? BF_SHR : 0);
}

/* DIM [SHARED] n[( )] [AS type] {, ..} */
static void dim(const bool redim)
{
    decl.redim = redim;
    decl.at++;
    const bool shared = *decl.at == TK_SHARED;
    if (shared)
        decl.at++;
    decl.scope = shared ? 0 : decl.proc;
    dimlist(shared ? BF_SHR : 0);
}

/* CONST n = e {, n = e} */
static void konst(void)
{
    decl.at++;
    for (;;) {
        if (*decl.at != M_VAR)
            return;
        uint8_t sfx;
        const uint16_t n = name(&sfx);
        if (*decl.at != TK_EQ)
            return;
        decl.at++;
        uint8_t et = ET_I4; /* the value's type: its items' */
        int32_t depth = 0;
        for (;;) {
            const uint8_t c = *decl.at;
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
            decl.at = decl_item(decl.at);
        }
        if (decl.proc && scop_mshr(n)) /* a procedure's of a DIM SHARED name */
            repl_error(E_SN);
        Binding *const b = scop_bind(decl.proc, n);
        b->flags |= BF_CON;
        if (!decl.proc)
            b->flags |= BF_SHR; /* the module's: every procedure's */
        if (sfx)
            et = comp_suffix(sfx);
        b->et[0] = et;
        if (*decl.at != ',')
            return;
        decl.at++;
    }
}

/* The name at p used: with a suffix and no binding in decl.proc's scope, one of BF_USE. */
static void use(const uint8_t *const p)
{
    const uint16_t n = comp_nameof(p);
    if (!comp_suffixof(p) || scop_get(decl.proc, n))
        return;
    scop_bind(decl.proc, n)->flags = BF_USE;
}

/* The name at p: followed by (, an array known in decl.proc's scope. */
static void arrayuse(const uint8_t *const p)
{
    if (*comp_skipname(p) == '(')
        array(comp_nameof(p), decl.proc);
}

/* LBOUND or UBOUND at p: of an array known in decl.proc's scope, or one the module shares. */
static void bound(const uint8_t *const p)
{
    if (p[1] != '(' || p[2] != M_VAR)
        return;
    const uint16_t n = comp_nameof(p + 2);
    const uint16_t scope = decl.proc | SC_ARR;
    if (scop_get(scope, n))
        return;
    if (scope != SC_ARR && scop_get(SC_ARR, n)) {
        const Binding *b = scop_get(0, n);
        if (b && (b->flags & BF_SHR))
            return;
    }
    repl_error(E_AND);
}

/* To the next statement: past a ':' out of strings; REM's and DATA's text skipped. False: the
   end. */
static bool next(void)
{
    for (;;) {
        uint8_t c = *decl.at;
        if (!c || c == TK_REM)
            return false;
        if (c == ':') {
            decl.at++;
            return true;
        }
        if (c == TK_DATA) {
            decl.at = comp_skipdata(decl.at + 1);
            if (!*decl.at)
                return false;
            decl.at++;
            return true;
        }
        if (c == M_VAR) {
            use(decl.at);
            arrayuse(decl.at);
        } else if (c == TK_LBOUND || c == TK_UBOUND)
            bound(decl.at);
        decl.at = decl_item(decl.at);
    }
}

/* On standard error, a warning for each DEF SEG, VARSEG, SSEG: no effect, flat. Not in the
   editor. */
static void warn(const uint8_t *const body)
{
#if !IDE
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
        if (repl.fline) {
            memcpy(w, "line ", 5);
            w += 5;
            char dig[5];
            int n = 0;
            for (uint32_t v = repl.fline; v; v /= 10)
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
        uint32_t put;
        pfil_write(PFIL_STDERR, buf, (uint32_t)(w - buf), &put);
    }
#else
    (void)body;
#endif
}

void decl_line(const uint8_t *const body)
{
    warn(body);
    decl.at = body;
    if (*decl.at == M_INT) /* a line number */
        decl.at += INT_TOKEN_SIZE;
    do {
        const uint8_t c = *decl.at;
        switch (c) {
        case TK_DIM:
            dim(false);
            break;
        case TK_REDIM:
            dim(true);
            break;
        case TK_CONST:
            konst();
            break;
        case TK_SUB:
            subfn(NM_SUB);
            break;
        case TK_FUNCTION:
            subfn(NM_FUNC);
            break;
        case TK_DECLARE:
            declare();
            break;
        case TK_DEF:
            deffn();
            break;
        case TK_SHARED:
            shared(BF_REF);
            break;
        case TK_STATIC:
            shared(BF_STAT);
            break;
        case TK_END: /* END SUB, END FUNCTION: the module; END TYPE */
            if (decl.at[1] == TK_TYPE)
                decl.type = 0;
            else if (decl.at[1] == TK_SUB || decl.at[1] == TK_FUNCTION)
                decl.proc = 0;
            break;
        case TK_OPTION:
            option();
            break;
        case TK_TYPE:
            typedef_();
            break;
        case M_VAR:
            field();
            break;
        case TK_X:
            if (decl.at[1] == TK1_COMMON)
                common();
            else if (decl.at[1] == TK1_DEFSHORT) {
                decl.at += 2;
                deflist(ET_SH);
            }
            break;
        default:
            if (c >= TK_DEFINT && c <= TK_DEFSTR) {
                decl.at++;
                deflist(deftype[c - TK_DEFINT]);
            }
        }
    } while (next());
}
