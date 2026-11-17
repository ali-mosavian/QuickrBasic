; ==========================================================================
; text.asm - the editor's text: the program store's lines; the clipboard;
; files
; ==========================================================================
; holds:    tx_init, tx_get, tx_text, tx_repl, tx_insl, tx_dell, tx_read,
;           tx_write, tx_mark, tx_mtog, tx_mclr, tx_view, tx_whole, cb_*
; uses:     prg_*, lx_line, ls_body, lm_* (the line map), ui_body, ui_canon
; data:     The text is the program store: a line a record, tx_all of
;           them, at least one, in order, each found by the line map. A
;           line is its record's pre bytes (its indent and number as typed),
;           then its body as LIST spells it; an LF_ASIS line is its pre
;           alone (a line with a syntax error, kept as typed; tx_asis asks
;           it of the next line stored). A line's mark (a breakpoint) is its
;           LF_BP. The clipboard: cb_len bytes in clpb, whole lines (cb_ln
;           1, each ended by CR) or a piece of one.
;           The editor sees a view: tx_n lines from tx_v0 of the tx_all,
;           its line numbers from 0 (a procedure's lines, view.asm). A line
;           added or removed goes to tx_hook (EAX the line, EDX 1 or -1).
; calls:
;   tx_repl, tx_insl   tx_put: tx_make, the record in, the map again
;   tx_read            a line at a time (rd_byte) to tx_add
; ==========================================================================

include x86f.inc

        .code

RDSZ    = 4096                  ; a file's bytes read, written at a time

; ::::::::::::::
; tx_init - a new program: one empty line.
; ::::::::::::::
tx_init:
        mov     [g_ui.tx.tx_mod],1
        call    prg_init
        xor     ecx,ecx
        call    tx_add
        call    lm_build
        jmp     tx_whole

; ::::::::::::::
; tx_view - the editor's view: EDX lines from line EAX; tx_whole - the
; whole text.
;
; keeps:  all
; ::::::::::::::
tx_whole:
        and     [g_ui.tx.tx_v0],0
        push    eax
        mov     eax,[g_ui.tx.tx_all]
        mov     [g_ui.tx.tx_n],eax
        pop     eax
        ret
tx_view:
        mov     [g_ui.tx.tx_v0],eax
        mov     [g_ui.tx.tx_n],edx
        ret

; ::::::::::::::
; tx_rec - the view's line EAX's record.
;
; out:    EBX  it
; keeps:  all but EBX
; ::::::::::::::
tx_rec:
        push    eax
        add     eax,[g_ui.tx.tx_v0]
        call    lm_off
        pop     eax
        ret

; ::::::::::::::
; tx_get - line EAX into EDI.
;
; out:    ECX  its length
; keeps:  all but ECX
; ::::::::::::::
tx_get:
        push    ebx
        call    tx_rec
        call    tx_text
        pop     ebx
        ret

; ::::::::::::::
; tx_text - the line of record EBX into EDI.
;
; out:    ECX  its length
; keeps:  all but ECX
; ::::::::::::::
tx_text:
        push    eax
        push    esi
        push    edi
        mov     ecx,gs:[ebx].TLINE.pre
        mov     esi,gs:[ebx].TLINE.len
        sub     esi,ecx
        add     esi,ebx
        add     esi,[g_state.mm.progbase]
        rep     movsb
        test    gs:[ebx].TLINE.fl,LF_ASIS
        jnz     @txt_r
        push    dword ptr [g_state.io.sbx]     ; sbuf's user's, kept
        push    word ptr [g_state.io.ocap]      ; ocap, pcol
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
        lea     esi,[ebx+HDR]
        add     esi,[g_state.mm.progbase]
        pushad
        call    ls_body
        popad
        mov     ecx,[g_state.io.sbx]
        mov     esi,offset g_sbuf
        rep     movsb
        pop     word ptr [g_state.io.ocap]
        pop     dword ptr [g_state.io.sbx]
@txt_r:
        mov     ecx,edi
        pop     edi
        sub     ecx,edi
        pop     esi
        pop     eax
        ret

; ::::::::::::::
; tx_make - the line of ECX characters at ESI made a record's: its body in
; cbuf, its number, its text before the body; as typed when tx_asis (used
; up) or when LIST would spell it past an editor line.
;
; in:     ESI  the line, kept till prg_fill
; out:    bodyptr, bodylen, preptr, prelen, lnum  for prg_fill
;         AL   its fl
; ::::::::::::::
tx_make:
        mov     [g_state.mm.preptr],esi
        push    esi                     ; a copy, 0-ended, worked on: the
        push    ecx                     ; lexer changes its text
        mov     edi,offset g_ui.bf.uc_line
        rep     movsb
        mov     byte ptr [edi],0
        pop     ecx
        mov     esi,offset g_ui.bf.uc_line
        mov     edi,esi
        call    ui_body
        mov     edx,edi                 ; EDX the body
        mov     ebx,edi
        sub     ebx,esi
        mov     [g_state.mm.prelen],ebx
        mov     [g_state.rn.lnum],NONUM
        mov     edi,esi                 ; its number, 64000 when past
@@:
        cmp     byte ptr [edi],' '
        jne     @F
        inc     edi
        jmp     @B
@@:
        push    edi
        xor     eax,eax
@txk_d:
        cmp     edi,edx
        jae     @txk_n
        movzx   ebx,byte ptr [edi]
        sub     bl,'0'
        cmp     bl,9
        ja      @txk_n
        imul    eax,10
        add     eax,ebx
        cmp     eax,64000
        jb      @F
        mov     eax,64000
@@:
        inc     edi
        jmp     @txk_d
@txk_n:
        pop     ebx
        cmp     edi,ebx
        je      @txk_b
        mov     [g_state.rn.lnum],ax
@txk_b:
        push    ecx
        mov     [g_state.rn.line],DLINE
        mov     esi,edx
        mov     edi,offset g_cbuf
        call    lx_line
        sub     edi,offset g_cbuf
        mov     [g_state.mm.bodylen],edi
        mov     [g_state.mm.bodyptr],offset g_cbuf
        pop     ecx
        pop     esi
        xor     eax,eax
        xchg    al,[g_ui.tx.tx_asis]
        test    al,al
        jnz     @txk_a
        push    ecx                     ; LIST's spelling's length
        push    esi
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
        mov     esi,offset g_cbuf
        call    ls_body
        and     [g_state.io.ocap],0
        pop     esi
        pop     ecx
        mov     eax,[g_state.io.sbx]
        add     eax,[g_state.mm.prelen]
        cmp     eax,LNMAX
        mov     al,0
        jbe     @txk_r
@txk_a:
        mov     [g_state.mm.prelen],ecx
        mov     al,LF_ASIS
@txk_r:
        ret

; ::::::::::::::
; tx_put - the line of ECX characters at ESI stored as text line EAX:
; before it (DL 1), in its place (DL 0, its mark kept).
;
; error:  Out of memory
; ::::::::::::::
tx_put:
        push    eax
        push    edx
        call    tx_make
        mov     cl,al
        pop     edx
        pop     eax
        call    lm_off
        test    dl,dl
        jnz     @F
        mov     dl,gs:[ebx].TLINE.fl
        and     dl,LF_BP
        or      cl,dl
        call    prg_cut
@@:
        mov     [g_state.mm.addfl],cl
        call    prg_ins
        mov     [g_state.mm.addfl],0
        mov     [g_ui.tx.tx_mod],1
        jmp     lm_build

; ::::::::::::::
; tx_add - the line of ECX characters at ESI stored after the rest, the
; map not made.
;
; error:  Out of memory
; ::::::::::::::
tx_add:
        call    tx_make
        mov     [g_state.mm.addfl],al
        and     [g_state.mm.addat],0
        mov     esi,[g_state.mm.bodyptr]
        mov     ecx,[g_state.mm.bodylen]
        call    prg_add
        mov     [g_state.mm.addfl],0
        ret

; ::::::::::::::
; tx_repl - line EAX becomes CX characters from ESI.
;
; out:    CF  clear
; keeps:  all
; ::::::::::::::
tx_repl:
        pushad
        add     eax,[g_ui.tx.tx_v0]
        movzx   ecx,cx
        xor     edx,edx
        call    tx_put
        popad
        clc
        ret

; ::::::::::::::
; tx_insl - a new line of CX characters from ESI before line EAX.
;
; in:     EAX  at most tx_n
; out:    CF  clear
; keeps:  all
; ::::::::::::::
tx_insl:
        pushad
        cmp     eax,[g_ui.tx.tx_n]       ; past the view: its end
        jbe     @F
        mov     eax,[g_ui.tx.tx_n]
@@:
        add     eax,[g_ui.tx.tx_v0]
        movzx   ecx,cx
        push    eax
        mov     dl,1
        call    tx_put
        pop     eax
        inc     [g_ui.tx.tx_n]
        mov     edx,1
        call    tx_tell
        popad
        clc
        ret

; tx_tell - line EAX added (EDX 1) or removed (-1): tx_hook told. Keeps
; all.
tx_tell:
        cmp     [g_ui.tx.tx_hook],0
        je      @F
        pushad
        call    [g_ui.tx.tx_hook]
        popad
@@:
        ret

; ::::::::::::::
; tx_dell - remove line EAX; the last one left is emptied.
;
; keeps:  all
; ::::::::::::::
tx_dell:
        pushad
        cmp     [g_ui.tx.tx_n],1        ; the view's last line is emptied
        ja      @F
        xor     ecx,ecx
        call    tx_repl
        popad
        ret
@@:
        call    tx_rec
        call    prg_cut
        call    prg_drop
        mov     [g_ui.tx.tx_mod],1
        call    lm_build
        dec     [g_ui.tx.tx_n]
        mov     eax,[esp+28]
        add     eax,[g_ui.tx.tx_v0]
        mov     edx,-1
        call    tx_tell
        popad
        ret

; ::::::::::::::
; tx_mark - the view's line EAX's mark.
;
; out:    AL  it: 0, 1
; keeps:  all but EAX
; ::::::::::::::
tx_mark:
        push    ebx
        call    tx_rec
        mov     al,gs:[ebx].TLINE.fl
        and     al,LF_BP
        shr     al,1
        pop     ebx
        ret

; ::::::::::::::
; tx_mtog - the view's line EAX's mark: on, off.
;
; keeps:  all
; ::::::::::::::
tx_mtog:
        push    ebx
        call    tx_rec
        xor     gs:[ebx].TLINE.fl,LF_BP
        pop     ebx
        ret

; ::::::::::::::
; tx_mclr - no line marked.
;
; keeps:  all
; ::::::::::::::
tx_mclr:
        push    eax
        push    ebx
        mov     ebx,PROGBASE
@@:
        mov     eax,gs:[ebx].TLINE.len
        test    eax,eax
        jz      @F
        and     gs:[ebx].TLINE.fl,not LF_BP
        add     ebx,eax
        jmp     @B
@@:
        pop     ebx
        pop     eax
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
; cb_lines - lines EAX to EDX (inclusive) to the clipboard, as many as fit.
; ::::::::::::::
cb_lines:
        pushad
        mov     edi,offset g_clpb
@cbl_l:
        cmp     eax,edx
        ja      @cbl_d
        push    edi
        mov     edi,offset g_ui.bf.sy_buf
        call    tx_get
        pop     edi
        lea     ebx,[edi+ecx+1]
        cmp     ebx,offset g_clpb+0FFFFh
        ja      @cbl_d
        mov     esi,offset g_ui.bf.sy_buf
        rep     movsb
        mov     byte ptr [edi],NL
        inc     edi
        inc     eax
        jmp     @cbl_l
@cbl_d:
        sub     edi,offset g_clpb
        mov     [g_ui.tx.cb_len],di
        mov     [g_ui.tx.cb_ln],1
        popad
        ret

; ::::::::::::::
; tx_read - the file named at EDX the text: CR LF and LF end lines, a tab
; goes to the next column of 8, ^Z ends it, a long line is cut, a line
; ending in '_' goes on in the next. Each line as the editor stores it:
; ui_canon's syntax error keeps it as typed.
;
; out:    CF  set: AX the DOS error, the text as it was
; error:  Out of memory
; ::::::::::::::
tx_read:
        mov     ax,3D00h
        int     21h
        jc      @trd_r
        mov     [g_rdh],ax
        and     [g_rdpos],0
        and     [g_rdcnt],0
        mov     [g_ui.tx.tx_mod],1
        call    prg_init
        and     [g_rdn],0
@trd_0:
        mov     edi,offset g_ui.bf.sy_buf
        xor     edx,edx                 ; EDX the column
        call    cn_init
@trd_l:
        call    rd_byte
        jc      @trd_e
        cmp     al,1Ah
        je      @trd_e
        cmp     al,10
        je      @trd_n
        cmp     al,NL
        jne     @trd_t
        call    rd_peek
        jc      @trd_n
        cmp     al,10
        jne     @trd_n
        inc     [g_rdpos]
@trd_n:
        call    cn_eol
        pushfd
        call    cn_init
        popfd
        jc      @trd_j
        call    @trd_p
        jmp     @trd_0
@trd_j:                                 ; continued: the '_' a blank, the next
        cmp     byte ptr [edi-1],' '    ; line's indent dropped
        jne     @F
        dec     edi
        dec     edx
        jmp     @trd_j
@@:
        cmp     byte ptr [edi-1],'_'
        jne     @trd_k
        mov     byte ptr [edi-1],' '
@trd_k:
        call    rd_peek
        jc      @trd_l
        cmp     al,' '
        je      @F
        cmp     al,9
        jne     @trd_l
@@:
        inc     [g_rdpos]
        jmp     @trd_k
@trd_t:
        call    cn_byte
        cmp     edx,LNMAX
        jae     @trd_l
        cmp     al,9
        jne     @trd_c
@trd_s:
        mov     al,' '
        stosb
        inc     edx
        test    edx,7
        jz      @trd_l
        cmp     edx,LNMAX
        jb      @trd_s
        jmp     @trd_l
@trd_c:
        stosb
        inc     edx
        jmp     @trd_l
@trd_e:
        cmp     edi,offset g_ui.bf.sy_buf       ; the last line, unended
        jne     @F
        cmp     [g_rdn],0
        jne     @trd_z
@@:
        call    @trd_p
@trd_z:
        mov     bx,[g_rdh]
        mov     ah,3Eh
        int     21h
        call    lm_build
        call    tx_whole
        clc
@trd_r:
        ret
@trd_p:                                 ; the line in sy_buf stored
        mov     esi,offset g_ui.bf.sy_buf
        mov     ecx,edi
        sub     ecx,esi
        push    ecx
        call    ui_canon
        pop     ecx
        jnc     @F
        mov     [g_ui.tx.tx_asis],1
@@:
        mov     esi,offset g_ui.bf.sy_buf
        call    tx_add
        inc     [g_rdn]
        ret

; rd_peek - the file's next byte, not taken: AL; CF the end. Keeps all
; but EAX.
rd_peek:
        push    ebx
        push    ecx
        push    edx
        mov     ecx,[g_rdpos]
        cmp     ecx,[g_rdcnt]
        jb      @rdp_h
        mov     bx,[g_rdh]
        mov     ecx,RDSZ
        mov     edx,offset g_rdb
        mov     ah,3Fh
        int     21h
        jc      @rdp_e
        movzx   eax,ax
        test    eax,eax
        jz      @rdp_e
        mov     [g_rdcnt],eax
        xor     ecx,ecx
        mov     [g_rdpos],ecx
@rdp_h:
        mov     al,[g_rdb+ecx]
        clc
        jmp     @rdp_r
@rdp_e:
        stc
@rdp_r:
        pop     edx
        pop     ecx
        pop     ebx
        ret

; rd_byte - the file's next byte, taken: AL; CF the end.
rd_byte:
        call    rd_peek
        jc      @F
        inc     [g_rdpos]
@@:
        ret

; ::::::::::::::
; tx_write - the text to the file named at EDX, lines ended by CR LF.
;
; out:    CF  set: AX the DOS error
; ::::::::::::::
tx_write:
        mov     ah,3Ch
        xor     ecx,ecx
        int     21h
        jc      @twr_r
        mov     [g_rdh],ax
        and     [g_rdcnt],0
        and     [g_rderr],0
        xor     eax,eax
@twr_l:
        cmp     eax,[g_ui.tx.tx_all]
        jae     @twr_d
        call    lm_off
        mov     edi,offset g_ui.bf.sy_buf
        call    tx_text
        mov     word ptr [edi+ecx],0A0Dh
        add     ecx,2
        mov     ebx,[g_rdcnt]
        add     ebx,ecx
        cmp     ebx,RDSZ
        jbe     @F
        call    wr_flush
        cmp     ecx,RDSZ
        jbe     @F
        mov     esi,edi                 ; longer than the buffer: as it is
        call    wr_out
        jmp     @twr_n
@@:
        mov     esi,edi
        mov     edi,[g_rdcnt]
        add     edi,offset g_rdb
        add     [g_rdcnt],ecx
        rep     movsb
@twr_n:
        inc     eax
        jmp     @twr_l
@twr_d:
        call    wr_flush
        mov     bx,[g_rdh]
        mov     ah,3Eh
        int     21h
        mov     ax,[g_rderr]
        cmp     ax,1                    ; CF: an error
        cmc
@twr_r:
        ret

; wr_flush - the buffer's bytes written. wr_out - ECX bytes at ESI. A DOS
; error to rderr. Keep all.
wr_flush:
        pushad
        mov     ecx,[g_rdcnt]
        mov     esi,offset g_rdb
        and     [g_rdcnt],0
        jmp     @F
wr_out:
        pushad
@@:
        jecxz   @wro_r
        mov     edx,esi
        mov     bx,[g_rdh]
        mov     ah,40h
        int     21h
        jnc     @wro_r
        mov     [g_rderr],ax
@wro_r:
        popad
        ret

        .data?
g_rdh   dw      ?                       ; the file
g_rderr dw      ?                       ; tx_write's DOS error, 0 none
g_rdpos dd      ?                       ; g_rdb's next byte, its bytes
g_rdcnt dd      ?
g_rdn   dd      ?                       ; tx_read's lines
g_rdb   db      RDSZ dup(?)
        .code

        end
