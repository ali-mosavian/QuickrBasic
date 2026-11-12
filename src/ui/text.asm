; ==========================================================================
; text.asm - the editor's text, the clipboard, files
; ==========================================================================
; holds:    tx_init, tx_off, tx_len, tx_get, tx_repl, tx_insl, tx_dell,
;           tx_read, tx_write, tx_mark, tx_mtog, tx_mclr, cb_*
; uses:     txb, clpb, wrkb; INT 21h
; data:     The text is lines ended by CR, tx_end bytes, tx_n lines, at
;           least one; a position in it is an offset in txb. tx_ci, tx_co:
;           a line's index and offset, where the next lookup starts. The
;           clipboard: cb_len bytes in clpb, whole
;           lines (cb_ln 1, each ended by CR) or a piece of one. A line's
;           mark (txmk, 0 or 1: a breakpoint) moves with it.
; calls:
;   tx_repl, tx_insl, tx_dell  tx_off; tx_move the rest along
;   tx_read            the file to wrkb, then CR-ended lines, tabs out
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; tx_init - one empty line.
; ::::::::::::::
tx_init:
        mov     byte ptr [g_txb],NL
        mov     [g_ui.tx.tx_end],1
        mov     [g_ui.tx.tx_n],1
        xor     ax,ax
        mov     [g_ui.tx.tx_ci],ax
        mov     [g_ui.tx.tx_co],ax
        jmp     tx_mclr

; ::::::::::::::
; tx_off - the offset of line AX.
;
; in:     AX  the line, below tx_n
; out:    EBX  its first byte, an offset in txb
; keeps:  all but EBX
; ::::::::::::::
tx_off:
        push    ecx
        push    edi
        movzx   ecx,ax
        mov     edi,offset g_txb
        cmp     ax,[g_ui.tx.tx_ci]
        jb      @txo_s
        sub     cx,[g_ui.tx.tx_ci]
        movzx   edi,[g_ui.tx.tx_co]
        add     edi,offset g_txb
@txo_s:
        jecxz   @txo_d
        push    eax
        mov     al,NL
@txo_l:
        push    ecx
        mov     ecx,10000h
        repne   scasb
        pop     ecx
        loop    @txo_l
        pop     eax
@txo_d:
        sub     edi,offset g_txb
        mov     [g_ui.tx.tx_ci],ax
        mov     [g_ui.tx.tx_co],di
        mov     ebx,edi
        pop     edi
        pop     ecx
        ret

; ::::::::::::::
; tx_len - the length of the line at offset EBX.
;
; out:    ECX  it, its CR not counted
; keeps:  all but ECX
; ::::::::::::::
tx_len:
        push    edi
        push    eax
        lea     edi,[g_txb+ebx]
        mov     al,NL
        mov     ecx,-1
        repne   scasb
        not     ecx
        dec     ecx
        pop     eax
        pop     edi
        ret

; ::::::::::::::
; tx_get - line AX into EDI.
;
; out:    ECX  its length
; keeps:  all but ECX
; ::::::::::::::
tx_get:
        push    ebx
        push    esi
        push    edi
        call    tx_off
        call    tx_len
        lea     esi,[g_txb+ebx]
        push    ecx
        rep     movsb
        pop     ecx
        pop     edi
        pop     esi
        pop     ebx
        ret

; ::::::::::::::
; tx_move - ECX bytes from offset ESI to offset EDI in txb, overlapping
; either way.
;
; keeps:  all
; ::::::::::::::
tx_move:
        pushad
        add     esi,offset g_txb
        add     edi,offset g_txb
        cmp     edi,esi
        jbe     @txm_f
        lea     esi,[esi+ecx-1]
        lea     edi,[edi+ecx-1]
        std
        rep     movsb
        cld
        jmp     @txm_x
@txm_f:
        rep     movsb
@txm_x:
        popad
        ret

; ::::::::::::::
; tx_room - CF set, an Out of memory box, when CX more bytes do not fit.
; ::::::::::::::
tx_room:
        push    eax
        mov     ax,[g_ui.tx.tx_end]
        add     ax,cx
        jc      @txr_n
        cmp     ax,TXMAX
        ja      @txr_n
        pop     eax
        clc
        ret
@txr_n:
        pop     eax
        push    esi
        mov     esi,offset u_oom
        call    dl_ok
        pop     esi
        stc
        ret

; ::::::::::::::
; tx_repl - line AX becomes CX characters from ESI.
;
; out:    CF  set: no room, the line as it was
; keeps:  all
; ::::::::::::::
tx_repl:
        pushad
        call    tx_off
        movzx   edx,cx                  ; the new length
        call    tx_len
        mov     ebp,ecx                 ; the old
        mov     ecx,edx
        sub     ecx,ebp
        jbe     @txp_s
        call    tx_room
        jc      @txp_x
@txp_s:
        movzx   ecx,[g_ui.tx.tx_end]
        sub     ecx,ebx
        sub     ecx,ebp                 ; the tail from the old end
        push    esi
        lea     esi,[ebx+ebp]
        lea     edi,[ebx+edx]
        call    tx_move
        pop     esi
        mov     eax,edx
        sub     eax,ebp
        add     [g_ui.tx.tx_end],ax
        lea     edi,[g_txb+ebx]
        mov     ecx,edx
        rep     movsb
        clc
@txp_x:
        popad
        ret

; ::::::::::::::
; tx_insl - a new line of CX characters from ESI before line AX.
;
; in:     AX  at most tx_n
; out:    CF  set: no room
; keeps:  all
; ::::::::::::::
tx_insl:
        pushad
        movzx   ecx,cx
        inc     ecx
        call    tx_room
        dec     ecx
        jc      @txi_x
        movzx   ebx,[g_ui.tx.tx_end]
        cmp     ax,[g_ui.tx.tx_n]
        jae     @txi_e
        call    tx_off
@txi_e:
        push    esi
        push    ecx
        mov     esi,ebx
        lea     edi,[ebx+ecx+1]
        movzx   ecx,[g_ui.tx.tx_end]
        sub     ecx,ebx
        call    tx_move
        pop     ecx
        pop     esi
        lea     edi,[g_txb+ebx]
        push    ecx
        rep     movsb
        mov     al,NL
        stosb
        pop     ecx
        inc     ecx
        add     [g_ui.tx.tx_end],cx
        mov     edx,[esp+28]            ; the marks from line AX up one (pushad's)
        movzx   eax,[g_ui.tx.tx_n]
        inc     [g_ui.tx.tx_n]
        movzx   edx,dx
@txi_m:
        cmp     eax,edx
        jbe     @F
        mov     cl,[g_txmk+eax-1]
        mov     [g_txmk+eax],cl
        dec     eax
        jmp     @txi_m
@@:
        mov     byte ptr [g_txmk+edx],0
        mov     [g_ui.tx.tx_ci],0
        mov     [g_ui.tx.tx_co],0
        clc
@txi_x:
        popad
        ret

; ::::::::::::::
; tx_dell - remove line AX; the last one left is emptied.
;
; keeps:  all
; ::::::::::::::
tx_dell:
        pushad
        cmp     [g_ui.tx.tx_n],1
        ja      @txd_d
        xor     ecx,ecx
        call    tx_repl
        popad
        ret
@txd_d:
        call    tx_off
        call    tx_len
        inc     ecx
        lea     esi,[ebx+ecx]
        mov     edi,ebx
        sub     [g_ui.tx.tx_end],cx
        movzx   ecx,[g_ui.tx.tx_end]
        sub     ecx,ebx
        call    tx_move
        dec     [g_ui.tx.tx_n]
        movzx   eax,word ptr [esp+28]   ; the marks after line AX down one
@txd_m:
        cmp     ax,[g_ui.tx.tx_n]
        jae     @F
        mov     cl,[g_txmk+eax+1]
        mov     [g_txmk+eax],cl
        inc     eax
        jmp     @txd_m
@@:
        mov     [g_ui.tx.tx_ci],0
        mov     [g_ui.tx.tx_co],0
        popad
        ret

; ::::::::::::::
; cb_put - CX bytes from ESI to the clipboard; AL 1 whole lines.
; ::::::::::::::
cb_put:
        pushad
        mov     [g_ui.tx.cb_ln],al
        mov     [g_ui.tx.cb_len],cx
        movzx   ecx,cx
        mov     edi,offset g_clpb
        rep     movsb
        popad
        ret

; ::::::::::::::
; cb_lines - lines AX to DX (inclusive) to the clipboard.
; ::::::::::::::
cb_lines:
        pushad
        call    tx_off
        mov     esi,ebx
        xchg    ax,dx
        call    tx_off
        call    tx_len
        lea     ecx,[ebx+ecx+1]
        sub     ecx,esi
        mov     [g_ui.tx.cb_len],cx
        mov     [g_ui.tx.cb_ln],1
        add     esi,offset g_txb
        mov     edi,offset g_clpb
        rep     movsb
        popad
        ret

; ::::::::::::::
; tx_read - the file named at EDX into the text: CR LF and LF end lines, a
; tab goes to the next column of 8, ^Z ends it, a long line is cut.
;
; out:    CF  set: AX the DOS error, the text as it was
; ::::::::::::::
tx_read:
        mov     ax,3D00h
        int     21h
        jc      @trd_r
        mov     ebx,eax
        mov     edx,offset g_wrkb
        mov     ecx,TXMAX
        mov     ah,3Fh
        int     21h
        pushf
        push    eax
        mov     ah,3Eh
        int     21h
        pop     ecx
        popf
        jc      @trd_r
        movzx   ecx,cx
        mov     esi,offset g_wrkb
        mov     edi,offset g_txb
        xor     edx,edx                 ; EDX the column, EBX the lines
        xor     ebx,ebx
        jecxz   @trd_e
@trd_l:
        lodsb
        cmp     al,1Ah
        je      @trd_e
        cmp     al,10
        je      @trd_n
        cmp     al,NL
        jne     @trd_t
        cmp     byte ptr [esi],10
        jne     @trd_n
        inc     esi
        dec     ecx
        jz      @trd_n1
@trd_n:
        mov     al,NL
        stosb
        inc     ebx
        xor     edx,edx
        jmp     @trd_c
@trd_n1:
        mov     al,NL
        stosb
        inc     ebx
        xor     edx,edx
        jmp     @trd_e
@trd_t:
        cmp     edx,LNMAX
        jae     @trd_c
        cmp     al,9
        jne     @trd_p
@trd_s:
        mov     al,' '
        stosb
        inc     edx
        test    edx,7
        jz      @trd_c
        cmp     edx,LNMAX
        jb      @trd_s
        jmp     @trd_c
@trd_p:
        stosb
        inc     edx
@trd_c:
        cmp     edi,offset g_txb+TXMAX-LNMAX
        jae     @trd_e
        loop    @trd_l
@trd_e:
        cmp     edi,offset g_txb
        je      @trd_z
        cmp     byte ptr [edi-1],NL
        je      @trd_f
@trd_z:
        mov     al,NL
        stosb
        inc     ebx
@trd_f:
        sub     edi,offset g_txb
        mov     [g_ui.tx.tx_end],di
        mov     [g_ui.tx.tx_n],bx
        mov     [g_ui.tx.tx_ci],0
        mov     [g_ui.tx.tx_co],0
        call    tx_mclr
        clc
@trd_r:
        ret

; ::::::::::::::
; tx_mark - line AX's mark.
;
; out:    AL  it
; keeps:  all but EAX
; ::::::::::::::
tx_mark:
        movzx   eax,ax
        mov     al,[g_txmk+eax]
        ret

; ::::::::::::::
; tx_mtog - line AX's mark: on, off.
;
; keeps:  all
; ::::::::::::::
tx_mtog:
        push    eax
        movzx   eax,ax
        xor     byte ptr [g_txmk+eax],1
        pop     eax
        ret

; ::::::::::::::
; tx_mclr - no line marked.
;
; keeps:  all
; ::::::::::::::
tx_mclr:
        pushad
        mov     edi,offset g_txmk
        mov     ecx,TXMAX/4
        xor     eax,eax
        rep     stosd
        popad
        ret

        .data?
g_txmk  db      TXMAX dup(?)            ; a byte per line
        .code

; ::::::::::::::
; tx_write - the text to the file named at EDX, lines ended by CR LF.
;
; out:    CF  set: AX the DOS error
; ::::::::::::::
tx_write:
        push    edx
        mov     esi,offset g_txb
        mov     edi,offset g_wrkb
        movzx   ecx,[g_ui.tx.tx_end]
@twr_l:
        lodsb
        stosb
        cmp     al,NL
        jne     @twr_n
        mov     al,10
        stosb
@twr_n:
        cmp     edi,offset g_wrkb+0FFF0h
        jae     @twr_e
        loop    @twr_l
@twr_e:
        pop     edx
        sub     edi,offset g_wrkb
        push    edi
        mov     ah,3Ch
        xor     ecx,ecx
        int     21h
        pop     ecx
        jc      @twr_r
        mov     ebx,eax
        mov     edx,offset g_wrkb
        mov     ah,40h
        int     21h
        pushf
        push    eax
        mov     ah,3Eh
        int     21h
        pop     eax
        popf
@twr_r:
        ret


        end
