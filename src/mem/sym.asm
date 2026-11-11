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
        mov     edi,VARBASE
        mov     ecx,SVLIM-VARBASE
        xor     al,al
        rep     stosb
        xor     ebx,ebx
@vc_a:
        test    [sinfo+ebx],80h
        jz      @vc_n
        mov     [vt+ebx*4],0
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
; keeps:  ESI, EDI
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
        xor     ebx,ebx
@sf_lp:
        cmp     bl,[nsyms]
        je      @sf_new
        mov     cl,[sinfo+ebx]
        test    cl,SI_INF
        jz      @F
        xor     cl,cl                   ; inferred INT: still the plain name
@@:
        cmp     dl,cl
        jne     @sf_s
        mov     cx,[skey+ebx*2]
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
        mov     [sinfo+ebx],dl
        mov     cx,[nkey]
        mov     [skey+ebx*2],cx
        and     [vt+ebx*4],0
        test    dl,dl
        js      @sf_m
        lea     ecx,[VARBASE+ebx*4]
        mov     [vt+ebx*4],ecx          ; a slot's id is in its address
        and     word ptr [SVBASE+ebx*8+D_LEN],0
        and     dword ptr [ecx],0
        add     ecx,VS
        mov     [vend],ecx
@sf_m:
        inc     [nsyms]
@sf_ok:
        mov     al,bl
        ret
