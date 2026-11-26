/* Quickr's code generator: places: records and lists.
 *
 * # How it works
 * A record is QBasic's TYPE: a variable's a box R.ADDR finds, a field FL.LD and FL.ST at its
 * offset. A list is a dynamic array: an element I.LD.A1 and its kin, its methods CALL QX. A
 * record or list result is written through a last parameter, the caller's place; a fresh one is
 * built straight there.
 */
#include "qr/qcg_int.h"
#include "comp/declare.h"
#include "qr/qnat.h"

static void qcg_elem(QCg *g, const QExpr *e);
static uint32_t qcg_addr(QCg *g, const QExpr *e);
static void qcg_loadfrom(QCg *g, const QExpr *e);
static void qcg_listdo(QCg *g, const QExpr *e, const QExpr *dst);
static void qcg_build(QCg *g, const QExpr *dst, const QExpr *e);
static void qcg_place(QCg *g, const QExpr *dst, const QExpr *e);

/* ---- places: records and lists */

const QTy *qcg_ty(const QCg *const g, const QType t) { return qast_ty(g->env->prog, t); }

bool qcg_rec(const QCg *const g, const QType t) { return qast_record(g->env->prog, t); }

static bool qcg_list(const QCg *const g, const QType t) { return qast_array(g->env->prog, t); }

/* Is t's value kept in a place, a record's or a list's, not on the stack? */
bool qcg_byplace(const QCg *const g, const QType t) { return qcg_rec(g, t) || qcg_list(g, t); }

uint32_t qcg_scalar_et(const QType t)
{
    switch (t) {
    case QY_SINGLE:
        return ET_R4;

    case QY_DOUBLE:
    case QY_LONG:
    case QY_ULONG:
        return ET_R8;

    case QY_STRING:
        return ET_SD;

    case QY_BYTE:
        return ET_SB;

    case QY_UBYTE:
        return ET_BY;

    case QY_SHORT:
        return ET_SH;

    case QY_USHORT:
        return ET_US;

    default:
        return ET_I4;
    }
}

/* Is t kept in a TYPE: a record, a fixed array, or a list that is an element (qr/qlink.c)? */
static bool qcg_typed(const QCg *const g, const QType t)
{
    return qcg_rec(g, t) || (qcg_list(g, t) && qcg_ty(g, t)->boxed);
}

/* Type t's ET_*: a record's or a boxed array's ET_REC + its TYPE, a scalar's qcg_scalar_et. */
static uint32_t qcg_et(const QCg *const g, const QType t)
{
    return qcg_typed(g, t) ? ET_REC + qcg_ty(g, t)->td : qcg_scalar_et(t);
}

/* Is t kept in 2 bytes or 1 in a record or a list: N.LD.A, P.REFN? */
static bool qcg_short(const QCg *const g, const QType t) { return et_narrow(qcg_et(g, t)); }

/* A value of type t's bytes in a record or an array. */
uint32_t qcg_size(const QCg *const g, const QType t)
{
    switch (qcg_et(g, t)) {
    case ET_R8:
        return 8;

    case ET_SD:
        return DSZ;

    case ET_SH:
    case ET_US:
        return 2;

    case ET_BY:
    case ET_SB:
        return 1;

    default:
        return qcg_typed(g, t) ? qcg_ty(g, t)->size : 4;
    }
}

QExpr qcg_named(QVar *const v) { return (QExpr){.kind = QE_NAME, .var = v, .type = v->type}; }

/* Place a's field k. */
QExpr qcg_fieldof(const QCg *const g, const QExpr *const a, const uint32_t k)
{
    return (QExpr){
        .kind = QE_FIELD,
        .a = (QExpr *)a,
        .field = k,
        .type = qcg_ty(g, a->type)->fields[k].type,
    };
}

/* The address on top moved on by off bytes. */
static void qcg_offset(QCg *const g, const uint32_t off)
{
    if (!off)
        return;

    uint8_t *const right = g->c->out;
    code_int(g->c, off);
    qcg_arith(g->c, right, QY_INT, QA_ADD);
}

/* A fresh record or list built in its temporary (sema's): that temporary's place. */
QExpr qcg_settle(QCg *const g, const QExpr *const e)
{
    const QExpr t = qcg_named(e->var);
    qcg_build(g, &t, e);
    return t;
}

/* The copy of a for's x that element place e names (qr/qsem.c qsem_each), its E.NEXT's: a number,
   or a record in cells; a resumable's, by reference, a number of 4 bytes or 8. Null none. */
const QVar *qcg_copyat(const QCg *const g, const QExpr *const e)
{
    const QVar *const v = e->kind == QE_INDEX ? e->var : nullptr;

    if (!v)
        return nullptr;
    if (qcg_rec(g, v->type))
        return v->cells && !v->byref ? v : nullptr;

    return !v->byref || !qcg_short(g, v->type) ? v : nullptr;
}

/* Record place a's field f's offset from a's address, pushed; f's own added. */
static uint32_t qcg_fieldat(QCg *const g, const QExpr *const f)
{
    return qcg_addr(g, f->a) + qcg_ty(g, f->a->type)->fields[f->field].off;
}

/* The variable a list or array place names: its own, or the temporary a fresh one is built in. */
static QVar *qcg_listvar(QCg *const g, const QExpr *const a)
{
    if (a->kind == QE_NAME)
        return a->var;

    qcg_settle(g, a);
    return a->var;
}

/* Is list place e held in a TYPE's bytes: a record's field, a list of lists' element, a dict's
   value? */
static bool qcg_held(const QCg *const g, const QExpr *const e)
{
    return e->kind == QE_FIELD || (e->kind == QE_INDEX && qcg_list(g, e->type));
}

/* Held list place e's TYPE's address pushed; the bytes returned still to add, a field's. */
static uint32_t qcg_heldat(QCg *const g, const QExpr *const e)
{
    if (e->kind == QE_FIELD)
        return qcg_fieldat(g, e);

    qcg_elem(g, e);
    return 0;
}

/* QX.MAPAT's and QX.MAPDEL's operands: map place m's key, its ETs, then its address, last: the
   key's evaluation may move the heap. */
static void qcg_mapargs(QCg *const g, const QExpr *const m, const QExpr *const key)
{
    const QTy *const y = qcg_ty(g, m->type);
    qcg_as(g, key, y->key);
    code_int(g->c, qcg_et(g, y->key));
    code_int(g->c, qcg_et(g, y->elem));
    qcg_offset(g, qcg_addr(g, m));
}

/* Element place e, a[i]: its address pushed. A field's list's by FE.ADDR, its fixed array's by
   FX.IDX; a variable's by RA.ADDR; a map's value by QX.MAPAT. */
static void qcg_elem(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QExpr *const a = e->a;
    const QTy *const y = qcg_ty(g, a->type);

    if (y->kind == QT_MAP) { /* an insert may grow the map */
        const bool waiting = code_calling(c, e->insert);
        qcg_mapargs(g, a, e->b);
        code_int(c, e->insert ? QM_SET : QM_READ);
        qcg_x(c, QX_MAPAT);
        code_called(c, waiting, e->insert);
        c->held = true;
        return;
    }

    if (qcg_held(g, a)) {
        const uint32_t off = qcg_heldat(g, a);

        if (y->kind == QT_LIST) {
            qcg_expr(g, e->b);
            code_op(c, O_FE_ADDR);
            code_dword(c, off);
            code_byte(c, 1);
            code_dword(c, qcg_size(g, y->elem));
            return;
        }

        qcg_offset(g, off);
        qcg_expr(g, e->b);
        code_op(c, O_FX_IDX);
        code_dword(c, 0);
        code_dword(c, y->count);
        code_dword(c, qcg_size(g, y->elem));
        return;
    }

    const QVar *const v = qcg_listvar(g, a);
    qcg_expr(g, e->b);
    code_opid(c, O_RA_ADDR, v->id, false);
    code_byte(c, 1);
}

/* Record place e's address pushed; the bytes returned still to add, a field's. */
static uint32_t qcg_addr(QCg *const g, const QExpr *const e)
{
    if (qast_fresh(e)) {
        const QExpr t = qcg_settle(g, e);
        return qcg_addr(g, &t);
    }

    const QVar *const v =
        e->kind == QE_NAME ? e->var : (qcg_rec(g, e->type) ? qcg_copyat(g, e) : nullptr);

    if (v) {
        code_opid(g->c, v->cells ? O_V_CELL : O_R_ADDR, v->id, false);
        return 0;
    }

    if (e->kind == QE_FIELD)
        return qcg_fieldat(g, e);

    qcg_elem(g, e);
    return 0;
}

/* List place e's cell's address pushed: a variable's, a parameter's through its ref, a held
   one's. */
void qcg_cell(QCg *const g, const QExpr *const e)
{
    if (qcg_held(g, e)) {
        qcg_offset(g, qcg_heldat(g, e));
        return;
    }

    const QVar *const v = qcg_listvar(g, e);
    code_opid(g->c, qcg_par(v) ? O_V_REF : O_V_CELL, v->id, false);
}

/* A field's load or store of type t, off bytes past the address under it. */
static void qcg_fieldop(QCg *const g, const bool store, const uint32_t off, const QType t)
{
    Code *const c = g->c;

    if (t == QY_STRING) {
        code_xop(c, store ? X_SF_ST : X_SF_LD);
        code_word(c, (uint16_t)off);
        code_word(c, 0); /* a variable string's Desc */
        return;
    }

    code_op(c, store ? O_FL_ST : O_FL_LD);
    code_word(c, (uint16_t)off);
    code_byte(c, (uint8_t)qcg_et(g, t));
}

/* Field place e as its word's variable, *w, when its record is inline (qr/qlink.c) and it is a
   number in its first QAST_WORDS bytes. */
static bool qcg_word(const QCg *const g, const QExpr *const e, QVar *const w)
{
    if (e->kind != QE_FIELD || qcg_byplace(g, e->type) || e->type == QY_STRING)
        return false;

    uint32_t off = 0;
    const QExpr *r = e;

    for (; r->kind == QE_FIELD; r = r->a)
        off += qcg_ty(g, r->a->type)->fields[r->field].off;

    const QVar *const v = r->kind == QE_NAME ? r->var : qcg_copyat(g, r);

    if (!v || !v->cells || off >= QAST_WORDS || !qcg_ty(g, v->type)->word[off])
        return false;

    *w = (QVar){
        .type = e->type,
        .kind = QV_VAR,
        .id = v->words + qcg_ty(g, v->type)->word[off] - 1,
    };

    return true;
}

/* Word w's load or store: a narrow field's by N.LD.V or N.ST.V, its ET. */
static void qcg_wordop(const QCg *const g, const bool store, const QVar *const w)
{
    if (!qcg_short(g, w->type)) {
        (store ? qcg_store : qcg_load)(g->c, w);
        return;
    }

    code_opid(g->c, store ? O_N_ST_V : O_N_LD_V, w->id, false);
    code_byte(g->c, (uint8_t)qcg_et(g, w->type));
}

/* Field place e of a number in a record parameter's record: that parameter, *off the field's
   bytes into its record; null none. A parameter by its ref cell (qr/qlink.c): an inlined fn's is
   its caller's variable, a temporary's or none. */
static const QVar *qcg_pfield(const QCg *const g, const QExpr *const e, uint32_t *const off)
{
    if (e->kind != QE_FIELD || qcg_byplace(g, e->type) || e->type == QY_STRING)
        return nullptr;

    const QExpr *r = e;
    *off = 0;

    for (; r->kind == QE_FIELD; r = r->a)
        *off += qcg_ty(g, r->a->type)->fields[r->field].off;

    if (r->kind != QE_NAME || r->var->cells || !symb_is_ref(g->env->sy, r->var->id) ||
        *off > UINT16_MAX)
        return nullptr;

    return r->var;
}

/* Record parameter par's field's load or store, off bytes into its record: P.FL.LD or P.FL.ST. */
static void qcg_pfieldop(
    const QCg *const g,
    const bool store,
    const QVar *const par,
    const uint32_t off,
    const QType t
)
{
    code_opid(g->c, store ? O_P_FL_ST : O_P_FL_LD, par->id, false);
    code_word(g->c, (uint16_t)off);
    code_byte(g->c, (uint8_t)qcg_et(g, t));
}

/* Is element place e a variable's list's, loaded and stored by I.LD.A1 and its kin? */
static bool qcg_byindex(const QCg *const g, const QExpr *const e)
{
    return e->kind == QE_INDEX && !qcg_held(g, e->a) && qcg_list(g, e->a->type);
}

/* Scalar place e's value pushed: an address it pushes taken by the load. */
void qcg_loadat(QCg *const g, const QExpr *const e)
{
    const bool held = g->c->held;
    qcg_loadfrom(g, e);
    g->c->held = held;
}

static void qcg_loadfrom(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    QVar w;

    if (e->kind == QE_NAME) {
        qcg_load(c, e->var);
        return;
    }

    if (qcg_word(g, e, &w)) {
        qcg_wordop(g, false, &w);
        return;
    }

    if (qcg_copyat(g, e)) { /* a for's x: its copy, E.NEXT's */
        qcg_load(c, qcg_copyat(g, e));
        return;
    }

    uint32_t off;
    const QVar *const par = qcg_pfield(g, e, &off);

    if (par) {
        qcg_pfieldop(g, false, par, off, e->type);
        return;
    }

    if (e->kind == QE_FIELD) {
        qcg_fieldop(g, false, qcg_fieldat(g, e), e->type);
        return;
    }

    if (qcg_byindex(g, e)) {
        const QVar *const v = qcg_listvar(g, e->a);
        qcg_expr(g, e->b);

        if (qcg_short(g, e->type)) {
            code_opid(c, O_N_LD_A, v->id, false);
            code_byte(c, 1);
            code_byte(c, (uint8_t)qcg_et(g, e->type));
            return;
        }

        code_opid(c, qcg_pick(e->type, O_I_LD_A1, O_F_LD_A1, O_D_LD_A1, O_S_LD_A1), v->id, false);
        return;
    }

    qcg_elem(g, e);
    qcg_fieldop(g, false, 0, e->type);
}

/* A list element's store, the index under the value. */
static void qcg_storeat(QCg *const g, const QVar *const list, const QType type)
{
    if (qcg_short(g, type)) {
        code_opid(g->c, O_N_ST_A, list->id, false);
        code_byte(g->c, 1);
        code_byte(g->c, (uint8_t)qcg_et(g, type));
        return;
    }

    code_opid(g->c, qcg_pick(type, O_I_ST_A1, O_F_ST_A1, O_D_ST_A1, O_S_ST_A1), list->id, false);
}

/* A scalar place's store: what goes before the value (an address, an index), then its op. */
typedef struct {
    uint32_t off;
    const QVar *list; /* by I.ST.A1 and its kin */
    const QVar *par;  /* by P.FL.ST, a record parameter's field */
    bool at;          /* by FL.ST or SF.ST, the address under the value */
} QCgPut;

static QCgPut qcg_before(QCg *const g, const QExpr *const e)
{
    QVar w;
    uint32_t off;

    if (e->kind == QE_NAME || qcg_word(g, e, &w))
        return (QCgPut){};

    const QVar *const par = qcg_pfield(g, e, &off);

    if (par)
        return (QCgPut){.off = off, .par = par};

    if (e->kind == QE_FIELD)
        return (QCgPut){.off = qcg_fieldat(g, e), .at = true};

    if (qcg_byindex(g, e)) {
        const QVar *const v = qcg_listvar(g, e->a);
        qcg_expr(g, e->b);
        return (QCgPut){.list = v};
    }

    qcg_elem(g, e);
    return (QCgPut){.at = true};
}

static void qcg_put(QCg *const g, const QExpr *const e, const QCgPut p)
{
    if (p.par)
        qcg_pfieldop(g, true, p.par, p.off, e->type);
    else if (p.at)
        qcg_fieldop(g, true, p.off, e->type);
    else if (p.list)
        qcg_storeat(g, p.list, e->type);
    else if (e->kind == QE_NAME)
        qcg_store(g->c, e->var);
    else {
        QVar w;
        qcg_word(g, e, &w);
        qcg_wordop(g, true, &w);
    }
}

/* Record place p emptied: its strings "", its lists empty, the rest 0. */
static void qcg_clear(QCg *const g, const QExpr *const p)
{
    qcg_offset(g, qcg_addr(g, p));
    code_int(g->c, qcg_ty(g, p->type)->td);
    code_call(g->c, K_RCLR);
}

/* Record place p, emptied, given its fields' defaults, its records' too. */
static void qcg_defaults(QCg *const g, const QExpr *const p)
{
    const QTy *const y = qcg_ty(g, p->type);

    for (uint32_t k = 0; y->kind == QT_REC && k < y->nfields; k++) {
        const QExpr f = qcg_fieldof(g, p, k);

        if (y->fields[k].def)
            qcg_set(g, &f, y->fields[k].def);
        else if (qcg_rec(g, f.type))
            qcg_defaults(g, &f);
    }
}

/* A record of type t copied, its destination's and its source's addresses on the stack: R.COPY,
   or RCOPY when it holds strings or lists. */
static void qcg_recop(QCg *const g, const QType t)
{
    const QTy *const y = qcg_ty(g, t);

    if (y->deep) {
        code_int(g->c, y->td);
        code_call(g->c, K_RCOPY);
        return;
    }

    code_xop(g->c, X_R_COPY);
    code_word(g->c, (uint16_t)y->size);
}

/* Record place src's value copied to record place dst. */
void qcg_copyrec(QCg *const g, const QExpr *const dst, const QExpr *const src)
{
    qcg_offset(g, qcg_addr(g, dst));
    qcg_offset(g, qcg_addr(g, src));
    qcg_recop(g, dst->type);
}

/* x?: T! place x checked, its error copied to the fn's result and returned; x's place, a fresh
   one's temporary, for its value's read. */
QExpr qcg_tried(QCg *const g, const QExpr *const e)
{
    const QExpr t = qast_fresh(e->a) ? qcg_settle(g, e->a) : *e->a;
    const QExpr err = qcg_fieldof(g, &t, QF_FLAG);
    qcg_loadat(g, &err);
    const QCgHop ok = qcg_hop(g, QJ_FALSE);
    const QExpr res = qcg_named(e->fn->res);
    static const uint32_t parts[] = {QF_FLAG, QF_CODE, QF_MESSAGE, QF_LINE};

    for (uint32_t k = 0; k < countof(parts); k++) {
        const QExpr to = qcg_fieldof(g, &res, parts[k]);
        const QExpr from = qcg_fieldof(g, &t, parts[k]);
        qcg_set(g, &to, &from);
    }

    qcg_dropall(g, &e->fn->own);
    code_op(g->c, O_PRET);
    code_dword(g->c, e->fn->proc);
    code_dword(g->c, 0);
    qcg_land(g, ok);
    return t;
}

/* m.get(k) built in dst: has whether k's value's address is not 0, then the value from it, a
   record's copied; a T or none value's copied whole, its own flag over has. */
static void qcg_get(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    Code *const c = g->c;
    const QVar *const at = e->temps[0];
    const QExpr has = qcg_fieldof(g, dst, QF_FLAG);
    const QExpr val = qcg_fieldof(g, dst, QF_VAL);

    qcg_mapargs(g, e->a, e->b);
    code_int(c, QM_FIND);
    qcg_x(c, QX_MAPAT);
    qcg_store(c, at);

    for (uint32_t k = 0; k < 2; k++) { /* has set, then a jump past the value when not */
        const QCgPut put = k ? (QCgPut){} : qcg_before(g, &has);

        qcg_load(c, at);
        uint8_t *const right = c->out;
        code_int(c, 0);
        qcg_compare(c, QY_INT, QK_NE, right);

        if (!k)
            qcg_put(g, &has, put);
    }

    const QCgHop none = qcg_hop(g, QJ_FALSE);

    if (dst->type == qcg_ty(g, e->a->type)->elem) { /* a T or none's: whole, its flag too */
        qcg_offset(g, qcg_addr(g, dst));
        qcg_load(c, at);
        qcg_recop(g, dst->type);
    } else if (qcg_rec(g, val.type)) {
        qcg_offset(g, qcg_addr(g, &val));
        qcg_load(c, at);
        qcg_recop(g, val.type);
    } else if (qcg_list(g, val.type)) {
        qcg_load(c, at);
        qcg_cell(g, &val);
        code_call(c, K_ACOPY);
    } else {
        const QCgPut put = qcg_before(g, &val);
        qcg_load(c, at);
        qcg_fieldop(g, false, 0, val.type);
        qcg_put(g, &val, put);
    }

    qcg_land(g, none);
}

/* A slice's two ends pushed: 0 and the most an int holds for those left out. */
void qcg_ends(QCg *const g, const QExpr *const e)
{
    if (e->b)
        qcg_expr(g, e->b);
    else
        code_int(g->c, 0);

    if (e->c)
        qcg_expr(g, e->c);
    else
        code_int(g->c, INT32_MAX);
}

/* Fixed array place e's first element's address pushed: inline, or a variable's array's. */
static void qcg_fixedat(QCg *const g, const QExpr *const e)
{
    if (qcg_held(g, e)) {
        qcg_offset(g, qcg_heldat(g, e));
        return;
    }

    code_int(g->c, 0);
    code_opid(g->c, O_RA_ADDR, qcg_listvar(g, e)->id, false);
    code_byte(g->c, 1);
}

/* List place src's elements to list place dst: moved (src left empty), or copied. A fixed array
   inline either side is copied as its TYPE (qr/qlink.c). */
static void qcg_listto(
    QCg *const g,
    const QExpr *const dst,
    const QExpr *const src,
    const bool move
)
{
    if (qcg_ty(g, dst->type)->kind == QT_ARRAY && (qcg_held(g, dst) || qcg_held(g, src))) {
        qcg_fixedat(g, dst);
        qcg_fixedat(g, src);
        qcg_recop(g, dst->type);
        return;
    }

    qcg_cell(g, src);
    qcg_cell(g, dst);
    code_call(g->c, move ? K_AMOVE : K_ACOPY);
}

/* p = p + e, p - e or p * e of a long variable p in one op, L.TO.V: e's value, an int cell's as it
   is. False: not one. */
static bool qcg_longto(QCg *const g, const QExpr *const p, const QExpr *const v)
{
    static const uint8_t ks[] = {[QK_PLUS] = LTO_ADD, [QK_MINUS] = LTO_SUB, [QK_STAR] = LTO_MUL};

    if (p->kind != QE_NAME || p->type != QY_LONG || qcg_par(p->var) || v->kind != QE_BINARY ||
        v->wrap || v->work != QY_LONG || v->a->kind != QE_NAME || v->a->var != p->var ||
        (v->op != QK_PLUS && v->op != QK_MINUS && v->op != QK_STAR))
        return false;

    const QExpr *b = v->b;

    if (b->kind == QE_CONV && !b->wrap && qast_integer(b->a->type))
        b = b->a;

    const bool cell = b->kind != QE_CONST && qast_integer(b->type) && qcg_icell(b->type);

    if (cell)
        qcg_expr(g, b);
    else
        qcg_as(g, v->b, QY_LONG);

    code_opid(g->c, O_L_TO_V, p->var->id, false);
    code_byte(g->c, ks[v->op] | (cell ? LTO_INT : 0));
    return true;
}

/* Scalar or compound value v stored in place p. */
void qcg_set(QCg *const g, const QExpr *const p, const QExpr *const v)
{
    if (qcg_byplace(g, p->type)) {
        qcg_into(g, p, v);
        return;
    }

    if (qcg_longto(g, p, v))
        return;

    const bool held = g->c->held;
    const QCgPut put = qcg_before(g, p);
    qcg_as(g, v, p->type);
    qcg_put(g, p, put);
    g->c->held = held; /* its address taken by the store */
}

/* Record, tuple or enum value e (QE_NEW) built in place dst, field by field: emptied first when
   a field left out is no number. */
static void qcg_new(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    const QTy *const y = qcg_ty(g, e->type);
    const bool en = y->kind == QT_ENUM;
    const uint32_t first = en ? y->vars[e->field].first : 0;
    bool clear = false;

    for (uint32_t k = 0; k < e->nargv; k++)
        clear |= !e->argv[k] && qcg_byplace(g, y->fields[first + k].type);

    if (clear)
        qcg_clear(g, dst);

    if (en) {
        const QExpr tag = qcg_fieldof(g, dst, 0);
        const QExpr v = {.kind = QE_CONST, .type = QY_INT, .val.i = e->field};

        qcg_set(g, &tag, &v);
    }

    for (uint32_t k = 0; k < e->nargv; k++) {
        const QExpr f = qcg_fieldof(g, dst, first + k);
        const QExpr zero = {.kind = QE_CONST, .type = f.type};

        if (e->argv[k])
            qcg_set(g, &f, e->argv[k]);
        else if (qcg_rec(g, f.type))
            qcg_defaults(g, &f);
        else if (!clear || !qcg_byplace(g, f.type))
            qcg_set(g, &f, &zero);
    }
}

/* A by-reference argument, a 0 and an address: a variable's cell, a field's, an element's, a
   list's cell. */
void qcg_ref(QCg *const g, const QExpr *const a)
{
    Code *const c = g->c;

    if (qcg_list(g, a->type)) {
        code_int(c, 0);
        qcg_cell(g, a);

        if (qcg_held(g, a) && qcg_ty(g, a->type)->kind == QT_ARRAY) { /* inline: a cell made */
            code_int(c, (int32_t)(ARR_FIELD | decl_fieldkey(qcg_ty(g, a->type)->td, 0)));
            code_call(c, K_FCELL);
        }

        return;
    }

    const QVar *const v = a->kind == QE_NAME ? a->var : qcg_copyat(g, a);

    if (v && !qcg_rec(g, a->type)) {
        code_int(c, 0);
        code_opid(c, qcg_par(v) ? O_V_REF : O_V_CELL, v->id, false);
        return;
    }

    const uint32_t off = qcg_addr(g, a);

    if (qcg_short(g, a->type)) { /* its ref tagged: read and written as 2 bytes or 1 */
        qcg_offset(g, off);
        code_xop(c, X_P_REFN);
        code_byte(c, (uint8_t)qcg_et(g, a->type));
        return;
    }

    code_op(c, O_P_REF);
    code_word(c, (uint16_t)off);
}

/* Is e the string " "? */
static bool qcg_space(const QExpr *const e)
{
    const QPiece *const s = e->kind == QE_CONST && e->type == QY_STRING ? e->val.s : nullptr;
    return s && !s->next && s->lit->len == 1 && s->lit->text[0] == ' ';
}

/* print a; b, c: each spelled, a , to the next zone; a new line unless a separator ends it. A
   " " is QPR's space, as cheap as print(a, b)'s was. */
static void qcg_print(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;

    for (uint32_t i = 0; i < e->nargv; i++) {
        const QExpr *const a = e->argv[i];

        if (qcg_space(a))
            code_int(c, QRT_SPACE);
        else {
            qcg_expr(g, a);
            code_int(c, qcg_kind(a->type));
        }

        code_call(c, K_QPR);

        if (e->ops[i] == QK_COMMA)
            code_call(c, K_PZONE);
    }

    if (!e->nargv || e->ops[e->nargv - 1] == QK_EOF)
        code_call(c, K_PNL);
}

/* Argument a for a parameter of type t: by reference where it has a place the call can't change,
   else a value cell and its ref; a string's counted held, on the stack under the rest. */
static void qcg_arg(QCg *const g, const QExpr *const a, const QType t, uint32_t *const held)
{
    if (qcg_byplace(g, t) || (!qast_fresh(a) && a->type == t && !a->byval)) {
        qcg_ref(g, a);
        return;
    }

    qcg_expr(g, a);
    code_opb(g->c, O_BYVAL, a->type == QY_STRING);
    *held += a->type == QY_STRING;
    qcg_hold(g, a->type, 1);
}

/* A call through fn value e->a: its arguments as qcg_call's, a record's or list's result's place
   dst, then the value, QX.CALL. */
static void qcg_icall(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    const QTy *const y = qcg_ty(g, e->a->type);
    uint32_t held = 0;
    const bool waiting = code_calling(g->c, true);

    for (uint32_t k = 0; k < e->nargv; k++)
        qcg_arg(g, e->argv[k], y->fields[k].type, &held);

    if (qcg_byplace(g, y->elem))
        qcg_ref(g, dst);

    qcg_expr(g, e->a);
    g->strings -= held;
    qcg_x(g->c, QX_FNCALL);
    code_called(g->c, waiting, true);
}

/* Native fn call e: its entry's steps, each argument as its parameter's type. */
static void qcg_native(QCg *const g, const QExpr *const e)
{
    Code *const c = g->c;
    const QNat *const nat = e->fn->nat;
    uint32_t held = 0;
    const bool waiting = code_calling(c, true); /* one may run a lambda */

    for (uint32_t i = 0; i < nat->n; i++) {
        const QNatStep st = nat->steps[i];

        switch (st.kind) {
        case QN_ARG: {
            const QType t = e->fn->params[st.n].var->type;
            qcg_as(g, e->argv[st.n], t);
            qcg_hold(g, t, 1);
            held += t == QY_STRING;
            break;
        }

        case QN_OP:
            code_op(c, st.op);
            break;

        case QN_OPB:
            code_opb(c, st.op, (uint8_t)st.n);
            break;

        case QN_INT:
            code_int(c, (uint32_t)st.n);
            break;

        case QN_CALL:
            code_call(c, st.op);
            break;

        case QN_SYS:
            code_opb(c, O_SYSCALL, st.op);
            break;

        case QN_X:
            code_int(c, (uint32_t)st.n);
            code_call(c, K_QX);
        }
    }

    g->strings -= held;
    code_called(c, waiting, true);
}

/* Fn f called, its arguments pushed. */
void qcg_pcall(QCg *const g, const QFn *const f)
{
    Code *const c = g->c;

    if (f->ncall) {
        code_ncall(c, f->proc);
        return;
    }

    code_op(c, O_PCALL); /* the procedure, its locals' cells, its parameters (link's), its body */
    code_dword(c, f->proc);
    code_dword(c, 0);
    code_word(c, 0);

    if (!f->foreign) {
        qcg_target(g, f->def, 0, PAST_JMP);
        return;
    }

    for (uint32_t i = 0; i < countof(f->entry); i++)
        code_dword(c, f->entry[i]);
}

/* An import's: each module it names that an IMPORT runs (QMod.start: a QBasic one, or any of a
   QBasic program's) run, the first time (comp/import.c). */
void qcg_imports(QCg *const g, const QStmt *const s)
{
    const QImport *const lists[] = {s->imps, s->kind == QS_FROM ? s->names : nullptr};

    for (uint32_t j = 0; j < countof(lists); j++)
        for (const QImport *im = lists[j]; im; im = im->next) {
            const QMod *const mod =
                im->mod < g->env->prog->nmods ? &g->env->prog->mods[im->mod] : nullptr;

            if (!mod || !mod->start || (j && !s->imps->folder))
                continue;

            code_xop(g->c, X_IMPORT);
            code_dword(g->c, mod->modu);
            code_dword(g->c, mod->start);
        }
}

/* A fn's call: each argument by reference where it has a place, else a value cell and its ref;
   a record's or list's result's place, dst, last. A parameter bound to a lambda takes none. */
void qcg_call(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    if (e->builtin == QB_PRINT) {
        qcg_print(g, e);
        return;
    }

    if (e->builtin == QB_ICALL) {
        qcg_icall(g, e, dst);
        return;
    }

    const QFn *const f = e->fn;
    uint32_t held = 0;

    if (f->native) {
        qcg_native(g, e);
        return;
    }

    const bool waiting = code_calling(g->c, true);

    for (uint32_t k = 0; k < f->npar; k++) {
        const QParam *const p = &f->params[k];
        const QExpr state = f->resumable ? qcg_fieldof(g, e->c, k) : (QExpr){};

        const QExpr *const a = !f->resumable              ? e->argv[k]
                               : k < f->nown && p->hidden ? f->keep[k] /* a lambda's capture */
                                                          : &state;

        if (p->lam) /* bound to its lambda: no value */
            continue;

        if (p->mut) {
            qcg_ref(g, a);
            continue;
        }

        qcg_arg(g, a, p->var->type, &held);
    }

    if (f->byarg)
        qcg_ref(g, dst);

    g->strings -= held;
    qcg_pcall(g, f);
    code_called(g->c, waiting, true);
}

/* Lambda application e's arguments, each stored to its parameter's temporary; those read where
   they are need none. */
void qcg_applied(QCg *const g, const QExpr *const e)
{
    for (uint32_t k = 0; k < e->nargv; k++)
        if (e->temps[k]) {
            const QExpr t = qcg_named(e->temps[k]);
            qcg_set(g, &t, e->argv[k]);
        }
}

/* CALL QX op. */
void qcg_x(Code *const c, const QrtX op)
{
    code_int(c, op);
    code_call(c, K_QX);
}

/* A list's method; a record's pop copied to dst. */

/* List or array place a's elements for QX.SWAP and QX.SORT: a list's or an array variable's cell;
   a fixed array field's first element's address and count, QX_INLINE returned. */
static uint32_t qcg_elems(QCg *const g, const QExpr *const a)
{
    const QTy *const y = qcg_ty(g, a->type);

    if (!qcg_held(g, a) || y->kind != QT_ARRAY) {
        qcg_cell(g, a);
        return 0;
    }

    qcg_offset(g, qcg_heldat(g, a));
    code_int(g->c, y->count);
    return QX_INLINE;
}

/* A list's or a map's method: one may grow, shrink or sort by a lambda. */
void qcg_listop(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    const bool waiting = code_calling(g->c, true);
    qcg_listdo(g, e, dst);
    code_called(g->c, waiting, true);
}

static void qcg_listdo(QCg *const g, const QExpr *const e, const QExpr *const dst)
{
    Code *const c = g->c;
    const QType elem = qcg_ty(g, e->a->type)->elem;
    const uint32_t et = qcg_et(g, elem);

    if (qcg_ty(g, e->a->type)->kind == QT_MAP) { /* remove(k) */
        qcg_mapargs(g, e->a, e->argv[0]);
        qcg_x(c, QX_MAPDEL);
        return;
    }

    switch (e->builtin) {
    case QB_SWAP: {
        const uint32_t how = qcg_elems(g, e->a);
        code_int(c, et);
        qcg_expr(g, e->argv[0]);
        qcg_expr(g, e->argv[1]);
        code_int(c, QX_SWAP | how);
        code_call(c, K_QX);
        return;
    }

    case QB_SORT: { /* by a lambda: its code after the CALL, a QJ over it, run from QX.SORT */
        const uint32_t how = qcg_elems(g, e->a);
        code_int(c, et);

        if (!e->nargv) {
            code_int(c, qcg_kind(elem));
            code_int(c, QX_SORT | how);
            code_call(c, K_QX);
            return;
        }

        code_int(c, QX_SORT | QX_BY | how);
        code_call(c, K_QX);
        qcg_flush(g);
        uint8_t *const over = qcg_qj(c, QJ_ALWAYS);
        qcg_flush(g);
        qcg_store(c, e->temps[1]);
        qcg_store(c, e->temps[0]);
        qcg_expr(g, e->argv[0]);
        qcg_land(g, (QCgHop){.qj = over});

        return;
    }

    case QB_PUSH:
    case QB_INSERT: { /* one more at the end, the value there; insert's moved to its place */
        const QExpr *v = e->argv[e->nargv - 1];
        QExpr held;

        if (qcg_byplace(g, elem) && qast_fresh(v)) { /* built first: the element's address stays */
            held = qcg_settle(g, v);
            v = &held;
        }

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, QX_PUSH);

        /* the push may move a held list's record, and a list goes in as a place: found again */
        if (qcg_held(g, e->a) || qcg_list(g, elem)) {
            code_op(c, O_DROP);
            QCgLast l;
            qcg_lastof(&l, e->a, elem);
            qcg_set(g, &l.at, v);
        } else if (qcg_rec(g, elem)) {
            code_opid(c, O_RA_ADDR, qcg_listvar(g, e->a)->id, false);
            code_byte(c, 1);
            qcg_offset(g, qcg_addr(g, v));
            qcg_recop(g, elem);
        } else {
            qcg_as(g, v, elem);
            qcg_storeat(g, qcg_listvar(g, e->a), elem);
        }

        qcg_moved(g, e->argv[e->nargv - 1]);

        if (e->builtin == QB_PUSH)
            return;

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_expr(g, e->argv[0]);
        qcg_x(c, QX_SINK);
        return;
    }

    case QB_POP:
        if (dst && qcg_list(g, elem)) { /* the last's list moved out */
            qcg_cell(g, e->a);
            qcg_x(c, QX_LAST);
            qcg_cell(g, dst);
            code_call(c, K_AMOVE);
        } else if (dst) {
            qcg_offset(g, qcg_addr(g, dst));
            qcg_cell(g, e->a);
            qcg_x(c, QX_LAST);
            qcg_recop(g, elem);
        } else
            qcg_droplast(g, e->a, elem);

        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, dst || qcg_typed(g, elem) ? QX_DROP : QX_POP);
        return;

    default: /* remove: moved last, then dropped */
        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_expr(g, e->argv[0]);
        qcg_x(c, QX_RAISE);
        qcg_droplast(g, e->a, elem);
        qcg_cell(g, e->a);
        code_int(c, et);
        qcg_x(c, QX_DROP);
    }
}

/* x in a, a list: QX.HAS. */
void qcg_has(QCg *const g, const QExpr *const e)
{
    if (qcg_ty(g, e->b->type)->kind == QT_MAP) {
        qcg_mapargs(g, e->b, e->a);
        code_int(g->c, QM_FIND);
        qcg_x(g->c, QX_MAPAT);
        uint8_t *const right = g->c->out;
        code_int(g->c, 0);
        qcg_compare(g->c, QY_INT, e->neg ? QK_EQ : QK_NE, right);
        return;
    }

    const QType elem = qcg_ty(g, e->b->type)->elem;
    qcg_cell(g, e->b);
    code_int(g->c, qcg_et(g, elem) | (qast_wide(elem) ? QRT_RAW : 0));
    qcg_as(g, e->a, elem);
    qcg_x(g->c, QX_HAS);

    if (e->neg)
        code_op(g->c, O_I_NOT);
}

/* Fresh record or list e built straight in place dst. */
static void qcg_build(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    switch (e->kind) {
    case QE_NEW:
        qcg_new(g, dst, e);
        return;

    case QE_LIST:
        qcg_listop(g, e, dst);
        return;

    case QE_COND: {
        qcg_expr(g, e->c);
        const QCgHop other = qcg_hop(g, QJ_FALSE);
        qcg_into(g, dst, e->a);
        const QCgHop over = qcg_hop(g, QJ_ALWAYS);
        qcg_land(g, other);
        qcg_into(g, dst, e->b);
        qcg_land(g, over);
        return;
    }

    case QE_TRY: {
        const QExpr t = qcg_tried(g, e);
        const QExpr v = qcg_fieldof(g, &t, QF_VAL);
        qcg_into(g, dst, &v);
        return;
    }

    case QE_APPLY:
        qcg_applied(g, e);
        qcg_into(g, dst, e->a);
        return;

    case QE_ITER: { /* built in its acc, or its T? after */
        qcg_iter(g, e);
        const QExpr acc = qcg_named(e->iter->acc);

        if (e->iter->result)
            qcg_into(g, dst, e->iter->result);
        else if (dst->kind != QE_NAME || dst->var != e->iter->acc)
            qcg_into(g, dst, &acc);

        return;
    }

    default: /* a call's */
        if (e->builtin == QB_GET) {
            qcg_get(g, dst, e);
            return;
        }

        if (e->builtin == QB_SLICE) { /* the ends first: they may move the heap */
            const bool waiting = code_calling(g->c, true);
            qcg_ends(g, e);
            qcg_cell(g, e->a);
            qcg_cell(g, dst);
            qcg_x(g->c, QX_LSLICE);
            code_called(g->c, waiting, true);
            return;
        }

        if (e->builtin == QB_COPY) {
            if (qcg_list(g, e->type))
                qcg_listto(g, dst, e->a, false);
            else
                qcg_copyrec(g, dst, e->a);

            return;
        }

        qcg_call(g, e, dst);
    }
}

/* Record or list value e to place dst: built there, or in its temporary first; a place's
   copied, a list variable's moved (sema saw it is not used again). */
void qcg_into(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    qcg_place(g, dst, e);
    qcg_moved(g, e);
}

/* qcg_into's value e to place dst. */
static void qcg_place(QCg *const g, const QExpr *const dst, const QExpr *const e)
{
    if (qast_fresh(e) && e->var) {
        const QExpr t = qcg_settle(g, e);

        if (qcg_list(g, e->type))
            qcg_listto(g, dst, &t, true);
        else
            qcg_copyrec(g, dst, &t);

        return;
    }

    if (qast_fresh(e)) {
        qcg_build(g, dst, e);
        return;
    }

    if (qcg_list(g, e->type))
        qcg_listto(g, dst, e, e->kind == QE_NAME && !e->copy && qcg_ty(g, e->type)->owns);
    else
        qcg_copyrec(g, dst, e);
}

/* Variable v, declared with no value: 0, "", an empty list, an array's elements 0, a record's
   fields their defaults. */
void qcg_fresh(QCg *const g, QVar *const v)
{
    Code *const c = g->c;
    const QExpr p = qcg_named(v);

    if (qcg_rec(g, v->type)) {
        qcg_clear(g, &p);
        qcg_defaults(g, &p);
        return;
    }

    if (!qcg_list(g, v->type)) {
        qcg_zero(g, v->type);
        qcg_store(c, v);
        return;
    }

    const QTy *const y = qcg_ty(g, v->type);

    if (y->kind == QT_ARRAY) { /* REDIM v(0 TO count - 1): DIM's ( lo hi mlo mhi id flags ) */
        code_int(c, 0);
        code_int(c, y->count - 1);
        code_int(c, 0);
        code_int(c, 0);
        code_int(c, v->id);
        code_int(c, DIM_REDIM | 1);
        code_call(c, K_DIM);
        return;
    }

    qcg_cell(g, &p); /* its old elements freed, then empty */
    code_call(c, K_ERASE);
    code_int(c, v->id);
    code_int(c, 0);
    code_call(c, K_AEMPTY);
}
