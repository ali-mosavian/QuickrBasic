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
        mov     ah,10                   ; \ MOD XOR EQV IMP: 10 .. 14
        cmp     al,'\'
        je      @cp_x
        sub     al,TK_MOD-11
        cmp     al,11
        jb      @cp_e
        cmp     al,14
        jbe     @cp_y
        add     al,TK_MOD-11
        sub     al,TK_PLUS
        cmp     al,TK_LT-TK_PLUS+1
        jae     @cp_e
        jmp     @cp_y
@cp_x:
        mov     al,ah
@cp_y:
        movzx   ebx,al
        mov     ebp,esp
        mov     cl,[g_cprec+ebx]
        cmp     byte ptr [ebp],cl
        jae     @cp_e
        cmp     bl,10
        jae     @cp_a
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
        test    dl,dl                   ; a relation: 80h + its mask
        js      @cp_kl
        cmp     dl,5
        jb      @cp_kl
        cmp     dl,7
        jb      @F
        cmp     dl,10
        jb      @cp_kl
@@:
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
        test    dl,dl
        js      @F
        cmp     dl,10
        jae     @cp_xo
@@:
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
@cp_xo:
        push    edx
        call    cm_toi
        pop     edx
        movzx   ebx,dl
        mov     al,[g_xop+ebx-10]
        call    cm_op
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
        call    st_get
        cmp     al,M_VAR
        jne     @clv_v
        call    rc_ref                  ; a record's field: its address
        jc      @clv_v
        mov     ax,[g_rfoff]
        mov     [g_state.cm.ctv],ax
        mov     [g_state.cm.ctn],0FFh
        mov     al,[g_rfet]
        mov     [g_ctfet],al
        cmp     al,ET_REC
        jae     err_tm
        mov     dx,[g_rffix]
        mov     [g_ctffix],dx
        movzx   eax,al
        mov     al,[g_etk+eax]
        mov     [g_state.cm.ctk],al
        ret
@clv_v:
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
        call    st_get
        cmp     al,M_VAR
        jne     @F
        call    rc_ref
        jnc     rc_load
@@:
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
        cmp     al,TK_LEN
        je      @cu_ln
        cmp     al,TK_X
        jne     @cu_7a
        mov     ah,[esi+1]
        mov     cl,K_VPTR
        cmp     ah,TK1_VARPTR
        je      @cu_vp
        mov     cl,K_VSEG
        cmp     ah,TK1_VARSEG
        je      @cu_vp
        mov     al,TK_X
@cu_7a:
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
        mov     al,[esi]
        jmp     cm_xfn
@cu_vp:                                 ; VARPTR(v), VARSEG(v): its address
        push    ecx
        add     esi,2
        call    cm_chkop
        call    cm_addr
        call    cm_chkcl
        pop     eax
        call    cm_call
        mov     [g_state.cm.cty],T_INT
        ret
@cu_ln:                                 ; LEN(r): a record's, a field's size
        push    esi
        push    edi
        inc     esi
        call    st_get
        cmp     al,'('
        jne     @cul_n
        inc     esi
        call    st_get
        cmp     al,M_VAR
        jne     @cul_n
        call    rc_ref
        jc      @cul_n
        call    st_get
        cmp     al,')'
        jne     @cul_n
        inc     esi
        pop     edi
        add     esp,4
        movzx   eax,[g_rfet]
        cmp     al,ET_REC
        jae     @cul_f
        cmp     al,ET_SD
        je      @cul_f
        movzx   eax,[g_etsz+eax]
        jmp     cm_lint
@cul_f:
        movzx   eax,[g_rffix]
        jmp     cm_lint
@cul_n:
        pop     edi
        pop     esi
        mov     al,[esi]
        jmp     @cu_7a

; ::::::::::::::
; cm_addr - a variable's linear address: a record's or a field's code, else
; its slot's, its Desc's.
;
; error:  ?SYNTAX ERROR  not a variable
; ::::::::::::::
cm_addr:
        call    st_get
        cmp     al,M_VAR
        jne     err_sn
        call    rc_ref
        jc      @cad_v
        mov     ax,[g_rfoff]
        test    ax,ax
        jz      @cad_r
        movzx   eax,ax
        call    cm_lint
        mov     al,O_I_ADD
        call    cm_op
@cad_r:
        ret
@cad_v:
        call    var_id
        lea     eax,[VARBASE+ebx*4]
        mov     dl,[g_sinfo+ebx]
        and     dl,3
        cmp     dl,2
        jne     @F
        lea     eax,[SVBASE+ebx*8]
@@:
        jmp     cm_lint

; ::::::::::::::
; cm_xfn - QBasic's other functions, by g_xfn: f(args), each converted to
; its kind; the count after them when it varies; then CALL K, or none.
;
; in:     SI  at the token
; error:  ?SYNTAX ERROR  not one, or too few arguments
; ::::::::::::::
cm_xfn:
        movzx   eax,al
        cmp     al,TK_X
        jne     @F
        movzx   eax,byte ptr [esi+1]
        add     eax,100h
        inc     esi
@@:
        inc     esi
        mov     ebx,offset g_xfn
@cxf_l:
        cmp     word ptr [ebx],0
        je      err_sn
        cmp     ax,[ebx]
        je      @cxf_f
        add     ebx,XF_SZ
        jmp     @cxf_l
@cxf_f:
        push    ebx
        call    cm_chkop
        xor     ecx,ecx                 ; arguments
        xor     edx,edx                 ; the kind's index
@cxf_a:
        push    ecx
        push    edx
        mov     ebx,[esp+8]
        mov     al,[ebx+XF_ARG+edx]
        cmp     al,'R'
        je      @cxf_r
        push    eax
        call    cm_expr
        pop     eax
        pop     edx
        pop     ecx
        push    ecx
        push    edx
        mov     ebx,[esp+8]
        jecxz   @F                      ; a leading optional left out
        jmp     @cxf_c
@@:
        test    byte ptr [ebx+XF_MIN],80h
        jz      @cxf_c
        cmp     [g_state.cm.cty],T_STR
        jne     @cxf_c
        cmp     al,'S'
        je      @cxf_c
        inc     dword ptr [esp]
        mov     edx,[esp]
        mov     al,[ebx+XF_ARG+edx]
@cxf_c:
        call    cm_xarg
@cxf_n:
        pop     edx
        pop     ecx
        inc     ecx
        inc     edx
        mov     ebx,[esp]
        cmp     dl,[ebx+XF_MAX]
        jae     @cxf_e
        push    ecx
        push    edx
        call    cm_optcm
        pop     edx
        pop     ecx
        jz      @cxf_a
@cxf_e:
        mov     al,[ebx+XF_MIN]
        and     al,7Fh
        cmp     dl,al
        jb      err_sn
        push    ecx
        call    cm_chkcl
        pop     ecx
        mov     ebx,[esp]
        mov     al,[ebx+XF_MIN]
        and     al,7Fh
        cmp     al,[ebx+XF_MAX]
        je      @F
        mov     eax,ecx
        call    cm_lint
@@:
        pop     ebx
        mov     al,[ebx+XF_K]
        test    al,al
        jz      @F
        push    ebx
        call    cm_call
        pop     ebx
@@:
        mov     al,[ebx+XF_RES]
        mov     [g_state.cm.cty],al
        xor     eax,eax
        mov     [g_state.cm.cll],eax
        mov     [g_state.cm.clle],eax
        ret
@cxf_r:                                 ; an array, n or n(): its id
        call    st_get
        cmp     al,M_VAR
        jne     err_sn
        mov     [g_vfs],2
        call    var_id
        mov     [g_vfs],0
        cmp     byte ptr [esi],'('
        jne     @F
        cmp     byte ptr [esi+1],')'
        jne     err_sn
        add     esi,2
@@:
        mov     eax,ebx
        call    cm_lint
        jmp     @cxf_n

; ::::::::::::::
; cm_xarg - the value as kind AL: I an INT, F a FLOAT, S a string, X a
; string or a number, then 1 or 0.
; ::::::::::::::
cm_xarg:
        cmp     al,'I'
        je      cm_toi
        cmp     al,'F'
        je      cm_tof
        cmp     al,'S'
        je      cm_tos
        test    [g_state.cm.cty],T_STR
        mov     eax,1
        jnz     cm_lint
        call    cm_toi
        xor     eax,eax
        jmp     cm_lint

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
XF_K    = 2                     ; g_xfn: token:2, K, least|80h leading
XF_MIN  = 3                     ; optional, most, result type, the kinds
XF_MAX  = 4
XF_RES  = 5
XF_ARG  = 6
XF_SZ   = 9
xfn     macro   t,k,mn,mx,r,a
        dw      t
        db      k,mn,mx,r
        db      a
        endm
g_xfn   label   byte
        xfn     100h+TK1_CINT,0,1,1,T_INT,<'I',0,0>
        xfn     100h+TK1_CLNG,0,1,1,T_INT,<'I',0,0>
        xfn     100h+TK1_CSNG,0,1,1,T_FLT,<'F',0,0>
        xfn     100h+TK1_CDBL,0,1,1,T_FLT,<'F',0,0>
        xfn     TK_FIX,K_FIX,1,1,T_FLT,<'F',0,0>
        xfn     TK_INSTR,K_INSTR,82h,3,T_INT,<'I','S','S'>
        xfn     TK_LBOUND,K_LBND,1,2,T_INT,<'R','I',0>
        xfn     TK_UBOUND,K_UBND,1,2,T_INT,<'R','I',0>
        xfn     100h+TK1_SPACES,K_SPACE,1,1,T_STR,<'I',0,0>
        xfn     100h+TK1_STRINGS,K_STRG,2,2,T_STR,<'I','X',0>
        xfn     100h+TK1_LTRIMS,K_LTRIM,1,1,T_STR,<'S',0,0>
        xfn     100h+TK1_RTRIMS,K_RTRIM,1,1,T_STR,<'S',0,0>
        xfn     100h+TK1_UCASES,K_UCASE,1,1,T_STR,<'S',0,0>
        xfn     100h+TK1_LCASES,K_LCASE,1,1,T_STR,<'S',0,0>
        xfn     100h+TK1_HEXS,K_HEX,1,1,T_STR,<'I',0,0>
        xfn     100h+TK1_OCTS,K_OCT,1,1,T_STR,<'I',0,0>
        dw      0
g_cprec db      78h,78h,7Bh,7Bh,7Fh,50h,46h,64h,64h,64h
        db      7Ah,79h,44h,42h,40h     ; \ MOD XOR EQV IMP
g_xop   db      O_I_DIV,O_I_MOD,O_I_XOR,O_I_EQV,O_I_IMP
        .code

        .data
g_cmact dd      cm_expr,cm_lval,cm_lnum,cm_tob,cm_toa,cm_tos,cm_str1,cm_fv
        dd      cm_fst,cm_fork,cm_for,cm_for1,cm_nv,cm_if,cm_on,cm_onl
        dd      cm_print,cm_def,cm_list,cm_dv,cm_dat,cm_rem,cm_store,cm_in
        dd      cm_rd,cm_get,cm_goto,cm_gosub,cm_run,cm_res,cm_ret,cm_n0
        dd      cm_n1,cm_narg,cm_fop,cm_swap
        .code

        end
