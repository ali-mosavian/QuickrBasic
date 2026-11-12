; ==========================================================================
; expr.asm - the expression compiler
; ==========================================================================
; holds:    cm_expr, cm_p, cm_un and the operand loads
; uses:     cm_op .., st_get, flt_in, var_id
; data:     Every operand is pushed; an operator takes the cells it needs. A
;           right operand that is one load folds into the operator: F.ADD.V,
;           I.CMP.U8 ...
;           prec: OR 46h, AND 50h, NOT 5Ah, > = < 64h, + - 79h, * / 7Bh,
;           unary - 7Dh, ^ 7Fh
; calls:
;   cm_p               cm_un; the operators tighter than its precedence
; ==========================================================================

include x86f.inc
        .code

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
        push    eax
        cmp     esp,[g_state.rn.cplim]
        jb      err_om
        call    cm_un
@cp_l:
        call    st_get
        sub     al,TK_PLUS
        cmp     al,TK_LT-TK_PLUS+1
        jae     @cp_e
        movzx   ebx,al
        mov     ebp,esp
        mov     cl,[g_cprec+ebx]
        cmp     byte ptr [ebp],cl
        jae     @cp_e
        cmp     bl,TK_GT-TK_PLUS
        jb      @cp_a
        mov     dl,80h
@cp_r:
        or      dl,[g_relbit+ebx-(TK_GT-TK_PLUS)]
        inc     esi
        call    st_get
        sub     al,TK_GT
        cmp     al,3
        jae     @cp_d
        add     al,TK_GT-TK_PLUS
        movzx   ebx,al
        jmp     @cp_r
@cp_a:
        mov     dl,bl
        inc     esi
@cp_d:
        cmp     dl,5
        jb      @cp_kl
        cmp     dl,7
        jae     @cp_kl
        push    ebx
        push    edx
        call    cm_toi
        pop     edx
        pop     ebx
@cp_kl:
        mov     al,[g_state.cm.cty]
        mov     ah,0
        push    ebx
        call    cm_ileaf
        mov     ecx,ebx
        pop     ebx
        jne     @cp_nf
        mov     ah,1
@cp_nf:
        push    edx
        push    eax
        push    ecx
        push    edi
        mov     al,[g_cprec+ebx]
        call    cm_p
        pop     ebx                      ; the right operand
        pop     ecx                      ; the left, an INT load when AH 1
        pop     eax                      ; AL the left's type
        pop     edx                      ; the operator
        cmp     dl,5
        jb      @cp_n1
        cmp     dl,7
        jae     @cp_n1
        push    edx
        push    ebx
        call    cm_toi
        pop     ebx
        pop     eax
        dec     al                      ; AND 4, OR 5
        call    cm_iop
        jmp     @cp_l
@st_expr1:
        jmp     err_tm
@cp_n1:
        cmp     al,T_STR
        jne     @cp_nm
        test    [g_state.cm.cty],T_STR
        jz      @st_expr1
        test    dl,dl
        jz      @cp_cat
        jns     @st_expr1
        and     dl,7
        mov     cl,dl
        mov     [g_state.cm.ccmp],edi
        mov     al,O_S_CMP
        call    cm_opx
        mov     [g_state.cm.ccmpe],edi
        mov     [g_state.cm.cty],T_INT
        jmp     @cp_l
@cp_cat:
        mov     al,O_S_CAT
        call    cm_op
        jmp     @cp_l
@cp_nm:
        test    [g_state.cm.cty],T_STR
        jnz     @st_expr1
        cmp     dl,4
        je      @cp_pw
        test    al,al
        js      @cp_fl
        cmp     [g_state.cm.cty],T_INT
        jne     @cp_lf
        cmp     dl,3                    ; INT / INT is a FLOAT, as QBasic's
        je      @cp_lf
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
        cmp     [g_state.cm.cty],T_INT
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
        mov     [g_state.cm.cty],T_FLT
        jmp     @cp_l
@cp_lf:
        call    cm_lflt
@cp_fl:
        push    edx
        push    ebx
        call    cm_tof
        pop     ebx
        pop     edx
        test    dl,dl
        js      @cp_fc
        mov     al,dl
        call    cm_fop2
        jmp     @cp_l
@cp_fc:
        call    cm_fcmp
        jmp     @cp_l
@cp_e:
        pop     eax
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
        xchg    ebx,ecx
        mov     al,[ebx]
        cmp     al,O_I_LD_V
        jne     @clf_k
        mov     byte ptr [ebx],O_F_LD_VI
        jmp     @clf_r
@clf_k:
        cmp     al,O_I_LD_U8
        jne     @clf_x
        mov     byte ptr [ebx],O_F_LD_U8
@clf_r:
        xchg    ebx,ecx
        ret
@clf_x:
        xchg    ebx,ecx
@clf_i:
        push    edx
        mov     al,O_I_TOF
        call    cm_ins
        pop     edx
        inc     ebx
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
        push    ecx
        mov     ecx,edi
        sub     ecx,ebx
        cmp     cl,ah
        jne     @crt_r
        test    ch,ch
        jne     @crt_r
        cmp     al,[ebx]
@crt_r:
        pop     ecx
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
        mov     [g_state.cm.cty],T_INT
        mov     dl,al
        mov     ax,3*256+O_I_LD_V
        call    cm_right
        jne     @cio_k
        add     dl,O_I_ADD_V
        mov     [ebx],dl
        ret
@cio_k:
        mov     ax,2*256+O_I_LD_U8
        call    cm_right
        jne     @cio_p
        add     dl,O_I_ADD_U8
        mov     [ebx],dl
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
        mov     [g_state.cm.cty],T_INT
        and     dl,7
        mov     ax,3*256+O_I_LD_V
        call    cm_right
        mov     cl,O_I_CMP_V
        je      @cic_f
        mov     ax,2*256+O_I_LD_U8
        call    cm_right
        mov     cl,O_I_CMP_U8
        je      @cic_f
        mov     [g_state.cm.ccmp],edi
        mov     cl,dl
        mov     al,O_I_CMP
        call    cm_opx
        mov     [g_state.cm.ccmpe],edi
        ret
@cic_f:
        mov     [g_state.cm.ccmp],ebx
        mov     [ebx],cl
        mov     al,dl
        call    cm_byt
        mov     [g_state.cm.ccmpe],edi
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
        mov     [g_state.cm.cty],T_FLT
        mov     dl,al
        call    cm_fkind
        jne     @cfp_p
        add     dl,cl
        mov     [ebx],dl
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
        mov     ax,3*256+O_F_LD_V
        mov     cl,O_F_ADD_V
        call    cm_right
        je      @cfk_r
        mov     al,O_F_LD_VI
        mov     cl,O_F_ADD_VI
        call    cm_right
        je      @cfk_r
        mov     ax,2*256+O_F_LD_U8
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
        mov     [g_state.cm.cty],T_INT
        and     dl,7
        call    cm_fkind
        jne     @cfc_p
        mov     [g_state.cm.ccmp],ebx
        sub     cl,O_F_ADD_V
        shr     cl,2
        movzx   ebx,cl
        mov     al,[g_fcmpk+ebx]
        mov     ebx,[g_state.cm.ccmp]
        mov     [ebx],al
        mov     al,dl
        call    cm_byt
        mov     [g_state.cm.ccmpe],edi
        ret
@cfc_p:
        mov     [g_state.cm.ccmp],edi
        mov     cl,dl
        mov     al,O_F_CMP
        call    cm_opx
        mov     [g_state.cm.ccmpe],edi
        ret

        .data
g_fcmpk db      O_F_CMP_V,O_F_CMP_VI,O_F_CMP_U8,O_F_CMP_F32
        .code

; ::::::::::::::
; cm_lval - a target: the name, its subscripts compiled.
;
; in:     SI  at M_VAR, or spaces before it
; out:    ctv, ctk, ctn  the target
; error:  ?SYNTAX ERROR  no M_VAR
; ::::::::::::::
cm_lval:
        call    cm_name
        call    cm_tgt
        test    [g_state.cm.ctk],80h
        jz      @rt_exp1
        push    dword ptr [g_state.cm.ctv]
        call    cm_subs0
        pop     dword ptr [g_state.cm.ctv]
        mov     [g_state.cm.ctn],cl
@rt_exp1:
        ret

; ::::::::::::::
; cm_load - a variable's value: X.LD.V id, a DEF FN's call, or the
; subscripts and X.LD.A1 id, X.LD.A id n.
;
; in:     SI  at M_VAR
; ::::::::::::::
cm_load:
        call    st_get
        cmp     al,M_VAR
        jne     @cld_v
        movzx   ecx,byte ptr [esi+1]    ; a FUNCTION's name: its call, but
        movzx   eax,word ptr [esi+ecx+2]        ; its result in it
        test    [g_nflag+eax],NM_FUNC
        jz      @cld_v
        movzx   edx,[g_nprc+eax]
        test    edx,edx
        jz      @cld_v
        cmp     byte ptr [esi+ecx+4],'('
        je      ct_fcall
        cmp     dl,[g_state.cm.cproc]
        jne     ct_fcall
@cld_v:
        call    cm_name
        mov     dl,[g_sinfo+ebx]
        test    dl,dl
        js      @cld_a
        mov     ecx,ebx
        and     dl,43h
        cmp     dl,3
        je      @cld_fn
        cmp     dl,1
        je      @cld_i
        ja      @cld_s
        mov     [g_state.cm.cty],T_FLT
        mov     al,O_F_LD_V
        jmp     cm_opid
@cld_i:
        mov     [g_state.cm.cll],edi
        mov     al,O_I_LD_V
        call    cm_opid
        mov     [g_state.cm.clle],edi
        mov     [g_state.cm.cty],T_INT
        ret
@cld_s:
        mov     [g_state.cm.cty],T_STR
        mov     al,O_S_LD_V
        jmp     cm_opid
@cld_fn:
        push    ebx                     ; DEF FN's: ( expr ), FN.CALL
        call    cm_paren
        call    cm_tof
        pop     ecx
        mov     al,O_FN_CALL
        jmp     cm_opid
@cld_a:
        push    ebx
        push    edx
        call    cm_subs0
        pop     edx
        and     dl,3
        movzx   ebx,dl
        mov     al,[g_ctys+ebx]
        mov     [g_state.cm.cty],al
        mov     al,[g_lda1+ebx]
        cmp     cl,1
        je      @cld_1
        mov     al,[g_ldan+ebx]
        pop     ebx
        push    ecx
        mov     ecx,ebx
        call    cm_opid
        pop     eax
        jmp     cm_byt
@cld_1:
        pop     ecx
        jmp     cm_opid

        .data
g_ctys  db      T_FLT,T_INT,T_STR
g_lda1  db      O_F_LD_A1,O_I_LD_A1,O_S_LD_A1
g_ldan  db      O_F_LD_A,O_I_LD_A,O_S_LD_A
        .code

; ::::::::::::::
; cm_subs0 - ( expr {, expr} ), each an INT.
;
; in:     SI  at '('
; out:    CL  how many
; error:  ?SYNTAX ERROR  no ( or )
; ::::::::::::::
cm_subs0:
        call    cm_chkop
        xor     ecx,ecx
@cs_l:
        inc     ecx
        push    ecx
        call    cm_expr
        call    cm_toi
        call    st_get
        cmp     al,TK_TO                ; lo TO hi: from 0 to hi
        jne     @F
        inc     esi
        mov     al,O_DROP
        call    cm_op
        call    cm_expr
        call    cm_toi
@@:
        call    cm_optcm
        pop     ecx
        jz      @cs_l
        push    ecx
        call    cm_chkcl
        pop     ecx
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
        inc     esi

; ::::::::::::::
; cm_str1 - cm_str after the opening quote.
;
; in:     SI  after the opening quote
; ::::::::::::::
cm_str1:
        mov     [g_state.cm.cty],T_STR
        mov     ax,'""'
        call    str_span
        movzx   ecx,ax
        push    esi
        mov     al,O_S_LD_TXT
        call    cm_op
        mov     al,cl
        call    cm_byt
        mov     eax,ebx
        add     eax,[g_state.cm.ctxlin]
        call    cm_dw
        pop     esi
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
        mov     eax,[esi+1]
        add     esi,5
        jmp     cm_lint
@cu_2:
        cmp     al,'('
        je      cm_paren
        mov     ecx,O_F_NEG*256+7Dh
        cmp     al,TK_MINUS
        je      @cu_u
        mov     ecx,O_I_NOT*256+5Ah
        cmp     al,TK_NOT
        jne     @cu_4
@cu_u:
        push    ecx
        mov     al,cl
        inc     esi
        call    cm_p
        pop     ecx
        cmp     ch,O_F_NEG
        je      @cu_nf
        call    cm_toi
        mov     al,O_I_NOT
@jt_expr2:
        jmp     cm_op
@cu_nf:
        test    [g_state.cm.cty],T_STR
        jnz     err_tm
        cmp     [g_state.cm.cty],T_FLT
        mov     al,O_F_NEG
        je      @jt_expr2
        mov     al,O_I_NEG              ; I.LD.I16 is unsigned: no -literal fold
        jmp     cm_op
@cu_4:
        cmp     al,TK_PLUS
        jne     @cu_5
        inc     esi
        jmp     cm_un
@cu_5:
        cmp     al,'"'
        je      cm_str
        cmp     al,'.'
        jb      @cu_7
        cmp     al,'9'
        ja      @cu_7
        call    flt_in
        mov     [g_state.cm.cty],T_FLT
        push    eax
        mov     al,O_F_LD_F32
        call    cm_op
        pop     eax
        jmp     cm_dw
@cu_7:
        sub     al,TK_SGN
        cmp     al,TK_MID-TK_SGN
        ja      @cu_8
        push    eax
        inc     esi
        call    cm_chkop
        call    cm_expr
        pop     eax
        cmp     al,TK_MID-TK_SGN-2
        jb      @cu_a1
        push    eax
        call    cm_chkcm
        call    cm_tos
        call    cm_expr
        call    cm_toa
        pop     eax
        cmp     al,TK_MID-TK_SGN
        jne     @cu_a2
        push    eax
        call    cm_optcm
        pop     eax
        mov     cl,K_MID2-K_FN
        jnz     @cu_a3
        push    eax
        call    cm_expr
        call    cm_toa
        pop     eax
@cu_a2:
        mov     cl,al
@cu_a3:
        push    ecx
        call    cm_chkcl
        pop     eax
        mov     [g_state.cm.cty],T_STR
        add     al,K_FN
        jmp     cm_call
@cu_a1:
        push    eax
        call    cm_chkcl
        pop     ecx
        call    cm_farg
        mov     al,cl
        add     al,K_FN
        jmp     cm_call
@cu_8:
        jmp     err_sn

; ::::::::::::::
; cm_farg - function CL's argument converted; cty its result.
;
; in:     CL   the function, TK_SGN based
;         cty  the argument's type
; keeps:  CL
; error:  ?TYPE MISMATCH  the wrong type
; ::::::::::::::
cm_farg:
        movzx   ebx,cl
        mov     al,[g_fnsig+ebx]
        push    ecx
        push    eax
        and     al,0Fh
        movzx   ebx,al
        add     ebx,ebx
        call    [g_fnarg+ebx*2]
        pop     eax
        pop     ecx
        and     al,0F0h
        cmp     al,0B0h                 ; T_STR's mark
        jne     @cfa_t
        mov     al,T_STR
@cfa_t:
        mov     [g_state.cm.cty],al
        ret

; ::::::::::::::
; cm_nany - POS's argument: a number.
;
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_nany:
        test    [g_state.cm.cty],T_STR
        jnz     err_tm
        ret

        .data
g_fnarg dd      cm_tof,cm_toa,cm_tos,cm_rts,cm_tob,cm_nany
        .code

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
        .data
g_fnsig db      FA_F+FR_I,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_Y+FR_F,FA_N+FR_I
        db      FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F,FA_F+FR_F
        db      FA_F+FR_F,FA_F+FR_F,FA_A+FR_I,FA_S+FR_I,FA_F+FR_S,FA_S+FR_F
        db      FA_S+FR_I,FA_B+FR_S
        .code

        .data
g_cprec db      79h,79h,7Bh,7Bh,7Fh,50h,46h,64h,64h,64h
        .code

        .data
g_cmact dd      cm_expr,cm_lval,cm_lnum,cm_tob,cm_toa,cm_tos,cm_str1,cm_fv
        dd      cm_fst,cm_fork,cm_for,cm_for1,cm_nv,cm_if,cm_on,cm_onl
        dd      cm_print,cm_def,cm_list,cm_dv,cm_dat,cm_rem,cm_store,cm_in
        dd      cm_rd,cm_get,cm_goto,cm_gosub,cm_run,cm_res,cm_ret,cm_n0
        dd      cm_n1,cm_narg,cm_fop,cm_swap
        .code

        end
