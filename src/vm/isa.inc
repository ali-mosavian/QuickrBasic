; ==========================================================================
; vm/isa.inc - the instruction set: the ops, the CALL routines, their tables
; ==========================================================================
; data:     oplist: each op's name, handler, size (0 varies) and flags. O_* the
;           ops, K_* the CALL routines.
; ==========================================================================

; The ops: name, handler, size (0 varies), flags: FX an
; expression's op, FV a variable's id after the op, FB the handler serves
; several ops and reads which in BX.
FX      = 1
FV      = 2
FB      = 4                     ; the handler is shared: BX = the op * 2

oplist  macro   m
        m       EOL,op_eol,1,0
        m       I_LD_V,bc_I_LD_V,2,FX+FV
        m       I_LD_I16,bc_I_LD_I16,3,FX
        m       I_LD_U8,bc_I_LD_U8,2,FX
        m       I_LD_A1,op_ilda1,2,FX
        m       I_LD_A,op_lda,3,FX
        m       F_LD_V,bc_F_LD_V,2,FX+FV
        m       F_LD_VI,bc_F_LD_VI,2,FX+FV
        m       F_LD_F32,bc_F_LD_F32,5,FX
        m       F_LD_U8,bc_F_LD_U8,2,FX
        m       F_LD_A1,op_flda1,2,FX
        m       F_LD_A,op_lda,3,FX
        m       S_LD_V,bc_S_LD_V,2,FX+FV
        m       S_LD_TXT,op_sldt,6,FX
        m       S_LD_A1,op_slda1,2,FX
        m       S_LD_A,op_slda,3,FX
        m       F_LD_PI,bc_F_LD_PI,1,FX
        m       I_ST_V,bc_I_ST_V,2,FV
        m       F_ST_V,bc_F_ST_V,2,FV
        m       S_ST_V,op_sstv,2,FV
        m       I_ST_A1,op_ista1,2,0
        m       F_ST_A1,op_fsta1,2,0
        m       S_ST_A1,op_ssta1,2,0
        m       I_ST_A,op_istan,3,0
        m       F_ST_A,op_fstan,3,0
        m       S_ST_A,op_sstan,3,0
        m       I_ADD,bc_I_ADD,1,FX
        m       I_SUB,bc_I_SUB,1,FX
        m       I_MUL,bc_I_MUL,1,FX
        m       I_DIV,bc_I_DIV,1,FX
        m       I_AND,bc_I_AND,1,FX
        m       I_OR,bc_I_OR,1,FX
        m       I_ADD_V,bc_I_ADD_V,2,FX+FV
        m       I_SUB_V,bc_I_SUB_V,2,FX+FV
        m       I_MUL_V,bc_I_MUL_V,2,FX+FV
        m       I_DIV_V,bc_I_DIV_V,2,FX+FV
        m       I_AND_V,bc_I_AND_V,2,FX+FV
        m       I_OR_V,bc_I_OR_V,2,FX+FV
        m       I_ADD_U8,bc_I_ADD_U8,2,FX
        m       I_SUB_U8,bc_I_SUB_U8,2,FX
        m       I_MUL_U8,bc_I_MUL_U8,2,FX
        m       I_DIV_U8,bc_I_DIV_U8,2,FX
        m       I_AND_U8,bc_I_AND_U8,2,FX
        m       I_OR_U8,bc_I_OR_U8,2,FX
        m       I_NEG,bc_I_NEG,1,FX
        m       I_NOT,bc_I_NOT,1,FX
        m       F_ADD,bc_F_ADD,1,FX+FB
        m       F_SUB,bc_F_SUB,1,FX+FB
        m       F_MUL,bc_F_MUL,1,FX+FB
        m       F_DIV,bc_F_DIV,1,FX+FB
        m       F_ADD_V,bc_F_ADD_V,2,FX+FV+FB
        m       F_SUB_V,bc_F_SUB_V,2,FX+FV+FB
        m       F_MUL_V,bc_F_MUL_V,2,FX+FV+FB
        m       F_DIV_V,bc_F_DIV_V,2,FX+FV+FB
        m       F_ADD_VI,bc_F_ADD_VI,2,FX+FV+FB
        m       F_SUB_VI,bc_F_SUB_VI,2,FX+FV+FB
        m       F_MUL_VI,bc_F_MUL_VI,2,FX+FV+FB
        m       F_DIV_VI,bc_F_DIV_VI,2,FX+FV+FB
        m       F_ADD_U8,bc_F_ADD_U8,2,FX+FB
        m       F_SUB_U8,bc_F_SUB_U8,2,FX+FB
        m       F_MUL_U8,bc_F_MUL_U8,2,FX+FB
        m       F_DIV_U8,bc_F_DIV_U8,2,FX+FB
        m       F_ADD_F32,bc_F_ADD_F32,5,FX+FB
        m       F_SUB_F32,bc_F_SUB_F32,5,FX+FB
        m       F_MUL_F32,bc_F_MUL_F32,5,FX+FB
        m       F_DIV_F32,bc_F_DIV_F32,5,FX+FB
        m       F_NEG,bc_F_NEG,1,FX
        m       I_CMP,bc_I_CMP,2,FX+FB
        m       I_CMP_V,bc_I_CMP_V,3,FX+FV+FB
        m       I_CMP_U8,bc_I_CMP_U8,3,FX+FB
        m       F_CMP,bc_F_CMP,2,FX+FB
        m       F_CMP_V,bc_F_CMP_V,3,FX+FV+FB
        m       F_CMP_VI,bc_F_CMP_VI,3,FX+FV+FB
        m       F_CMP_U8,bc_F_CMP_U8,3,FX+FB
        m       F_CMP_F32,bc_F_CMP_F32,6,FX+FB
        m       S_CMP,op_scmp,2,FX+FB
        m       I_CMPIF,bc_I_CMPIF,2,FB
        m       I_CMPIF_V,bc_I_CMPIF_V,3,FV+FB
        m       I_CMPIF_U8,bc_I_CMPIF_U8,3,FB
        m       F_CMPIF,bc_F_CMPIF,2,FB
        m       F_CMPIF_V,bc_F_CMPIF_V,3,FV+FB
        m       F_CMPIF_VI,bc_F_CMPIF_VI,3,FV+FB
        m       F_CMPIF_U8,bc_F_CMPIF_U8,3,FB
        m       F_CMPIF_F32,bc_F_CMPIF_F32,6,FB
        m       S_CMPIF,op_scmp,2,FB
        m       I_IF,bc_I_IF,1,0
        m       F_IF,bc_F_IF,1,0
        m       S_IF,op_sif,1,0
        m       S_CAT,op_scat,1,FX
        m       I_TOF,bc_I_TOF,1,FX
        m       F_TOI,bc_F_TOI,1,FX
        m       F_TOB,bc_F_TOB,1,FX
        m       F_TOA,bc_F_TOA,1,FX
        m       I_TOB,bc_I_TOB,1,FX
        m       I_TOA,bc_I_TOA,1,FX
        m       DROP,bc_DROP,1,FX
        m       CALL,op_call,2,0
        m       FN_DEF,op_fndef,3,0
        m       FN_CALL,op_fncall,2,FX
        m       FN_ENTER,op_fnent,2,FV
        m       FN_RET,op_fnret,2,0
        m       GOTO,op_goto,5,0
        m       GOSUB,op_gosub,5,0
        m       ON,op_on,0,0
        m       RETURN,op_ret,1,0
        m       I_FOR,op_for,2,FV+FB
        m       F_FOR,op_for,2,FV+FB
        m       I_FOR1,op_for,2,FV+FB
        m       F_FOR1,op_for,2,FV+FB
        m       NEXT,op_nx0,1,0
        m       NEXT_V,op_nextv,2,FV
        m       ERR,op_err,2,0
        endm

opn     = 0
opd     macro   nm,h,sz,fl
O_&nm   = opn
opn     = opn+1
        dd      h
        endm
opszm   macro   nm,h,sz,fl
        db      sz
        endm
opflm   macro   nm,h,sz,fl
        db      fl
        endm

        .data
ops     label   dword
        oplist  opd
O_NOPS  = opn
        .code

; The CALL routines: the statements' first, then from K_IPOW an expression's.
K_IPR   = 0
K_FPR   = 1
K_SPR   = 2
K_PNL   = 3
K_PZONE = 4
K_PTAB  = 5
K_PSPC  = 6
K_FIN   = 7
K_SIN   = 8
K_FGET  = 9
K_SGET  = 10
K_INBEG = 11
K_INPR  = 12
K_INFILE = 13
K_INEND = 14
K_FREAD = 15
K_SREAD = 16
K_RES   = 17
K_RESL  = 18
K_DIM   = 19
K_OPEN  = 20
K_CLOSE = 21
K_FIOUT = 22
K_FIIN  = 23
K_FICLR = 24
K_LOAD  = 25
K_SAVE  = 26
K_VFY   = 27
K_RUN   = 28
K_RUNL  = 29
K_LIST  = 30
K_CONT  = 31
K_CLR   = 32
K_NEW   = 33
K_NODIR = 34
K_END   = 35
K_STOP  = 36
K_POKE  = 37
K_WAIT  = 38
K_WAIT2 = 39
K_SYS   = 40
K_STTI  = 41
K_IPOW  = 42
K_FPOW  = 43
K_FTI   = 44
K_STI   = 45
K_IST   = 46
K_FN    = 47                    ; SGN .. MID$ by token, then MID$ of two
K_LEFT  = K_FN+TK_MID-TK_SGN-2
K_MID2  = K_FN+TK_MID-TK_SGN+1
K_N     = K_MID2+1

        .data
calls   dd      sc_ipr,sc_fpr,sc_spr,sc_pnl,sc_pzone,sc_ptab,sc_pspc
        dd      sc_fin,sc_sin,sc_fget,sc_sget,sc_inbeg,sc_inpr,sc_infile,sc_inend
        dd      sc_fread,sc_sread,sc_res,sc_resl,sc_dim
        dd      sc_open,sc_close,sc_fiout,sc_fiin,sc_ficlr,sc_load,sc_save,sc_vfy
        dd      sc_run,sc_runl,sc_list,sc_cont,sc_clr,sc_new,sc_nodir,sc_end,sc_stop
        dd      sc_poke,sc_wait,sc_wait2,sc_sys,sc_stti
        dd      sc_ipow,sc_fpow,sc_fti,sc_sti,sc_ist
        dd      20 dup(sc_fn1)
        dd      sc_left,sc_right,sc_mid,sc_mid2

bflt    dd      flt_add,flt_sub,flt_mul,flt_div,fn_pow
fntab   dd      fn_sgn,fn_int,fn_abs,fn_usr,fn_fre,fn_pos,fn_sqr,fn_rnd,fn_log,fn_exp
        dd      fn_cos,fn_sin,fn_tan,fn_atn,fn_peek,fn_len,fn_fstr,fn_val,fn_asc
        dd      fn_chr
        .code
