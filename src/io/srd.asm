; ==========================================================================
; srd.asm - a source file's lines, streamed to the lexer
; ==========================================================================
; holds:    sr_line, sr_mem, sr_more, cn_init, cn_byte, cn_eol
; uses:     plt_getb, ch_skiplf
; data:     A logical line is physical lines joined where one ends in a
;           blank and '_', outside a string and a comment: BC's
;           continuation; the '_' becomes a space. The window, inbuf, holds
;           what of it fits, 0 after; sr_more slides it for lx_line and
;           reads on, so a line is as long as the store takes. Blanks in
;           code wait until a byte follows them: a line's last ones go.
;           cn_*: whether a physical line continues, by its bytes.
; ==========================================================================

include x86f.inc
        .code

SRCAP   = INMAX-4                       ; the window's text: then it slides

; ::::::::::::::
; sr_line - the next logical line of the input channel into inbuf.
;
; out:    EAX  its length in the window
;         CF   set at the end, nothing read
; ::::::::::::::
sr_line:
        and     [g_srmem],0
        mov     [g_srfe],offset g_inbuf
        and     [g_srend],0
        and     [g_srpend],0
        and     [g_srany],0
        call    cn_init
        call    sr_fill
        mov     eax,[g_srfe]
        sub     eax,offset g_inbuf
        cmp     [g_srany],0
        jne     @F
        stc
        ret
@@:
        clc
        ret

; ::::::::::::::
; sr_mem - the line at ESI, 0-ended, into inbuf after EDI: what fits, the
; rest by sr_more. No line ends in it: none continues.
;
; out:    EAX  the window's length
; ::::::::::::::
sr_mem:
        mov     [g_srmem],esi
        mov     [g_srfe],edi
        and     [g_srend],0
        and     [g_srpend],0
        call    cn_init
        call    sr_fill
        mov     eax,[g_srfe]
        sub     eax,offset g_inbuf
        ret

; ::::::::::::::
; sr_more - lx_line's window used up: the bytes from ESI on to its start,
; then more of the line after them.
;
; in:     ESI  the first byte kept
; out:    ESI  that byte, moved
;         EAX  how far it moved
;         CF   set: the line has no more; nothing moved
; ::::::::::::::
sr_more:
        cmp     [g_srend],0
        jne     @srm_n
        push    ecx
        push    edi
        mov     ecx,[g_srfe]
        sub     ecx,esi
        mov     edi,offset g_inbuf
        mov     eax,esi
        sub     eax,edi
        push    eax
        rep     movsb
        mov     [g_srfe],edi
        call    sr_fill
        pop     eax
        pop     edi
        pop     ecx
        mov     esi,offset g_inbuf
        clc
        ret
@srm_n:
        stc
        ret

; sr_fill - the window filled on from srfe: to the logical line's end
; (srend set) or SRCAP; 0 after.
sr_fill:
        push    eax
        push    ecx
        push    edi
        mov     edi,[g_srfe]
@srf_l:
        mov     eax,edi
        sub     eax,offset g_inbuf
        cmp     eax,SRCAP               ; full, a '_' not last: it slides
        jb      @srf_g
        cmp     byte ptr [edi-1],'_'
        jne     @srf_x
@srf_g:
        mov     ecx,[g_srmem]           ; a line in memory: its 0 the end
        jecxz   @srf_f
        mov     al,[ecx]
        test    al,al
        jz      @srf_z
        inc     [g_srmem]
        jmp     @srf_h
@srf_f:
        call    plt_getb
        jc      @srf_z
@srf_h:
        mov     [g_srany],1
        cmp     al,10
        je      @srf_e
        cmp     al,13
        jne     @F
        call    ch_skiplf               ; CR LF: one line's end
        jmp     @srf_e
@@:
        call    cn_byte
        cmp     [g_cnst],0
        jne     @srf_p                  ; in a string, a comment: as it is
        cmp     al,' '
        je      @srf_b
        cmp     al,9
        jne     @srf_p
@srf_b:
        inc     [g_srpend]              ; a blank in code: when a byte follows
        jmp     @srf_l
@srf_p:
        mov     ecx,[g_srpend]
        jecxz   @F
        push    eax
        mov     al,' '
        rep     stosb
        pop     eax
        and     [g_srpend],0
@@:
        stosb
        jmp     @srf_l
@srf_e:
        and     [g_srpend],0            ; the line's last blanks go
        call    cn_eol
        jnc     @srf_z
        inc     [g_state.io.fline]      ; continued: the '_' a blank, then the
        mov     byte ptr [edi-1],' '    ; next line's bytes
        call    cn_init
        jmp     @srf_l
@srf_z:
        mov     [g_srend],1
@srf_x:
        mov     byte ptr [edi],0
        mov     [g_srfe],edi
        pop     edi
        pop     ecx
        pop     eax
        ret

; ::::::::::::::
; cn_init - a physical line starts: in code, no word, no '_'.
; ::::::::::::::
cn_init:
        and     [g_cnst],0
        and     [g_cnw],0
        and     [g_cnu],0
        mov     [g_cnb],1               ; a blank before: the line's start
        ret

; ::::::::::::::
; cn_byte - the line's next byte AL: in a string, in a comment (' or REM),
; whether the last byte in code is a '_' a blank came before.
;
; keeps:  all
; ::::::::::::::
cn_byte:
        cmp     [g_cnst],2              ; a comment: to the line's end
        je      @cnb_r
        cmp     [g_cnst],1
        je      @cnb_s
        cmp     al,'"'
        je      @cnb_q
        cmp     al,"'"
        je      @cnb_c
        cmp     al,' '
        je      @cnb_b
        cmp     al,9
        je      @cnb_b
        push    eax                     ; a word's letters: REM a comment
        or      al,20h
        sub     al,'a'
        cmp     al,26
        pop     eax
        jae     @cnb_n
        cmp     [g_cnw],3
        jae     @F
        push    ebx
        movzx   ebx,[g_cnw]
        push    eax
        or      al,20h
        mov     [g_cnrem+ebx],al
        pop     eax
        pop     ebx
@@:
        inc     [g_cnw]
        jmp     @cnb_x
@cnb_n:
        call    @cnb_w
        cmp     al,'_'
        jne     @cnb_x
        mov     ah,[g_cnb]              ; a '_' after a blank: perhaps the end
        mov     [g_cnu],ah
        mov     [g_cnb],0
        ret
@cnb_b:
        call    @cnb_w
        mov     [g_cnb],1
        ret
@cnb_q:
        mov     [g_cnst],1
        jmp     @cnb_x
@cnb_s:
        cmp     al,'"'
        jne     @cnb_r
        and     [g_cnst],0
        jmp     @cnb_x
@cnb_c:
        mov     [g_cnst],2
@cnb_x:
        and     [g_cnu],0
        mov     [g_cnb],0
@cnb_r:
        ret
@cnb_w:                                 ; a word ended: REM's comment
        cmp     [g_cnw],3
        jne     @F
        cmp     word ptr [g_cnrem],'er'
        jne     @F
        cmp     [g_cnrem+2],'m'
        jne     @F
        mov     [g_cnst],2
@@:
        and     [g_cnw],0
        ret

; ::::::::::::::
; cn_eol - the physical line ended: CF set when it continues.
; ::::::::::::::
cn_eol:
        call    @cnb_w
        cmp     [g_cnst],2              ; '_' in a comment: the comment's
        je      @cne_n
        cmp     [g_cnu],0
        je      @cne_n
        stc
        ret
@cne_n:
        clc
        ret

        .data?
g_srfe  dd      ?                       ; the window's end: its 0
g_srmem dd      ?                       ; sr_mem's line still to read, 0 a file
g_srpend dd     ?                       ; blanks in code not yet put
g_srend db      ?                       ; 1: the logical line is all in
g_srany db      ?                       ; 1: a byte or a line's end was read
g_cnst  db      ?                       ; 0 code, 1 a string, 2 a comment
g_cnw   db      ?                       ; the word's letters so far
g_cnu   db      ?                       ; 1: the last byte in code a '_' after a blank
g_cnb   db      ?                       ; 1: the byte before a blank
g_cnrem db      3 dup(?)                ; the word's first letters, small
        .code

        end
