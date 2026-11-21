/* A stopped run's variables and values, as a debugger sees them: which a frame shows, where each
 * is, its value as text.
 *
 * # How it works
 * A frame's variables are its procedure's (symb's scope), its parameters first (blok). A value is
 * found where the frame sees it (session_cell): a number's or string's cell, an array's header, a
 * record's header (a record is a one-element array of its TYPE, symb_is_record). Its text is by its
 * type: a variable's by its kind, as the run keeps it.
 *
 * A record's field is at its offset in it (decl's TypeDef): a fixed array field is its key's view,
 * a dynamic one its cell's header. An array's elements are its view's (arry_view), each its
 * element type's bytes, a STRING * n's n.
 */
#include "vm/inspect.h"
#include "comp/blocks.h"
#include "comp/declare.h"
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

/* A number, or an array of them, named as a record, or an array of them, of its scope: DIM r AS
   t's look-up of r, before its AS. */
static bool record_name(const Session *const S, const uint32_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    const uint32_t scope = symb_varx(&S->symb, id)->scope;
    const uint32_t own = sc_is_proc(scope) ? blok_proc(&S->blok, scope)->name : scope;
    const Binding *const b = v->type < ET_REC ? scop_get(&S->scop, own, v->name) : nullptr;
    return b && b->rec[!!(v->info & VI_ARR)];
}

/* A variable a debugger leaves out: a DEF FN's, a FOR loop's hidden limit or step, a number of a
   record's name. */
static bool hidden(const Session *const S, const uint32_t id)
{
    const Var *const v = symb_var(&S->symb, id);
    return (v->info & (VI_ARR | VK_MASK)) == VK_FN ||
           symb_text(&S->symb, v->name)[1] == LOOP_NAME || record_name(S, id);
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

/* Frame f of the stopped run; none, the module's. */
static DebugFrame frame(const Session *const S, const uint32_t f)
{
    DebugFrame d = {.line = DLINE, .call = -1};

    session_frame(S, f, &d);
    return d;
}

uint32_t insp_frames(const Session *const S) { return session_nframes(S); }

uint32_t insp_line(const Session *const S, const uint32_t f) { return frame(S, f).line; }

bool insp_newest(const Session *const S, const uint32_t f)
{
    const uint32_t proc = frame(S, f).proc;

    for (uint32_t i = 0; i < f; i++)
        if (frame(S, i).proc == proc)
            return false;

    return true;
}

Status insp_eval(Session *const S, const uint32_t f, const char *const expr, char out[static 80])
{
    return insp_eval_in(S, insp_scope(S, f), expr, out);
}

uint32_t insp_scope(const Session *const S, const uint32_t f) { return frame(S, f).proc; }

Status insp_eval_in(
    Session *const S,
    const uint32_t scope,
    const char *const expr,
    char out[static 80]
)
{
    return session_eval(S, expr, scope, out);
}

bool insp_settable(const Session *const S, const uint32_t f)
{
    return !session_in_direct(S) && (!f || !frame(S, f).proc); /* the probe's: the stop's frame */
}

Status insp_set(Session *const S, const uint32_t f, const char *const path, const char *const value)
{
    enum { STMT = 512 };

    char stmt[STMT], out[80];
    const size_t np = strlen(path), nv = strlen(value);

    if (np + nv + 4 > STMT)
        return E_LS;

    memcpy(stmt, path, np);
    memcpy(stmt + np, " = ", 3);
    memcpy(stmt + np + 3, value, nv + 1);
    return session_exec(S, stmt, frame(S, f).proc, out);
}

uint32_t insp_params(
    const Session *const S,
    const uint32_t f,
    InspVar *const out,
    const uint32_t max
)
{
    const uint32_t proc = frame(S, f).proc;
    const uint16_t npar = proc ? blok_proc(&S->blok, proc)->npar : 0;
    uint32_t n = 0;

    for (; n < npar && n < max; n++)
        out[n] = var_of(S, blok_proc(&S->blok, proc)->params[n].var);

    return n;
}

uint32_t insp_vars(const Session *const S, const uint32_t f, InspVar *const out, const uint32_t max)
{
    const uint32_t proc = frame(S, f).proc;
    uint32_t n = insp_params(S, f, out, max);

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
    const uint32_t f,
    InspVar *const out,
    const uint32_t max
)
{
    const uint32_t proc = frame(S, f).proc;
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

InspValue insp_var(const Session *const S, const uint32_t f, const InspVar *const var)
{
    const Var *const v = symb_var(&S->symb, var->id);
    const DebugFrame at = frame(S, f);
    const Addr cell = session_cell(S, &at, var->id, -1);

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

uint32_t insp_chars(
    const Session *const S,
    const InspValue *const v,
    char *const d,
    const uint32_t room
)
{
    const Desc *const s = (const Desc *)at(v->at);
    const uint8_t *const p = v->fix ? at(v->at) : at(s->ptr);
    const uint32_t len = v->fix ? v->fix : s->len;

    for (uint32_t i = 0; i < len && i < room; i++)
        d[i] = p[i] ? (char)p[i] : ' ';

    return len;
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
        *d++ = '"';
        const uint32_t len = insp_chars(S, v, d, room);
        d += len < room ? len : room;
        *d++ = '"';
        return d;
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

uint32_t insp_type(const Session *const S, const InspValue *const v)
{
    return decl_type(&S->decl, v->type - ET_REC)->name;
}

uint32_t insp_nfields(const Session *const S, const InspValue *const v)
{
    return decl_type(&S->decl, v->type - ET_REC)->nfld;
}

InspValue insp_field(
    const Session *const S,
    const InspValue *const v,
    const uint32_t i,
    uint32_t *const name
)
{
    const uint32_t t = v->type - ET_REC;
    const Field *const f = &decl_type(&S->decl, t)->fields[i];
    const Addr place = v->at + f->off;
    const uint32_t type = decl_fieldtype(f);
    *name = f->name;

    if (f->ndim)
        return (InspValue){
            .kind = IV_ARRAY,
            .type = type,
            .fix = f->fix,
            .at = place,
            .key = decl_fieldkey(t, i),
        };

    if (f->dyn)
        return (InspValue){.kind = IV_ARRAY, .type = type, .at = ((const ArrSlot *)at(place))->hdr};

    if (f->sub)
        return (InspValue){.kind = IV_RECORD, .type = type, .at = place};

    return (InspValue){
        .kind = IV_SCALAR,
        .type = type,
        .fix = f->fix,
        .at = place,
    };
}

/* Array v's view; none made, no dimensions. */
static ArrView view(const Session *const S, const InspValue *const v)
{
    if (v->key)
        return arry_view_field(&S->arry, v->at, v->key);

    return v->at ? arry_view(v->at) : (ArrView){};
}

uint8_t insp_dims(const Session *const S, const InspValue *const v) { return view(S, v).dims; }

int32_t insp_lbound(const Session *const S, const InspValue *const v, const uint8_t i)
{
    const ArrView w = view(S, v);
    return arry_view_lbound(&w, i);
}

uint32_t insp_count(const Session *const S, const InspValue *const v, const uint8_t i)
{
    const ArrView w = view(S, v);
    return arry_view_count(&w, i);
}

uint32_t insp_elems(const Session *const S, const InspValue *const v)
{
    const ArrView w = view(S, v);
    return w.dims ? arry_view_elems(&w) : 0;
}

InspValue insp_elem(const Session *const S, const InspValue *const v, const uint32_t i)
{
    const ArrView w = view(S, v);
    const uint32_t size = v->fix ? v->fix : arry_et_size(&S->decl, w.et);
    const Addr place = w.data + i * size;

    if (w.et >= ET_REC)
        return (InspValue){.kind = IV_RECORD, .type = w.et, .at = place};

    return (InspValue){
        .kind = IV_SCALAR,
        .type = w.et,
        .fix = v->fix,
        .at = place,
    };
}
