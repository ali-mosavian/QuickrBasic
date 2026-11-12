; ==========================================================================
; link.asm - the code cache; lines compiled to streams
; ==========================================================================
; holds:    cm_link, cm_comp, cm_cache, cm_flush, cm_xlate; cm_walk
;           opsz, opfl
; uses:     cm_text, mem_line, op_size
; data:     A line's cp is its code in the cache, 0 when it has none.
; calls:
;   cm_link            the cache is empty
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
        mov     [g_state.cm.cline],bx
        call    bk_proc
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
        mov     esi,[g_state.lnk.ecode]
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
        inc     [g_state.lnk.nfl]
        ret

; ::::::::::::::
; cm_xlate - xbuf's ops as a stream at EDI: each op, its operands. A GOTO's
; or GOSUB's line is found now when it exists; an INPUT's
; end (16 bits, from cachb) are filled in. The CALL of INBEG takes 2 bytes
; more than in xbuf. Before the code, its statement map: for
; each statement the compile recorded (g_stb, g_stx), its body offset:1 and
; its code's offset:2; then their count:1 (vm/flow.asm, ex_bjump).
;
; in:     EDI   where it goes
;         elim  its limit
; out:    EDI   its end
;         ecode its code
;         CF    set when it does not fit
;         eeol  the EOL's element
; ::::::::::::::
cm_xlate:
        and     [g_state.lnk.einb],0
        movzx   ecx,[g_state.cm.nst]
        lea     eax,[ecx+ecx*2+1]
        add     eax,edi
        cmp     eax,[g_state.lnk.elim]
        ja      @cx_ov
        mov     [g_state.lnk.emap],edi
        mov     [eax-1],cl
        mov     edi,eax
        mov     [g_state.lnk.ecode],edi
        mov     [g_state.lnk.ek],0
        mov     esi,offset g_xbuf
@cx_l:
        call    cx_mark
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
; cx_mark - the statements whose code starts at SI: their map entries.
;
; keeps:  all
; ::::::::::::::
cx_mark:
        push    eax
        push    ebx
        push    ecx
@cxm_l:
        movzx   ebx,[g_state.lnk.ek]
        cmp     bl,[g_state.cm.nst]
        jae     @cxm_r
        movzx   eax,word ptr [g_stx+ebx*2]
        add     eax,offset g_xbuf
        cmp     eax,esi
        jne     @cxm_r
        lea     ecx,[ebx+ebx*2]
        add     ecx,[g_state.lnk.emap]
        mov     al,[g_stb+ebx]
        mov     [ecx],al
        mov     eax,edi
        sub     eax,[g_state.lnk.ecode]
        mov     [ecx+1],ax
        inc     [g_state.lnk.ek]
        jmp     @cxm_l
@cxm_r:
        pop     ecx
        pop     ebx
        pop     eax
        ret

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
; cm_link - before the program runs: empty the cache.
;
; keeps:  SI
; ::::::::::::::
cm_link:
        mov     [g_state.cm.cchk],0     ; a check an error left
        cmp     [g_state.lnk.linked],0
        jne     cm_rts
        push    esi
        call    cm_blocks
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

        .data?
g_lbuf  db      264 dup(?)
        .code

        end
