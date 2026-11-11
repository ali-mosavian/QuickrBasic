; ==========================================================================
; comp.asm - the compiler: a token line to stack-machine code
; ==========================================================================
; holds:    cm_line, cm_text, cm_code, cm_err; the statements and their
;           scripts cms; the emitters cm_op ..; the token readers st_get ..
; uses:     cm_expr, sym_find, str_span, op_size, err_*
; data:     SI the token cursor, DI the code's next byte.
;           cms, a statement's script: each byte, bit 7 the script's last;
;           under A_BASE CALL that routine, else action A_*
; calls:
;   cm_line            csp = SP
;     cm_new           the statement; ':' again, 0 cm_fin
;       cm_tok, cm_scr
;     cm_fin           EOL; SP = csp; ret to the compile's caller
;   cm_err             err_raise comes here while csp is set: ERR, EOL
; ==========================================================================

LAST    = 80h
A_BASE  = 64
A_E     = A_BASE
A_X     = A_BASE+1
A_C     = A_BASE+2
A_O     = A_BASE+3
A_FC    = A_BASE+4
A_ES    = A_BASE+5
A_M     = A_BASE+6
A_LM    = A_BASE+7
A_LOOP  = A_BASE+8
A_LV    = A_BASE+9
A_L     = A_BASE+10
A_TOB   = A_BASE+11
A_TOA   = A_BASE+12
A_TOS   = A_BASE+13
A_STR   = A_BASE+14
A_FV    = A_BASE+15
A_FST   = A_BASE+16
A_FK    = A_BASE+17
A_FOR   = A_BASE+18
A_FOR1  = A_BASE+19
A_NV    = A_BASE+20
A_IF    = A_BASE+21
A_ON    = A_BASE+22
A_ONL   = A_BASE+23
A_PR    = A_BASE+24
A_DEF   = A_BASE+25
A_LST   = A_BASE+26
A_DV    = A_BASE+27
A_DATA  = A_BASE+28
A_REM   = A_BASE+29
A_LET   = A_BASE+30
A_IN    = A_BASE+31
A_RD    = A_BASE+32
A_GET   = A_BASE+33
A_GOTO  = A_BASE+34
A_GOSUB = A_BASE+35
A_RUN   = A_BASE+36
A_RES   = A_BASE+37
A_RET   = A_BASE+38
A_N0    = A_BASE+39
A_N1    = A_BASE+40
A_NARG  = A_BASE+41
A_FOP   = A_BASE+42

; ::::::::::::::
; cm_line - compile the direct line at ln.
;
; in:     ln  the line
; out:    as cm_text
; ::::::::::::::
cm_line:
        mov     eax,[dslin]
        mov     [ctxlin],eax
        mov     [cxb],offset xbuf
        mov     esi,offset cbuf-1

; ::::::::::::::
; cm_text - compile a token body to code at cxb.
;
; in:     SI   before the body
;         cxb  where the code goes
; out:    xo   the code's length
; ::::::::::::::
cm_text:
        mov     [csp],esp
        xor     eax,eax
        mov     [cll],eax
        mov     [clle],eax
        mov     [ccmpe],eax
        mov     edi,[cxb]
        lea     eax,[edi+XLIM]
        mov     [cxlim],eax

; ::::::::::::::
; cm_new - compile the statement after SI.
;
; in:     SI  before it
; ::::::::::::::
cm_new:
        mov     [cth],0
        call    st_eosn
        jz      cm_ee
        test    al,al
        jns     @st_comp4
        call    cm_tok

; ::::::::::::::
; cm_nx - after a statement: ':' compiles the next, 0 ends the line.
;
; in:     SI  after the statement
; error:  ?SYNTAX ERROR  any other byte
; ::::::::::::::
cm_nx:
        call    st_eos
        jnz     @st_comp4

; ::::::::::::::
; cm_ee - at a statement's end: 0 the line ends, ':' another statement.
;
; in:     AL  0 or ':'
; ::::::::::::::
cm_ee:
        test    al,al
        jnz     cm_new
        jmp     cm_fin
@st_comp4:
        jmp     err_sn

; ::::::::::::::
; cm_om - ?OUT OF MEMORY: the code is full.
; ::::::::::::::
cm_om:
        mov     al,E_OM

; ::::::::::::::
; cm_err - the compile ends here: ERR AL, EOL. The statement's code before
; the ERR runs first.
;
; in:     AL   the error
;         csp  SP at the compile's start
; ::::::::::::::
cm_err:
        mov     ah,al
        mov     al,O_ERR
        stosw

; ::::::::::::::
; cm_fin - the line's end: EOL; the compile returns.
;
; in:     csp  SP at the compile's start
; ::::::::::::::
cm_fin:
        mov     al,O_EOL
        stosb
        mov     esp,[csp]

; ::::::::::::::
; cm_end - the compile is over.
;
; out:    csp  0
;         xo   the code's end
; ::::::::::::::
cm_end:
        and     [csp],0
        mov     eax,edi
        sub     eax,[cxb]
        mov     [xo],al
        ret

; ::::::::::::::
; cm_if - IF expr THEN|GOTO line or statements: a compare just made becomes
; its CMPIF, else X.IF.
;
; in:     SI  after IF
; error:  ?SYNTAX ERROR  no THEN or GOTO, or no statement after it
; ::::::::::::::
cm_if:
        call    cm_expr
        mov     al,[esi]
        cmp     al,TK_THEN
        je      @cmi_t
        cmp     al,TK_GOTO
        jne     @st_comp1
@cmi_t:
        cmp     edi,[ccmpe]
        jne     @cmi_n
        mov     ebx,[ccmp]
        add     byte ptr [ebx],O_I_CMPIF-O_I_CMP
        jmp     @cmi_d
@st_comp1:
        jmp     err_sn
@cmi_n:
        mov     al,O_I_IF
        cmp     [cty],T_INT
        je      @cmi_o
        mov     al,O_F_IF
        cmp     [cty],T_FLT
        je      @cmi_o
        mov     al,O_S_IF
@cmi_o:
        call    cm_op
@cmi_d:
        mov     [cth],1
        call    st_getn
        mov     ebx,s_goto-cms
        cmp     al,M_INT
        je      cm_scr
        test    al,al
        jns     @st_comp1

; ::::::::::::::
; cm_tok - compile the statement at SI: its script.
;
; in:     SI  at its first byte
;         AL  that byte, 80h or over
; error:  ?SYNTAX ERROR  not a keyword, M_VAR or GO TO
; ::::::::::::::
cm_tok:
        cmp     al,TK_NEW
        jbe     @cmk_k
        mov     ebx,s_let-cms
        cmp     al,M_VAR
        je      cm_scr
        cmp     al,TK_GO
        jne     @st_comp1
        mov     ebx,s_go-cms
        jmp     @cmk_s
@cmk_k:
        mov     [ctok],al
        mov     [ctks],esi
        movzx   ebx,al
        mov     bl,[cmt+ebx-80h]
@cmk_s:
        push    ebx
        call    st_getn
        pop     ebx

; ::::::::::::::
; cm_scr - run the script at offset BX of cms.
;
; in:     BX  the offset
;         SI  the statement, past the keyword
; ::::::::::::::
cm_scr:
        mov     [spc],ebx
@cms_l:
        call    cm_nb
        push    eax
        call    cm_do
        pop     eax
        test    al,al
        jns     @cms_l
        ret

; ::::::::::::::
; cm_do - run a script byte: a CALL or an action.
;
; in:     AL  the byte
; ::::::::::::::
cm_do:
        and     al,7Fh
        cmp     al,A_BASE
        jb      cm_call
        movzx   ebx,al
        add     ebx,ebx
        jmp     [cmact+ebx*2-4*A_BASE]

; ::::::::::::::
; cm_call - emit CALL AL.
;
; in:     AL  the routine
; ::::::::::::::
cm_call:
        mov     cl,al
        mov     al,O_CALL
        jmp     cm_opx

; ::::::::::::::
; cm_store - convert the value to the target's type and store it: X.ST.V,
; X.ST.A1, X.ST.A; TI$ by S.ST.TI.
;
; in:     ctv, ctk, ctn  the target: id, sinfo, subscripts
; error:  ?TYPE MISMATCH  the wrong type
;         ?SYNTAX ERROR   TI or ST
; ::::::::::::::
cm_store:
        call    cm_demf
        mov     al,[ctk]
        mov     ah,al
        and     al,43h
        cmp     al,40h
        jae     @cst_y
        and     al,3
        cmp     al,1
        je      @cst_i
        ja      @cst_s
        call    cm_tof
        mov     dl,1
        jmp     @cst_e
@cst_i:
        call    cm_toi
        xor     dl,dl
        jmp     @cst_e
@cst_s:
        call    cm_tos
        mov     dl,2
@cst_e:
        mov     cl,[ctv]
        mov     al,[ctn]
        test    al,al
        jnz     @cst_a
        mov     al,O_I_ST_V
        add     al,dl
        jmp     cm_opx
@cst_a:
        cmp     al,1
        jne     @cst_n
        mov     al,O_I_ST_A1
        add     al,dl
        jmp     cm_opx
@cst_n:
        mov     al,O_I_ST_A
        add     al,dl
        call    cm_opx
        mov     al,[ctn]
        jmp     cm_byt
@cst_y:
        test    ah,2
        jz      err_sn
        call    cm_tos
        mov     al,K_STTI
        jmp     cm_call

; ::::::::::::::
; cm_fork - the value as FOR's variable wants it.
;
; in:     cfk  its kind
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_fork:
        call    cm_demf
        cmp     [cfk],0
        je      cm_tof

; ::::::::::::::
; cm_toi - the value as an INT.
;
; out:    cty  T_INT
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_toi:
        test    [cty],T_STR
        jnz     @st_comp3
        cmp     [cty],T_INT
        je      cm_rts
        mov     [cty],T_INT
        mov     al,O_F_TOI
        jmp     cm_op

; ::::::::::::::
; cm_tob - the value as a byte: a U8 load needs nothing.
;
; out:    cty  T_INT
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_tob:
        mov     ecx,O_I_TOB*256+O_F_TOB
        jmp     @ctn_g
@st_comp3:
        jmp     err_tm

; ::::::::::::::
; cm_toa - the value as an address, 0..65535: a U8 load needs nothing.
;
; out:    cty  T_INT
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_toa:
        mov     ecx,O_I_TOA*256+O_F_TOA
@ctn_g:
        test    [cty],T_STR
        jnz     @st_comp3
        mov     al,cl
        cmp     [cty],T_INT
        jne     @ctn_e
        mov     [cty],T_INT
        call    cm_ileaf
        jne     @ctn_i
        cmp     byte ptr [ebx],O_I_LD_U8
        je      cm_rts
@ctn_i:
        mov     al,ch
@ctn_e:
        mov     [cty],T_INT
        jmp     cm_op

; ::::::::::::::
; cm_tos - the value must be a string.
;
; error:  ?TYPE MISMATCH  a number
; ::::::::::::::
cm_tos:
        test    [cty],T_STR
        jz      @st_comp3
        ret

; ::::::::::::::
; cm_ileaf - is the code's last op an INT load: I.LD.V, I.LD.U8, I.LD.I16?
;
; out:    ZF  set when it is
;         BX  the op
; keeps:  all but BX
; ::::::::::::::
cm_ileaf:
        cmp     edi,[clle]
        jne     @cil_r
        mov     ebx,[cll]
        cmp     byte ptr [ebx],O_I_LD_V
        je      @cil_r
        cmp     byte ptr [ebx],O_I_LD_U8
        je      @cil_r
        cmp     byte ptr [ebx],O_I_LD_I16
@cil_r:
        ret

; ::::::::::::::
; cm_tof - the value as a FLOAT: an INT load just made loads it as a FLOAT.
;
; out:    cty  T_FLT
; error:  ?TYPE MISMATCH  a string
; ::::::::::::::
cm_tof:
        test    [cty],T_STR
        jnz     err_tm
        cmp     [cty],T_FLT
        je      @rt_com2
        mov     [cty],T_FLT
        call    cm_ileaf
        jne     @ctf_o
        mov     al,[ebx]
        cmp     al,O_I_LD_V
        jne     @ctf_k
        mov     byte ptr [ebx],O_F_LD_VI
@rt_com2:
        ret
@ctf_k:
        cmp     al,O_I_LD_U8
        jne     @ctf_w
        mov     byte ptr [ebx],O_F_LD_U8
        ret
@ctf_w:
        mov     ax,[ebx+1]               ; I.LD.I16: F.LD.F32 and its mark
        push    ebx
        call    flt_from16
        pop     ebx
        mov     edi,ebx
        push    eax
        mov     al,O_F_LD_F32
        call    cm_op
        pop     eax
        jmp     cm_dw
@ctf_o:
        mov     al,O_I_TOF
        jmp     cm_op

; ::::::::::::::
; cm_dw - emit the dword EAX.
;
; in:     EAX  the dword
; ::::::::::::::
cm_dw:
        mov     ecx,4
@cdw_l:
        call    cm_byt
        shr     eax,8
        loop    @cdw_l
        ret

; ::::::::::::::
; cm_ins - insert byte AL at BX: the code after moves up.
;
; in:     AL  the byte
;         BX  the place
; keeps:  all but CX
; error:  ?OUT OF MEMORY  the code is full
; ::::::::::::::
cm_ins:
        cmp     edi,[cxlim]
        jae     @st_comp7
        push    esi
        push    edi
        mov     esi,edi
        dec     esi
        mov     ecx,edi
        sub     ecx,ebx
        std
        rep     movsb
        cld
        pop     edi
        pop     esi
        inc     edi
        mov     [ebx],al
        cmp     [cll],ebx
        jb      @cin_r
        inc     [cll]
        inc     [clle]
@cin_r:
        cmp     [ccmp],ebx
        jb      @cin_q
        inc     [ccmp]
        inc     [ccmpe]
@cin_q:
        ret
@st_comp7:
        jmp     cm_om

; ::::::::::::::
; cm_nb - the script's next byte.
;
; out:    AL  the byte
; keeps:  SI, DI
; ::::::::::::::
cm_nb:
        mov     ebx,[spc]
        inc     [spc]
        mov     al,[cms+ebx]
        ret

; ::::::::::::::
; cm_op - emit op AL.
;
; in:     AL  the op
; ::::::::::::::
cm_op:

; ::::::::::::::
; cm_byt - emit byte AL.
;
; in:     AL  the byte
; error:  ?OUT OF MEMORY  the code is full
; ::::::::::::::
cm_byt:
        cmp     edi,[cxlim]
        jae     @st_comp7
        stosb
        ret

; ::::::::::::::
; cm_opx - emit op AL, then byte CL.
;
; in:     AL, CL  the op, the byte
; ::::::::::::::
cm_opx:
        call    cm_op
        mov     al,cl
        jmp     cm_byt

; ::::::::::::::
; cm_opw - emit op AL, then word DX.
;
; in:     AL, DX  the op, the word
; ::::::::::::::
cm_opw:
        mov     [wv],dx
        call    cm_op

; ::::::::::::::
; cm_w - emit the word wv.
;
; in:     wv  the word
; ::::::::::::::
cm_w:
        mov     al,byte ptr [wv]
        call    cm_byt
        mov     al,byte ptr [wv+1]
        jmp     cm_byt

; ::::::::::::::
; cm_jw - emit a jump's line number wv and the room its line's address takes,
; which the first jump fills (ex_target).
;
; in:     wv  the number
; ::::::::::::::
cm_jw:
        call    cm_w
        xor     al,al
        call    cm_byt
        xor     al,al
        jmp     cm_byt

; ::::::::::::::
; cm_lint - emit an INT load of AX: I.LD.U8 under 256, else I.LD.I16.
;
; in:     AX  the number
; out:    cty  T_INT
; ::::::::::::::
cm_lint:
        mov     [cty],T_INT
        mov     [cll],edi
        test    ax,0FF00h
        jnz     @cli_w
        push    eax
        mov     al,O_I_LD_U8
        call    cm_op
        pop     eax
        call    cm_byt
        jmp     @cli_e
@cli_w:
        mov     dx,ax
        mov     al,O_I_LD_I16
        call    cm_opw
@cli_e:
        mov     [clle],edi
        ret

; ::::::::::::::
; cm_ax - expect the script's next byte, then step past it.
;
; error:  ?SYNTAX ERROR  another byte
; ::::::::::::::
cm_ax:
        call    cm_nb
        jmp     cm_chk

; ::::::::::::::
; cm_chkcl - cm_chk for ')'.
; ::::::::::::::
cm_chkcl:
        mov     al,')'
        jmp     cm_chk

; ::::::::::::::
; cm_chkop - cm_chk for '('.
; ::::::::::::::
cm_chkop:
        mov     al,'('
        jmp     cm_chk

; ::::::::::::::
; cm_chkcm - cm_chk for ','.
; ::::::::::::::
cm_chkcm:
        mov     al,','
        jmp     cm_chk

; ::::::::::::::
; cm_chkeq - cm_chk for TK_EQ.
; ::::::::::::::
cm_chkeq:
        mov     al,TK_EQ

; ::::::::::::::
; cm_chk - the byte at SI must be AL; step past it.
;
; in:     AL  the byte
;         SI  at it, or the spaces before it
; out:    as st_getn
; error:  ?SYNTAX ERROR  another byte
; ::::::::::::::
cm_chk:
        mov     ah,al
        call    st_get
        cmp     al,ah
        jne     @st_comp5

; ::::::::::::::
; st_getn - step to the next byte, skipping spaces.
;
; in:     SI  before the byte
; out:    as st_get
; ::::::::::::::
st_getn:
        inc     esi

; ::::::::::::::
; st_get - the byte at SI, skipping spaces.
;
; in:     SI  at the byte, or the spaces before it
; out:    SI  at the byte
;         AL  the byte
;         ZF  set at the end of the line
; keeps:  all but AL, SI
; ::::::::::::::
st_get:
        mov     al,[esi]
        test    al,al
        jz      @stg_r
        cmp     al,' '
        je      st_getn
@stg_r:
        ret
@st_comp5:
        jmp     err_sn

; ::::::::::::::
; st_eosn - st_eos at the next byte.
; ::::::::::::::
st_eosn:
        inc     esi

; ::::::::::::::
; st_eos - is the byte at SI the end of a statement?
;
; in:     SI  at the byte, or the spaces before it
; out:    ZF  set at 0 or ':'
;         AL  the byte
; ::::::::::::::
st_eos:
        call    st_get
        jz      @ste_r
        cmp     al,':'
@ste_r:
        ret

; ::::::::::::::
; cm_name - a name: M_VAR and its id.
;
; in:     SI  at M_VAR, or spaces before it
; out:    BX, AL  the id
;         SI      after it
; error:  ?SYNTAX ERROR  no M_VAR
; ::::::::::::::
cm_name:
        call    st_get
        cmp     al,M_VAR
        jne     @st_comp5

; ::::::::::::::
; var_id - the id of the name at SI: M_VAR, the length, the text, the id.
;
; in:     SI  at M_VAR
; out:    BX, AL  the id
;         SI      after it
; ::::::::::::::
var_id:
        movzx   ebx,byte ptr [esi+1]
        lea     esi,[esi+ebx+2]
        lodsb
        movzx   ebx,al
        ret

; ::::::::::::::
; cm_fv - FOR's variable: the target of the start's store.
;
; in:     SI  at the variable
; out:    cfid, cfk  its id, its kind
; error:  ?SYNTAX ERROR   not a variable, an array element
;         ?TYPE MISMATCH  a string
; ::::::::::::::
cm_fv:
        call    cm_name
        test    [sinfo+ebx],80h
        jnz     @st_comp2
        mov     [cfid],al
        call    cm_tgt
        cmp     [cinf],0
        je      @cfv_k
        mov     [cinfv],bl
        or      [sinfo+ebx],SI_FOR
@cfv_k:
        mov     al,[sinfo+ebx]
        mov     [ctk],al
        and     al,43h
        cmp     al,2
        je      @st_comp6
        mov     [cfk],al
        ret

; ::::::::::::::
; cm_tgt - variable BX is the target: ctv, ctk; no subscripts.
;
; in:     BX   the id
; keeps:  BX
; ::::::::::::::
cm_tgt:
        mov     [ctv],bl
        mov     al,[sinfo+ebx]
        mov     [ctk],al
        mov     [ctn],0
        ret
@st_comp6:
        jmp     err_tm
@st_comp2:
        jmp     err_sn

; ::::::::::::::
; cm_def - DEF FN name(var) = expr: NODIR, FN.DEF id len, FN.ENTER var, the
; body, FN.RET var.
;
; in:     SI  after DEF
; error:  ?SYNTAX ERROR   no FN ( ) =, or an array parameter
;         ?TYPE MISMATCH  a string parameter
; ::::::::::::::
cm_def:
        mov     al,K_NODIR
        call    cm_call
        mov     al,TK_FN
        call    cm_chk
        cmp     al,M_VAR
        jne     @st_comp2
        call    var_id
        push    eax
        call    cm_chkop
        call    cm_name
        call    cm_unf
        mov     al,[sinfo+ebx]
        and     al,43h
        cmp     al,2
        je      @st_comp6
        test    [sinfo+ebx],80h
        jnz     @st_comp2
        push    ebx
        call    cm_chkcl
        call    cm_chkeq
        pop     edx
        pop     ecx
        push    edx
        mov     al,O_FN_DEF
        call    cm_opx
        xor     al,al
        call    cm_byt
        pop     ecx
        push    ecx
        push    edi
        mov     al,O_FN_ENTER
        call    cm_opx
        call    cm_expr
        call    cm_tof
        pop     ebx
        pop     ecx
        mov     al,O_FN_RET
        call    cm_opx
        mov     eax,edi
        sub     eax,ebx
        mov     [ebx-1],al
        ret

; ::::::::::::::
; cm_dat - DATA: no code, READ finds the text in the body.
;
; in:     ctks  the keyword
; out:    SI    at the statement's end
; ::::::::::::::
cm_dat:
        mov     esi,[ctks]
        inc     esi
        jmp     cm_skip

; ::::::::::::::
; cm_rem - REM: the line's end.
; ::::::::::::::
cm_rem:
        jmp     cm_fin

; ::::::::::::::
; cm_lnum - a line number: M_INT; none there is 0.
;
; in:     SI  at it, or spaces before it
; out:    wv  the number
;         SI  after it
;         AL  the byte there
; error:  ?SYNTAX ERROR  64000 and over
; ::::::::::::::
cm_lnum:
        call    st_get
        cmp     al,M_INT
        jne     @cml_z
        mov     ax,[esi+1]
        mov     [wv],ax
        add     esi,3
        cmp     ah,0FAh
        jae     err_sn
        jmp     st_get
@cml_z:
        and     [wv],0
        ret

; ::::::::::::::
; cm_lnld - I.LD.I16 of the line number wv.
; ::::::::::::::
cm_lnld:
        mov     dx,[wv]
        mov     al,O_I_LD_I16
        jmp     cm_opw

; ::::::::::::::
; cm_run - RUN, RESTORE [line]: CALL, or the line and CALL .L.
;
; in:     SI  after the keyword
; ::::::::::::::
cm_run:
        mov     al,K_RUN
        jmp     @crn_g
; ::::::::::::::
; cm_res - RUN, RESTORE [line]: CALL, or the line and CALL .L.
; ::::::::::::::
cm_res:
        mov     al,K_RES
@crn_g:
        push    eax
        call    st_eos
        pop     eax
        jz      @jt_comp1
        push    eax
        call    cm_lnum
        call    cm_lnld
        pop     eax
        inc     al
@jt_comp1:
        jmp     cm_call

; ::::::::::::::
; cm_ao - step past the script's next byte, if it is at SI; else cm_skm.
;
; in:     SI  at the byte, or the spaces before it
; ::::::::::::::
cm_ao:
        call    cm_nb
        mov     ah,al
        call    st_get
        cmp     al,ah
        jne     cm_skm
        inc     esi
        ret

; ::::::::::::::
; cm_afc - the statement's end goes to the next A_M; else a comma.
;
; error:  ?SYNTAX ERROR  anything else
; ::::::::::::::
cm_afc:
        call    st_eos
        jz      cm_skm
        jmp     cm_chkcm

; ::::::::::::::
; cm_aes - the statement's end goes to the next A_M.
; ::::::::::::::
cm_aes:
        call    st_eos
        jnz     cm_rts

; ::::::::::::::
; cm_skm - the script goes on after its next A_M.
; ::::::::::::::
cm_skm:
        call    cm_nb
        and     al,7Fh
        cmp     al,A_M
        jne     cm_skm

; ::::::::::::::
; cm_rts - ret: also action A_M.
; ::::::::::::::
cm_rts:
        ret

; ::::::::::::::
; cm_loop - a comma, stepped past, goes back to the A_LM before.
; ::::::::::::::
cm_loop:
        call    cm_optcm
        jnz     cm_rts
        mov     ebx,[spc]
@cmo_b:
        dec     ebx
        cmp     [cms+ebx],A_LM
        jne     @cmo_b
        add     esp,8                   ; cm_do's return, cm_scr's saved byte
        jmp     cm_scr

; ::::::::::::::
; cm_n0 - count a CALL's arguments, then load the count.
; ::::::::::::::
cm_n0:
        mov     [ccnt],0
        ret
; ::::::::::::::
; cm_n1 - count a CALL's arguments, then load the count.
; ::::::::::::::
cm_n1:
        inc     [ccnt]
        ret
; ::::::::::::::
; cm_narg - count a CALL's arguments, then load the count.
; ::::::::::::::
cm_narg:
        movzx   ax,[ccnt]
        jmp     cm_lint

; ::::::::::::::
; cm_dv - a DIM item: the subscripts, its id, n, CALL DIM.
;
; in:     SI  at the name
; error:  ?SYNTAX ERROR  no name
; ::::::::::::::
cm_dv:
        call    cm_name
        push    ebx
        call    cm_subs0
        pop     eax
        push    ecx
        call    cm_lint
        pop     eax
        call    cm_lint
        mov     al,K_DIM
        jmp     cm_call

; ::::::::::::::
; cm_fst - FOR's start: stored as LET's.
; ::::::::::::::
cm_fst:
        jmp     cm_store

; ::::::::::::::
; cm_for - X.FOR id, X.FOR1 id by the variable's kind.
;
; in:     cfid, cfk  the variable
; ::::::::::::::
cm_for:
        mov     al,O_I_FOR
        jmp     @cfo_g
; ::::::::::::::
; cm_for1 - X.FOR id, X.FOR1 id by the variable's kind.
; ::::::::::::::
cm_for1:
        mov     al,O_I_FOR1
@cfo_g:
        cmp     [cfk],0
        jne     @cfo_e
        inc     al                      ; F.FOR, F.FOR1
@cfo_e:
        mov     cl,[cfid]
        jmp     cm_opx

; ::::::::::::::
; cm_nv - NEXT.V id, or NEXT.
;
; in:     SI  at the name, or not
; ::::::::::::::
cm_nv:
        call    st_get
        cmp     al,M_VAR
        jne     @cmv_n
        call    var_id
        mov     cl,al
        mov     al,O_NEXT_V
        jmp     cm_opx
@cmv_n:
        mov     al,O_NEXT
        jmp     cm_op

; ::::::::::::::
; cm_in - INPUT's, READ's, GET's value for the target, stored.
;
; in:     ctk  the target's sinfo
; ::::::::::::::
cm_in:
        mov     al,K_FIN
        jmp     @crx_g
; ::::::::::::::
; cm_rd - INPUT's, READ's, GET's value for the target, stored.
; ::::::::::::::
cm_rd:
        mov     al,K_FREAD
        jmp     @crx_g
; ::::::::::::::
; cm_get - INPUT's, READ's, GET's value for the target, stored.
; ::::::::::::::
cm_get:
        mov     al,K_FGET
@crx_g:
        mov     [cty],T_FLT
        mov     ah,[ctk]
        and     ah,3
        cmp     ah,2
        jne     @crx_c
        inc     al
        mov     [cty],T_STR
@crx_c:
        call    cm_call
        jmp     cm_store

; ::::::::::::::
; cm_print - PRINT items; PR.NL unless the list ends with ; , TAB( or SPC(.
;
; in:     SI  after PRINT
; error:  ?SYNTAX ERROR  no ) after TAB( or SPC(
; ::::::::::::::
cm_print:
        call    st_eos
        jz      @cmp_n
@cmp_i:
        cmp     al,';'
        je      @cmp_x
        cmp     al,','
        je      @cmp_c
        cmp     al,TK_TAB
        je      @cmp_t
        cmp     al,TK_SPC
        je      @cmp_s
        call    cm_expr
        mov     al,K_SPR
        cmp     [cty],T_STR
        je      @cmp_v
        mov     al,K_FPR
        cmp     [cty],T_FLT
        je      @cmp_v
        mov     al,K_IPR
@cmp_v:
        call    cm_call
        jmp     cm_print
@cmp_c:
        mov     al,K_PZONE
@cmp_o:
        call    cm_call
@cmp_x:
        call    st_eosn
        jnz     @cmp_i
        ret
@cmp_n:
        mov     al,K_PNL
        jmp     cm_call
@cmp_t:
        mov     al,K_PTAB
        jmp     @cmp_a
@cmp_s:
        mov     al,K_PSPC
@cmp_a:
        push    eax
        inc     esi
        call    cm_expr
        call    cm_tob
        call    cm_chkcl
        dec     esi
        pop     eax
        jmp     @cmp_o

; ::::::::::::::
; cm_list - LIST [from][-[to]]: the two numbers, CALL LIST.
;
; in:     SI  after LIST
; ::::::::::::::
cm_list:
        mov     [cst],esi
        call    cm_lnum
        push    eax
        call    cm_lnld
        pop     eax
        cmp     al,TK_MINUS
        je      @cli_d
        cmp     esi,[cst]
        jne     @cli_t
@cli_a:
        mov     [wv],0FFFFh
@cli_t:
        call    cm_lnld
        mov     al,K_LIST
        jmp     cm_call
@cli_d:
        call    st_eosn
        jz      @cli_a
        call    cm_lnum
        jmp     @cli_t

; ::::::::::::::
; cm_on - GOTO|GOSUB after ON's expression: ON kn, n 0 yet.
;
; in:     SI  at the keyword
; out:    onp  kn's place
; error:  ?SYNTAX ERROR  no GOTO or GOSUB (after TOB, as ON converted first)
; ::::::::::::::
cm_on:
        call    cm_tob
        mov     ah,[esi]
        xor     ah,TK_GOTO
        test    ah,0FBh
        jnz     err_sn
        xor     cl,cl
        test    ah,ah
        jz      @con_g
        mov     cl,80h
@con_g:
        mov     al,O_ON
        call    cm_opx
        lea     eax,[edi-1]
        mov     [onp],eax
        inc     esi
        ret

; ::::::::::::::
; cm_onl - one of ON's lines: num addr; n + 1.
;
; in:     SI   at it, or spaces before it
;         onp  kn's place
; ::::::::::::::
cm_onl:
        call    cm_lnum
        call    cm_jw
        mov     ebx,[onp]
        inc     byte ptr [ebx]
        ret

; ::::::::::::::
; cm_goto - GOTO, GOSUB num addr.
;
; in:     wv  the line
; ::::::::::::::
cm_goto:
        mov     al,O_GOTO
        jmp     @cgo_g
; ::::::::::::::
; cm_gosub - GOTO, GOSUB num addr.
; ::::::::::::::
cm_gosub:
        mov     al,O_GOSUB
@cgo_g:
        call    cm_op
        jmp     cm_jw

; ::::::::::::::
; cm_ret - RETURN.
; ::::::::::::::
cm_ret:
        mov     al,O_RETURN
        jmp     cm_op

; ::::::::::::::
; cm_fop - CALL LOAD, SAVE or VERIFY by the token kept in ctok.
;
; in:     ctok  the token
; ::::::::::::::
cm_fop:
        mov     al,[ctok]
        add     al,K_LOAD-TK_LOAD
        jmp     cm_call

; ::::::::::::::
; cm_optcm - step past a comma, if there is one.
;
; in:     SI  at the byte, or the spaces before it
; out:    ZF  set when there was one
; ::::::::::::::
cm_optcm:
        call    st_get
        cmp     al,','
        jne     @cmc_r
        inc     esi
        cmp     al,al
@cmc_r:
        ret

; ::::::::::::::
; cm_skip - skip DATA, DEF's body: to a ':' outside quotes.
;
; in:     SI  in the statement
; out:    SI  at the ':' or the 0
; ::::::::::::::
cm_skip:
        xor     cl,cl
@csk_lp:
        mov     al,[esi]
        test    al,al
        jz      @csk_r
        cmp     al,'"'
        jne     @csk_q
        inc     cl
        jmp     @csk_n
@csk_q:
        test    cl,1
        jnz     @csk_n
        cmp     al,':'
        je      @csk_r
        cmp     al,M_INT
        jne     @csk_v
        add     esi,2
        jmp     @csk_n
@csk_v:
        cmp     al,M_VAR
        jne     @csk_n
        movzx   ebx,byte ptr [esi+1]
        lea     esi,[esi+ebx+2]
@csk_n:
        inc     esi
        jmp     @csk_lp
@csk_r:
        ret
