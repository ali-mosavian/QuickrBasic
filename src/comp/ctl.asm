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
        cmp     al,M_VAR
        je      ct_name
        cmp     al,TK_LET
        je      ct_let
        cmp     al,TK_TYPE
        je      ct_none
        cmp     al,TK_CALL
        je      ct_call
        cmp     al,TK_SUB
        je      ct_sub
        cmp     al,TK_FUNCTION
        je      ct_sub
        cmp     al,TK_DECLARE
        je      ct_none
        cmp     al,TK_OPTION
        je      ct_none
        cmp     al,TK_DEF
        je      ct_dseg
        cmp     al,TK_MID
        je      ct_mids
        cmp     al,TK_ON
        je      ct_onerr
        cmp     al,TK_REDIM
        je      ct_redim
        cmp     al,TK_ERASE
        je      ct_erase
        cmp     al,TK_X
        je      ct_xst
        cmp     al,TK_SHARED
        je      ct_none
        cmp     al,TK_STATIC
        je      ct_none
        cmp     al,TK_EXIT
        jne     @F
        mov     al,[esi+1]
        cmp     al,TK_SUB
        je      ct_pret
        cmp     al,TK_FUNCTION
        je      ct_pret
        cmp     al,TK_DEF
        je      ct_pret
        jmp     ct_exit
@@:
        cmp     al,TK_END
        jne     @cts_n
        mov     al,[esi+1]
        cmp     al,TK_IF
        je      @cts_2
        cmp     al,TK_SELECT
        je      @cts_2
        cmp     al,TK_TYPE
        je      @cts_2
        cmp     al,TK_SUB
        je      ct_pret
        cmp     al,TK_FUNCTION
        je      ct_pret
        cmp     al,TK_DEF
        je      ct_pret
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
; ct_none - DECLARE, SHARED, STATIC: entered with the line (dc_line), no
; code.
; ::::::::::::::
ct_none:
        call    ct_skip
        clc
        ret

; ::::::::::::::
; ct_dseg - DEF SEG [= s]: no code, flat. DEF FN: ct_def.
; ::::::::::::::
ct_dseg:
        cmp     byte ptr [esi+1],TK_X
        jne     @cdg_n
        cmp     byte ptr [esi+2],TK1_SEG
        jne     @cdg_n
        jmp     ct_none                 ; dc_line warned
@cdg_n:
        cmp     byte ptr [esi+1],M_VAR
        je      ct_def
        stc
        ret

; ::::::::::::::
; ct_def - DEF FNn [(params)]: run into, passed as a FUNCTION; with = e, JMP
; past it, then the body: the result e, PRET.
; ::::::::::::::
ct_def:
        inc     esi
        push    esi
        call    ct_proc
        pop     ebx
        jnc     @F
        mov     esi,ebx
        call    ct_pchk
        jc      err_sn
        jmp     ct_none
@@:
        push    ebx                     ; the name
        mov     eax,[edx+P_SKIP]
        call    ct_jmp
        call    ct_skip0                ; past ( params )
        pop     ebx
        call    st_get
        cmp     al,TK_EQ
        jne     ct_none
        push    esi
        mov     esi,ebx
        mov     [g_vfs],1               ; its result
        call    cm_name
        mov     [g_vfs],0
        call    cm_tgt
        pop     esi
        inc     esi
        call    cm_expr
        call    cm_store
        jmp     ct_pret0

; ct_skip0 - past a ( .. ) at SI, if any. Uses EAX, ECX.
ct_skip0:
        cmp     byte ptr [esi],'('
        jne     @czp_r
        xor     ecx,ecx
@@:
        mov     al,[esi]
        test    al,al
        jz      @czp_r
        cmp     al,'('
        jne     @czp_c
        inc     ecx
@czp_c:
        cmp     al,')'
        jne     @czp_n
        call    dc_item
        loop    @B
        ret
@czp_n:
        call    dc_item
        jmp     @B
@czp_r:
        ret

; ::::::::::::::
; ct_redim - REDIM [PRESERVE]: DIM's grammar, each array's CALL DIM marked
; REDIM's.
; ::::::::::::::
ct_redim:
        inc     esi
        mov     [g_state.cm.credim],40h
        cmp     word ptr [esi],TK_X or (TK1_PRESERVE shl 8)
        jne     @F
        add     esi,2
        mov     [g_state.cm.credim],140h
@@:
        mov     ax,[g_ps_ent+(TK_DIM-80h)*2]
        call    cm_dfa
        and     [g_state.cm.credim],0
        clc
        ret

; ::::::::::::::
; ct_erase - ERASE a {, a}: each array's id, CALL ERASE.
; ::::::::::::::
ct_erase:
        inc     esi
@cer_l:
        mov     [g_vfs],2
        call    cm_name
        mov     [g_vfs],0
        movzx   eax,word ptr [g_vnam+ebx*2]
        movzx   ecx,[g_narec+eax]       ; a record array: its own variable
        jecxz   @F
        call    rc_avar
@@:
        mov     eax,ebx
        call    cm_lint
        mov     al,K_ERASE
        call    cm_call
        call    st_get
        cmp     al,','
        jne     @F
        inc     esi
        jmp     @cer_l
@@:
        clc
        ret

; ::::::::::::::
; ct_onerr - ON [LOCAL] ERROR GOTO line: ONERR or ONLERR num addr; another ON
; is not one.
; ::::::::::::::
ct_onerr:
        push    esi
        inc     esi
        call    st_get
        cmp     al,TK_X
        jne     @coe_n
        call    ct_evk
        jnc     @coe_e
        mov     ah,O_ONERR
        cmp     byte ptr [esi+1],TK1_LOCAL      ; PDS's ON LOCAL ERROR
        jne     @F
        add     esi,2
        call    st_get
        cmp     al,TK_X
        jne     err_sn
        mov     ah,O_ONLERR
@@:
        cmp     byte ptr [esi+1],TK1_ERROR
        jne     @coe_n
        add     esp,4
        add     esi,2
        push    eax
        call    st_get
        cmp     al,TK_GOTO
        jne     err_sn
        inc     esi
        call    cm_lnum
        pop     eax
        mov     al,ah
        call    cm_op
        call    cm_jw
        clc
        ret
@coe_n:
        pop     esi
        stc
        ret
@coe_e:                                 ; ON event(n) GOSUB line: ONEV kind
        add     esp,4                   ; num addr ( n -- )
        push    ecx
        call    ct_evn
        call    st_get
        cmp     al,TK_GOSUB
        jne     err_sn
        inc     esi
        call    cm_lnum
        mov     al,O_ONEV
        call    cm_op
        pop     eax
        call    cm_byt
        call    cm_jw
        clc
        ret

; ::::::::::::::
; ct_evk - is the TK_X word at SI an event's: TIMER, KEY, COM, PEN, STRIG?
;
; out:    CF   clear: ECX its EK_*, SI past it
; keeps:  EAX
; ::::::::::::::
ct_evk:
        mov     cl,[esi+1]
        mov     ch,EK_TIMER
        cmp     cl,TK1_TIMER
        je      @cek_y
        mov     ch,EK_KEY
        cmp     cl,TK1_KEY
        je      @cek_y
        mov     ch,EK_COM
        cmp     cl,TK1_COM
        je      @cek_y
        mov     ch,EK_PEN
        cmp     cl,TK1_PEN
        je      @cek_y
        mov     ch,EK_STRIG
        cmp     cl,TK1_STRIG
        je      @cek_y
        stc
        ret
@cek_y:
        movzx   ecx,ch
        add     esi,2
        clc
        ret

; ::::::::::::::
; ct_evn - kind ECX's (n): KEY, COM, STRIG; TIMER's in ON TIMER; else 0.
;
; in:     ESP+4  the kind
; ::::::::::::::
ct_evn:
        mov     ecx,[esp+4]
        cmp     ecx,EK_PEN
        je      @cen_0
        call    st_get
        cmp     al,'('
        jne     @cen_t
        call    cm_paren
        jmp     cm_toi
@cen_t:
        cmp     ecx,EK_TIMER
        jne     err_sn
@cen_0:
        xor     eax,eax
        jmp     cm_lint

; ::::::::::::::
; ct_evs - event(n) ON | OFF | STOP: CALL EVST ( n kind*4+state -- ); KEY n,
; s: CALL KEYDEF; KEY ON, OFF, LIST: the screen's, nothing.
; ::::::::::::::
ct_evs:
        call    ct_evk
        jc      @coe_s
        cmp     ecx,EK_KEY
        jne     @ces_n
        call    st_get
        cmp     al,'('
        je      @ces_n
        cmp     al,TK_ON
        je      @ces_k
        cmp     al,TK_LIST
        je      @ces_k
        cmp     al,TK_X
        jne     @F
        cmp     byte ptr [esi+1],TK1_OFF
        jne     @F
        inc     esi
@ces_k:
        inc     esi
        clc
        ret
@@:
        call    cm_expr                 ; KEY n, s
        call    cm_toi
        call    cm_chkcm
        call    cm_expr
        call    cm_tos
        mov     al,K_KEYDEF
        call    cm_call
        clc
        ret
@ces_n:
        push    ecx
        call    ct_evn
        call    st_get
        xor     edx,edx                 ; OFF 0, ON 1, STOP 2
        cmp     al,TK_X
        jne     @F
        cmp     byte ptr [esi+1],TK1_OFF
        jne     err_sn
        inc     esi
        jmp     @ces_s
@@:
        inc     edx
        cmp     al,TK_ON
        je      @ces_s
        inc     edx
        cmp     al,TK_STOP
        jne     err_sn
@ces_s:
        inc     esi
        pop     eax
        lea     eax,[eax*4+edx]
        call    cm_lint
        mov     al,K_EVST
        call    cm_call
        clc
        ret

; ::::::::::::::
; ct_xst - RESUME [NEXT | 0 | line]: RESUME mode num addr.
; ::::::::::::::
ct_xst:
        mov     al,[esi+1]
        cmp     al,TK1_COMMON
        je      ct_common
        cmp     al,TK1_RESUME
        jne     ct_evs
        add     esi,2
        and     [g_state.cm.wv],0
        and     [g_state.cm.wa],0
        call    st_get
        xor     cl,cl
        cmp     al,TK_NEXT
        jne     @F
        inc     esi
        inc     cl
        jmp     @cxs_o
@@:
        call    st_eos
        jz      @cxs_o
        call    cm_lnum
        xor     cl,cl
        mov     ax,[g_state.cm.wv]
        or      ax,[g_state.cm.wa]
        jz      @cxs_o
        mov     cl,2
@cxs_o:
        mov     al,O_RESUME
        call    cm_opx
        call    cm_jw
        clc
        ret
@coe_s:
        stc
        ret

; ::::::::::::::
; ct_common - COMMON [SHARED] [/block/] v[()] [AS type] {, ..}: each
; variable's id, CALL CMN. decl.asm takes the types.
; ::::::::::::::
ct_common:
        add     esi,2
        call    st_get
        cmp     al,TK_SHARED
        jne     @F
        inc     esi
@@:
        call    st_get
        cmp     al,TK_DIV
        jne     @ccm_v
        inc     esi
        call    st_get
        cmp     al,M_VAR
        jne     err_sn
        call    cm_skvar
        call    st_get
        cmp     al,TK_DIV
        jne     err_sn
        inc     esi
@ccm_v:
        call    cm_name
        call    st_get
        cmp     al,'('
        jne     @F
        inc     esi
        call    st_get
        cmp     al,')'
        jne     err_sn
        inc     esi
@@:
        call    cm_skas
        mov     eax,ebx
        call    cm_lint
        mov     al,K_CMN
        call    cm_call
        call    st_get
        cmp     al,','
        jne     @F
        inc     esi
        jmp     @ccm_v
@@:
        clc
        ret

; ::::::::::::::
; cm_skvar - past the M_VAR at SI.
; ::::::::::::::
cm_skvar:
        push    ecx
        movzx   ecx,byte ptr [esi+1]
        lea     esi,[esi+ecx+4]
        pop     ecx
        ret

; ::::::::::::::
; cm_skas - past AS type [* n], if there.
; ::::::::::::::
cm_skas:
        call    st_get
        cmp     al,TK_AS
        jne     @csa_r
        inc     esi
        call    st_get
        cmp     al,M_VAR
        je      cm_skvar
        cmp     al,TK_X
        jne     @F
        add     esi,2
        ret
@@:
        inc     esi
        call    st_get
        cmp     al,TK_MUL
        jne     @csa_r
        inc     esi
        call    st_get
        cmp     al,M_INT
        jne     err_sn
        add     esi,5
@csa_r:
        ret

; ::::::::::::::
; ct_mids - MID$(v, i [, n]) = s: v = its value with s over it (sc_mids).
; ::::::::::::::
ct_mids:
        inc     esi
        call    cm_chkop
        push    esi
        call    cm_lval
        mov     al,[g_state.cm.ctk]
        and     al,3
        cmp     al,2
        jne     err_tm
        pop     eax
        push    dword ptr [g_state.cm.ctv]
        mov     esi,eax
        call    cm_load
        call    cm_chkcm
        call    cm_expr
        call    cm_toi
        call    cm_optcm
        jnz     @cms_n
        call    cm_expr
        call    cm_toi
        jmp     @cms_v
@cms_n:
        mov     eax,SMAX
        call    cm_lint
@cms_v:
        call    cm_chkcl
        call    cm_chkeq
        call    cm_expr
        call    cm_tos
        mov     al,K_MIDS
        call    cm_call
        pop     dword ptr [g_state.cm.ctv]
        mov     [g_state.cm.cty],T_STR
        call    cm_store
        clc
        ret

; ::::::::::::::
; ct_skip - SI to the statement's end: 0, ':' or '.
; ::::::::::::::
ct_skip:
        mov     al,[esi]
        test    al,al
        jz      @csp_r
        cmp     al,':'
        je      @csp_r
        cmp     al,TK_REM
        je      @csp_r
        call    dc_item
        jmp     ct_skip
@csp_r:
        ret

; ::::::::::::::
; ct_sub - SUB, FUNCTION: run into, it is passed: JMP past its END.
; ::::::::::::::
ct_sub:
        inc     esi
        call    ct_proc
        jnc     @F
        call    ct_pchk
        jc      err_sn
        jmp     ct_none
@@:
        mov     eax,[edx+P_SKIP]
        call    ct_jmp
        jmp     ct_none

; ::::::::::::::
; ct_pret - END SUB, EXIT SUB, END FUNCTION, EXIT FUNCTION: PRET.
; ::::::::::::::
ct_pret:
        add     esi,2
ct_pret0:
        mov     al,O_PRET
        call    cm_op
        clc
        ret

; ::::::::::::::
; ct_proc - the procedure the name at SI is.
;
; out:    EDX  its entry in g_prc
;         AL   its index
;         CF   set: none; SI kept
;         SI   past the name
; ::::::::::::::
ct_proc:
        cmp     byte ptr [esi],M_VAR
        jne     @cpr_n
        movzx   ecx,byte ptr [esi+1]
        movzx   eax,word ptr [esi+ecx+2]
        test    [g_nflag+eax],NM_SUB+NM_FUNC+NM_FN
        jz      @cpr_n
        movzx   edx,[g_nprc+eax]
        test    edx,edx
        jz      @cpr_n
        lea     esi,[esi+ecx+4]
        mov     eax,edx
        shl     edx,5
        add     edx,offset g_prc
        clc
        ret
@cpr_n:
        stc
        ret

; ::::::::::::::
; ct_call - CALL n [( args )].
;
; error:  ?SYNTAX ERROR  no SUB n
; ::::::::::::::
ct_call:
        inc     esi
        call    ct_proc
        jnc     @F
        call    ct_pchk
        jc      err_sn
        jmp     ct_none
@@:
        push    eax
        push    edx
        cmp     byte ptr [esi],'('
        jne     @F
        inc     esi
        call    ct_args
        call    cm_chkcl
@@:
        pop     edx
        pop     eax
        jmp     ct_pcall

; ::::::::::::::
; ct_name - a statement starting with a name: a TYPE's field, a record's
; copy, a SUB's call; else not one.
; ::::::::::::::
ct_name:
        movzx   ecx,byte ptr [esi+1]
        cmp     byte ptr [esi+ecx+4],TK_AS
        je      ct_none
        call    ct_rcopy
        jnc     @ctn_r
        jmp     ct_scall
@ctn_r:
        ret

; ::::::::::::::
; ct_let - LET: a record's copy, else not one.
; ::::::::::::::
ct_let:
        inc     esi
        cmp     byte ptr [esi],M_VAR
        jne     @ctl_n
        call    ct_rcopy
        jnc     @ctn_r
@ctl_n:
        dec     esi
        stc
        ret

; ::::::::::::::
; ct_rcopy - a = b of two records of one TYPE: their addresses, R.COPY.
;
; in:     SI  at a
; out:    CF  set: not one, SI and the code kept
; ::::::::::::::
ct_rcopy:
        push    esi
        push    edi
        call    rc_ref
        jc      @crc_n
        movzx   eax,[g_rfet]
        cmp     al,ET_REC
        jb      @crc_n
        push    eax
        call    st_get
        cmp     al,TK_EQ
        jne     @crc_m
        inc     esi
        call    st_get
        cmp     al,M_VAR
        jne     @crc_m
        call    rc_ref
        jc      @crc_m
        pop     eax
        cmp     al,[g_rfet]
        jne     @crc_n
        call    st_eos
        jnz     @crc_n
        add     esp,8
        mov     al,O_R_COPY
        mov     dx,[g_rffix]
        call    cm_opw
        clc
        ret
@crc_m:
        pop     eax
@crc_n:
        pop     edi
        pop     esi
        stc
        ret

; ::::::::::::::
; rc_ref - a record's reference at SI: r, r.f, a(i), a(i).f, r.g.f, each
; field's offset added. Its address's code: R.ADDR, or the subscripts and
; RA.ADDR. r is the longest prefix before a dot a record's name.
;
; in:     SI  at M_VAR
; out:    CF     set: not one, SI and the code kept
;         rfet   the field's ET_*, ET_REC + t a whole record of TYPE t
;         rfoff  its offset
;         rffix  STRING * n's n, a record's size
;         SI     past it
; error:  ?SYNTAX ERROR  no such field
; ::::::::::::::
rc_ref:
        movzx   ecx,byte ptr [esi+1]
        movzx   eax,word ptr [esi+ecx+2]
        lea     edx,[esi+ecx+4]         ; past the name
        cmp     byte ptr [edx],'('
        jne     @rrf_s
        call    rc_nreca                ; a(i)
        test    ecx,ecx
        jz      @rrf_n
        push    ecx
        mov     esi,edx
        call    rc_avar
        push    ebx
        call    cm_subs0
        pop     edx
        push    ecx
        mov     al,O_RA_ADDR
        call    cm_opw
        pop     eax
        call    cm_byt
        pop     ecx
        cmp     byte ptr [esi],'.'      ; .f
        jne     rc_whole
        cmp     byte ptr [esi+1],M_VAR
        jne     rc_whole
        inc     esi
        movzx   edx,byte ptr [esi+1]
        lea     ebx,[esi+2]
        call    rc_path
        call    dc_item
        clc
        ret
@rrf_s:
        call    rc_nrec                 ; r
        jecxz   @rrf_d
        mov     esi,edx
        call    rc_saddr
        jmp     rc_whole
@rrf_d:
        movzx   ecx,byte ptr [esi+1]    ; r.f: the longest prefix a record's
        lea     ebx,[esi+2]
@rrf_l:
        dec     ecx
        jle     @rrf_n
        cmp     byte ptr [ebx+ecx],'.'
        jne     @rrf_l
        mov     [g_state.lx.nptr],ebx
        mov     [g_state.lx.nlen],cl
        push    ebx
        push    ecx
        push    edx
        call    nam_find
        pop     edx
        pop     ecx
        pop     ebx
        push    ecx
        call    rc_nrec
        mov     [g_rfnt],ecx
        pop     ecx
        cmp     [g_rfnt],0
        je      @rrf_l
        push    edx
        push    esi
        push    ecx
        mov     ecx,[g_rfnt]
        call    rc_saddr
        pop     eax                     ; the dot
        pop     esi
        movzx   edx,byte ptr [esi+1]
        lea     ebx,[esi+eax+3]
        sub     edx,eax
        dec     edx
        call    rc_path
        pop     esi
        clc
        ret
@rrf_n:
        stc
        ret

; ::::::::::::::
; rc_nrec, rc_nreca - the TYPE of record name EAX, of its array; 0 none: a
; parameter's of the procedure compiled is its own (var_par), else as DIM
; declared the name.
;
; out:    ECX  the TYPE
; keeps:  all else
; ::::::::::::::
rc_nrec:
        push    ebx                     ; EBX: 0 a record, 1 its array
        xor     ebx,ebx
        jmp     @rnr_g
rc_nreca:
        push    ebx
        mov     ebx,1
@rnr_g:
        push    edx
        push    eax
        mov     dl,80h                  ; a record parameter, either kind
        call    var_par
        jc      @rnr_p
        test    ebx,ebx
        jnz     @rnr_d
        xor     dl,dl                   ; a parameter, not a record
        call    var_par
        mov     ecx,0
        jc      @rnr_r
@rnr_d:
        mov     eax,[esp]
        movzx   ecx,[g_nrec+eax]
        test    ebx,ebx
        jz      @rnr_r
        movzx   ecx,[g_narec+eax]
        jmp     @rnr_r
@rnr_p:
        movzx   ecx,[g_vtyp+eax]
        sub     ecx,ET_REC
        jae     @rnr_r
        xor     ecx,ecx
@rnr_r:
        pop     eax
        pop     edx
        pop     ebx
        ret

; ::::::::::::::
; rc_saddr - R.ADDR of record EAX of TYPE ECX.
;
; keeps:  ECX, ESI
; ::::::::::::::
rc_saddr:
        push    ecx
        call    rc_svar
        mov     ecx,ebx
        mov     al,O_R_ADDR
        call    cm_opid
        pop     ecx
        ret

; ::::::::::::::
; rc_whole - the reference is all of a record of TYPE ECX.
; ::::::::::::::
rc_whole:
        and     [g_rfoff],0
        lea     eax,[ecx+ET_REC]
        mov     [g_rfet],al
        mov     ax,word ptr [g_typ+ecx*8+T_SIZE]
        mov     [g_rffix],ax
        clc
        ret

; ::::::::::::::
; rc_path - the fields f.g.. of TYPE ECX: the text at EBX, EDX long.
;
; out:    rfet, rfoff, rffix
; error:  ?SYNTAX ERROR  no such field
; ::::::::::::::
rc_path:
        push    esi
        push    edi
        call    @rpa_0
        pop     edi
        pop     esi
        ret
@rpa_0:
        and     [g_rfoff],0
        mov     al,[ebx+edx-1]          ; a suffix is not the name's
        cmp     al,'.'
        jae     @rpa_l
        dec     edx
@rpa_l:
        xor     eax,eax                 ; the part to a '.'
@@:
        cmp     eax,edx
        jae     @F
        cmp     byte ptr [ebx+eax],'.'
        je      @F
        inc     eax
        jmp     @B
@@:
        push    eax
        push    ebx
        push    ecx
        push    edx
        mov     [g_state.lx.nptr],ebx
        mov     [g_state.lx.nlen],al
        call    nam_find
        pop     edx
        pop     ecx
        pop     ebx
        movzx   esi,word ptr [g_typ+ecx*8+T_FST]
        movzx   edi,word ptr [g_typ+ecx*8+T_NF]
@rpa_f:
        test    edi,edi
        jz      err_sn
        cmp     ax,word ptr [g_fld+esi*8+F_NAM]
        je      @rpa_y
        inc     esi
        dec     edi
        jmp     @rpa_f
@rpa_y:
        lea     esi,[g_fld+esi*8]
        mov     ax,[esi+F_OFF]
        add     [g_rfoff],ax
        pop     eax
        movzx   ecx,byte ptr [esi+F_SUB]
        add     ebx,eax
        sub     edx,eax
        jz      @rpa_e
        test    ecx,ecx                 ; .g: a field of a TYPE's
        jz      err_sn
        inc     ebx
        dec     edx
        jmp     @rpa_l
@rpa_e:
        test    ecx,ecx
        jz      @F
        mov     ax,[g_rfoff]            ; a record in it
        push    eax
        call    rc_whole
        pop     eax
        mov     [g_rfoff],ax
        ret
@@:
        mov     al,[esi+F_ET]
        mov     [g_rfet],al
        mov     ax,[esi+F_FIX]
        mov     [g_rffix],ax
        ret

; ::::::::::::::
; rc_avar, rc_svar - the variable of record array, record EAX of TYPE ECX.
;
; out:    EBX  it
; keeps:  ESI, EDI
; ::::::::::::::
rc_avar:
        mov     dl,80h
        jmp     @rcv_g
rc_svar:
        mov     dl,81h
@rcv_g:
        call    var_scope
        add     cl,ET_REC
        call    var_find
        mov     ebx,eax
        ret

; ::::::::::::::
; rc_load - the field's value rc_ref found: FL.LD off et, SF.LD off n.
;
; out:    cty
; error:  ?TYPE MISMATCH  a whole record
; ::::::::::::::
rc_load:
        movzx   ebx,[g_rfet]
        cmp     bl,ET_REC
        jae     err_tm
        mov     dx,[g_rfoff]
        cmp     bl,ET_SD
        je      @rld_s
        push    ebx
        mov     al,O_FL_LD
        call    cm_opw
        pop     ebx
        mov     al,bl
        call    cm_byt
        cmp     bl,ET_SH
        jne     @F
        mov     [g_csh],1
        mov     [g_cshe],edi
@@:
        mov     al,[g_etk+ebx]
        movzx   eax,al
        mov     al,[g_ctys+eax]
        jmp     @rld_t
@rld_s:
        mov     al,O_SF_LD
        call    cm_opw
        mov     ax,[g_rffix]
        call    @rld_w
        mov     al,T_STR
@rld_t:
        mov     [g_state.cm.cty],al
        xor     eax,eax
        mov     [g_state.cm.cll],eax
        mov     [g_state.cm.clle],eax
        ret
@rld_w:                                 ; the word AX
        push    eax
        call    cm_byt
        pop     eax
        mov     al,ah
        jmp     cm_byt

; ::::::::::::::
; rc_store - store to the field cm_lval found: ctv its offset, ctfet,
; ctffix; FL.ST off et, SF.ST off n.
; ::::::::::::::
rc_store:
        movzx   ebx,[g_ctfet]
        cmp     bl,ET_SD
        je      @rst_s
        cmp     bl,ET_R8
        jne     @F
        call    cm_tod
        jmp     @rst_o
@@:
        cmp     [g_etk+ebx],0
        jne     @rst_i
        call    cm_tof
        jmp     @rst_o
@rst_i:
        call    cm_toi
@rst_o:
        mov     al,O_FL_ST
        mov     dx,[g_state.cm.ctv]
        call    cm_opw
        mov     al,[g_ctfet]
        jmp     cm_byt
@rst_s:
        call    cm_tos
        mov     al,O_SF_ST
        mov     dx,[g_state.cm.ctv]
        call    cm_opw
        mov     ax,[g_ctffix]
        jmp     @rld_w

; ::::::::::::::
; ct_scall - n args: a SUB's call without CALL; another name is not one.
; ::::::::::::::
ct_scall:
        push    esi
        call    ct_proc
        jc      @css_n
        cmp     byte ptr [edx+P_KIND],PK_SUB
        jne     @css_n
        add     esp,4
        push    eax
        push    edx
        call    st_eos
        jz      @F
        call    ct_args
@@:
        pop     edx
        pop     eax
        jmp     ct_pcall
@css_n:
        pop     esi
        call    ct_pchk
        jnc     ct_none
        ret

; ::::::::::::::
; ct_pchk - in the IDE's check of a line alone, a SUB or FUNCTION its
; declaration named passes: g_prc has it only once the program is linked.
;
; in:     SI  at the name
; out:    CF  clear: passes
; ::::::::::::::
ct_pchk:
        cmp     [g_state.cm.cchk],0
        je      @cpk_n
        cmp     byte ptr [esi],M_VAR
        jne     @cpk_n
        movzx   ecx,byte ptr [esi+1]
        movzx   eax,word ptr [esi+ecx+2]
        test    [g_nflag+eax],NM_SUB+NM_FUNC+NM_FN
        jz      @cpk_n
        clc
        ret
@cpk_n:
        stc
        ret

; ::::::::::::::
; ct_fcall - a FUNCTION's call in an expression: n [( args )].
;
; in:     SI   at the name
; out:    cty  its result's type
; ::::::::::::::
ct_fcall:
        call    ct_proc
        push    eax
        push    edx
        cmp     byte ptr [esi],'('
        jne     @F
        inc     esi
        call    ct_args
        call    cm_chkcl
@@:
        pop     edx
        pop     eax
        push    edx
        call    ct_pcall
        pop     edx
        movzx   eax,word ptr [edx+P_RES]
        movzx   eax,[g_sinfo+eax]
        and     eax,7
        mov     al,[g_ctys+eax]
        mov     [g_state.cm.cty],al
        xor     eax,eax
        mov     [g_state.cm.cll],eax
        mov     [g_state.cm.clle],eax
        ret

; ::::::::::::::
; ct_pcall - PCALL AL: the procedure EDX's call.
; ::::::::::::::
ct_pcall:
        push    edx
        push    eax
        mov     al,O_PCALL
        call    cm_op
        pop     eax
        call    cm_byt
        pop     edx
        mov     eax,[edx+P_ENTRY]
        call    cm_dw
        xor     al,al
        call    cm_byt
        call    cm_byt
        clc
        ret

; ::::::::::::::
; ct_args - the arguments of procedure EDX: for each parameter its value
; and its ref (op_pcall).
;
; error:  ?SYNTAX ERROR  too few
; ::::::::::::::
ct_args:
        mov     al,[edx+P_FLAG]         ; a DEF FN's: by value
        and     al,PF_DEF
        mov     [g_cbyv],al
        movzx   ecx,word ptr [edx+P_NPAR]
        jecxz   @cag_r
        movzx   ebx,word ptr [edx+P_PAR]
        lea     ebx,[g_ppar+ebx*2]
@cag_l:
        push    ecx
        push    ebx
        movzx   eax,word ptr [ebx]
        call    ct_arg
        pop     ebx
        pop     ecx
        add     ebx,2
        dec     ecx
        jz      @cag_r
        call    st_get
        cmp     al,','
        jne     err_sn
        inc     esi
        jmp     @cag_l
@cag_r:
        ret

; ::::::::::::::
; ct_arg - an argument to parameter EAX: a variable of its type alone is
; passed by reference, n() an array by its id, else the value.
; ::::::::::::::
ct_arg:
        movzx   edx,[g_sinfo+eax]
        test    dl,dl
        js      @car_a
        push    edx                     ; [ESP] its sinfo, kept across calls
        cmp     [g_cbyv],0
        jne     @car_e
        call    st_get
        cmp     al,M_VAR
        jne     @car_e
        movzx   ecx,byte ptr [esi+1]
        movzx   eax,word ptr [esi+ecx+2]
        test    [g_nflag+eax],NM_SUB+NM_FUNC+NM_FN
        jnz     @car_e
        mov     al,[esi+ecx+4]
        test    al,al
        jz      @car_v
        cmp     al,','
        je      @car_v
        cmp     al,')'
        je      @car_v
        cmp     al,':'
        je      @car_v
        cmp     al,TK_REM
        jne     @car_e
@car_v:
        push    esi
        call    var_id
        pop     esi
        mov     al,[g_sinfo+ebx]
        mov     ah,[esp]
        and     ax,0707h
        cmp     al,ah
        jne     @car_e
        pop     edx
        push    ebx
        call    cm_load
        pop     eax
        jmp     cm_lint
@car_e:
        call    cm_expr
        pop     eax
        test    al,SK_DBL
        jz      @F
        call    cm_tod
        jmp     @car_n
@@:
        and     al,3
        cmp     al,1
        je      @car_i
        ja      @car_s
        call    cm_tof
        jmp     @car_n
@car_i:
        call    cm_toi
        jmp     @car_n
@car_s:
        call    cm_tos
@car_n:
        mov     eax,0FFFFh
        jmp     cm_lint
@car_a:                                 ; an array n(), a record array's, or
        call    st_get                  ; a record n, by its id
        cmp     al,M_VAR
        jne     err_sn
        movzx   ecx,byte ptr [esi+1]
        movzx   eax,word ptr [esi+ecx+2]
        cmp     byte ptr [esi+ecx+4],'('
        jne     @car_r
        movzx   ebx,[g_narec+eax]
        test    ebx,ebx
        jnz     @F
        call    var_id
        jmp     @car_k
@@:
        lea     esi,[esi+ecx+4]
        mov     ecx,ebx
        call    rc_avar
@car_k:
        test    [g_sinfo+ebx],80h
        jz      err_sn
        push    ebx
        call    cm_chkop
        call    cm_chkcl
        pop     eax
@car_o:
        call    cm_lint
        mov     eax,0FFFFh
        jmp     cm_lint
@car_r:
        movzx   ebx,[g_nrec+eax]
        test    ebx,ebx
        jz      err_sn
        lea     esi,[esi+ecx+4]
        mov     ecx,ebx
        call    rc_svar
        mov     eax,ebx
        jmp     @car_o

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
; ct_here - the target in this line at offset CX, mode AL.
;
; out:    EAX  the target
; ::::::::::::::
ct_here:
        movzx   eax,al
        shl     eax,TAGSH
        or      ax,cx
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
        push    eax
        push    ecx
        call    cm_dnz
        pop     ecx
        pop     eax
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
        cmp     [g_state.cm.cty],T_DBL
        jne     @F
        call    cm_tof
@@:
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
        mov     dl,SK_DBL
        mov     cl,ET_R8
        cmp     [g_cttp],T_DBL
        je      @F
        xor     dl,dl
        mov     cl,ET_R4
@@:
        mov     [g_state.cm.cscope],0
        mov     [g_state.cm.cstat],0
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
        mov     al,O_D_LD_V
        cmp     [g_cttp],T_DBL
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
        cmp     [g_cttp],T_DBL
        jne     @F
        mov     al,O_D_ST_V             ; only a store: SWAP's
        ret
@@:
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
g_cbyv  db      ?                       ; ct_args: every argument by value
g_cttp  db      ?                       ; its type
g_rfnt  dd      ?                       ; rc_ref's: a prefix's TYPE
g_rfet  db      ?                       ; rc_ref's field: ET_*, its offset, n
g_rfoff dw      ?
g_rffix dw      ?
g_ctfet db      ?                       ; cm_lval's field: ET_*, n
g_ctffix dw     ?
        .code

        end
