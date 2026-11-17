; ==========================================================================
; sym.asm - the symbol table and the variables
; ==========================================================================
; holds:    var_clr, sym_find
; uses:     hbot, htop, rd_first
; data:     Per id, tables: skey the key, sinfo bit 7 array, bit 6 TI, TI$ or
;           ST, bit 5 a plain variable inferred INT, bits 1-0 the kind; vt a
;           variable's slot, an array's header.
; calls:
;   sym_find           a match of sinfo and the key, else a new id
; ==========================================================================

; ::::::::::::::
; var_clr - CLR: zero variables, drop arrays, reset READ, empty the heap.
;
; out:    aend, sfr, tsp, dlp, dyo  reset
; keeps:  SI
; ::::::::::::::
var_clr:
        mov     di,VARBASE
        mov     cx,SVLIM-VARBASE
        xor     al,al
        rep     stosb
        xor     bx,bx
@vc_a:
        test    [sinfo+bx],80h
        jz      @vc_n
        mov     di,bx
        add     di,bx
        mov     word ptr [vt+di],0
@vc_n:
        inc     bl
        jnz     @vc_a
        mov     eax,[hbot]
        mov     [aend],eax
        mov     eax,[htop]
        mov     [sfr],eax
        mov     [ahd],HDRBASE
        and     [tsp],0
        jmp     rd_first

; ::::::::::::::
; sym_find - the id of a name; a new one is made, a new variable zeroed.
;
; in:     AL     80h an array, 0 a variable
;         nkind  the kind
;         nkey   the name's first two bytes; hi 0 for a lone letter
; out:    AL  the id
; keeps:  SI, DI
; error:  ?OUT OF MEMORY  255 ids
; ::::::::::::::
sym_find:
        or      al,[nkind]
        mov     dl,al
        test    al,81h
        jnz     @sf_n
        mov     cx,[nkey]
        cmp     cx,'IT'
        je      @sf_y
        cmp     cx,'TS'
        jne     @sf_n
        test    dl,dl
        jnz     @sf_n
@sf_y:
        or      dl,40h
@sf_n:
        xor     bx,bx
@sf_lp:
        cmp     bl,[nsyms]
        je      @sf_new
        mov     cl,[sinfo+bx]
        test    cl,SI_INF
        jz      @F
        xor     cl,cl                   ; inferred INT: still the plain name
@@:
        cmp     dl,cl
        jne     @sf_s
        mov     ax,bx
        add     bx,bx
        mov     cx,[skey+bx]
        mov     bx,ax
        cmp     cx,[nkey]
        je      @sf_ok
@sf_s:
        inc     bl
        jnz     @sf_lp
@sf_om:
        jmp     err_om
@sf_new:
        cmp     bl,255
        jae     @sf_om
        mov     [sinfo+bx],dl
        mov     ax,bx
        add     bx,bx
        mov     cx,[nkey]
        mov     [skey+bx],cx
        and     word ptr [vt+bx],0
        test    dl,dl
        js      @sf_m
        mov     cx,bx
        add     cx,cx
        add     cx,VARBASE
        mov     [vt+bx],cx              ; a slot's id is in its address
        mov     bx,ax
        shl     bx,3
        and     word ptr [SVBASE+bx+D_LEN],0
        mov     bx,cx
        and     dword ptr [bx],0
        add     cx,VS
        mov     [vend],cx
@sf_m:
        mov     bx,ax
        inc     [nsyms]
@sf_ok:
        mov     al,bl
        ret
