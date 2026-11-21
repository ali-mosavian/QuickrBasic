/* A stopped run's variables and values, as a debugger sees them: which a frame shows, where each
 * is, its value as text.
 *
 * # How it works
 * A frame's variables are its procedure's (symb's scope), its parameters first (blok). A value is
 * found where the frame sees it (session_cell): a number's or string's cell, an array's header, a
 * record's header (a record is a one-element array of its TYPE, symb_is_record). Its text is by its
 * type: a variable's by its kind, as the run keeps it.
 */
#include "vm/inspect.h"
#include "comp/blocks.h"
#include "mem/array.h"
#include "mem/scope.h"
#include "mem/symbols.h"
#include "num/double.h"

enum { VK_FN = 3 }; /* a DEF FN's kind (Var.info) */

static const char sfx_char[] = "%&!#$";                                /* a suffix, */
static const uint8_t sfx_type[] = {ET_I2, ET_I4, ET_R4, ET_R8, ET_SD}; /* its type */
static const char sfx_of[] = {0, '%', '&', 0, '#', '$'};               /* by ET_*: the one shown */

static InspVar var_of(const Session *const S, const uint32_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    const uint32_t scope = symb_varx(&S->symb, id)->scope;
    return (InspVar){
        .id = id,
        .name = v->name,
        .sfx = v->type <= ET_SD ? sfx_of[v->type] : 0,
        .local = scope & ~SC_MOD,
    };
}

/* A variable a debugger leaves out: a DEF FN's, a FOR loop's hidden limit or step. */
static bool hidden(const Session *const S, const uint32_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    return (v->info & (VI_ARR | VK_MASK)) == VK_FN || symb_text(&S->symb, v->name)[1] == LOOP_NAME;
}

/* id is a parameter of procedure proc. */
static bool is_param(const Session *const S, const uint32_t proc, const uint32_t id)
{
    if (!proc)
        return false;

    const Proc *const p = blok_proc(&S->blok, proc);

    for (uint16_t k = 0; k < p->npar; k++)
        if (p->params[k].var == id)
            return true;

    return false;
}

uint32_t insp_vars(
    const Session *const S,
    const uint32_t proc,
    InspVar *const out,
    const uint32_t max
)
{
    uint32_t n = 0;

    if (proc) {
        const Proc *const p = blok_proc(&S->blok, proc);

        for (uint16_t k = 0; k < p->npar && n < max; k++)
            out[n++] = var_of(S, p->params[k].var);
    }

    for (uint32_t id = 1; id < S->symb.nvars && n < max; id++)
        if (symb_varx(&S->symb, id)->scope == proc && !hidden(S, id) && !is_param(S, proc, id))
            out[n++] = var_of(S, id);

    return n;
}

/* Module variable id is seen in procedure proc: DIM SHARED, or SHARED there. */
static bool seen(const Session *const S, const uint32_t proc, const uint32_t id)
{
    const uint32_t name = symb_var(&S->symb, id)->name;
    const Binding *b = scop_get(&S->scop, 0, name);

    if (b && b->flags & BF_SHR)
        return true;

    b = scop_get(&S->scop, blok_proc(&S->blok, proc)->name, name);
    return b && b->flags & BF_REF;
}

uint32_t insp_shared(
    const Session *const S,
    const uint32_t proc,
    InspVar *const out,
    const uint32_t max
)
{
    uint32_t n = 0;

    for (uint32_t id = 1; proc && id < S->symb.nvars && n < max; id++)
        if (!symb_varx(&S->symb, id)->scope && !hidden(S, id) && seen(S, proc, id))
            out[n++] = var_of(S, id);

    return n;
}

/* A variable's type as its cell keeps it: by its kind. */
static uint32_t cell_type(const Var *const v)
{
    if (v->info & SK_DBL)
        return ET_R8;

    switch (v->info & VK_MASK) {
    case VK_STR:
        return ET_SD;

    case VK_INT:
        return ET_I4;

    default:
        return ET_R4;
    }
}

InspValue insp_var(const Session *const S, const DebugFrame *const f, const InspVar *const var)
{
    const Var *const v = symb_var(&S->symb, var->id);
    const Addr cell = session_cell(S, f, var->id, -1);

    if (symb_is_record(&S->symb, var->id))
        return cell ? (InspValue){.kind = IV_RECORD, .type = v->type, .at = arry_view(cell).data}

                    : (InspValue){.kind = IV_NONE};

    if (v->info & VI_ARR)
        return (InspValue){.kind = IV_ARRAY, .type = arry_var_et(&S->symb, var->id), .at = cell};

    return (InspValue){.kind = IV_SCALAR, .type = cell_type(v), .at = cell};
}

char *insp_name(const Session *const S, char *const d, const uint32_t name)
{
    const uint8_t *const t = symb_text(&S->symb, name);
    memcpy(d, t + 1, t[0]);
    return d + t[0];
}

bool insp_is(
    const Session *const S,
    const InspVar *const var,
    const uint8_t *const s,
    const uint32_t n,
    const uint8_t sfx
)
{
    const Var *const v = symb_var(&S->symb, var->id);

    if (sfx && sfx_type[strchr(sfx_char, sfx) - sfx_char] != v->type)
        return false;

    const uint8_t *const t = symb_text(&S->symb, v->name);

    if ((uint8_t)n != t[0])
        return false;
    for (uint32_t i = 0; i < n; i++)
        if (upcase(s[i]) != t[1 + i])
            return false;

    return true;
}

/* n bytes of text s in quotes at d, room of them at most; past it. */
static char *quoted(char *d, const uint8_t *const s, const uint32_t n, const uint32_t room)
{
    const uint32_t len = n > room ? room : n;
    *d++ = '"';
    memcpy(d, s, len);
    d += len;
    *d++ = '"';
    return d;
}

char *insp_text(const Session *const S, const InspValue *const v, char *d, const uint32_t room)
{
    uint8_t buf[DUBL_TXT];
    uint32_t n;

    switch (v->type) {
    case ET_R8: {
        double x;
        memcpy(&x, at(v->at), sizeof x);
        n = dubl_fmt(x, buf);
        break;
    }

    case ET_R4:
    case ET_IMP:
        n = dubl_fmtf(rd32(v->at), buf);
        break;

    case ET_SD: {
        if (v->fix)
            return quoted(d, at(v->at), v->fix, room);

        const Desc *const s = (const Desc *)at(v->at);
        return quoted(d, at(s->ptr), s->len, room);
    }

    case ET_SH:
        n = dubl_fmt((int16_t)rd16(v->at), buf);
        break;

    case ET_BY:
        n = dubl_fmt(rd8(v->at), buf);
        break;

    default:
        n = dubl_fmt((int32_t)rd32(v->at), buf);
        break;
    }

    const uint8_t *s = buf;

    if (*s == ' ') {
        s++;
        n--;
    }

    memcpy(d, s, n);
    return d + n;
}
