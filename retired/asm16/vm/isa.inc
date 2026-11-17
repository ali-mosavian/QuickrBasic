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
        m       I_LD_V,op_nat,2,FX+FV
        m       I_LD_I16,op_nat,3,FX
        m       I_LD_U8,op_nat,2,FX
        m       I_LD_A1,op_ilda1,2,FX
        m       I_LD_A,op_lda,3,FX
        m       F_LD_V,op_nat,2,FX+FV
        m       F_LD_VI,op_nat,2,FX+FV
        m       F_LD_F32,op_nat,5,FX
        m       F_LD_U8,op_nat,2,FX
        m       F_LD_A1,op_flda1,2,FX
        m       F_LD_A,op_lda,3,FX
        m       S_LD_V,op_nat,2,FX+FV
        m       S_LD_TXT,op_sldt,6,FX
        m       S_LD_A1,op_slda1,2,FX
        m       S_LD_A,op_slda,3,FX
        m       F_LD_PI,op_nat,1,FX
        m       I_ST_V,op_nat,2,FV
        m       F_ST_V,op_nat,2,FV
        m       S_ST_V,op_sstv,2,FV
        m       I_ST_A1,op_ista1,2,0
        m       F_ST_A1,op_fsta1,2,0
        m       S_ST_A1,op_ssta1,2,0
        m       I_ST_A,op_istan,3,0
        m       F_ST_A,op_fstan,3,0
        m       S_ST_A,op_sstan,3,0
        m       I_ADD,op_nat,1,FX
        m       I_SUB,op_nat,1,FX
        m       I_MUL,op_nat,1,FX
        m       I_DIV,op_nat,1,FX
        m       I_AND,op_nat,1,FX
        m       I_OR,op_nat,1,FX
        m       I_ADD_V,op_nat,2,FX+FV
        m       I_SUB_V,op_nat,2,FX+FV
        m       I_MUL_V,op_nat,2,FX+FV
        m       I_DIV_V,op_nat,2,FX+FV
        m       I_AND_V,op_nat,2,FX+FV
        m       I_OR_V,op_nat,2,FX+FV
        m       I_ADD_U8,op_nat,2,FX
        m       I_SUB_U8,op_nat,2,FX
        m       I_MUL_U8,op_nat,2,FX
        m       I_DIV_U8,op_nat,2,FX
        m       I_AND_U8,op_nat,2,FX
        m       I_OR_U8,op_nat,2,FX
        m       I_NEG,op_nat,1,FX
        m       I_NOT,op_nat,1,FX
        m       F_ADD,op_nat,1,FX+FB
        m       F_SUB,op_nat,1,FX+FB
        m       F_MUL,op_nat,1,FX+FB
        m       F_DIV,op_nat,1,FX+FB
        m       F_ADD_V,op_nat,2,FX+FV+FB
        m       F_SUB_V,op_nat,2,FX+FV+FB
        m       F_MUL_V,op_nat,2,FX+FV+FB
        m       F_DIV_V,op_nat,2,FX+FV+FB
        m       F_ADD_VI,op_nat,2,FX+FV+FB
        m       F_SUB_VI,op_nat,2,FX+FV+FB
        m       F_MUL_VI,op_nat,2,FX+FV+FB
        m       F_DIV_VI,op_nat,2,FX+FV+FB
        m       F_ADD_U8,op_nat,2,FX+FB
        m       F_SUB_U8,op_nat,2,FX+FB
        m       F_MUL_U8,op_nat,2,FX+FB
        m       F_DIV_U8,op_nat,2,FX+FB
        m       F_ADD_F32,op_nat,5,FX+FB
        m       F_SUB_F32,op_nat,5,FX+FB
        m       F_MUL_F32,op_nat,5,FX+FB
        m       F_DIV_F32,op_nat,5,FX+FB
        m       F_NEG,op_nat,1,FX
        m       I_CMP,op_nat,2,FX+FB
        m       I_CMP_V,op_nat,3,FX+FV+FB
        m       I_CMP_U8,op_nat,3,FX+FB
        m       F_CMP,op_nat,2,FX+FB
        m       F_CMP_V,op_nat,3,FX+FV+FB
        m       F_CMP_VI,op_nat,3,FX+FV+FB
        m       F_CMP_U8,op_nat,3,FX+FB
        m       F_CMP_F32,op_nat,6,FX+FB
        m       S_CMP,op_scmp,2,FX+FB
        m       I_CMPIF,op_nat,2,FB
        m       I_CMPIF_V,op_nat,3,FV+FB
        m       I_CMPIF_U8,op_nat,3,FB
        m       F_CMPIF,op_nat,2,FB
        m       F_CMPIF_V,op_nat,3,FV+FB
        m       F_CMPIF_VI,op_nat,3,FV+FB
        m       F_CMPIF_U8,op_nat,3,FB
        m       F_CMPIF_F32,op_nat,6,FB
        m       S_CMPIF,op_scmp,2,FB
        m       I_IF,op_nat,1,0
        m       F_IF,op_nat,1,0
        m       S_IF,op_sif,1,0
        m       S_CAT,op_scat,1,FX
        m       I_TOF,op_nat,1,FX
        m       F_TOI,op_nat,1,FX
        m       F_TOB,op_nat,1,FX
        m       F_TOA,op_nat,1,FX
        m       I_TOB,op_nat,1,FX
        m       I_TOA,op_nat,1,FX
        m       DROP,op_nat,1,FX
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
        dw      h
        endm
opszm   macro   nm,h,sz,fl
        db      sz
        endm
opflm   macro   nm,h,sz,fl
        db      fl
        endm

DSEG    segment
ops     label   word
        oplist  opd
O_NOPS  = opn
DSEG    ends

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

DSEG    segment
calls   dw      sc_ipr,sc_fpr,sc_spr,sc_pnl,sc_pzone,sc_ptab,sc_pspc
        dw      sc_fin,sc_sin,sc_fget,sc_sget,sc_inbeg,sc_inpr,sc_infile,sc_inend
        dw      sc_fread,sc_sread,sc_res,sc_resl,sc_dim
        dw      sc_open,sc_close,sc_fiout,sc_fiin,sc_ficlr,sc_load,sc_save,sc_vfy
        dw      sc_run,sc_runl,sc_list,sc_cont,sc_clr,sc_new,sc_nodir,sc_end,sc_stop
        dw      sc_poke,sc_wait,sc_wait2,sc_sys,sc_stti
        dw      sc_ipow,sc_fpow,sc_fti,sc_sti,sc_ist
        dw      20 dup(sc_fn1)
        dw      sc_left,sc_right,sc_mid,sc_mid2

bflt    dw      flt_add,flt_sub,flt_mul,flt_div,fn_pow
fntab   dw      fn_sgn,fn_int,fn_abs,fn_usr,fn_fre,fn_pos,fn_sqr,fn_rnd,fn_log,fn_exp
        dw      fn_cos,fn_sin,fn_tan,fn_atn,fn_peek,fn_len,fn_fstr,fn_val,fn_asc
        dw      fn_chr
DSEG    ends
