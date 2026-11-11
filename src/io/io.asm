; ==========================================================================
; io.asm - console output and INPUT's lines
; ==========================================================================
; holds:    io_*
; uses:     plt_putc, plt_getline, msgs, kwlist
; calls:
;   io_field           io_prompt, io_read
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; io_int - print an INT as PRINT does: " " or "-", digits, " ".
;
; in:     EAX  the number
; keeps:  BX, SI, DI
; ::::::::::::::
io_int:
        mov     ecx,eax
        mov     al,' '
        test    ecx,ecx
        jns     @ioi_p
        neg     ecx
        mov     al,'-'
@ioi_p:
        call    io_putc
        mov     eax,ecx
        call    io_u24

; ::::::::::::::
; io_space - print a space.
;
; keeps:  all but AL
; ::::::::::::::
io_space:
        mov     al,' '

; ::::::::::::::
; io_putc - write a character and keep the output column.
;
; in:     AL    the character; NL ends the line
;         ocap  bit 7 sbuf, bit 6 VERIFY
; out:    pcol  the column
;         ocap  bit 0 set when VERIFY differs
; keeps:  all but AL
; ::::::::::::::
io_putc:
        test    [g_state.io.ocap],0C0h
        jnz     @iop_c
        inc     [g_state.io.pcol]
        cmp     al,NL
        jne     @jt_io1
        mov     [g_state.io.pcol],0
@jt_io1:
        jmp     plt_putc
@iop_c:
        js      @iop_s
        push    ebx
        mov     bl,al
        call    plt_getc
        cmp     al,bl
        je      @iop_e
        or      [g_state.io.ocap],1
@iop_e:
        pop     ebx
        ret
@iop_s:
        push    ebx
        movzx   ebx,[g_state.io.sbx]
        mov     [g_sbuf+ebx],al
        inc     [g_state.io.sbx]
        pop     ebx
        ret

; ::::::::::::::
; io_fresh - io_nl when the line has text.
;
; keeps:  all but AL
; ::::::::::::::
io_fresh:
        cmp     [g_state.io.pcol],0
        je      @iof_r

; ::::::::::::::
; io_nl - end the line.
;
; keeps:  all but AL
; ::::::::::::::
io_nl:
        mov     al,NL
        jmp     io_putc
@iof_r:
        ret

; ::::::::::::::
; io_num - print AX unsigned, digits only.
;
; in:     AX  the number
; keeps:  all but EAX
; ::::::::::::::
io_num:
        movzx   eax,ax

; ::::::::::::::
; io_u24 - print EAX unsigned, digits only.
;
; in:     EAX  the number
; keeps:  all but EAX
; ::::::::::::::
io_u24:
        push    ecx
        push    edx
        mov     ecx,10
        push    0FFFFh
@iou_d:
        xor     edx,edx
        div     ecx
        push    edx
        test    eax,eax
        jnz     @iou_d
@iou_p:
        pop     eax
        cmp     ax,0FFFFh
        je      @iou_e
        or      al,'0'
        call    io_putc
        jmp     @iou_p
@iou_e:
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; io_spaces - print CL spaces.
;
; in:     CL  the count
; keeps:  SI, DI
; ::::::::::::::
io_spaces:
        test    cl,cl
        jz      @ios_r
@ios_l:
        call    io_space
        dec     cl
        jnz     @ios_l
@ios_r:
        ret

; ::::::::::::::
; io_kw - print the keyword of a token.
;
; in:     AL  the token
; keeps:  all but AL
; ::::::::::::::
io_kw:
        and     al,7Fh
        push    ebx
        mov     ebx,offset g_kwlist
        jmp     io_nth

; ::::::::::::::
; io_msg - print message AL of msgs.
;
; in:     AL  the entry: E_*, MSG_*
; keeps:  all but AL
; ::::::::::::::
io_msg:
        push    ebx
        mov     ebx,offset g_msgs

; ::::::::::::::
; io_nth - print entry AL of the list at BX; a reference byte nests by io_msg.
;
; in:     AL  the entry
;         BX  the list; pushed by the caller, popped here
; keeps:  all but AL
; ::::::::::::::
io_nth:
        push    esi
        push    ecx
        push    edx
        mov     esi,ebx
        movzx   ecx,al
        jecxz    @ion_p
@ion_s:
        lodsb
        test    al,al
        jns     @ion_s
        loop    @ion_s
@ion_p:
        lodsb
        mov     dl,al
        and     al,7Fh
        cmp     al,5Fh
        jae     @ion_rh
        cmp     al,NL
        je      @ion_c
        cmp     al,20h
        jae     @ion_c
        add     al,43h
        and     al,7Fh
        jmp     @ion_r
@ion_rh:
        sub     al,3Ch
@ion_r:
        call    io_msg
        jmp     @ion_n
@ion_c:
        call    io_putc
@ion_n:
        test    dl,dl
        jns     @ion_p
        pop     edx
        pop     ecx
        pop     esi
        pop     ebx
        ret

; ::::::::::::::
; io_prompt - print "? " and read a line.
;
; out:    as io_read
; error:  ?OUT OF DATA  the end of the input
; ::::::::::::::
io_prompt:
        mov     al,'?'
        call    io_putc
        call    io_space

; ::::::::::::::
; io_read - read a line into inbuf.
;
; out:    inbuf  the line, 0-terminated
;         inlen  its length
;         inpos  FFh
;         pcol   0
; keeps:  SI
; error:  ?OUT OF DATA  the end of the input
; ::::::::::::::
io_read:
        call    plt_getline
        jnc     @F
        cmp     [g_state.rn.stpflg],0
        je      err_od
        and     [g_state.rn.stpflg],0              ; the STOP key: BREAK, no CONT
        and     [g_state.rn.contip],0
        xor     al,al                   ; E_BRK
        jmp     err_raise
@@:
        mov     [g_state.io.inlen],al
        movzx   ebx,al
        mov     byte ptr [g_inbuf+ebx],0
        mov     [g_state.io.pcol],0
        mov     [g_state.io.inpos],0FFh
        ret

; ::::::::::::::
; io_field - step to the next field; none left: ?? and a new line.
;
; in:     inpos  the end of the last field
; out:    inpos  the field
; keeps:  SI
; error:  ?OUT OF DATA  the end of the input
; ::::::::::::::
io_field:
        mov     bl,[g_state.io.inpos]
        inc     bl
        jz      @iof_d
        movzx   ebx,bl
        cmp     byte ptr [g_inbuf+ebx-1],','
        je      @iof_d
        test    [g_state.io.infl],80h
        jnz     @iof_rd
        mov     al,'?'
        call    io_putc
        call    io_prompt
        jmp     io_field
@iof_rd:
        call    io_read
        jmp     io_field
@iof_d:
        mov     [g_state.io.inpos],bl
        ret

        .data?
g_sbuf  db      256 dup(?)
g_inbuf db      256 dup(?)
        .code

include gen.inc

        end
