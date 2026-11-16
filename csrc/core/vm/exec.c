/* The executor: the dispatch loop, the STOP check, array, record and parameter access, the
 * string ops, CALL.
 *
 * # How it works
 * exec_run reads the op at vm.ip, keeps its place in vm.opip and calls its handler through a table
 * built from ops.def (exec.h). exec_until runs ops to an end address. exec_brk is the STOP check:
 * it counts ticks, runs an event, or raises BREAK.
 *
 * An array element is found by exec_elem1 (one subscript) or exec_elem (n subscripts), straight
 * from its header when it counts from 0 and the subscript is in range. A record is made, zeroed,
 * at its first use (exec_record). A parameter's cell is its argument's.
 *
 * K handlers (ops.def) are called by CALL through a second table.
 *
 * # Example
 *   A(3) = 7      pushes the value, then I_ST_A1 stores it at A's element 3
 *
 * # asm structure (src/vm/exec.asm)
 * | asm | here | |
 * |---|---|---|
 * | ex_lp | exec_run | MIRROR |
 * | ex_brk | exec_brk | MIRROR |
 * | arr_r1, arr_pn | exec_elem1, exec_elem | MIRROR |
 * | rec_get | exec_record | MIRROR |
 * | int_div | exec_idiv | MIRROR |
 * | g_ops, g_calls | ops, calls (tables from ops.def) | MIRROR |
 * | g_bflt | operands.c's tables | MOVED |
 * | g_fntab | funcs.c's fn1 table | MOVED |
 * | op_call | exec_call | MIRROR |
 * | op_sldt | exec_s_ld_txt | MIRROR |
 * | op_raddr, op_raddra, op_pref, op_vaddr, op_vld, op_vst | exec_r_addr, exec_ra_addr, exec_p_ref, exec_v_addr, exec_v_ld, exec_v_st | MIRROR |
 * | op_flld, op_flst, op_ichk16, op_sfld, op_sfst, op_rcopy | exec_fl_ld, exec_fl_st, exec_i_chk16, exec_sf_ld, exec_sf_st, exec_r_copy | MIRROR |
 * | op_ilda1, op_flda1, op_slda1, op_lda, op_slda | exec_i_ld_a1, exec_f_ld_a1, exec_s_ld_a1, exec_i_ld_a, exec_s_ld_a | MIRROR |
 * | op_sstv, op_ista1, op_fsta1, op_ssta1, op_istan, op_fstan, op_sstan | exec_s_st_v, exec_i_st_a1, exec_f_st_a1, exec_s_st_a1, exec_i_st_a, exec_f_st_a, exec_s_st_a | MIRROR |
 * | ex_cmpt | static | MIRROR |
 * | op_scmp, op_sif, op_scat | exec_s_cmp, exec_s_if, exec_s_cat | MIRROR |
 * | g_stkb, STKTOP | carved by exec_init; vm.stktop | D1 |
 *
 * - D1. exec_init carves the data stack from the arena; the asm's is a fixed buffer.
 */
#include "vm/exec.h"
#include "session.h"
#include "comp/link.h"
#include "io/file.h"
#include "io/repl.h"
#include "mem/alloc.h"
#include "mem/array.h"
#include "mem/copy.h"
#include "mem/string.h"
#include "mem/symbols.h"
#include "mem/program.h"
#include "num/double.h"
#include "plat/pclk.h"
#include "plat/pkey.h"
#include "vm/debug.h"
#include "vm/event.h"
#include "vm/flow.h"
#include "vm/hot.h"
#include "vm/stmts.h"

#define OP(name, h, size, fl) void h(Session *S);
#include "vm/ops.def"
#define K(name, h) void h(Session *S);
#include "vm/ops.def"

typedef void Handler(Session *S);

static Handler *const ops[O_NOPS] = {
#define OP(name, h, size, fl) h,
#include "vm/ops.def"
};

static const uint8_t flags[O_NOPS] = {
#define OP(name, h, size, fl) fl,
#include "vm/ops.def"
};

static Handler *const calls[K_N] = {
#define K(name, h) h,
#include "vm/ops.def"
};

uint8_t exec_flags(const uint8_t op) { return flags[op]; }

void exec_until(Session *const S, const uint8_t *const end)
{
    Vm *const v = &S->vm;

    while (v->ip != end) {
        v->opip = v->ip;
        exec_counted(v);
        ops[*v->ip++](S);
    }
}

void exec_init(Session *const S)
{
    const Addr stack = allo_carve(&S->allo, STKSZ);
    S->vm.stktop = (Cell *)at(stack + STKSZ - 16);
    S->vm.sp = S->vm.rsp = S->vm.stktop;
    S->vm.stklim = S->vm.stktop - STKROOM / sizeof(Cell);
    S->strg.stack = stack;
    S->strg.stack_end = stack + STKSZ;
    S->strg.sp = &S->vm.sp;
    pkey_init(&S->vm.stop, SF_TICK);
    pclk_init(&S->vm.stop, SF_TICK);
}

/* Element sub of 1-D array id, size bytes each, straight from its header to *e; false: it does
   not count from 0, or sub is out of its range, for arry_ref. */
static inline bool elem1_fast(
    const Symb *const sy,
    const Addr fp,
    const uint16_t id,
    const int32_t sub,
    const uint32_t size,
    Addr *const e
)
{
    const Addr hdr = ((const ArrSlot *)at(flow_cell(sy, fp, id)))->hdr;

    if (!hdr)
        return false;

    const ArrayHdr *const a = (const ArrayHdr *)at(hdr);

    if (a->dims != 1 || (uint32_t)sub >= a->fast)
        return false;

    *e = a->data + (uint32_t)sub * size;
    return true;
}

/* The ops exec_run runs inline (vm/hot.h): name, handler. */
#define HOT_OPS(X)                                                                                 \
    X(i_ld_v, oper_i_ld_v)                                                                         \
    X(i_st_v, oper_i_st_v)                                                                         \
    X(f_st_v, oper_f_st_v)                                                                         \
    X(i_ld_u8, oper_i_ld_u8)                                                                       \
    X(i_ld_i16, oper_i_ld_i16)                                                                     \
    X(i_ld_i32, oper_i_ld_i32)                                                                     \
    X(f_ld_f32, oper_f_ld_f32)                                                                     \
    X(i_cmp, oper_i_cmp)                                                                           \
    X(i_cmp_v, oper_i_cmp_v)                                                                       \
    X(i_cmp_u8, oper_i_cmp_u8)                                                                     \
    X(f_ld_u8, oper_f_ld_u8)                                                                       \
    X(f_ld_vi, oper_f_ld_vi)                                                                       \
    X(f_ld_pi, oper_f_ld_pi)                                                                       \
    X(f_neg, oper_f_neg)                                                                           \
    X(i_not, oper_i_not)                                                                           \
    X(drop, oper_drop)                                                                             \
    X(i_tof, oper_i_tof)                                                                           \
    X(f_cmp, oper_f_cmp)                                                                           \
    X(f_cmp_v, oper_f_cmp_v)                                                                       \
    X(f_cmp_vi, oper_f_cmp_vi)                                                                     \
    X(f_cmp_u8, oper_f_cmp_u8)                                                                     \
    X(f_cmp_f32, oper_f_cmp_f32)                                                                   \
    X(i_cmp_i16, oper_i_cmp_i16)                                                                   \
    X(i_set_u8, oper_i_set_u8)                                                                     \
    X(i_set_i16, oper_i_set_i16)                                                                   \
    X(i_set_i32, oper_i_set_i32)                                                                   \
    X(i_mov, oper_i_mov)                                                                           \
    X(d_ld_v, dubl_ldv)                                                                            \
    X(d_st_v, dubl_stv)                                                                            \
    X(d_ld_k, dubl_ldk)                                                                            \
    X(d_neg, dubl_neg)                                                                             \
    X(d_cmp, dubl_cmp)                                                                             \
    X(d_nz, dubl_nz)                                                                               \
    X(i_tod, dubl_itod)                                                                            \
    X(f_tod, dubl_ftod)                                                                            \
    X(d_tof, dubl_tof)                                                                             \
    X(i_ld_l, oper_i_ld_l)                                                                         \
    X(i_st_l, oper_i_st_l)                                                                         \
    X(f_st_l, oper_f_st_l)                                                                         \
    X(f_ld_li, oper_f_ld_li)                                                                       \
    X(i_cmp_l, oper_i_cmp_l)                                                                       \
    X(f_cmp_l, oper_f_cmp_l)                                                                       \
    X(f_cmp_li, oper_f_cmp_li)                                                                     \
    X(d_ld_l, oper_d_ld_l)                                                                         \
    X(d_st_l, oper_d_st_l)

/* f_ld_f32's and f_st_v's run as i_ld_i32's and i_st_v's. */
static inline Status hot_f_st_v(Regs *const r) { return hot_i_st_v(r); }
static inline Status hot_f_st_l(Regs *const r) { return hot_i_st_l(r); }
static inline Status hot_f_ld_f32(Regs *const r) { return hot_i_ld_i32(r); }

/* exec_run's parts for each hot op: its label for its handler's opcodes; its code, which a
   failing INT op leaves through fail at its own start. */
#define HOT_AT(name, handler)                                                                      \
    if (h == handler)                                                                              \
        at_op[i] = &&hot_##name;
#define HOT_AT_INT(name, src, op) HOT_AT(name, oper_##name)
#define HOT_AT_FAIL(name, handler) HOT_AT(name, handler)
#define HOT_AT_FLT(name, src, op) HOT_AT(name, oper_##name)
#define HOT_RUN(name, handler)                                                                     \
    hot_##name : r.ip++;                                                                           \
    hot_##name(&r);                                                                                \
    goto next;
#define HOT_RUN_FAIL(name, handler)                                                                \
    hot_##name : st = hot_##name(&r);                                                              \
    if (st != ST_OK) {                                                                             \
        op = r.ip;                                                                                 \
        goto fail;                                                                                 \
    }                                                                                              \
    r.ip++;                                                                                        \
    goto next;
#define HOT_RUN_FLT(name, src, oper)                                                               \
    hot_##name : r.ip++;                                                                           \
    st = hot_##name(&r);                                                                           \
    if (st != ST_OK) {                                                                             \
        op = r.ip - hot_fsize(src);                                                                \
        goto fail;                                                                                 \
    }                                                                                              \
    goto next;
/* A CMPIF, an IF: true, on; false, the line ends (flow_nextln) unless it is the program's last or
   the direct line: then the op's handler, which nothing here has changed. */
#define CMPIF_RUN(name, handler, src, rel)                                                         \
    cmpif_##name:                                                                                  \
    {                                                                                              \
        Regs t = {r.ip + 1, r.sp, r.fp};                                                           \
        const uint8_t got = rel(&t, src);                                                          \
                                                                                                   \
        if (got & hot_u8(&t)) {                                                                    \
            r = t;                                                                                 \
            goto next;                                                                             \
        }                                                                                          \
                                                                                                   \
        uint8_t *const code = flow_next_code(v, &S->strg, &S->prog, &S->allo);                     \
                                                                                                   \
        if (!code)                                                                                 \
            goto slow;                                                                             \
                                                                                                   \
        r = (Regs){code, t.sp, t.fp};                                                              \
    }                                                                                              \
    goto next;
#define CMPIF_AT(name, handler, src, rel)                                                          \
    if (h == handler)                                                                              \
        at_op[i] = &&cmpif_##name;

/* An INT compare-and-branch: its compare true, past it; else JF's jump, a stop and a jump not
   resolved its handler's. */
#define JFC_AT(name, src)                                                                          \
    if (h == flow_##name)                                                                          \
        at_op[i] = &&name;
#define JFC_RUN(name, src)                                                                         \
    name: {                                                                                        \
        Regs t = {r.ip + 1, r.sp, r.fp};                                                           \
                                                                                                   \
        if (hot_jfc(&t, src)) {                                                                    \
            r = (Regs){t.ip + sizeof(Jump), t.sp, t.fp};                                           \
            goto next;                                                                             \
        }                                                                                          \
                                                                                                   \
        if (v->stop)                                                                               \
            goto slow;                                                                             \
                                                                                                   \
        uint8_t *const code = flow_jump_code(v, &S->strg, &S->allo, t.ip);                         \
                                                                                                   \
        if (!code)                                                                                 \
            goto slow;                                                                             \
                                                                                                   \
        r = (Regs){code, t.sp, t.fp};                                                              \
    }                                                                                              \
    goto next;

#define HOT_AT_INT_ST(NAME, name, src, op) HOT_AT(name##_st, oper_##name##_st)
#define HOT_RUN_INT_ST(NAME, name, src, oper)                                                      \
    hot_##name##_st : r.ip++;                                                                      \
    st = hot_##name##_st(&r);                                                                      \
    if (st != ST_OK) {                                                                             \
        op = r.ip - hot_size(src) - 2 * VCELL;                                                     \
        goto fail;                                                                                 \
    }                                                                                              \
    goto next;
#define HOT_RUN_INT(name, src, oper)                                                               \
    hot_##name : r.ip++;                                                                           \
    st = hot_##name(&r);                                                                           \
    if (st != ST_OK) {                                                                             \
        op = r.ip - hot_size(src);                                                                 \
        goto fail;                                                                                 \
    }                                                                                              \
    goto next;

/* The loop: ip and sp in registers (r), a hot op inline, the rest through the ops table with
   vm.ip, vm.sp and vm.opip stored around it. */
[[noreturn, gnu::noinline]]
static void exec_loop(Session *const S)
{
    Vm *const v = &S->vm;
    static void *at_op[256]; /* each op's label: its hot code, else cold */

    if (!at_op[0]) {
        for (uint32_t i = 0; i < countof(at_op); i++) {
            Handler *const h = i < O_NOPS ? ops[i] : nullptr;
            at_op[i] = &&cold;
            HOT_OPS(HOT_AT)
            HOT_INTS(HOT_AT_INT)
            HOT_FLTS(HOT_AT_FLT)
            HOT_INT_STS(HOT_AT_INT_ST)
            HOT_FAILS(HOT_AT_FAIL)
            HOT_CMPIFS(CMPIF_AT)

            if (h == flow_i_jt)
                at_op[i] = &&i_jt;

            if (h == flow_f_jf)
                at_op[i] = &&f_jf;
            if (h == flow_f_jt)
                at_op[i] = &&f_jt;
            if (h == flow_jmp)
                at_op[i] = &&jmp;
            if (h == oper_i_if)
                at_op[i] = &&i_if;
            if (h == oper_f_if)
                at_op[i] = &&f_if;
            if (h == flow_nextln)
                at_op[i] = &&nxln;
            if (h == flow_gosub)
                at_op[i] = &&gosub;
            if (h == flow_return)
                at_op[i] = &&ret;
            if (h == flow_pcall)
                at_op[i] = &&pcall;
            if (h == flow_pret)
                at_op[i] = &&pret;
            if (h == flow_pret_fv)
                at_op[i] = &&pret_fv;
            if (h == flow_pret_fl)
                at_op[i] = &&pret_fl;

            if (h == flow_i_jf)
                at_op[i] = &&i_jf;

            if (h == flow_i_next_up)
                at_op[i] = &&i_next_up;
            if (h == flow_i_next_dn)
                at_op[i] = &&i_next_dn;

            HOT_JFCS(JFC_AT)

            if (h == flow_eol)
                at_op[i] = i == O_EOL_TO ? &&eol_to : &&eol;

            if (h == dbug_dbg)
                at_op[i] = &&dbg;
            if (h == exec_i_ld_a1 || h == exec_f_ld_a1)
                at_op[i] = &&i_ld_a1;
            if (h == exec_i_st_a1 || h == exec_f_st_a1)
                at_op[i] = &&i_st_a1;
        }
    }

    Regs r = {v->ip, v->sp, v->fp};

    uint8_t *op; /* a failed op's */
    Status st;

next: /* r.ip at the op: each label steps past it */
    exec_counted(v);
    goto *at_op[*r.ip];

/* An op's handler, from the ops table: vm.ip, vm.sp and vm.opip stored around it. cold is the
   dispatch's, slow a hot op's way out: one label for both had every dispatch copy ip and sp into
   the registers the fallbacks left them in, three instructions an op. */
#define COLD_RUN()                                                                                 \
    v->ip = r.ip + 1;                                                                              \
    v->sp = r.sp;                                                                                  \
    v->opip = r.ip;                                                                                \
    ops[*r.ip](S);                                                                                 \
    r = (Regs){v->ip, v->sp, v->fp};                                                               \
    goto next;

cold:
    COLD_RUN()
slow:
    COLD_RUN()

    HOT_OPS(HOT_RUN)
    HOT_INTS(HOT_RUN_INT)
    HOT_FLTS(HOT_RUN_FLT)
    HOT_INT_STS(HOT_RUN_INT_ST)
    HOT_FAILS(HOT_RUN_FAIL)
    HOT_CMPIFS(CMPIF_RUN)

/* A jump: past it, or to its target; a stop, a jump not resolved: its handler's. */
#define JUMP_ON(taken, pop)                                                                        \
    if (!(taken)) {                                                                                \
        r.sp++;                                                                                    \
        r.ip += JSIZE;                                                                             \
        goto next;                                                                                 \
    }                                                                                              \
                                                                                                   \
    if (v->stop)                                                                                   \
        goto slow;                                                                                 \
                                                                                                   \
    {                                                                                              \
        uint8_t *const code = flow_jump_code(v, &S->strg, &S->allo, r.ip + 1);                     \
                                                                                                   \
        if (!code)                                                                                 \
            goto slow;                                                                             \
                                                                                                   \
        r.sp += (pop);                                                                             \
        r.ip = code;                                                                               \
    }                                                                                              \
    goto next;

i_jt:
    JUMP_ON(r.sp->i, 1)
f_jf:
    JUMP_ON(!(r.sp->u & ~SNGL_SIGN), 1)
f_jt:
    JUMP_ON(r.sp->u & ~SNGL_SIGN, 1)

jmp:
    JUMP_ON(true, 0)

/* IF: true, on; false, the line ends as a CMPIF's. */
#define IF_RUN(yes)                                                                                \
    if (yes) {                                                                                     \
        r.sp++;                                                                                    \
        r.ip++;                                                                                    \
        goto next;                                                                                 \
    }                                                                                              \
                                                                                                   \
    {                                                                                              \
        uint8_t *const code = flow_next_code(v, &S->strg, &S->prog, &S->allo);                     \
                                                                                                   \
        if (!code)                                                                                 \
            goto slow;                                                                             \
                                                                                                   \
        r.sp++;                                                                                    \
        r.ip = code;                                                                               \
    }                                                                                              \
    goto next;

i_if:
    IF_RUN(r.sp->i)
f_if:
    IF_RUN(r.sp->u & ~SNGL_SIGN)

nxln: /* to the next stored line; the program's end, the direct line: flow_nextln's */
{
    uint8_t *const code = flow_next_code(v, &S->strg, &S->prog, &S->allo);

    if (!code)
        goto slow;

    r.ip = code;
}

    goto next;

gosub: /* a frame to come back to, then the line; a stop, not linked, frames full, the direct line:
          flow_gosub's */
    if (v->stop || !S->link.linked || v->nframes == MAX_FRAMES)
        goto slow;

    {
        const uint32_t line = ((const LineRef *)(r.ip + 1))->line;
        const Line *const ln = prog_line(&S->prog, line);

        if (!ln->len)
            goto slow;

        const Place back = {v->line, addr_of(r.ip + 1 + sizeof(LineRef)) - S->allo.cache};

        Frame *const f = &v->frames[v->nframes++];
        f->kind = FRAME_GOSUB;
        f->ret = back;

        S->strg.tmp_used = S->strg.tmp_floor;
        S->link.cursor = line;
        v->line = line;
        r.ip = at(S->allo.cache + ln->code);
    }

    goto next;

ret: /* RETURN to a GOSUB's frame; a stop, an event's frame, none: flow_return's */
    if (v->stop || !v->nframes || v->frames[v->nframes - 1].kind != FRAME_GOSUB)
        goto slow;

    {
        const Place back = v->frames[--v->nframes].ret;
        S->strg.tmp_used = S->strg.tmp_floor;
        v->line = back.line;
        r.ip = at(S->allo.cache + back.code);
    }

    goto next;

pcall: /* PCALL proc n npar body: the frame at the arguments, n local cells zeroed below it, pushed,
          into the body; a stop, a limit: flow_pcall's */
    if (v->stop)
        goto slow;

    {
        const uint32_t n = load16(r.ip + 2);
        const Jump *const j = (const Jump *)(r.ip + 5);

        if (r.sp < v->stklim || (uint32_t)(r.sp - v->stklim) < n || v->nframes == MAX_FRAMES ||
            j->dest <= TO_NEXT_STMT)
            goto slow;

        const Addr fp = addr_of(r.sp);
        Frame *const f = &v->frames[v->nframes++]; /* field by field: no padding zeroed */
        f->kind = FRAME_CALL;
        f->ret = (Place){v->line, addr_of(r.ip + 5 + sizeof(Jump)) - S->allo.cache};

        f->call.proc = r.ip[1];
        f->call.temp_floor = S->strg.tmp_floor;
        f->call.fp = fp;
        f->call.npar = r.ip[4];
        f->call.caller = r.fp;
        f->call.on_error = 0;
        r.fp = v->fp = fp;
        r.sp -= n;

        for (uint32_t i = 0; i < n; i++)
            r.sp[i] = (Cell){};

        f->call.stack = r.sp;
        S->strg.tmp_floor = S->strg.tmp_used;
        v->line = j->line;
        r.ip = at(S->allo.cache + j->dest);
    }

    goto next;

/* PRET: the call on the top frame ended, its arguments popped; a stop, another frame on top:
   flow_pret's. */
#define PRET_RUN(result)                                                                           \
    if (v->stop || !v->nframes || v->frames[v->nframes - 1].kind != FRAME_CALL)                    \
        goto slow;                                                                                 \
                                                                                                   \
    {                                                                                              \
        const Frame *const f = &v->frames[v->nframes - 1];                                         \
        const Call c = f->call;                                                                    \
        const Place back = f->ret;                                                                 \
        [[maybe_unused]]                                                                           \
        const Cell x = (result);                                                                   \
        v->nframes--;                                                                              \
        r.sp = (Cell *)at(c.fp) + 2 * c.npar;                                                      \
        r.fp = v->fp = c.caller;                                                                   \
        S->strg.tmp_used = S->strg.tmp_floor; /* the callee's */                                   \
        S->strg.tmp_floor = c.temp_floor;                                                          \
        v->line = back.line;                                                                       \
        r.ip = at(S->allo.cache + back.code);                                                      \
        PRET_PUSH                                                                                  \
    }                                                                                              \
    goto next;

#define PRET_PUSH

pret: /* a SUB's */
    PRET_RUN((Cell){})
#undef PRET_PUSH
#define PRET_PUSH *--r.sp = x;
pret_fv: /* a FUNCTION's number: its result's cell */
    PRET_RUN(load_cell(at(load32(r.ip + 2))))
pret_fl: /*   in the frame */
    PRET_RUN(load_cell(at(r.fp + load32(r.ip + 2))))
#undef PRET_PUSH

i_jf: /* JF: past its jump, or to its target; a stop, a jump not resolved: flow_i_jf's */
    if (r.sp->i) {
        r.sp++;
        r.ip += JSIZE;
        goto next;
    }

    if (v->stop)
        goto slow;

    {
        uint8_t *const code = flow_jump_code(v, &S->strg, &S->allo, r.ip + 1);

        if (!code)
            goto slow;

        r.sp++;
        r.ip = code;
    }

    goto next;

/* NEXT: stepped, back to the body until past; a stop, a jump not resolved: its handler's. */
#define NEXT_RUN(up)                                                                               \
    {                                                                                              \
        const Jump *const j = (const Jump *)(r.ip + NEXT_SIZE - sizeof(Jump));                     \
                                                                                                   \
        if (v->stop || j->dest <= TO_NEXT_STMT)                                                    \
            goto slow;                                                                             \
                                                                                                   \
        bool back;                                                                                 \
                                                                                                   \
        if ((st = hot_next(r.ip, up, &back)) != ST_OK) {                                           \
            op = r.ip;                                                                             \
            goto fail;                                                                             \
        }                                                                                          \
                                                                                                   \
        if (!back) {                                                                               \
            r.ip += NEXT_SIZE;                                                                     \
            goto next;                                                                             \
        }                                                                                          \
                                                                                                   \
        S->strg.tmp_used = S->strg.tmp_floor;                                                      \
        v->line = j->line;                                                                         \
        r.ip = at(S->allo.cache + j->dest);                                                        \
    }                                                                                              \
    goto next;

i_next_up:
    NEXT_RUN(true)
i_next_dn:
    NEXT_RUN(false)

    HOT_JFCS(JFC_RUN)

dbg: /* a statement's start, where nothing could stop; else dbug_dbg's */
    if (v->line != DLINE && dbug_may_stop(&S->dbug, prog_line(&S->prog, v->line)->flags))
        goto slow;

    r.ip++;
    goto next;

eol: /* to the next stored line; a stop, the end, the direct line: flow_eol's */
    if (v->stop)
        goto slow;

    {
        uint8_t *const code = flow_next_code(v, &S->strg, &S->prog, &S->allo);

        if (!code)
            goto slow;

        r.ip = code;
    }

    goto next;

eol_to: /* to the next line link_resolve set; a stop, the last line: flow_eol's */
    if (v->stop || !load32(r.ip + 1))
        goto slow;

    S->strg.tmp_used = S->strg.tmp_floor;
    v->line = load32(r.ip + 1);
    r.ip = at(S->allo.cache + load32(r.ip + 5));
    goto next;

i_ld_a1: /* a 1-D array's element counting from 0, in range; else exec_i_ld_a1's */
{
    Addr e;

    if (!elem1_fast(&S->symb, r.fp, load16(r.ip + 1), r.sp->i, VS, &e))
        goto slow;

    r.ip += 3;
    r.sp->a = rd32(e);
}

    goto next;

i_st_a1: /* ( sub x -- ) */
{
    Addr e;

    if (!elem1_fast(&S->symb, r.fp, load16(r.ip + 1), r.sp[1].i, VS, &e))
        goto slow;

    r.ip += 3;
    wr32(e, r.sp->u);
    r.sp += 2;
}

    goto next;

fail:
    v->ip = r.ip;
    v->sp = r.sp;
    v->opip = op;
    fault_status(&S->fault, st);
}

/* The loop is its own function: setjmp here, where it wants the frame pointer, leaves the loop its
   seventh register. */
void exec_run(Session *const S)
{
    setjmp(S->vm.loop);
    S->vm.running = true;
    exec_loop(S);
}

bool exec_brk(Session *const S)
{
    if (S->vm.stop & SF_TICK)
        evnt_poll(&S->evnt, &S->vm);
    if (!S->vm.stop)
        return false;
    if (!(S->vm.stop & SF_BRK))
        return evnt_run(S);

    pkey_flush(); /* the Ctrl+C, not a key for the editor */
    S->vm.ip = S->vm.opip;

    if (S->dbug.emit && S->vm.line != DLINE) { /* the IDE: on to the next statement's start */
        S->vm.stop = 0;
        S->dbug.mode = DM_STEP;
        return true;
    }

    stmt_stop(S);
}

int32_t exec_idiv(Fault *const f, const int32_t a, const int32_t b, int32_t *const rem)
{
    int32_t q;
    fault_check(f, hot_idiv(a, b, &q, rem));
    return q;
}

/* ---- arrays and records --------------------------------------------------------------------- */

/* A subscript past a word is Subscript out of range. */
static uint16_t subscript(Arry *const ar, const int32_t sub)
{
    if (sub != (int16_t)sub)
        arry_bad_sub(ar);

    return (uint16_t)sub;
}

Addr exec_elem1(Arry *const ar, const uint16_t id, const int32_t sub)
{
    ar->subs[0] = subscript(ar, sub);
    return arry_ref(ar, id, 1);
}

Addr exec_elem(Arry *const ar, Vm *const vm)
{
    const uint16_t id = exec_u16(vm);
    const uint8_t n = exec_u8(vm);

    for (uint8_t i = n; i-- > 0;)
        ar->subs[i] = subscript(ar, exec_popi(vm));

    return arry_ref(ar, id, n);
}

/* Element sub of the 1-D array whose id is at vm->ip, size bytes each: straight from its header
   when it counts from 0 and sub is in range, else through arry_ref. */
static Addr elem1(Session *const S, const int32_t sub, const uint32_t size)
{
    const uint16_t id = exec_u16(&S->vm);
    Addr e;
    return elem1_fast(&S->symb, S->vm.fp, id, sub, size, &e) ? e : exec_elem1(&S->arry, id, sub);
}

Addr exec_record(Arry *const ar, const uint16_t id)
{
    const Addr hdr = arry_slot(ar, id)->hdr;

    if (hdr)
        return hdr;

    ar->counts[0] = 1;
    ar->lbounds[0] = 0;
    return arry_new(ar, id, 1, false);
}

void exec_i_ld_a1(Session *const S) { exec_pusha(&S->vm, rd32(elem1(S, exec_popi(&S->vm), VS))); }
void exec_f_ld_a1(Session *const S) { exec_i_ld_a1(S); }
void exec_s_ld_a1(Session *const S) { exec_pusha(&S->vm, elem1(S, exec_popi(&S->vm), DSZ)); }
void exec_i_ld_a(Session *const S) { exec_pusha(&S->vm, rd32(exec_elem(&S->arry, &S->vm))); }
void exec_s_ld_a(Session *const S) { exec_pusha(&S->vm, exec_elem(&S->arry, &S->vm)); }

void exec_i_st_a1(Session *const S)
{
    const uint32_t x = exec_pop(&S->vm).u;
    wr32(elem1(S, exec_popi(&S->vm), VS), x);
}
void exec_f_st_a1(Session *const S) { exec_i_st_a1(S); }

void exec_s_st_a1(Session *const S)
{
    const Addr s = exec_popa(&S->vm);
    strg_set(&S->strg, s, elem1(S, exec_popi(&S->vm), DSZ));
}

void exec_i_st_a(Session *const S)
{
    const uint32_t x = exec_pop(&S->vm).u;
    wr32(exec_elem(&S->arry, &S->vm), x);
}
void exec_f_st_a(Session *const S) { exec_i_st_a(S); }

void exec_s_st_a(Session *const S)
{
    const Addr s = exec_popa(&S->vm);
    strg_set(&S->strg, s, exec_elem(&S->arry, &S->vm));
}

/* A record's address, the record made at its first use. */
void exec_r_addr(Session *const S)
{
    exec_pusha(&S->vm, ((const ArrayHdr *)at(exec_record(&S->arry, exec_u16(&S->vm))))->data);
}

/* A record array's element. */
void exec_ra_addr(Session *const S) { exec_pusha(&S->vm, exec_elem(&S->arry, &S->vm)); }

/* Record b's n bytes over record a's. */
void exec_r_copy(Session *const S)
{
    const Addr b = exec_popa(&S->vm);
    const Addr a = exec_popa(&S->vm);
    copy_mem(a, b, exec_u16(&S->vm));
}

/* ---- record fields -------------------------------------------------------------------------- */

static Cell cell_at(const Addr a) { return load_cell(at(a)); }

/* A record's number field: a SHORT, a DOUBLE, else 4 bytes. */
void exec_fl_ld(Session *const S)
{
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    const uint8_t type = exec_u8(&S->vm);

    if (type == ET_SH)
        exec_pushi(&S->vm, (int16_t)rd16(a));
    else if (type == ET_R8)
        exec_push(&S->vm, cell_at(a));
    else
        exec_pusha(&S->vm, rd32(a));
}

/* Overflow: a SHORT's past 16 bits. */
void exec_fl_st(Session *const S)
{
    const uint16_t off = exec_u16(&S->vm);
    const uint8_t type = exec_u8(&S->vm);
    const Cell x = exec_pop(&S->vm);
    const Addr a = exec_popa(&S->vm) + off;

    if (type == ET_R8)
        store_cell(at(a), x);
    else if (type == ET_SH) {
        if (x.i != (int16_t)x.i)
            repl_error(S, E_OV);

        wr16(a, (uint16_t)x.u);
    } else
        wr32(a, x.u);
}

/* A SHORT's value. Overflow past 16 bits. */
void exec_i_chk16(Session *const S) { hot_op(S, hot_i_chk16); }

/* A STRING * n field, as a temporary. */
void exec_sf_ld(Session *const S)
{
    const uint16_t off = exec_u16(&S->vm);
    const uint16_t n = exec_u16(&S->vm);
    const Addr chars = strg_alloc(&S->strg, n); /* the record does not move */
    const Addr a = exec_popa(&S->vm) + off;
    memcpy(at(chars), at(a), n);
    exec_pusha(&S->vm, strg_new(&S->strg, chars, n));
}

/* Into a STRING * n field: cut, or padded with spaces. */
void exec_sf_st(Session *const S)
{
    const Desc s = strg_get(&S->strg, exec_popa(&S->vm));
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    const uint16_t n = exec_u16(&S->vm);
    const uint16_t k = s.len < n ? s.len : n;
    memcpy(at(a), at(s.ptr), k);
    memset(at(a + k), ' ', n - k);
}

/* ---- parameters ----------------------------------------------------------------------------- */

/* An argument passed by reference: no value, the cell for PCALL. */
void exec_p_ref(Session *const S)
{
    const Addr a = exec_popa(&S->vm) + exec_u16(&S->vm);
    exec_pusha(&S->vm, 0);
    exec_pusha(&S->vm, a);
}

/* A scalar parameter's argument's address: in its ref cell, at its offset from the frame. */
static Addr param_cell(Vm *const vm) { return rd32(vm->fp + exec_u16(vm)); }

void exec_v_addr(Session *const S) { exec_pusha(&S->vm, param_cell(&S->vm)); }

typedef enum : uint8_t { CELL_NUMBER, CELL_DOUBLE, CELL_STRING } CellKind;

/* A parameter's value through its argument's address. */
void exec_v_ld(Session *const S)
{
    const Addr cell = param_cell(&S->vm);
    const CellKind kind = exec_u8(&S->vm);

    if (kind == CELL_DOUBLE)
        exec_push(&S->vm, cell_at(cell));
    else if (kind == CELL_STRING)
        exec_pusha(&S->vm, cell);
    else
        exec_pusha(&S->vm, rd32(cell));
}

void exec_v_st(Session *const S)
{
    const Addr cell = param_cell(&S->vm);
    const CellKind kind = exec_u8(&S->vm);
    const Cell x = exec_pop(&S->vm);

    if (kind == CELL_DOUBLE)
        store_cell(at(cell), x);
    else if (kind == CELL_STRING)
        strg_set(&S->strg, x.a, cell);
    else
        wr32(cell, x.u);
}

/* BYVAL kind: an argument by value, its address pushed above it: the parameter is its own cell. A
   string (kind 1) is copied into the cell, the cell its owner. */
void exec_byval(Session *const S)
{
    Cell *const v = S->vm.sp;

    if (exec_u8(&S->vm)) {
        const Addr value = v->a;
        *v = (Cell){};

        strg_set(&S->strg, value, addr_of(v));
    }

    exec_pusha(&S->vm, addr_of(v));
}

/* ---- strings -------------------------------------------------------------------------------- */

/* Text in the program: a temporary pointing at it. */
void exec_s_ld_txt(Session *const S)
{
    const uint8_t len = exec_u8(&S->vm);
    const Addr text = exec_u32(&S->vm);
    exec_pusha(&S->vm, strg_push(&S->strg, (Desc){.len = len, .ptr = text}));
}

void exec_s_st_v(Session *const S)
{
    const Addr cell = exec_cell(&S->vm);
    strg_set(&S->strg, exec_popa(&S->vm), cell);
}

void exec_s_st_l(Session *const S)
{
    const Addr cell = S->vm.fp + exec_cell(&S->vm);
    strg_set(&S->strg, exec_popa(&S->vm), cell);
}

/* A compare pushes -1 or 0; a CMPIF's false ends the line. */
void exec_s_cmp(Session *const S)
{
    const uint8_t op = *S->vm.opip;
    const Addr right = exec_popa(&S->vm);
    const Addr left = exec_popa(&S->vm);
    const bool yes = strg_cmp(&S->strg, left, right) & exec_u8(&S->vm);

    if (op < O_I_CMPIF)
        exec_pushi(&S->vm, yes ? -1 : 0);
    else if (!yes)
        flow_nextln(S);
}

/* IF on a string: empty ends the line. */
void exec_s_if(Session *const S)
{
    if (!strg_get(&S->strg, exec_popa(&S->vm)).len)
        flow_nextln(S);
}

/* String too long. */
void exec_s_cat(Session *const S)
{
    const Addr right = exec_popa(&S->vm);
    const Addr left = exec_popa(&S->vm);
    exec_pusha(&S->vm, strg_cat(&S->strg, left, right));
}

/* CALL k: routine k. */
void exec_call(Session *const S) { calls[exec_u8(&S->vm)](S); }
