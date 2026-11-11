; ==========================================================================
; link.asm - types inferred, the code cache; lines compiled to streams
; ==========================================================================
; holds:    cm_link, cm_infer, cm_infclr, cm_comp, cm_cache, cm_flush,
;           cm_xlate; cm_walk
;           opsz, opfl
; uses:     cm_text, mem_line, op_size
; data:     A line's cp is its code in the cache, 0 when it has none.
; calls:
;   cm_link            cm_infer, then the cache is empty
;     cm_infer         every line compiled, its stores walked; FOR lines
;                      again until the types hold
;   cm_cache           cm_comp, the code copied past the program
; ==========================================================================

include x86f.inc
        .code

        .data
g_opsz  label   byte
        oplist  opszm
g_opfl  label   byte
        oplist  opflm
        .code

; ::::::::::::::
; cm_comp - compile line BX to xbuf.
;
; in:     EBX  the line
; out:    xo  the code's length
; keeps:  EBX
; ::::::::::::::
cm_comp:
        push    ebx
        mov     edi,offset g_lbuf
        call    mem_line
        mov     [g_state.cm.cxb],offset g_xbuf
        lea     eax,[g_progb+ebx]
        sub     eax,offset g_lbuf
        mov     [g_state.cm.ctxlin],eax
        mov     esi,offset g_lbuf+HDR-1
        call    cm_text
        pop     ebx
        ret

; ::::::::::::::
; cm_cache - the code of line BX: compiled into the cache.
;
; in:     EBX  the line
; out:    ESI  its code
; keeps:  EBX
; error:  ?OUT OF MEMORY  the cache is full
; ::::::::::::::
cm_cache:
        call    cm_comp
@cc_t:
        mov     edi,[g_state.lnk.cend]
        mov     [g_state.lnk.elim],offset g_cachb+CLIM
        push    ebx
        call    cm_xlate
        pop     ebx
        jnc     @cc_f
        cmp     [g_state.lnk.cend],offset g_cachb+CACHE0
        je      err_om
        push    ebx
        call    cm_flush
        pop     ebx
        jmp     @cc_t
@cc_f:
        mov     esi,[g_state.lnk.cend]
        mov     [g_state.lnk.cend],edi
        mov     eax,esi
        sub     eax,offset g_cachb
        mov     word ptr [g_progb+ebx+L_CP],ax
        ret

; ::::::::::::::
; cm_flush - the cache is full: the frames' and CONT's ips become offsets in
; their lines' code, every line loses its code, the cache is empty.
; ::::::::::::::
cm_flush:
        mov     ebx,[g_state.rn.cstk]
@cfl_f:
        cmp     ebx,CSTOP
        jae     @cfl_c
        movzx   edi,word ptr [ebx+F_LN]
        cmp     edi,DLINE
        je      @cfl_n
        mov     ax,[ebx+F_IP]
        cmp     ax,CACHE0
        jb      @cfl_n
        sub     ax,word ptr [g_progb+edi+L_CP]
        mov     [ebx+F_IP],ax
@cfl_n:
        movzx   ecx,byte ptr [ebx+F_MARK]
        add     ebx,ecx
        jmp     @cfl_f
@cfl_c:
        movzx   edi,[g_state.rn.contip]
        test    edi,edi
        jz      @cfl_l
        mov     ax,[g_state.rn.contsi]
        cmp     ax,CACHE0
        jb      @cfl_l
        sub     ax,word ptr [g_progb+edi+L_CP]
        mov     [g_state.rn.contsi],ax
@cfl_l:
        mov     ebx,PROGBASE
@cfl_x:
        movzx   ecx,byte ptr [g_progb+ebx]
        jecxz   @cfl_d
        and     word ptr [g_progb+ebx+L_CP],0
        add     ebx,ecx
        jmp     @cfl_x
@cfl_d:
        mov     [g_state.lnk.cend],offset g_cachb+CACHE0
        ret

; ::::::::::::::
; cm_xlate - xbuf's ops as a stream at EDI: each op, its operands. A GOTO's
; or GOSUB's line is found now when it exists; a DEF's length, an INPUT's
; end (16 bits, from cachb) are filled in. FN.DEF and the CALL of INBEG
; take 2 bytes more than in xbuf.
;
; in:     EDI   where it goes
;         elim  its limit
; out:    EDI   its end
;         CF    set when it does not fit
;         eeol  the EOL's element
; ::::::::::::::
cm_xlate:
        and     [g_state.lnk.einb],0
        mov     esi,offset g_xbuf
@cx_l:
        movzx   ebx,byte ptr [esi]
        call    op_size
        lea     eax,[edi+ecx+XSLK]
        cmp     eax,[g_state.lnk.elim]
        ja      @cx_ov
        cmp     bl,O_EOL
        jne     @cx_n
        mov     [g_state.lnk.eeol],edi
@cx_n:
        mov     dl,bl
        movsb
        dec     ecx
        cmp     dl,O_FN_DEF
        je      @cx_def
        cmp     dl,O_FN_RET
        je      @cx_ret
        cmp     dl,O_CALL
        je      @cx_call
@cx_c:
        rep     movsb
        cmp     dl,O_GOTO
        je      @cx_j
        cmp     dl,O_GOSUB
        je      @cx_j
@cx_t:
        cmp     dl,O_EOL
        jne     @cx_l
        mov     ebx,[g_state.lnk.einb]
        test    ebx,ebx
        jz      @cx_d
        mov     eax,[g_state.lnk.eeol]
        sub     eax,offset g_cachb
        mov     [ebx],ax
@cx_d:
        clc
        ret
@cx_ov:
        stc
        ret
@cx_j:
        push    [g_state.rn.lnum]
        mov     ax,[edi-4]
        mov     [g_state.rn.lnum],ax
        push    edx
        call    prg_findw
        pop     edx
        pop     [g_state.rn.lnum]
        jnc     @cx_t
        mov     [edi-2],bx
        jmp     @cx_t
@cx_def:
        movsb
        inc     esi
        mov     [g_state.lnk.efn],edi
        xor     ax,ax
        stosw
        jmp     @cx_t
@cx_ret:
        movsb
        mov     ebx,[g_state.lnk.efn]
        mov     eax,edi
        sub     eax,ebx
        sub     eax,2
        mov     [ebx],ax
        jmp     @cx_t
@cx_call:
        mov     al,[esi]
        movsb
        cmp     al,K_INBEG
        jne     @cx_i
        mov     [g_state.lnk.einb],edi
        xor     ax,ax
        stosw
        jmp     @cx_t
@cx_i:
        cmp     al,K_INEND
        jne     @cx_t
        mov     ebx,[g_state.lnk.einb]
        mov     eax,edi
        sub     eax,offset g_cachb
        mov     [ebx],ax
        and     [g_state.lnk.einb],0
        jmp     @cx_t

; ::::::::::::::
; cm_walk - call DX with each op of the code at SI.
;
; in:     SI  the code, in DS
;         DX  the routine: AL the op, SI at it
; ::::::::::::::
cm_walk:
        mov     al,[esi]
        cmp     al,O_EOL
        je      cm_rts
        call    edx
        call    op_size
        add     esi,ecx
        jmp     cm_walk

; ::::::::::::::
; cm_cbdem - cm_walk: demote what is assigned other than by FOR, mark FOR's;
; a FOR sets cfail.
;
; in:     AL  the op
;         SI  at it
; keeps:  CX, DX, SI
; ::::::::::::::
cm_cbdem:
        movzx   ebx,byte ptr [esi+1]
        cmp     al,O_FN_ENTER
        je      @ccb_x
        cmp     al,O_I_ST_V
        je      @ccb_v
        cmp     al,O_F_ST_V
        je      @ccb_v
        cmp     al,O_I_FOR
        jb      @rt_lin1
        cmp     al,O_F_FOR1
        ja      @rt_lin1
        mov     [g_state.cm.cfail],1
@rt_lin1:
        ret
@ccb_v:
        call    cm_isfor
        jne     @ccb_x
        or      [g_sinfo+ebx],SI_FOR
        ret
@ccb_x:
        test    [g_sinfo+ebx],SI_INF
        jnz     cm_dem
        ret

; ::::::::::::::
; cm_isfor - is the store at SI a FOR's start? Its FOR follows after the
; expression ops of the limit and step.
;
; in:     SI  at the store
;         BL  its id
; out:    ZF  set when it is
; keeps:  all
; ::::::::::::::
cm_isfor:
        push    eax
        push    ecx
        push    esi
        push    ebx
@cif_n:
        call    op_size
        add     esi,ecx
        mov     al,[esi]
        cmp     al,O_CALL
        jne     @cif_x
        cmp     byte ptr [esi+1],K_IPOW
        jae     @cif_n
        jmp     @cif_no
@cif_x:
        movzx   ebx,al
        test    [g_opfl+ebx],FX
        jnz     @cif_n
        cmp     al,O_I_FOR
        jb      @cif_no
        cmp     al,O_F_FOR1
        ja      @cif_no
        pop     ebx
        cmp     bl,[esi+1]
        jmp     @cif_r
@cif_no:
        pop     ebx
        or      esp,esp                 ; ZF clear
@cif_r:
        pop     esi
        pop     ecx
        pop     eax
        ret

; ::::::::::::::
; cm_link - before the program runs: infer the types, empty the cache.
;
; keeps:  SI
; ::::::::::::::
cm_link:
        cmp     [g_state.lnk.linked],0
        jne     cm_rts
        push    esi
        call    cm_infer
        and     [g_state.lnk.lc],0
        mov     ebx,PROGBASE
@clk_l:
        movzx   ecx,byte ptr [g_progb+ebx]
        jecxz   @clk_d
        and     word ptr [g_progb+ebx+L_CP],0
        add     ebx,ecx
        jmp     @clk_l
@clk_d:
        mov     [g_state.lnk.cend],offset g_cachb+CACHE0
        mov     [g_state.lnk.linked],1
        pop     esi
        ret

; ::::::::::::::
; cm_infclr - no variable is inferred INT.
; ::::::::::::::
cm_infclr:
        xor     ebx,ebx
@cic_l:
        cmp     bl,[g_state.lx.nsyms]
        je      @rt_lin4
        test    [g_sinfo+ebx],SI_INF
        jz      @cic_n
        and     [g_sinfo+ebx],NOT (SI_INF+1)
@cic_n:
        inc     bl
        jnz     @cic_l
@rt_lin4:
        ret

; ::::::::::::::
; cm_infer - type the variables a FOR alone assigns, with INT bounds, as INT.
; Every line is compiled and its stores walked; a FOR's line is compiled again
; until nothing changes, as the bounds' types decide.
; ::::::::::::::
cm_infer:
        call    cm_infclr
        xor     ebx,ebx
@cin_a:
        cmp     bl,[g_state.lx.nsyms]
        je      @cin_w
        test    [g_sinfo+ebx],0C3h
        jnz     @cin_b
        or      [g_sinfo+ebx],SI_INF+1
@cin_b:
        inc     bl
        jnz     @cin_a
@cin_w:
        mov     ebx,PROGBASE
@cin_1:
        cmp     byte ptr [g_progb+ebx],0
        je      @cin_u0
        call    cm_comp
        mov     [g_state.cm.cfail],0
        mov     esi,offset g_xbuf
        mov     edx,offset cm_cbdem
        push    ebx
        call    cm_walk
        pop     ebx
        movzx   ax,[g_state.cm.cfail]
        mov     word ptr [g_progb+ebx+L_CP],ax
        movzx   ecx,byte ptr [g_progb+ebx]
        add     ebx,ecx
        jmp     @cin_1
@cin_u0:
        xor     ebx,ebx
@cin_u:
        cmp     bl,[g_state.lx.nsyms]
        je      @cin_p
        test    [g_sinfo+ebx],SI_FOR
        jnz     @cin_s
        test    [g_sinfo+ebx],SI_INF
        jz      @cin_n
        call    cm_dem
        jmp     @cin_n
@cin_s:
        and     [g_sinfo+ebx],NOT SI_FOR
@cin_n:
        inc     bl
        jnz     @cin_u
@cin_p:
        mov     [g_state.cm.cinf],1
        mov     [g_state.cm.cinfc],0
        mov     [g_state.cm.cinfv],255
        mov     ebx,PROGBASE
@cin_l:
        cmp     byte ptr [g_progb+ebx],0
        je      @cin_d
        cmp     word ptr [g_progb+ebx+L_CP],0
        je      @cin_m
        call    cm_comp
@cin_m:
        movzx   ecx,byte ptr [g_progb+ebx]
        add     ebx,ecx
        jmp     @cin_l
@cin_d:
        xor     ebx,ebx
@cin_c:
        cmp     bl,[g_state.lx.nsyms]
        je      @cin_e
        and     [g_sinfo+ebx],NOT SI_FOR
        inc     bl
        jnz     @cin_c
@cin_e:
        cmp     [g_state.cm.cinfc],0
        jne     @cin_p
        mov     [g_state.cm.cinf],0
        ret

; ::::::::::::::
; cm_unf - in a pass, a variable assigned other than by FOR is a FLOAT.
;
; in:     BX  the id
; keeps:  BX
; ::::::::::::::
cm_unf:
        cmp     [g_state.cm.cinf],0
        je      cm_rts
        mov     [g_state.cm.cinfv],255
        test    [g_sinfo+ebx],SI_INF
        jz      cm_rts

; ::::::::::::::
; cm_dem - variable BX is a FLOAT after all.
;
; in:     BX  the id
; out:    cinfc  1
; keeps:  BX
; ::::::::::::::
cm_dem:
        and     [g_sinfo+ebx],NOT (SI_INF+1)
        mov     [g_state.cm.cinfc],1
        ret

; ::::::::::::::
; cm_demf - in a pass, a FLOAT bound makes the FOR's variable a FLOAT.
;
; in:     cty    the bound's type
;         cinfv  the variable, 255 none
; ::::::::::::::
cm_demf:
        cmp     [g_state.cm.cinf],0
        je      @rt_lin5
        cmp     [g_state.cm.cty],T_FLT
        jne     @rt_lin5
        movzx   ebx,[g_state.cm.cinfv]
        cmp     bl,255
        je      @rt_lin5
        test    [g_sinfo+ebx],SI_INF
        jnz     cm_dem
@rt_lin5:
        ret

        .data?
g_lbuf  db      264 dup(?)
        .code

        end
