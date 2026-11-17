; ==========================================================================
; expr.asm - the expression compiler
; ==========================================================================
; holds:    cm_expr, cm_p, cm_un and the operand loads
; uses:     cm_op .., st_get, flt_in, sym_find
; data:     Every operand is pushed; an operator takes the cells it needs. A
;           right operand that is one load folds into the operator: F.ADD.V,
;           I.CMP.U8 ...
;           prec: OR 46h, AND 50h, NOT 5Ah, > = < 64h, + - 79h, * / 7Bh,
;           unary - 7Dh, ^ 7Fh
; calls:
;   cm_p               cm_un; the operators tighter than its precedence
; ==========================================================================

; ::::::::::::::
; cm_expr - compile an expression.
;
; in:     SI  at its first byte
; out:    SI  after it and the spaces
; ::::::::::::::
cm_expr:
        xor     al,al

; ::::::::::::::
; cm_p - compile while the operators bind tighter than precedence AL.
;
; in:     AL  the precedence
;         SI  at the first byte
; out:    SI  after the operands taken and the spaces
; error:  ?OUT OF MEMORY  the stack is nearly full
; ::::::::::::::
cm_p:
        push    ax
        cmp     sp,[stklim]
        jb      err_om
        call    cm_un
@cp_l:
        call    st_get
        sub     al,TK_PLUS
        cmp     al,TK_LT-TK_PLUS+1
        jae     @cp_e
        movzx   bx,al
        mov     bp,sp
        mov     cl,[cprec+bx]
        cmp     byte ptr [bp],cl
        jae     @cp_e
        cmp     bl,TK_GT-TK_PLUS
        jb      @cp_a
        mov     dl,80h
@cp_r:
        or      dl,[relbit+bx-(TK_GT-TK_PLUS)]
        inc     si
        call    st_get
        sub     al,TK_GT
        cmp     al,3
        jae     @cp_d
        add     al,TK_GT-TK_PLUS
        movzx   bx,al
        jmp     @cp_r
@cp_a:
        mov     dl,bl
        inc     si
@cp_d:
        cmp     dl,5
        jb      @cp_kl
        cmp     dl,7
        jae     @cp_kl
        push    bx
        push    dx
        call    cm_toi
        pop     dx
        pop     bx
@cp_kl:
        mov     al,[cty]
        mov     ah,0
        push    bx
        call    cm_ileaf
        mov     cx,bx
        pop     bx
        jne     @cp_nf
        mov     ah,1
@cp_nf:
        push    dx
        push    ax
        push    cx
        push    di
        mov     al,[cprec+bx]
        call    cm_p
        pop     bx                      ; the right operand
        pop     cx                      ; the left, an INT load when AH 1
        pop     ax                      ; AL the left's type
        pop     dx                      ; the operator
        cmp     dl,5
        jb      @cp_n1
        cmp     dl,7
        jae     @cp_n1
        push    dx
        push    bx
        call    cm_toi
        pop     bx
        pop     ax
        dec     al                      ; AND 4, OR 5
        call    cm_iop
        jmp     @cp_l
@st_expr1:
        jmp     err_tm
@cp_n1:
        cmp     al,T_STR
        jne     @cp_nm
        test    [cty],T_STR
        jz      @st_expr1
        test    dl,dl
        jz      @cp_cat
        jns     @st_expr1
        and     dl,7
        mov     cl,dl
        mov     [ccmp],di
        mov     al,O_S_CMP
        call    cm_opx
        mov     [ccmpe],di
        mov     [cty],T_INT
        jmp     @cp_l
@cp_cat:
        mov     al,O_S_CAT
        call    cm_op
        jmp     @cp_l
@cp_nm:
        test    [cty],T_STR
        jnz     @st_expr1
        cmp     dl,4
        je      @cp_pw
        test    al,al
        js      @cp_fl
        cmp     [cty],T_INT
        jne     @cp_lf
        test    dl,dl
        js      @cp_ic
        mov     al,dl
        call    cm_iop
        jmp     @cp_l
@cp_ic:
        call    cm_icmp
        jmp     @cp_l
@cp_pw:
        test    al,al
        js      @cp_pf
        cmp     [cty],T_INT
        jne     @cp_pl
        mov     al,K_IPOW
        call    cm_call
        jmp     @cp_l
@cp_pl:
        call    cm_lflt
@cp_pf:
        call    cm_tof
        mov     al,K_FPOW
        call    cm_call
        mov     [cty],T_FLT
        jmp     @cp_l
@cp_lf:
        call    cm_lflt
@cp_fl:
        push    dx
        push    bx
        call    cm_tof
        pop     bx
        pop     dx
        test    dl,dl
        js      @cp_fc
        mov     al,dl
        call    cm_fop2
        jmp     @cp_l
@cp_fc:
        call    cm_fcmp
        jmp     @cp_l
@cp_e:
        pop     ax
        ret

; ::::::::::::::
; cm_lflt - the left operand, an INT, as a FLOAT: its load in place, else
; I.TOF before the right operand.
;
; in:     AH  1 when CX is its load, ending at BX
;         CX  the load
;         BX  the right operand
; out:    BX  the right operand, moved
; keeps:  DX
; ::::::::::::::
cm_lflt:
        test    ah,ah
        jz      @clf_i
        xchg    bx,cx
        mov     al,[bx]
        cmp     al,O_I_LD_V
        jne     @clf_k
        mov     byte ptr [bx],O_F_LD_VI
        jmp     @clf_r
@clf_k:
        cmp     al,O_I_LD_U8
        jne     @clf_x
        mov     byte ptr [bx],O_F_LD_U8
@clf_r:
        xchg    bx,cx
        ret
@clf_x:
        xchg    bx,cx
@clf_i:
        push    dx
        mov     al,O_I_TOF
        call    cm_ins
        pop     dx
        inc     bx
        ret

; ::::::::::::::
; cm_right - is the right operand at BX one op of kind AL, sized AH?
;
; in:     AL  the op
;         AH  its size
;         BX  the right operand
; out:    ZF  set when it is
; keeps:  all
; ::::::::::::::
cm_right:
        push    cx
        mov     cx,di
        sub     cx,bx
        cmp     cl,ah
        jne     @crt_r
        test    ch,ch
        jne     @crt_r
        cmp     al,[bx]
@crt_r:
        pop     cx
        ret

; ::::::::::::::
; cm_iop - INT operator AL (0..5: + - * / AND OR): the right operand's load
; folds into .V or .U8.
;
; in:     AL  the operator
;         BX  the right operand
; out:    cty  T_INT
; ::::::::::::::
cm_iop:
        mov     [cty],T_INT
        mov     dl,al
        mov     ax,2*256+O_I_LD_V
        call    cm_right
        jne     @cio_k
        add     dl,O_I_ADD_V
        mov     [bx],dl
        ret
@cio_k:
        mov     al,O_I_LD_U8
        call    cm_right
        jne     @cio_p
        add     dl,O_I_ADD_U8
        mov     [bx],dl
        ret
@cio_p:
        mov     al,dl
        add     al,O_I_ADD
        jmp     cm_op

; ::::::::::::::
; cm_icmp - an INT relation: I.CMP mask, or the right's load folded.
;
; in:     DL  80h + the mask
;         BX  the right operand
; out:    cty  T_INT
; ::::::::::::::
cm_icmp:
        mov     [cty],T_INT
        and     dl,7
        mov     ax,2*256+O_I_LD_V
        call    cm_right
        mov     cl,O_I_CMP_V
        je      @cic_f
        mov     al,O_I_LD_U8
        call    cm_right
        mov     cl,O_I_CMP_U8
        je      @cic_f
        mov     [ccmp],di
        mov     cl,dl
        mov     al,O_I_CMP
        call    cm_opx
        mov     [ccmpe],di
        ret
@cic_f:
        mov     [ccmp],bx
        mov     [bx],cl
        mov     al,dl
        call    cm_byt
        mov     [ccmpe],di
        ret

; ::::::::::::::
; cm_fop2 - FLOAT operator AL (0..3: + - * /): the right's load folds into
; .V, .VI, .U8 or .F32.
;
; in:     AL  the operator
;         BX  the right operand, a FLOAT
; out:    cty  T_FLT
; ::::::::::::::
cm_fop2:
        mov     [cty],T_FLT
        mov     dl,al
        call    cm_fkind
        jne     @cfp_p
        add     dl,cl
        mov     [bx],dl
        ret
@cfp_p:
        mov     al,dl
        add     al,O_F_ADD
        jmp     cm_op

; ::::::::::::::
; cm_fkind - the right operand's load: the first op of its folded group.
;
; in:     BX  the right operand
; out:    ZF  set when it is a load that folds
;         CL  O_F_ADD_V, _VI, _U8 or _F32
; keeps:  BX, DX
; ::::::::::::::
cm_fkind:
        mov     ax,2*256+O_F_LD_V
        mov     cl,O_F_ADD_V
        call    cm_right
        je      @cfk_r
        mov     al,O_F_LD_VI
        mov     cl,O_F_ADD_VI
        call    cm_right
        je      @cfk_r
        mov     al,O_F_LD_U8
        mov     cl,O_F_ADD_U8
        call    cm_right
        je      @cfk_r
        mov     ax,5*256+O_F_LD_F32
        mov     cl,O_F_ADD_F32
        call    cm_right
@cfk_r:
        ret

; ::::::::::::::
; cm_fcmp - a FLOAT relation: F.CMP mask, or the right's load folded.
;
; in:     DL  80h + the mask
;         BX  the right operand, a FLOAT
; out:    cty  T_INT
; ::::::::::::::
cm_fcmp:
        mov     [cty],T_INT
        and     dl,7
        call    cm_fkind
        jne     @cfc_p
        mov     [ccmp],bx
        sub     cl,O_F_ADD_V
        shr     cl,2
        movzx   bx,cl
        mov     al,[fcmpk+bx]
        mov     bx,[ccmp]
        mov     [bx],al
        mov     al,dl
        call    cm_byt
        mov     [ccmpe],di
        ret
@cfc_p:
        mov     [ccmp],di
        mov     cl,dl
        mov     al,O_F_CMP
        call    cm_opx
        mov     [ccmpe],di
        ret

DSEG    segment
fcmpk   db      O_F_CMP_V,O_F_CMP_VI,O_F_CMP_U8,O_F_CMP_F32
DSEG    ends

; ::::::::::::::
; cm_lval - a target: the name, its subscripts compiled.
;
; in:     SI  at M_VAR, or spaces before it
; out:    ctv, ctk, ctn  the target
; error:  ?SYNTAX ERROR  no M_VAR
; ::::::::::::::
cm_lval:
        call    cm_name
        call    cm_unf
        call    cm_tgt
        test    [ctk],80h
        jz      @rt_exp1
        push    word ptr [ctv]
        call    cm_subs0
        pop     word ptr [ctv]
        mov     [ctn],cl
@rt_exp1:
        ret

; ::::::::::::::
; cm_load - a variable's value: X.LD.V id, a CALL for TI TI$ ST, or the
; subscripts and X.LD.A1 id, X.LD.A id n.
;
; in:     SI  at M_VAR
; ::::::::::::::
cm_load:
        call    cm_name
        mov     dl,[sinfo+bx]
        test    dl,dl
        js      @cld_a
        mov     cl,bl
        and     dl,43h
        cmp     dl,40h
        jae     @cld_y
        cmp     dl,1
        je      @cld_i
        ja      @cld_s
        mov     [cty],T_FLT
        mov     al,O_F_LD_V
        jmp     cm_opx
@cld_i:
        mov     [cll],di
        mov     al,O_I_LD_V
        call    cm_opx
        mov     [clle],di
        mov     [cty],T_INT
        ret
@cld_s:
        mov     [cty],T_STR
        mov     al,O_S_LD_V
        jmp     cm_opx
@cld_y:
        mov     [cty],T_STR
        mov     al,K_STI
        test    dl,2
        jnz     @jt_expr1
        add     bx,bx
        mov     [cty],T_INT
        mov     al,K_IST
        cmp     byte ptr [skey+bx],'S'
        je      @jt_expr1
        mov     [cty],T_FLT
        mov     al,K_FTI
@jt_expr1:
        jmp     cm_call
@cld_a:
        push    bx
        push    dx
        call    cm_subs0
        pop     dx
        and     dl,3
        movzx   bx,dl
        mov     al,[ctys+bx]
        mov     [cty],al
        mov     al,[lda1+bx]
        cmp     cl,1
        je      @cld_1
        mov     al,[ldan+bx]
        mov     ch,cl
        pop     bx
        mov     cl,bl
        call    cm_opx
        mov     al,ch
        jmp     cm_byt
@cld_1:
        pop     bx
        mov     cl,bl
        jmp     cm_opx

DSEG    segment
ctys    db      T_FLT,T_INT,T_STR
lda1    db      O_F_LD_A1,O_I_LD_A1,O_S_LD_A1
ldan    db      O_F_LD_A,O_I_LD_A,O_S_LD_A
DSEG    ends

; ::::::::::::::
; cm_subs0 - ( expr {, expr} ), each an INT.
;
; in:     SI  at '('
; out:    CL  how many
; error:  ?SYNTAX ERROR  no ( or )
; ::::::::::::::
cm_subs0:
        call    cm_chkop
        xor     cx,cx
@cs_l:
        inc     cx
        push    cx
        call    cm_expr
        call    cm_toi
        call    cm_optcm
        pop     cx
        jz      @cs_l
        push    cx
        call    cm_chkcl
        pop     cx
        ret

; ::::::::::::::
; cm_paren - ( expr ).
;
; in:     SI  at '('
; error:  ?SYNTAX ERROR  no ( or )
; ::::::::::::::
cm_paren:
        call    cm_chkop
        call    cm_expr
        jmp     cm_chkcl

; ::::::::::::::
; cm_str - "literal": S.LD.TXT len, the text.
;
; in:     SI  at the opening quote
; out:    SI  past the closing quote, or at the 0
; ::::::::::::::
cm_str:
        inc     si

; ::::::::::::::
; cm_str1 - cm_str after the opening quote.
;
; in:     SI  after the opening quote
; ::::::::::::::
cm_str1:
        mov     [cty],T_STR
        mov     ax,'""'
        call    str_span
        mov     cx,ax
        push    si
        mov     al,O_S_LD_TXT
        call    cm_op
        mov     al,cl
        call    cm_byt
        movzx   eax,bx
        add     eax,[ctxlin]
        call    cm_dw
        pop     si
        jmp     str_skq

; ::::::::::::::
; cm_un - compile an operand: variable, element, literal, ( expr ), - NOT +,
; function.
;
; in:     SI  at it
; out:    SI  after it
; error:  ?SYNTAX ERROR  not an operand
; ::::::::::::::
cm_un:
        call    st_get
        cmp     al,M_VAR
        je      cm_load
        cmp     al,M_INT
        jne     @cu_2
        mov     ax,[si+1]
        add     si,3
        test    ah,80h
        jz      cm_lint
        call    flt_fromu16
        jmp     @cu_fk
@cu_2:
        cmp     al,'('
        je      cm_paren
        mov     cx,O_F_NEG*256+7Dh
        cmp     al,TK_MINUS
        je      @cu_u
        mov     cx,O_I_NOT*256+5Ah
        cmp     al,TK_NOT
        jne     @cu_4
@cu_u:
        push    cx
        mov     al,cl
        inc     si
        call    cm_p
        pop     cx
        cmp     ch,O_F_NEG
        je      @cu_nf
        call    cm_toi
        mov     al,O_I_NOT
@jt_expr2:
        jmp     cm_op
@cu_nf:
        test    [cty],T_STR
        jnz     err_tm
        cmp     [cty],T_FLT
        mov     al,O_F_NEG
        je      @jt_expr2
        call    cm_ileaf
        jne     @cu_ng
        mov     al,[bx]
        cmp     al,O_I_LD_V
        je      @cu_ng
        mov     ax,[bx+1]
        cmp     byte ptr [bx],O_I_LD_U8
        jne     @cu_n16
        xor     ah,ah
@cu_n16:
        neg     ax
        mov     di,bx
        jmp     cm_lint
@cu_ng:
        mov     al,O_I_NEG
        jmp     cm_op
@cu_4:
        cmp     al,TK_PLUS
        jne     @cu_5
        inc     si
        jmp     cm_un
@cu_5:
        cmp     al,TK_FN
        je      @cu_fn
        cmp     al,'"'
        je      cm_str
        cmp     al,'.'
        jb      @cu_7
        cmp     al,'9'
        ja      @cu_7
        call    flt_in
@cu_fk:
        mov     [cty],T_FLT
        push    eax
        mov     al,O_F_LD_F32
        call    cm_op
        pop     eax
        jmp     cm_dw
@cu_fn:
        call    st_getn
        cmp     al,M_VAR
        jne     err_sn
        call    var_id
        push    ax
        call    cm_paren
        call    cm_tof
        pop     cx
        mov     al,O_FN_CALL
        jmp     cm_opx
@cu_7:
        sub     al,TK_SGN
        cmp     al,TK_MID-TK_SGN
        ja      @cu_8
        push    ax
        inc     si
        call    cm_chkop
        call    cm_expr
        pop     ax
        cmp     al,TK_MID-TK_SGN-2
        jb      @cu_a1
        push    ax
        call    cm_chkcm
        call    cm_tos
        call    cm_expr
        call    cm_toa
        pop     ax
        cmp     al,TK_MID-TK_SGN
        jne     @cu_a2
        push    ax
        call    cm_optcm
        pop     ax
        mov     cl,K_MID2-K_FN
        jnz     @cu_a3
        push    ax
        call    cm_expr
        call    cm_toa
        pop     ax
@cu_a2:
        mov     cl,al
@cu_a3:
        push    cx
        call    cm_chkcl
        pop     ax
        mov     [cty],T_STR
        add     al,K_FN
        jmp     cm_call
@cu_a1:
        push    ax
        call    cm_chkcl
        pop     cx
        call    cm_farg
        mov     al,cl
        add     al,K_FN
        jmp     cm_call
@cu_8:
        cmp     al,TK_PI-TK_SGN
        jne     err_sn
        inc     si
        mov     [cty],T_FLT
        mov     al,O_F_LD_PI
        jmp     cm_op

; ::::::::::::::
; cm_farg - function CL's argument converted; cty its result.
;
; in:     CL   the function, TK_SGN based
;         cty  the argument's type
; keeps:  CL
; error:  ?TYPE MISMATCH  the wrong type
; ::::::::::::::
cm_farg:
        movzx   bx,cl
        mov     al,[fnsig+bx]
        push    cx
        push    ax
        and     al,0Fh
        movzx   bx,al
        add     bx,bx
        call    word ptr [fnarg+bx]
        pop     ax
        pop     cx
        and     al,0F0h
        cmp     al,0B0h                 ; T_STR's mark
        jne     @cfa_t
        mov     al,T_STR
@cfa_t:
        mov     [cty],al
        ret

; ::::::::::::::
; cm_nany - POS's argument: a number.
;
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_nany:
        test    [cty],T_STR
        jnz     err_tm
        ret

DSEG    segment
fnarg   dw      cm_tof,cm_toa,cm_tos,cm_rts,cm_tob,cm_nany
DSEG    ends

FA_F    = 0
FA_A    = 1
FA_S    = 2
FA_Y    = 3
FA_B    = 4
FA_N    = 5
FR_I    = T_INT
FR_F    = T_FLT
FR_S    = 0B0h

; Per function, TK_SGN based: the argument, the result.
DSEG    segment
fnsig   db      FA_F+FR_I,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_Y+FR_F,FA_N+FR_I
        db      FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F
        db      FA_F+FR_F,FA_F+FR_F,FA_A+FR_I,FA_S+FR_I,FA_F+FR_S,FA_S+FR_F
        db      FA_S+FR_I,FA_B+FR_S
DSEG    ends

DSEG    segment
cprec   db      79h,79h,7Bh,7Bh,7Fh,50h,46h,64h,64h,64h
DSEG    ends

DSEG    segment
cmact   dw      cm_expr,cm_ax,cm_chkcm,cm_ao,cm_afc,cm_aes,cm_rts,cm_rts
        dw      cm_loop,cm_lval,cm_lnum,cm_tob,cm_toa,cm_tos,cm_str1,cm_fv
        dw      cm_fst,cm_fork,cm_for,cm_for1,cm_nv,cm_if,cm_on,cm_onl
        dw      cm_print,cm_def,cm_list,cm_dv,cm_dat,cm_rem,cm_store,cm_in
        dw      cm_rd,cm_get,cm_goto,cm_gosub,cm_run,cm_res,cm_ret,cm_n0
        dw      cm_n1,cm_narg,cm_fop
DSEG    ends

; The script of each statement, END (80h) .. NEW (A2h), as an offset in cms.
DSEG    segment
cmt     db      s_end-cms,s_for-cms,s_next-cms,s_data-cms,s_inputn-cms,s_input-cms
        db      s_dim-cms,s_read-cms,s_let-cms,s_goto-cms,s_run-cms,s_if-cms
        db      s_res-cms,s_gosub-cms,s_ret-cms,s_rem-cms,s_stop-cms,s_on-cms
        db      s_wait-cms,s_file-cms,s_file-cms,s_file-cms,s_def-cms,s_poke-cms
        db      s_printn-cms,s_print-cms,s_cont-cms,s_list-cms,s_clr-cms,s_cmd-cms
        db      s_sys-cms,s_open-cms,s_close-cms,s_get-cms,s_new-cms

cms     label   byte
s_end   db      K_END or LAST
s_data  db      A_DATA or LAST
s_if    db      A_IF or LAST
s_ret   db      A_RET or LAST
s_rem   db      A_REM or LAST
s_stop  db      K_STOP or LAST
s_def   db      A_DEF or LAST
s_print db      A_PR or LAST
s_cont  db      K_CONT or LAST
s_list  db      A_LST or LAST
s_clr   db      K_CLR or LAST
s_new   db      K_NEW or LAST
s_run   db      A_RUN or LAST
s_res   db      A_RES or LAST
s_for   db      A_FV,A_X,TK_EQ,A_E,A_FST,A_X,TK_TO,A_E,A_FK,A_O,TK_STEP,A_E,A_FK
        db      A_FOR or LAST,A_M,A_FOR1 or LAST
s_next  db      A_LM,A_NV,A_LOOP or LAST
s_inputn db     A_E,A_TOB,K_FIIN,A_C,K_INFILE,A_LM,A_LV,A_IN,A_LOOP,K_INEND
        db      K_FICLR or LAST
s_input db      K_INBEG,A_O,'"',A_STR,K_SPR,A_X,';',A_M,K_INPR,A_LM,A_LV,A_IN
        db      A_LOOP,K_INEND or LAST
s_dim   db      A_LM,A_DV,A_LOOP or LAST
s_read  db      A_LM,A_LV,A_RD,A_LOOP or LAST
s_let   db      A_LV,A_X,TK_EQ,A_E,A_LET or LAST
s_go    db      A_X,TK_TO
s_goto  db      A_L,A_GOTO or LAST
s_gosub db      A_L,A_GOSUB or LAST
s_on    db      A_E,A_ON,A_LM,A_ONL,A_LOOP or LAST
s_wait  db      A_E,A_TOA,A_C,A_E,A_TOB,A_O,',',A_E,A_TOB,K_WAIT or LAST,A_M
        db      K_WAIT2 or LAST
s_poke  db      A_E,A_TOA,A_C,A_E,A_TOB,K_POKE or LAST
s_file  db      A_N0,A_ES,A_E,A_TOS,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1
        db      A_FC,A_M,A_NARG,A_FOP or LAST
s_open  db      A_N0,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC,A_E,A_TOB,A_N1,A_FC
        db      A_E,A_TOS,A_N1,A_M,A_NARG,K_OPEN or LAST
s_close db      A_E,A_TOB,K_CLOSE or LAST
s_sys   db      A_E,A_TOA,K_SYS or LAST
s_printn db     A_E,A_TOB,K_FIOUT,A_FC,A_M,A_PR,K_FICLR or LAST
s_cmd   db      A_E,A_TOB,K_FIOUT,A_FC,A_M,A_PR or LAST
s_get   db      A_O,'#',A_E,A_TOB,K_FIIN,A_C,A_LM,A_LV,A_GET,A_LOOP,K_FICLR or LAST
        db      A_M,K_NODIR,A_LM,A_LV,A_GET,A_LOOP or LAST
DSEG    ends
