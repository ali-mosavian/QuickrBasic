; ==========================================================================
; scr.asm - the text screen: cells, strings, boxes, shadows
; ==========================================================================
; holds:    sc_at, sc_fill, sc_str, sc_strn, sc_box, sc_sep, sc_shadow,
;           sc_attr, sc_cur, sc_shape, sc_flip
; uses:     INT 10h; port 3DAh
; data:     Drawing goes to a copy of page 0 at scrb; sc_flip shows it.
;           An attribute is a slot, the theme's (theme.asm).
;           A position is DH the row, DL the column. A string's '&' marks
;           its hotkey, drawn with BH; a plain string has BH = AH.
; calls:
;   sc_box             a frame, filled, its title centred
;     sc_fill          a run of one cell
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; sc_at - EDI = the cell at DH, DL in scrb.
;
; keeps:  all but EDI
; ::::::::::::::
sc_at:
        push    eax
        movzx   eax,dh
        imul    eax,80
        movzx   edi,dl
        add     edi,eax
        lea     edi,[g_scrb+edi*2]
        pop     eax
        ret

; ::::::::::::::
; sc_fill - CX cells of AX from DH, DL.
;
; out:    DL  past them
; keeps:  all but DL, EDI
; ::::::::::::::
sc_fill:
        push    eax
        push    ecx
        call    th_ah
        movzx   ecx,cx
        call    sc_at
        add     dl,cl
        rep     stosw
        pop     ecx
        pop     eax
        ret

; ::::::::::::::
; sc_str - the 0-ended string at ESI, attribute AH, its hotkey BH.
;
; out:    ESI  past its 0
;         DL   past it
; keeps:  all but ESI, DL, EDI
; ::::::::::::::
sc_str:
        push    eax
        push    ebx
        call    th_ah
        xchg    ah,bh
        call    th_ah
        xchg    ah,bh
        call    sc_at
@scs_l:
        lodsb
        test    al,al
        jz      @scs_e
        cmp     al,'&'
        jne     @scs_p
        lodsb
        push    eax
        mov     ah,bh
        stosw
        pop     eax
        inc     dl
        jmp     @scs_l
@scs_p:
        stosw
        inc     dl
        jmp     @scs_l
@scs_e:
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; sc_strn - CX characters at ESI, attribute AH; '&' not special.
;
; out:    ESI  past them
;         DL   past them
; keeps:  all but ESI, DL, EDI
; ::::::::::::::
sc_strn:
        jcxz    @scn_r
        push    eax
        push    ecx
        call    th_ah
        movzx   ecx,cx
        call    sc_at
        add     dl,cl
@scn_l:
        lodsb
        stosw
        loop    @scn_l
        pop     ecx
        pop     eax
@scn_r:
        ret

; ::::::::::::::
; sc_slen - the length of the 0-ended string at ESI, '&' not counted.
;
; out:    ECX  it
; keeps:  all but ECX
; ::::::::::::::
sc_slen:
        push    esi
        push    eax
        xor     ecx,ecx
@ssl_l:
        lodsb
        test    al,al
        jz      @ssl_e
        cmp     al,'&'
        je      @ssl_l
        inc     ecx
        jmp     @ssl_l
@ssl_e:
        pop     eax
        pop     esi
        ret

; ::::::::::::::
; sc_box - a single-line frame of CH rows, CL columns at DH, DL, filled
; with spaces, attribute AH; ESI its title, centred, or 0.
;
; keeps:  all
; ::::::::::::::
sc_box:
        pushad
        mov     bx,cx
        mov     al,0DAh
        mov     cl,0BFh
        call    @scb_r
        movzx   ecx,bh
        sub     ecx,2
        jbe     @scb_b
@scb_m:
        inc     dh
        push    ecx
        mov     al,0B3h
        mov     cl,al
        mov     ch,' '
        call    @scb_s
        pop     ecx
        loop    @scb_m
@scb_b:
        inc     dh
        mov     al,0C0h
        mov     cl,0D9h
        call    @scb_r
        popad
        test    esi,esi
        jz      @scb_x
        pushad
        mov     bx,cx
        call    sc_slen
        add     ecx,2
        movzx   edi,bl
        sub     edi,ecx
        shr     edi,1
        add     dx,di
        mov     bh,ah
        mov     al,' '
        call    @scb_1
        call    sc_str
        mov     al,' '
        call    @scb_1
        popad
@scb_x:
        ret
; a border row: AL left, CL right, CH 0C4h
@scb_r:
        mov     ch,0C4h
; a row: AL left, CH the inside, CL right
@scb_s:
        push    edx
        call    @scb_1
        mov     al,ch
        push    ecx
        movzx   cx,bl
        sub     cx,2
        call    sc_fill
        pop     ecx
        mov     al,cl
        call    @scb_1
        pop     edx
        ret
@scb_1:
        push    ecx
        mov     cx,1
        call    sc_fill
        pop     ecx
        ret

; ::::::::::::::
; sc_sep - a separator ├──┤ of CL columns at DH, DL.
;
; keeps:  all but EDI
; ::::::::::::::
sc_sep:
        push    eax
        push    ecx
        push    edx
        movzx   cx,cl
        push    ecx
        mov     al,0C3h
        mov     cx,1
        call    sc_fill
        pop     ecx
        push    ecx
        sub     cx,2
        mov     al,0C4h
        call    sc_fill
        mov     al,0B4h
        mov     cx,1
        call    sc_fill
        pop     ecx
        pop     edx
        pop     ecx
        pop     eax
        ret

; ::::::::::::::
; sc_attr - CX cells from DH, DL take attribute AH; their characters stay.
;
; keeps:  all but EDI
; ::::::::::::::
sc_attr:
        jcxz    @sat_r
        push    eax
        push    ecx
        call    th_ah
        movzx   ecx,cx
        call    sc_at
        inc     edi
@sat_l:
        mov     [edi],ah
        add     edi,2
        loop    @sat_l
        pop     ecx
        pop     eax
@sat_r:
        ret

; ::::::::::::::
; sc_shadow - the shadow of a box of CH rows, CL columns at DH, DL: two
; columns right, one row below, dark.
;
; keeps:  all
; ::::::::::::::
sc_shadow:
        pushad
        mov     bx,cx
        mov     ah,A_SHD
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
        popad
        pushad
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
        mov     ah,A_SHD
        call    sc_attr
@ssh_x:
        popad
        ret

; ::::::::::::::
; sc_cur - the cursor at DH, DL on page 0.
; ::::::::::::::
sc_cur:
        pushad
        mov     ah,2
        xor     bh,bh
        int     10h
        popad
        ret

; ::::::::::::::
; sc_shape - the cursor's shape: CX its start and end lines; 2000h hides it.
; ::::::::::::::
sc_shape:
        pushad
        mov     ah,1
        int     10h
        popad
        ret

; ::::::::::::::
; sc_flip - the drawn screen to page 0, in the vertical retrace: no tearing.
; ::::::::::::::
sc_flip:
        pushad
        mov     dx,3DAh
@scf_a:
        in      al,dx                   ; past a retrace already going
        test    al,8
        jnz     @scf_a
@scf_b:
        in      al,dx
        test    al,8
        jz      @scf_b
        mov     esi,offset g_scrb
        mov     edi,VIDEO
        mov     ecx,2000
        rep     movsw
        popad
        ret

        .data?
g_scrb  db      4000 dup(?)             ; the screen as drawn, then shown
        .code

        end
