; ==========================================================================
; text.asm - the editor's text, the clipboard, files
; ==========================================================================
; holds:    tx_init, tx_off, tx_len, tx_get, tx_repl, tx_insl, tx_dell,
;           tx_read, tx_write, cb_*
; uses:     txseg, clpseg, wrkseg; INT 21h
; data:     The text is lines ended by CR, tx_end bytes, tx_n lines, at
;           least one. tx_ci, tx_co: a line's index and offset, where the
;           next lookup starts. The clipboard: cb_len bytes in clpseg, whole
;           lines (cb_ln 1, each ended by CR) or a piece of one.
; calls:
;   tx_repl, tx_insl, tx_dell  tx_off; tx_move the rest along
;   tx_read            the file to wrkseg, then CR-ended lines, tabs out
; ==========================================================================

TXMAX   = 0FF00h                ; the text's bytes at most
LNMAX   = 255                   ; a line's characters at most

; ::::::::::::::
; tx_init - one empty line.
; ::::::::::::::
tx_init:
        push    es
        mov     es,[txseg]
        mov     byte ptr es:[0],NL
        pop     es
        mov     [tx_end],1
        mov     [tx_n],1
        xor     ax,ax
        mov     [tx_ci],ax
        mov     [tx_co],ax
        ret

; ::::::::::::::
; tx_off - the offset of line AX.
;
; in:     AX  the line, below tx_n
; out:    BX  its first byte
; keeps:  all but BX
; ::::::::::::::
tx_off:
        push    es
        push    cx
        push    di
        mov     es,[txseg]
        mov     cx,ax
        xor     di,di
        cmp     ax,[tx_ci]
        jb      @txo_s
        sub     cx,[tx_ci]
        mov     di,[tx_co]
@txo_s:
        jcxz    @txo_d
        push    ax
        mov     al,NL
@txo_l:
        push    cx
        mov     cx,0FFFFh
        repne   scasb
        pop     cx
        loop    @txo_l
        pop     ax
@txo_d:
        mov     [tx_ci],ax
        mov     [tx_co],di
        mov     bx,di
        pop     di
        pop     cx
        pop     es
        ret

; ::::::::::::::
; tx_len - the length of the line at offset BX.
;
; out:    CX  it, its CR not counted
; keeps:  all but CX
; ::::::::::::::
tx_len:
        push    es
        push    di
        push    ax
        mov     es,[txseg]
        mov     di,bx
        mov     al,NL
        mov     cx,0FFFFh
        repne   scasb
        not     cx
        dec     cx
        pop     ax
        pop     di
        pop     es
        ret

; ::::::::::::::
; tx_get - line AX into DI.
;
; out:    CX  its length
; keeps:  all but CX
; ::::::::::::::
tx_get:
        push    bx
        push    si
        push    di
        push    ds
        call    tx_off
        call    tx_len
        mov     si,bx
        mov     ds,ss:[txseg]
        push    cx
        rep     movsb
        pop     cx
        pop     ds
        pop     di
        pop     si
        pop     bx
        ret

; ::::::::::::::
; tx_move - CX bytes from SI to DI in txseg, overlapping either way.
;
; keeps:  all
; ::::::::::::::
tx_move:
        pusha
        push    ds
        push    es
        mov     ax,ss:[txseg]
        mov     ds,ax
        mov     es,ax
        cmp     di,si
        jbe     @txm_f
        add     si,cx
        dec     si
        add     di,cx
        dec     di
        std
        rep     movsb
        cld
        jmp     @txm_x
@txm_f:
        rep     movsb
@txm_x:
        pop     es
        pop     ds
        popa
        ret

; ::::::::::::::
; tx_room - CF set, an Out of memory box, when CX more bytes do not fit.
; ::::::::::::::
tx_room:
        push    ax
        mov     ax,[tx_end]
        add     ax,cx
        jc      @txr_n
        cmp     ax,TXMAX
        ja      @txr_n
        pop     ax
        clc
        ret
@txr_n:
        pop     ax
        push    si
        mov     si,offset u_oom
        call    dl_ok
        pop     si
        stc
        ret

; ::::::::::::::
; tx_repl - line AX becomes CX characters from SI.
;
; out:    CF  set: no room, the line as it was
; keeps:  all
; ::::::::::::::
tx_repl:
        pusha
        call    tx_off
        mov     dx,cx                   ; the new length
        call    tx_len
        mov     bp,cx                   ; the old
        mov     cx,dx
        sub     cx,bp
        jbe     @txp_s
        call    tx_room
        jc      @txp_x
@txp_s:
        mov     cx,[tx_end]
        sub     cx,bx
        sub     cx,bp                   ; the tail from the old end
        push    si
        mov     si,bx
        add     si,bp
        mov     di,bx
        add     di,dx
        call    tx_move
        pop     si
        mov     ax,dx
        sub     ax,bp
        add     [tx_end],ax
        push    es
        mov     es,[txseg]
        mov     di,bx
        mov     cx,dx
        rep     movsb
        pop     es
        clc
@txp_x:
        popa
        ret

; ::::::::::::::
; tx_insl - a new line of CX characters from SI before line AX.
;
; in:     AX  at most tx_n
; out:    CF  set: no room
; keeps:  all
; ::::::::::::::
tx_insl:
        pusha
        inc     cx
        call    tx_room
        dec     cx
        jc      @txi_x
        mov     bx,[tx_end]
        cmp     ax,[tx_n]
        jae     @txi_e
        call    tx_off
@txi_e:
        push    si
        push    cx
        mov     si,bx
        mov     di,bx
        add     di,cx
        inc     di
        mov     cx,[tx_end]
        sub     cx,bx
        call    tx_move
        pop     cx
        pop     si
        push    es
        mov     es,[txseg]
        mov     di,bx
        push    cx
        rep     movsb
        mov     al,NL
        stosb
        pop     cx
        pop     es
        inc     cx
        add     [tx_end],cx
        inc     [tx_n]
        mov     [tx_ci],0
        mov     [tx_co],0
        clc
@txi_x:
        popa
        ret

; ::::::::::::::
; tx_dell - remove line AX; the last one left is emptied.
;
; keeps:  all
; ::::::::::::::
tx_dell:
        pusha
        cmp     [tx_n],1
        ja      @txd_d
        xor     cx,cx
        call    tx_repl
        popa
        ret
@txd_d:
        call    tx_off
        call    tx_len
        inc     cx
        mov     si,bx
        add     si,cx
        mov     di,bx
        sub     [tx_end],cx
        mov     cx,[tx_end]
        sub     cx,bx
        call    tx_move
        dec     [tx_n]
        mov     [tx_ci],0
        mov     [tx_co],0
        popa
        ret

; ::::::::::::::
; cb_put - CX bytes from SI in DS to the clipboard; AL 1 whole lines.
; ::::::::::::::
cb_put:
        pusha
        mov     [cb_ln],al
        mov     [cb_len],cx
        push    es
        mov     es,[clpseg]
        xor     di,di
        rep     movsb
        pop     es
        popa
        ret

; ::::::::::::::
; cb_lines - lines AX to DX (inclusive) to the clipboard.
; ::::::::::::::
cb_lines:
        pusha
        call    tx_off
        mov     si,bx
        xchg    ax,dx
        call    tx_off
        call    tx_len
        add     bx,cx
        inc     bx
        mov     cx,bx
        sub     cx,si
        mov     [cb_len],cx
        mov     [cb_ln],1
        push    ds
        push    es
        mov     es,ss:[clpseg]
        mov     ds,ss:[txseg]
        xor     di,di
        rep     movsb
        pop     es
        pop     ds
        popa
        ret

; ::::::::::::::
; tx_read - the file named at DX into the text: CR LF and LF end lines, a
; tab goes to the next column of 8, ^Z ends it, a long line is cut.
;
; out:    CF  set: AX the DOS error, the text as it was
; ::::::::::::::
tx_read:
        mov     ax,3D00h
        int     21h
        jc      @trd_r
        mov     bx,ax
        push    ds
        mov     ds,ss:[wrkseg]
        xor     dx,dx
        mov     cx,TXMAX
        mov     ah,3Fh
        int     21h
        pop     ds
        pushf
        push    ax
        mov     ah,3Eh
        int     21h
        pop     cx
        popf
        jc      @trd_r
        push    es
        push    ds
        mov     es,ss:[txseg]
        mov     ds,ss:[wrkseg]
        xor     si,si
        xor     di,di
        xor     dx,dx                   ; DX the column, BX the lines
        xor     bx,bx
        jcxz    @trd_e
@trd_l:
        lodsb
        cmp     al,1Ah
        je      @trd_e
        cmp     al,10
        je      @trd_n
        cmp     al,NL
        jne     @trd_t
        cmp     byte ptr [si],10
        jne     @trd_n
        inc     si
        dec     cx
        jz      @trd_n1
@trd_n:
        mov     al,NL
        stosb
        inc     bx
        xor     dx,dx
        jmp     @trd_c
@trd_n1:
        mov     al,NL
        stosb
        inc     bx
        xor     dx,dx
        jmp     @trd_e
@trd_t:
        cmp     dx,LNMAX
        jae     @trd_c
        cmp     al,9
        jne     @trd_p
@trd_s:
        mov     al,' '
        stosb
        inc     dx
        test    dx,7
        jz      @trd_c
        cmp     dx,LNMAX
        jb      @trd_s
        jmp     @trd_c
@trd_p:
        stosb
        inc     dx
@trd_c:
        cmp     di,TXMAX-LNMAX
        jae     @trd_e
        loop    @trd_l
@trd_e:
        cmp     di,0
        je      @trd_z
        cmp     byte ptr es:[di-1],NL
        je      @trd_f
@trd_z:
        mov     al,NL
        stosb
        inc     bx
@trd_f:
        pop     ds
        pop     es
        mov     [tx_end],di
        mov     [tx_n],bx
        mov     [tx_ci],0
        mov     [tx_co],0
        clc
@trd_r:
        ret

; ::::::::::::::
; tx_write - the text to the file named at DX, lines ended by CR LF.
;
; out:    CF  set: AX the DOS error
; ::::::::::::::
tx_write:
        push    dx
        push    es
        push    ds
        mov     es,ss:[wrkseg]
        mov     ds,ss:[txseg]
        xor     si,si
        xor     di,di
        mov     cx,ss:[tx_end]
@twr_l:
        lodsb
        stosb
        cmp     al,NL
        jne     @twr_n
        mov     al,10
        stosb
@twr_n:
        cmp     di,0FFF0h
        jae     @twr_e
        loop    @twr_l
@twr_e:
        pop     ds
        pop     es
        pop     dx
        push    di
        mov     ah,3Ch
        xor     cx,cx
        int     21h
        pop     cx
        jc      @twr_r
        mov     bx,ax
        push    ds
        mov     ds,ss:[wrkseg]
        xor     dx,dx
        mov     ah,40h
        int     21h
        pop     ds
        pushf
        push    ax
        mov     ah,3Eh
        int     21h
        pop     ax
        popf
@twr_r:
        ret
