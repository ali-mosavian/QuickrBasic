; ==========================================================================
; ctl.asm - the block statements compiled: IF blocks, SELECT CASE, DO,
; WHILE, EXIT
; ==========================================================================
; holds:    ct_stmt, ct_bif, ct_else, ct_fskip; cm_swap
; uses:     bk_find, cm_expr, cm_op, cm_dw, cm_icmp, cm_fcmp, cm_lflt,
;           cm_tof, nam_find, var_find
; data:     Each statement finds its entry in g_blk by its position and
;           jumps to the targets cm_blocks gave it:
;             IF c THEN         c, JF NXT
;             ELSEIF c THEN     JMP END, c, JF NXT
;             ELSE              JMP END
;             SELECT CASE e     e to its value's variable, JMP NXT
;             CASE tests        JMP END, the tests or'd, JF NXT
;             DO [WHILE|UNTIL c]    c, JF|JT END
;             LOOP [WHILE|UNTIL c]  JMP NXT, or c, JT|JF NXT
;             WHILE c, WEND     c, JF END; JMP NXT
;             EXIT FOR, EXIT DO FPOP, JMP END; JMP END
;           A direct line has no entries: its targets are FFFFh, an error
;           when run. A SELECT's value is the variable #n, n its depth.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; ct_stmt - a block statement at SI, compiled.
;
; in:     SI  at its keyword
; out:    CF  set: not one
;         SI  after it
; ::::::::::::::
ct_stmt:
        mov     al,[esi]
        cmp     al,TK_ELSEIF
        je      ct_elseif
        cmp     al,TK_ELSE
        je      ct_belse
        cmp     al,TK_ENDIF
        je      @cts_1
        cmp     al,TK_SELECT
        je      ct_select
        cmp     al,TK_CASE
        je      ct_case
        cmp     al,TK_DO
        je      ct_do
        cmp     al,TK_LOOP
        je      ct_loop
        cmp     al,TK_WHILE
        je      ct_while
        cmp     al,TK_WEND
        je      ct_wend
        cmp     al,TK_EXIT
        je      ct_exit
        cmp     al,TK_END
        jne     @cts_n
        mov     al,[esi+1]
        cmp     al,TK_IF
        je      @cts_2
        cmp     al,TK_SELECT
        je      @cts_2
@cts_n:
        stc
        ret
@cts_2:
        inc     esi
@cts_1:
        inc     esi                     ; END IF, END SELECT: no code
        clc
        ret

; ::::::::::::::
; ct_bif - a block IF's test: JF to its next clause.
;
; in:     SI     at THEN
;         ctks   the IF
;         cty    the test's type
; out:    SI     past THEN
; ::::::::::::::
ct_bif:
        push    esi
        mov     esi,[g_state.cm.ctks]
        call    ct_ent
        pop     esi
        inc     esi
        mov     ebx,B_NXT
        call    ct_tgt
        jmp     ct_jf

; ::::::::::::::
; ct_else - a one-line IF's ELSE: the one after SI not taken by an inner IF.
;
; in:     SI   after the test, at THEN or GOTO
; out:    EAX  the target: the statement after the ELSE
;         CF   set: there is none
; keeps:  ESI
; ::::::::::::::
ct_else:
        push    esi
        xor     ecx,ecx
@cte_l:
        mov     al,[esi]
        test    al,al
        jz      @cte_n
        cmp     al,TK_REM
        je      @cte_n
        cmp     al,TK_IF
        jne     @F
        inc     ecx
@@:
        cmp     al,TK_ELSE
        jne     @cte_i
        jecxz   @cte_y
        dec     ecx
@cte_i:
        call    dc_item
        jmp     @cte_l
@cte_n:
        pop     esi
        stc
        ret
@cte_y:
        inc     esi
        sub     esi,[g_state.cm.cbase]
        mov     ecx,esi
        mov     al,0
        call    ct_here
        pop     esi
        clc
        ret

; ::::::::::::::
; ct_here - the target in this line at offset CL, mode AL.
;
; out:    EAX  the target
; ::::::::::::::
ct_here:
        movzx   eax,al
        shl     eax,8
        mov     al,cl
        shl     eax,16
        mov     ax,[g_state.cm.cline]
        ret

; ::::::::::::::
; ct_ent - the entry of the statement at SI.
;
; out:    EDX  it, 0 in the direct line
; keeps:  all but EDX
; error:  ?SYNTAX ERROR  none
; ::::::::::::::
ct_ent:
        xor     edx,edx
        cmp     [g_state.cm.cline],DLINE
        je      @cten_r
        push    eax
        push    ebx
        push    ecx
        mov     bx,[g_state.cm.cline]
        mov     ecx,esi
        sub     ecx,[g_state.cm.cbase]
        call    bk_find
        jc      err_sn
        shl     eax,4
        lea     edx,[g_blk+eax]
        pop     ecx
        pop     ebx
        pop     eax
@cten_r:
        ret

; ::::::::::::::
; ct_tgt - the entry's target at field BX.
;
; in:     EDX  the entry, 0 none
; out:    EAX  the target; FFFFh as its line, none
; ::::::::::::::
ct_tgt:
        or      eax,-1
        test    edx,edx
        jz      @ctt_r
        mov     eax,[edx+ebx]
@ctt_r:
        ret

; ::::::::::::::
; ct_jf - JF target EAX, by the test's type; ct_jt JT.
;
; in:     cty  the test's type
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
ct_jf:
        mov     cl,O_I_JF
        jmp     @ctj_t
ct_jt:
        mov     cl,O_I_JT
@ctj_t:
        test    [g_state.cm.cty],T_STR
        jnz     err_tm
        cmp     [g_state.cm.cty],T_FLT
        jne     @F
        inc     cl                      ; F.JF, F.JT
@@:
        xchg    eax,ecx
        jmp     @ctj_o
; ::::::::::::::
; ct_jmp - JMP target EAX.
; ::::::::::::::
ct_jmp:
        mov     ecx,eax
        mov     al,O_JMP
@ctj_o:
        push    ecx
        call    cm_op
        pop     eax
        call    cm_dw
        xor     al,al
        call    cm_byt
        call    cm_byt
        clc
        ret

; ::::::::::::::
; ct_fskip - after FOR's op: FSKIP past its NEXT, a loop not entered.
;
; in:     ctks  the FOR
; ::::::::::::::
ct_fskip:
        push    esi
        mov     esi,[g_state.cm.ctks]
        call    ct_ent
        pop     esi
        mov     ebx,B_END
        call    ct_tgt
        mov     ecx,eax
        mov     al,O_FSKIP
        jmp     @ctj_o

; ::::::::::::::
; ct_cond - the test after the keyword at SI.
; ::::::::::::::
ct_cond:
        inc     esi
        jmp     cm_expr

; ::::::::::::::
; ct_elseif - ELSEIF c THEN.
; ::::::::::::::
ct_elseif:
        call    ct_ent
        push    edx
        mov     ebx,B_END
        call    ct_tgt
        call    ct_jmp
        call    ct_cond
        pop     edx
        cmp     byte ptr [esi],TK_THEN
        jne     err_sn
        inc     esi
        mov     ebx,B_NXT
        call    ct_tgt
        jmp     ct_jf

; ::::::::::::::
; ct_belse - a block's ELSE.
; ::::::::::::::
ct_belse:
        call    ct_ent
        inc     esi
        mov     ebx,B_END
        call    ct_tgt
        jmp     ct_jmp

; ::::::::::::::
; ct_do - DO [WHILE c | UNTIL c].
; ::::::::::::::
ct_do:
        call    ct_ent
        inc     esi
        mov     ebx,B_END
        mov     al,[esi]
        cmp     al,TK_WHILE
        je      @ctd_w
        cmp     al,TK_UNTIL
        je      @ctd_u
        clc
        ret
@ctd_w:
        push    ebx
        push    edx
        call    ct_cond
        pop     edx
        pop     ebx
        call    ct_tgt
        jmp     ct_jf
@ctd_u:
        push    ebx
        push    edx
        call    ct_cond
        pop     edx
        pop     ebx
        call    ct_tgt
        jmp     ct_jt

; ::::::::::::::
; ct_loop - LOOP [WHILE c | UNTIL c].
; ::::::::::::::
ct_loop:
        call    ct_ent
        inc     esi
        mov     ebx,B_NXT
        mov     al,[esi]
        cmp     al,TK_WHILE
        je      @ctd_u                  ; while c: back when true
        cmp     al,TK_UNTIL
        je      @ctd_w                  ; until c: back when false
        call    ct_tgt
        jmp     ct_jmp

; ::::::::::::::
; ct_while - WHILE c.
; ::::::::::::::
ct_while:
        call    ct_ent
        mov     ebx,B_END
        jmp     @ctd_w

; ::::::::::::::
; ct_wend - WEND.
; ::::::::::::::
ct_wend:
        call    ct_ent
        inc     esi
        mov     ebx,B_NXT
        call    ct_tgt
        jmp     ct_jmp

; ::::::::::::::
; ct_exit - EXIT FOR, EXIT DO.
;
; error:  ?SYNTAX ERROR  another EXIT
; ::::::::::::::
ct_exit:
        call    ct_ent
        mov     al,[esi+1]
        add     esi,2
        cmp     al,TK_DO
        je      @cex_j
        cmp     al,TK_FOR
        jne     err_sn
        mov     al,O_FPOP
        call    cm_op
@cex_j:
        mov     ebx,B_END
        call    ct_tgt
        jmp     ct_jmp

; ::::::::::::::
; ct_select - SELECT CASE e: e to its variable; on to the first CASE.
; ::::::::::::::
ct_select:
        call    ct_ent
        push    edx
        inc     esi
        cmp     byte ptr [esi],TK_CASE
        jne     err_sn
        inc     esi
        call    cm_expr
        pop     edx
        xor     eax,eax                 ; the depth
        test    edx,edx
        jz      @F
        mov     al,[edx+B_AUX]
        mov     cl,[g_state.cm.cty]
        or      cl,1                    ; compiled: T_INT is 0
        mov     [edx+B_AUX+1],cl
@@:
        mov     cl,[g_state.cm.cty]
        push    edx
        call    ct_var
        mov     al,O_I_ST_V
        cmp     [g_state.cm.cty],T_INT
        je      @F
        mov     al,O_F_ST_V
        cmp     [g_state.cm.cty],T_FLT
        je      @F
        mov     al,O_S_ST_V
@@:
        call    cm_opid
        pop     edx
        mov     ebx,B_NXT
        call    ct_tgt
        jmp     ct_jmp

; ::::::::::::::
; ct_var - the variable #n of a SELECT: depth AL, type CL (T_*).
;
; out:    CX  the variable
;         ctv, ctk  it, its type
; ::::::::::::::
ct_var:
        push    esi
        push    edi
        add     al,'0'
        mov     [g_ctnm+1],al
        mov     [g_state.lx.nptr],offset g_ctnm
        mov     [g_state.lx.nlen],2
        push    ecx
        call    nam_find
        pop     ecx
        mov     [g_cttp],cl
        mov     dl,1
        mov     cl,ET_I4
        cmp     [g_cttp],T_INT
        je      @F
        mov     dl,2
        mov     cl,ET_SD
        test    [g_cttp],T_STR
        jnz     @F
        xor     dl,dl
        mov     cl,ET_R4
@@:
        call    var_find
        mov     [g_ctvr],ax
        mov     ecx,eax
        pop     edi
        pop     esi
        ret

; ::::::::::::::
; ct_case - CASE tests | CASE ELSE.
;
; error:  ?SYNTAX ERROR  its SELECT not compiled
; ::::::::::::::
ct_case:
        call    ct_ent
        push    edx
        mov     ebx,B_END
        call    ct_tgt
        call    ct_jmp
        inc     esi
        pop     edx
        cmp     byte ptr [esi],TK_ELSE
        jne     @F
        inc     esi
        clc
        ret
@@:
        push    edx
        xor     eax,eax                 ; the SELECT: its depth, its type
        mov     cl,T_FLT
        test    edx,edx
        jz      @ctc_v
        movzx   eax,word ptr [edx+B_AUX]
        shl     eax,4
        mov     cx,word ptr [g_blk+eax+B_AUX]
        mov     al,cl
        mov     cl,ch
        test    cl,cl
        jz      err_sn
        and     cl,NOT 1
@ctc_v:
        call    ct_var
        call    ct_item
@ctc_l:
        cmp     byte ptr [esi],','
        jne     @ctc_e
        inc     esi
        call    ct_item
        mov     al,O_I_OR
        call    cm_op
        jmp     @ctc_l
@ctc_e:
        pop     edx
        mov     [g_state.cm.cty],T_INT
        mov     ebx,B_NXT
        call    ct_tgt
        jmp     ct_jf

; ::::::::::::::
; ct_item - a CASE test: IS rel e, e TO e, e; an INT.
; ::::::::::::::
ct_item:
        cmp     byte ptr [esi],TK_IS
        jne     @cti_e
        inc     esi
        xor     edx,edx
@cti_r:
        movzx   eax,byte ptr [esi]
        sub     al,TK_GT
        cmp     al,3
        jae     @cti_c
        or      dl,[g_relbit+eax]
        inc     esi
        jmp     @cti_r
@cti_c:
        test    dl,dl
        jz      err_sn
        jmp     ct_cmp
@cti_e:
        push    esi                     ; e TO e: a TO before ',' or the end
@cti_t:
        mov     al,[esi]
        test    al,al
        jz      @cti_1
        cmp     al,':'
        je      @cti_1
        cmp     al,','
        je      @cti_1
        cmp     al,TK_TO
        je      @cti_2
        call    dc_item
        jmp     @cti_t
@cti_1:
        pop     esi
        mov     dl,2                    ; =
        jmp     ct_cmp
@cti_2:
        pop     esi
        mov     dl,3                    ; >=
        call    ct_cmp
        cmp     byte ptr [esi],TK_TO
        jne     err_sn
        inc     esi
        mov     dl,6                    ; <=
        call    ct_cmp
        mov     al,O_I_AND
        jmp     cm_op

; ::::::::::::::
; ct_cmp - the SELECT's value, relation DL, the expression at SI: an INT.
;
; in:     DL     the relation's mask: > 1, = 2, < 4
;         ctvr, cttp  the variable, its type
; out:    cty    T_INT
; error:  ?TYPE MISMATCH  a string against a number
; ::::::::::::::
ct_cmp:
        push    edx
        push    edi                     ; the left: the variable's load
        call    ct_ldop
        call    cm_opid
        push    edi
        call    cm_expr
        pop     ebx                     ; the right
        pop     ecx
        pop     edx
        mov     al,[g_state.cm.cty]
        test    [g_cttp],T_STR
        jnz     @ctm_s
        test    al,T_STR
        jnz     err_tm
        or      dl,80h
        cmp     [g_cttp],T_INT
        jne     @ctm_f
        cmp     al,T_INT
        je      cm_icmp
        mov     ah,1                    ; the INT load: a FLOAT's
        call    cm_lflt
@ctm_f:
        push    edx
        push    ebx
        call    cm_tof
        pop     ebx
        pop     edx
        jmp     cm_fcmp
@ctm_s:
        test    al,T_STR
        jz      err_tm
        and     dl,7
        mov     cl,dl
        mov     al,O_S_CMP
        call    cm_opx
        mov     [g_state.cm.cty],T_INT
        ret

; ::::::::::::::
; cm_swap - SWAP a, b: a to the variable #S, b to a, #S to b.
;
; in:     SI  at a
; error:  ?TYPE MISMATCH  a and b of two types
; ::::::::::::::
cm_swap:
        push    esi                     ; a
        call    cm_load
        mov     cl,[g_state.cm.cty]
        mov     al,'S'-'0'
        call    ct_var
        call    ct_stv
        call    st_get
        cmp     al,','
        jne     err_sn
        inc     esi
        mov     edx,esi                 ; b
        pop     esi
        push    edx
        call    cm_lval                 ; a = b
        pop     esi
        push    esi
        push    dword ptr [g_state.cm.ctv]
        call    cm_load
        mov     al,[g_state.cm.cty]
        cmp     al,[g_cttp]
        jne     err_tm
        pop     dword ptr [g_state.cm.ctv]
        call    cm_store
        pop     edx
        push    esi                     ; b's end
        mov     esi,edx
        call    cm_lval                 ; b = #S
        call    ct_ldop
        call    cm_opid
        mov     al,[g_cttp]
        mov     [g_state.cm.cty],al
        call    cm_store
        pop     esi
        ret

; ::::::::::::::
; ct_stv - store to the variable ctvr, of type cttp.
; ::::::::::::::
ct_stv:
        mov     al,O_I_ST_V
        call    ct_vop
        jmp     cm_opid

; ::::::::::::::
; ct_ldop - the load of the variable ctvr, of type cttp.
;
; out:    AL  the op
;         CX  ctvr
; ::::::::::::::
ct_ldop:
        mov     cx,[g_ctvr]
        mov     al,O_I_LD_V
        cmp     [g_cttp],T_INT
        je      @ctl_r
        mov     al,O_F_LD_V
        cmp     [g_cttp],T_FLT
        je      @ctl_r
        mov     al,O_S_LD_V
@ctl_r:
        ret

; ::::::::::::::
; ct_vop - AL, an I. op of a variable, as the F. or S. one of type cttp.
;
; out:    AL  the op
;         CX  ctvr
; ::::::::::::::
ct_vop:
        mov     cx,[g_ctvr]
        cmp     [g_cttp],T_INT
        je      @ctv_r
        inc     al
        cmp     [g_cttp],T_FLT
        je      @ctv_r
        inc     al
@ctv_r:
        ret

        .data
g_ctnm  db      "#0"                    ; a SELECT's value: its name
        .data?
g_ctvr  dw      ?                       ; that variable
g_cttp  db      ?                       ; its type
        .code

        end
