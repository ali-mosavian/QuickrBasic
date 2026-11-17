; ==========================================================================
; scr.asm - the text screen: cells, strings, boxes, shadows
; ==========================================================================
; holds:    sc_at, sc_fill, sc_str, sc_strn, sc_box, sc_sep, sc_shadow,
;           sc_attr, sc_cur, sc_shape, sc_flip
; uses:     INT 10h; port 3DAh
; data:     Drawing goes to a copy of page 0 at scrseg; sc_flip shows it.
;           A position is DH the row, DL the column. A string's '&' marks
;           its hotkey, drawn with BH; a plain string has BH = AH.
; calls:
;   sc_box             a frame, filled, its title centred
;     sc_fill          a run of one cell
; ==========================================================================

; ::::::::::::::
; sc_at - DI = the cell at DH, DL.
;
; keeps:  all but DI
; ::::::::::::::
sc_at:
        push    ax
        movzx   ax,dh
        imul    ax,80
        movzx   di,dl
        add     di,ax
        add     di,di
        pop     ax
        ret

; ::::::::::::::
; sc_fill - CX cells of AX from DH, DL.
;
; out:    DL  past them
; keeps:  all but DL, DI
; ::::::::::::::
sc_fill:
        push    es
        push    cx
        call    sc_at
        mov     es,ss:[scrseg]
        add     dl,cl
        rep     stosw
        pop     cx
        pop     es
        ret

; ::::::::::::::
; sc_str - the 0-ended string at SI, attribute AH, its hotkey BH.
;
; out:    SI  past its 0
;         DL  past it
; keeps:  all but SI, DL, DI
; ::::::::::::::
sc_str:
        push    es
        push    ax
        call    sc_at
        mov     es,ss:[scrseg]
@scs_l:
        lodsb
        test    al,al
        jz      @scs_e
        cmp     al,'&'
        jne     @scs_p
        lodsb
        push    ax
        mov     ah,bh
        stosw
        pop     ax
        inc     dl
        jmp     @scs_l
@scs_p:
        stosw
        inc     dl
        jmp     @scs_l
@scs_e:
        pop     ax
        pop     es
        ret

; ::::::::::::::
; sc_strn - CX characters at SI, attribute AH; '&' not special.
;
; out:    SI  past them
;         DL  past them
; keeps:  all but SI, DL, DI
; ::::::::::::::
sc_strn:
        jcxz    @scn_r
        push    es
        push    ax
        push    cx
        call    sc_at
        mov     es,ss:[scrseg]
        add     dl,cl
@scn_l:
        lodsb
        stosw
        loop    @scn_l
        pop     cx
        pop     ax
        pop     es
@scn_r:
        ret

; ::::::::::::::
; sc_slen - the length of the 0-ended string at SI, '&' not counted.
;
; out:    CX  it
; keeps:  all but CX
; ::::::::::::::
sc_slen:
        push    si
        push    ax
        xor     cx,cx
@ssl_l:
        lodsb
        test    al,al
        jz      @ssl_e
        cmp     al,'&'
        je      @ssl_l
        inc     cx
        jmp     @ssl_l
@ssl_e:
        pop     ax
        pop     si
        ret

; ::::::::::::::
; sc_box - a single-line frame of CH rows, CL columns at DH, DL, filled
; with spaces, attribute AH; SI its title, centred, or 0.
;
; keeps:  all
; ::::::::::::::
sc_box:
        pusha
        mov     bx,cx
        mov     al,0DAh
        mov     cl,0BFh
        call    @scb_r
        movzx   cx,bh
        sub     cx,2
        jbe     @scb_b
@scb_m:
        inc     dh
        push    cx
        mov     al,0B3h
        mov     cl,al
        mov     ch,' '
        call    @scb_s
        pop     cx
        loop    @scb_m
@scb_b:
        inc     dh
        mov     al,0C0h
        mov     cl,0D9h
        call    @scb_r
        popa
        test    si,si
        jz      @scb_x
        pusha
        mov     bx,cx
        call    sc_slen
        add     cx,2
        movzx   di,bl
        sub     di,cx
        shr     di,1
        add     dx,di
        mov     bh,ah
        mov     al,' '
        call    @scb_1
        call    sc_str
        mov     al,' '
        call    @scb_1
        popa
@scb_x:
        ret
; a border row: AL left, CL right, CH 0C4h
@scb_r:
        mov     ch,0C4h
; a row: AL left, CH the inside, CL right
@scb_s:
        push    dx
        call    @scb_1
        mov     al,ch
        push    cx
        movzx   cx,bl
        sub     cx,2
        call    sc_fill
        pop     cx
        mov     al,cl
        call    @scb_1
        pop     dx
        ret
@scb_1:
        push    cx
        mov     cx,1
        call    sc_fill
        pop     cx
        ret

; ::::::::::::::
; sc_sep - a separator ├──┤ of CL columns at DH, DL.
;
; keeps:  all but DI
; ::::::::::::::
sc_sep:
        push    ax
        push    cx
        push    dx
        movzx   cx,cl
        push    cx
        mov     al,0C3h
        mov     cx,1
        call    sc_fill
        pop     cx
        push    cx
        sub     cx,2
        mov     al,0C4h
        call    sc_fill
        mov     al,0B4h
        mov     cx,1
        call    sc_fill
        pop     cx
        pop     dx
        pop     cx
        pop     ax
        ret

; ::::::::::::::
; sc_attr - CX cells from DH, DL take attribute AH; their characters stay.
;
; keeps:  all but DI
; ::::::::::::::
sc_attr:
        jcxz    @sat_r
        push    es
        push    cx
        call    sc_at
        mov     es,ss:[scrseg]
        inc     di
@sat_l:
        mov     es:[di],ah
        add     di,2
        loop    @sat_l
        pop     cx
        pop     es
@sat_r:
        ret

; ::::::::::::::
; sc_shadow - the shadow of a box of CH rows, CL columns at DH, DL: two
; columns right, one row below, dark.
;
; keeps:  all
; ::::::::::::::
sc_shadow:
        pusha
        mov     bx,cx
        mov     ah,08h
        inc     dh
        add     dl,bl
        movzx   si,bh
@ssh_r:
        cmp     dh,25
        jae     @ssh_b
        cmp     dl,80
        jae     @ssh_n
        mov     cx,2
        cmp     dl,79
        jne     @ssh_a
        dec     cx
@ssh_a:
        call    sc_attr
@ssh_n:
        inc     dh
        dec     si
        jnz     @ssh_r
@ssh_b:
        popa
        pusha
        mov     bx,cx
        add     dh,bh
        cmp     dh,25
        jae     @ssh_x
        add     dl,2
        movzx   cx,bl
        mov     al,80
        sub     al,dl
        jbe     @ssh_x
        cmp     cl,al
        jbe     @ssh_c
        mov     cl,al
@ssh_c:
        mov     ah,08h
        call    sc_attr
@ssh_x:
        popa
        ret

; ::::::::::::::
; sc_cur - the cursor at DH, DL on page 0.
; ::::::::::::::
sc_cur:
        pusha
        mov     ah,2
        xor     bh,bh
        int     10h
        popa
        ret

; ::::::::::::::
; sc_shape - the cursor's shape: CX its start and end lines; 2000h hides it.
; ::::::::::::::
sc_shape:
        pusha
        mov     ah,1
        int     10h
        popa
        ret

; ::::::::::::::
; sc_flip - the drawn screen to page 0, in the vertical retrace: no tearing.
; ::::::::::::::
sc_flip:
        pusha
        push    ds
        push    es
        mov     dx,3DAh
@scf_a:
        in      al,dx                   ; past a retrace already going
        test    al,8
        jnz     @scf_a
@scf_b:
        in      al,dx
        test    al,8
        jz      @scf_b
        mov     ds,ss:[scrseg]
        push    0B800h
        pop     es
        xor     si,si
        xor     di,di
        mov     cx,2000
        rep     movsw
        pop     es
        pop     ds
        popa
        ret
