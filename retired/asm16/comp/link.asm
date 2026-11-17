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

DSEG    segment
opsz    label   byte
        oplist  opszm
opfl    label   byte
        oplist  opflm
DSEG    ends

; ::::::::::::::
; cm_comp - compile line BX to xbuf.
;
; in:     BX  the line
; out:    xo  the code's length
; keeps:  BX
; ::::::::::::::
cm_comp:
        push    bx
        mov     di,offset lbuf
        call    mem_line
        mov     [cxb],offset xbuf
        mov     eax,[pglin]
        movzx   edx,bx
        add     eax,edx
        sub     eax,offset lbuf
        mov     [ctxlin],eax
        mov     si,offset lbuf+HDR-1
        call    cm_text
        pop     bx
        ret

; ::::::::::::::
; cm_cache - the code of line BX: compiled into the cache.
;
; in:     BX  the line
; out:    SI  its code
; keeps:  BX
; error:  ?OUT OF MEMORY  the cache is full
; ::::::::::::::
cm_cache:
        call    cm_comp
@cc_t:
        mov     di,[cend]
        mov     [elim],CLIM
        push    bx
        call    cm_xlate
        pop     bx
        jnc     @cc_f
        cmp     [cend],offset bss_e
        je      err_om
        push    bx
        call    cm_flush
        pop     bx
        jmp     @cc_t
@cc_f:
        mov     si,[cend]
        mov     [cend],di
        mov     gs:[bx+L_CP],si
        ret

; ::::::::::::::
; cm_flush - the cache is full: the frames' and CONT's ips become offsets in
; their lines' code, every line loses its code, the cache is empty.
; ::::::::::::::
cm_flush:
        mov     bx,[cstk]
@cfl_f:
        cmp     bx,CSTOP
        jae     @cfl_c
        mov     di,[bx+F_LN]
        cmp     di,DLINE
        je      @cfl_n
        mov     ax,[bx+F_IP]
        cmp     ax,offset bss_e
        jb      @cfl_n
        sub     ax,gs:[di+L_CP]
        mov     [bx+F_IP],ax
@cfl_n:
        movzx   cx,byte ptr [bx+F_MARK]
        add     bx,cx
        jmp     @cfl_f
@cfl_c:
        mov     di,[contip]
        test    di,di
        jz      @cfl_l
        mov     ax,[contsi]
        cmp     ax,offset bss_e
        jb      @cfl_l
        sub     ax,gs:[di+L_CP]
        mov     [contsi],ax
@cfl_l:
        mov     bx,PROGBASE
@cfl_x:
        movzx   cx,byte ptr gs:[bx]
        jcxz    @cfl_d
        and     word ptr gs:[bx+L_CP],0
        add     bx,cx
        jmp     @cfl_x
@cfl_d:
        mov     [cend],offset bss_e
        ret

; ::::::::::::::
; cm_xlate - xbuf's ops as a stream at DI. Each op is mov si,operands; jmp
; handler, with mov bx,op*2 before the jmp for a shared handler; its operands
; follow. A DEF's length, an INPUT's end are filled in.
;
; in:     DI    where it goes, the cache's DS address
;         elim  its limit
; out:    DI    its end
;         CF    set when it does not fit
;         eeol  the EOL's element
; ::::::::::::::
cm_xlate:
        and     [einb],0
        mov     si,offset xbuf
@cx_l:
        movzx   bx,byte ptr [si]
        call    op_size
        mov     ax,di
        add     ax,cx
        add     ax,XSLK
        cmp     ax,[elim]
        ja      @cx_ov
        cmp     bl,O_EOL
        jne     @cx_n
        mov     [eeol],di
@cx_n:
        mov     dl,bl
        call    cm_native
        jc      @cx_g
        add     si,cx
        jmp     @cx_l
@cx_g:
        mov     al,0BEh
        stosb
        lea     ax,[di+5]
        test    [opfl+bx],FB
        jz      @cx_a
        add     ax,3
@cx_a:
        stosw
        test    [opfl+bx],FB
        jz      @cx_e
        mov     al,0BBh
        stosb
        mov     ax,bx
        add     ax,ax
        stosw
@cx_e:
        mov     al,0E9h
        stosb
        mov     dl,bl
        add     bx,bx
        mov     ax,[ops+bx]
        lea     bx,[di+2+CSOFF]         ; the stream in CS's view
        sub     ax,bx
        stosw
        inc     si
        dec     cx
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
        mov     bx,[einb]
        test    bx,bx
        jz      @cx_d
        mov     ax,[eeol]
        mov     [bx],ax
@cx_d:
        clc
        ret
@cx_ov:
        stc
        ret
@cx_j:
        push    [lnum]
        mov     ax,[di-4]
        mov     [lnum],ax
        call    prg_findw
        pop     [lnum]
        jnc     @cx_t
        mov     [di-2],bx
        mov     ax,offset op_gotol
        cmp     dl,O_GOTO
        je      @cx_k
        mov     ax,offset op_gosubl
@cx_k:
        lea     bx,[di-4+CSOFF]
        sub     ax,bx
        mov     [di-6],ax
        jmp     @cx_t
@cx_def:
        movsb
        inc     si
        mov     [efn],di
        xor     ax,ax
        stosw
        jmp     @cx_t
@cx_ret:
        movsb
        mov     bx,[efn]
        mov     ax,di
        sub     ax,bx
        sub     ax,2
        mov     [bx],ax
        jmp     @cx_t
@cx_call:
        mov     al,[si]
        movsb
        cmp     al,K_INBEG
        jne     @cx_i
        mov     [einb],di
        xor     ax,ax
        stosw
        jmp     @cx_t
@cx_i:
        cmp     al,K_INEND
        jne     @cx_t
        mov     bx,[einb]
        mov     [bx],di
        and     [einb],0
        jmp     @cx_t

; ::::::::::::::
; cm_walk - call DX with each op of the code at SI.
;
; in:     SI  the code, in DS
;         DX  the routine: AL the op, SI at it
; ::::::::::::::
cm_walk:
        mov     al,[si]
        cmp     al,O_EOL
        je      cm_rts
        call    dx
        call    op_size
        add     si,cx
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
        movzx   bx,byte ptr [si+1]
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
        mov     [cfail],1
@rt_lin1:
        ret
@ccb_v:
        call    cm_isfor
        jne     @ccb_x
        or      [sinfo+bx],SI_FOR
        ret
@ccb_x:
        test    [sinfo+bx],SI_INF
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
        push    ax
        push    cx
        push    si
        push    bx
@cif_n:
        call    op_size
        add     si,cx
        mov     al,[si]
        cmp     al,O_CALL
        jne     @cif_x
        cmp     byte ptr [si+1],K_IPOW
        jae     @cif_n
        jmp     @cif_no
@cif_x:
        movzx   bx,al
        test    [opfl+bx],FX
        jnz     @cif_n
        cmp     al,O_I_FOR
        jb      @cif_no
        cmp     al,O_F_FOR1
        ja      @cif_no
        pop     bx
        cmp     bl,[si+1]
        jmp     @cif_r
@cif_no:
        pop     bx
        or      sp,sp                   ; ZF clear
@cif_r:
        pop     si
        pop     cx
        pop     ax
        ret

; ::::::::::::::
; cm_link - before the program runs: infer the types, empty the cache.
;
; keeps:  SI
; ::::::::::::::
cm_link:
        cmp     [linked],0
        jne     cm_rts
        push    si
        call    cm_infer
        and     [lc],0
        mov     bx,PROGBASE
@clk_l:
        movzx   cx,byte ptr gs:[bx]
        jcxz    @clk_d
        and     word ptr gs:[bx+L_CP],0
        add     bx,cx
        jmp     @clk_l
@clk_d:
        mov     [cend],offset bss_e
        mov     [linked],1
        pop     si
        ret

; ::::::::::::::
; cm_infclr - no variable is inferred INT.
; ::::::::::::::
cm_infclr:
        xor     bx,bx
@cic_l:
        cmp     bl,[nsyms]
        je      @rt_lin4
        test    [sinfo+bx],SI_INF
        jz      @cic_n
        and     [sinfo+bx],NOT (SI_INF+1)
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
        xor     bx,bx
@cin_a:
        cmp     bl,[nsyms]
        je      @cin_w
        test    [sinfo+bx],0C3h
        jnz     @cin_b
        or      [sinfo+bx],SI_INF+1
@cin_b:
        inc     bl
        jnz     @cin_a
@cin_w:
        mov     bx,PROGBASE
@cin_1:
        cmp     byte ptr gs:[bx],0
        je      @cin_u0
        call    cm_comp
        mov     [cfail],0
        mov     si,offset xbuf
        mov     dx,offset cm_cbdem
        push    bx
        call    cm_walk
        pop     bx
        movzx   ax,[cfail]
        mov     gs:[bx+L_CP],ax
        movzx   cx,byte ptr gs:[bx]
        add     bx,cx
        jmp     @cin_1
@cin_u0:
        xor     bx,bx
@cin_u:
        cmp     bl,[nsyms]
        je      @cin_p
        test    [sinfo+bx],SI_FOR
        jnz     @cin_s
        test    [sinfo+bx],SI_INF
        jz      @cin_n
        call    cm_dem
        jmp     @cin_n
@cin_s:
        and     [sinfo+bx],NOT SI_FOR
@cin_n:
        inc     bl
        jnz     @cin_u
@cin_p:
        mov     [cinf],1
        mov     [cinfc],0
        mov     [cinfv],255
        mov     bx,PROGBASE
@cin_l:
        cmp     byte ptr gs:[bx],0
        je      @cin_d
        cmp     word ptr gs:[bx+L_CP],0
        je      @cin_m
        call    cm_comp
@cin_m:
        movzx   cx,byte ptr gs:[bx]
        add     bx,cx
        jmp     @cin_l
@cin_d:
        xor     bx,bx
@cin_c:
        cmp     bl,[nsyms]
        je      @cin_e
        and     [sinfo+bx],NOT SI_FOR
        inc     bl
        jnz     @cin_c
@cin_e:
        cmp     [cinfc],0
        jne     @cin_p
        mov     [cinf],0
        ret

; ::::::::::::::
; cm_unf - in a pass, a variable assigned other than by FOR is a FLOAT.
;
; in:     BX  the id
; keeps:  BX
; ::::::::::::::
cm_unf:
        cmp     [cinf],0
        je      cm_rts
        mov     [cinfv],255
        test    [sinfo+bx],SI_INF
        jz      cm_rts

; ::::::::::::::
; cm_dem - variable BX is a FLOAT after all.
;
; in:     BX  the id
; out:    cinfc  1
; keeps:  BX
; ::::::::::::::
cm_dem:
        and     [sinfo+bx],NOT (SI_INF+1)
        mov     [cinfc],1
        ret

; ::::::::::::::
; cm_demf - in a pass, a FLOAT bound makes the FOR's variable a FLOAT.
;
; in:     cty    the bound's type
;         cinfv  the variable, 255 none
; ::::::::::::::
cm_demf:
        cmp     [cinf],0
        je      @rt_lin5
        cmp     [cty],T_FLT
        jne     @rt_lin5
        movzx   bx,[cinfv]
        cmp     bl,255
        je      @rt_lin5
        test    [sinfo+bx],SI_INF
        jnz     cm_dem
@rt_lin5:
        ret
