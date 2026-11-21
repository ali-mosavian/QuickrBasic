/* CALL K_NCALL: a native procedure called, its code an extension's entry (io/native.c).
 *
 * # How it works
 * The arguments are on the stack as a PCALL's (comp/codegen.c cg_arg), each a value cell under a
 * ref cell, the last on top; the procedure's number above them. Each argument's data is at its ref
 * (a by-value one's at its own value cell): a number's cell, a string's Desc, a record's bytes, an
 * array's ArrSlot. Each is laid out as its kind says (qx/qx.h; link checked the entry's against the
 * DECLARE), the entry called, the arguments popped and a FUNCTION's result pushed. The entry's
 * error is raised then, at the calling line: nothing unwinds through the extension's frames.
 *
 * # asm structure
 * | asm | here | |
 * |---|---|---|
 * | (none) | ncal_call | NEW |
 */
#include "session.h"
#include "mem/array.h"
#include "mem/loc.h"
#include "mem/string.h"
#include "plat/pmem.h"

/* The array whose ArrSlot is at a, as an entry sees it: none yet, no elements. */
static QxArray ncal_array(const Session *const S, const Addr a)
{
    const ArrSlot *const slot = (const ArrSlot *)at(a);

    if (!slot->hdr)
        return (QxArray){};

    const ArrView v = arry_view(slot->hdr);
    return (QxArray){
        .data = at(v.data),
        .count = arry_view_elems(&v),
        .elem = arry_et_size(&S->decl, v.et),
        .lbound = arry_view_lbound(&v, 0),
    };
}

/* A string result pushed: its bytes copied out first, as the copy's room may move the strings
   they could be. Longer than a string can be: Illegal function call. */
static void ncal_string(Session *const S, const QxStr r)
{
    if (r.len > SMAX)
        fault_error(&S->fault, E_IQ);

    char *const copy = r.len ? pmem_resize(nullptr, r.len) : nullptr;

    if (r.len && !copy)
        fault_error(&S->fault, E_OM);
    if (r.len)
        memcpy(copy, r.ptr, r.len);

    const Addr chars = strg_alloc(&S->strg, (uint16_t)r.len);

    if (r.len)
        memcpy(at(chars), copy, r.len);

    pmem_free(copy);
    exec_pusha(&S->vm, strg_new(&S->strg, chars, (uint16_t)r.len));
}

void ncal_call(Session *const S)
{
    Vm *const vm = &S->vm;

    if (vm->stop && exec_brk(S)) /* a Break, or an event's handler first: this op again after */
        return;

    const Proc *const p = blok_proc(&S->blok, (uint32_t)exec_popi(vm));
    const uint16_t npar = p->npar;
    QxArg args[npar + 1];
    QxArray arrays[npar + 1];
    const char *k = p->native->kinds;

    for (uint16_t i = 0; i < npar; i++, k++) {
        const Addr a = vm->sp[2 * (npar - 1u - i)].a & REF_ADDR;

        if (*k == '&') {
            k++;
            args[i].ref = at(a);
            continue;
        }

        const Cell c = load_cell(at(a));

        switch (*k) {
        case 'F':
            args[i].f = c.f;
            break;

        case 'D':
            args[i].d = c.d;
            break;

        case 'S': {
            const Desc *const d = (const Desc *)at(a);
            args[i].s = (QxStr){(const char *)at(d->ptr), d->len};

            break;
        }

        case 'T':
            args[i].ref = at(a);
            break;

        case 'R':
            arrays[i] = ncal_array(S, a);
            args[i].a = &arrays[i];
            break;

        default: /* I, W, U */
            args[i].i = c.i;
        }
    }

    NatvCall call = {.c = {.host = &S->natv.host, .arg = args}, .stop = &vm->stop};

    p->native->fn(&call.c);
    vm->sp += 2 * npar;

    if (call.c.error)
        fault_qberr(&S->fault, call.c.error);

    switch (p->nres) {
    case 0:
        break;

    case 'S':
        ncal_string(S, call.c.result.s);
        break;

    case 'F':
        exec_push(vm, (Cell){.f = call.c.result.f});

        break;

    case 'D':
        exec_pushd(vm, call.c.result.d);
        break;

    default:
        exec_pushi(vm, call.c.result.i);
    }
}
