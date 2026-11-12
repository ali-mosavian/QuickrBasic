; ==========================================================================
; edit.asm - editing: lines, the selection, the clipboard
; ==========================================================================
; holds:    le_* (one line), ed_* (the program window), im_* (the immediate
;           window), ui_canon
; uses:     tx_*, cb_*, lx_line, cm_line, cm_walk, ls_body, dl_ok
; data:     A line buffer (LB): col:2 len:2 dirty:1 text. The cursor may be
;           past the text; typing there pads it. The program window edits
;           line ed_line in ed_lb and stores it when the cursor leaves
;           (ed_commit), keywords capitalised and the syntax checked. A
;           selection from (sel_l, sel_c) to the cursor is a piece of one
;           line, or whole lines when it spans several.
; calls:
;   ed_commit          ui_canon, tx_repl
;   ed_enter, ed_bs, ed_del  split and join lines
;   ed_copy, ed_clear, ed_paste  the selection and the clipboard
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; le_pad - the text of LB BX padded with spaces to its cursor.
;
; keeps:  all
; ::::::::::::::
le_pad:
        pushad
        movzx   ecx,word ptr [ebx+TLBUF.col]
        movzx   edi,word ptr [ebx+TLBUF.len]
        cmp     cx,di
        jbe     @lep_r
        mov     [ebx+TLBUF.len],cx
        sub     cx,di
        lea     edi,[ebx+edi+TLBUF.txt]
        mov     al,' '
        rep     stosb
@lep_r:
        popad
        ret

; ::::::::::::::
; le_ins - AL typed into LB BX: inserted, or over the character there.
;
; in:     DX  the most characters
; out:    CF  set: full
; keeps:  all
; ::::::::::::::
le_ins:
        pushad
        movzx   esi,word ptr [ebx+TLBUF.col]
        cmp     si,dx
        jae     @lei_f
        call    le_pad
        cmp     [g_ui.ed.ed_ins],0
        jne     @lei_i
        cmp     si,[ebx+TLBUF.len]
        jb      @lei_s
@lei_i:
        movzx   ecx,word ptr [ebx+TLBUF.len]
        cmp     cx,dx
        jae     @lei_f
        sub     cx,si                   ; the characters from the cursor
        movzx   edx,word ptr [ebx+TLBUF.len]
        lea     edi,[ebx+edx+TLBUF.txt]
        lea     esi,[edi-1]
        std
        rep     movsb
        cld
        inc     word ptr [ebx+TLBUF.len]
        movzx   esi,word ptr [ebx+TLBUF.col]
@lei_s:
        mov     [ebx+esi+TLBUF.txt],al
        inc     word ptr [ebx+TLBUF.col]
        mov     byte ptr [ebx+TLBUF.dty],1
        popad
        clc
        ret
@lei_f:
        popad
        stc
        ret

; ::::::::::::::
; le_cut - characters CX to DX of LB BX out; the cursor to CX.
;
; keeps:  all
; ::::::::::::::
le_cut:
        pushad
        mov     ax,[ebx+TLBUF.len]
        cmp     dx,ax
        jbe     @lec_a
        mov     dx,ax
@lec_a:
        mov     [ebx+TLBUF.col],cx
        cmp     cx,dx
        jae     @lec_r
        movzx   edi,cx
        lea     edi,[ebx+edi+TLBUF.txt]
        movzx   esi,dx
        lea     esi,[ebx+esi+TLBUF.txt]
        sub     ax,dx                   ; what follows
        sub     dx,cx
        sub     [ebx+TLBUF.len],dx
        movzx   ecx,ax
        rep     movsb
        mov     byte ptr [ebx+TLBUF.dty],1
@lec_r:
        popad
        ret

; ::::::::::::::
; le_bs - Backspace in LB BX.
;
; out:    CF  set: at column 0, nothing done
; keeps:  all
; ::::::::::::::
le_bs:
        cmp     word ptr [ebx+TLBUF.col],0
        je      @leb_n
        push    ecx
        push    edx
        movzx   ecx,word ptr [ebx+TLBUF.col]
        dec     cx
        mov     dx,cx
        inc     dx
        call    le_cut
        mov     [ebx+TLBUF.col],cx
        pop     edx
        pop     ecx
        clc
        ret
@leb_n:
        stc
        ret

; ::::::::::::::
; le_del - Del in LB BX.
;
; out:    CF  set: at or past the end, nothing done
; keeps:  all
; ::::::::::::::
le_del:
        push    ecx
        push    edx
        movzx   ecx,word ptr [ebx+TLBUF.col]
        cmp     cx,[ebx+TLBUF.len]
        jae     @led_n
        mov     dx,cx
        inc     dx
        call    le_cut
        pop     edx
        pop     ecx
        clc
        ret
@led_n:
        pop     edx
        pop     ecx
        stc
        ret

; ::::::::::::::
; le_indent - the spaces LB BX starts with.
;
; out:    CX  their count
; keeps:  all but CX
; ::::::::::::::
le_indent:
        push    esi
        xor     ecx,ecx
        lea     esi,[ebx+TLBUF.txt]
@lin_l:
        cmp     cx,[ebx+TLBUF.len]
        jae     @lin_r
        cmp     byte ptr [esi],' '
        jne     @lin_r
        inc     esi
        inc     cx
        jmp     @lin_l
@lin_r:
        pop     esi
        ret

; ::::::::::::::
; le_home - to the first character, or to column 0 when there.
; ::::::::::::::
le_home:
        call    le_indent
        cmp     cx,[ebx+TLBUF.col]
        jne     @lh_s
        xor     ecx,ecx
@lh_s:
        mov     [ebx+TLBUF.col],cx
        ret

; ::::::::::::::
; le_end - past the last character.
; ::::::::::::::
le_end:
        mov     ax,[ebx+TLBUF.len]
        mov     [ebx+TLBUF.col],ax
        ret

; ::::::::::::::
; le_left - the cursor one column left.
; ::::::::::::::
le_left:
        cmp     word ptr [ebx+TLBUF.col],0
        je      @lel_r
        dec     word ptr [ebx+TLBUF.col]
@lel_r:
        ret
; ::::::::::::::
; le_right - the cursor one column right.
; ::::::::::::::
le_right:
        cmp     word ptr [ebx+TLBUF.col],LNMAX
        jae     @lel_r
        inc     word ptr [ebx+TLBUF.col]
        ret

; ::::::::::::::
; le_wleft - the cursor to the word before.
; ::::::::::::::
le_wleft:
        movzx   esi,word ptr [ebx+TLBUF.col]
        cmp     si,[ebx+TLBUF.len]
        jbe     @lwl_s
        movzx   esi,word ptr [ebx+TLBUF.len]
@lwl_s:
        test    esi,esi
        jz      @lwl_r
        dec     esi
        cmp     byte ptr [ebx+esi+TLBUF.txt],' '
        je      @lwl_s
@lwl_w:
        test    esi,esi
        jz      @lwl_r
        cmp     byte ptr [ebx+esi+TLBUF.txt-1],' '
        je      @lwl_r
        dec     esi
        jmp     @lwl_w
@lwl_r:
        mov     [ebx+TLBUF.col],si
        ret
; ::::::::::::::
; le_wright - the cursor to the next word.
; ::::::::::::::
le_wright:
        movzx   esi,word ptr [ebx+TLBUF.col]
@lwr_w:
        cmp     si,[ebx+TLBUF.len]
        jae     @lwr_r
        cmp     byte ptr [ebx+esi+TLBUF.txt],' '
        je      @lwr_s
        inc     esi
        jmp     @lwr_w
@lwr_s:
        cmp     si,[ebx+TLBUF.len]
        jae     @lwr_r
        cmp     byte ptr [ebx+esi+TLBUF.txt],' '
        jne     @lwr_r
        inc     esi
        jmp     @lwr_s
@lwr_r:
        mov     [ebx+TLBUF.col],si
        ret

; ::::::::::::::
; le_insn - CX characters from SI into LB BX at its cursor.
;
; out:    CF  set: they do not fit, nothing done
; ::::::::::::::
le_insn:
        call    le_pad
        mov     ax,[ebx+TLBUF.len]
        add     ax,cx
        cmp     ax,LNMAX
        ja      @lis_f
        push    esi
        push    ecx
        mov     dx,[ebx+TLBUF.len]
        sub     dx,[ebx+TLBUF.col]  ; the tail
        movzx   esi,word ptr [ebx+TLBUF.len]
        lea     esi,[ebx+esi+TLBUF.txt-1]
        movzx   edi,cx
        add     edi,esi
        movzx   ecx,dx
        std
        rep     movsb
        cld
        pop     ecx
        pop     esi
        movzx   edi,word ptr [ebx+TLBUF.col]
        lea     edi,[ebx+edi+TLBUF.txt]
        movzx   ecx,cx
        add     [ebx+TLBUF.len],cx
        add     [ebx+TLBUF.col],cx
        rep     movsb
        mov     byte ptr [ebx+TLBUF.dty],1
        clc
        ret
@lis_f:
        stc
        ret

; ::::::::::::::
; ed_load - line AX into ed_lb; the column stays.
; ::::::::::::::
ed_load:
        mov     [g_ui.ed.ed_line],ax
        mov     edi,offset g_ui.bf.ed_lb.txt
        call    tx_get
        mov     [g_ui.bf.ed_lb.len],cx
        and     byte ptr [g_ui.bf.ed_lb.dty],0
        ret

; ::::::::::::::
; ed_commit - a changed line back to the text: spaces at its end cut, its
; keywords capitalised, its syntax checked.
;
; out:    CF  set: a syntax error was shown; the line is stored as typed
; ::::::::::::::
ed_commit:
        cmp     byte ptr [g_ui.bf.ed_lb.dty],0
        je      @edc_r
        and     byte ptr [g_ui.bf.ed_lb.dty],0
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
@edc_t:
        jcxz    @edc_s
        movzx   ebx,cx
        cmp     byte ptr [g_ui.bf.ed_lb.txt-1+ebx],' '
        jne     @edc_s
        dec     cx
        jmp     @edc_t
@edc_s:
        mov     [g_ui.bf.ed_lb.len],cx
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        mov     esi,offset g_ui.bf.ed_lb.txt
        call    ui_canon
        pushf
        jc      @edc_w
        movzx   ecx,[g_state.io.sbx]
        mov     [g_ui.bf.ed_lb.len],cx
        mov     esi,offset g_sbuf
        mov     edi,offset g_ui.bf.ed_lb.txt
        rep     movsb
@edc_w:
        mov     ax,[g_ui.ed.ed_line]
        mov     esi,offset g_ui.bf.ed_lb.txt
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        call    tx_repl
        popf
        jc      @edc_e
        call    vw_commit
        clc
        ret
@edc_e:
        mov     esi,offset g_ui.bf.ui_ebuf
        call    dl_err
        stc
@edc_r:
        ret

; ::::::::::::::
; ui_canon - the line of CX characters at SI as LIST shows it: in sbuf,
; sbx long; its label and indent kept.
;
; out:    CF  set: a syntax error, ui_ebuf its message; sbuf not made
; ::::::::::::::
ui_canon:
        mov     [g_ui.fd.uc_src],esi
        mov     [g_ui.fd.uc_n],cx
        cmp     cx,200
        ja      @uca_k
        movzx   ebx,cx
        mov     byte ptr [esi+ebx],0
        mov     edi,esi                 ; EDI past the label and the indent
@uca_b:
        cmp     byte ptr [edi],' '
        jne     @uca_d
        inc     edi
        jmp     @uca_b
@uca_d:
        mov     al,[edi]
        cmp     al,'0'
        jb      @uca_e
        cmp     al,'9'
        ja      @uca_e
        inc     edi
        jmp     @uca_d
@uca_e:
        cmp     byte ptr [edi],' '
        jne     @uca_f
        inc     edi
        jmp     @uca_e
@uca_f:
        cmp     byte ptr [edi],0
        je      @uca_k
        mov     [g_ui.fd.uc_pre],edi
        push    esi
        mov     esi,edi
        mov     edi,offset g_inbuf
@uca_c:
        lodsb
        stosb
        test    al,al
        jnz     @uca_c
        mov     [g_state.rn.ln],DLINE
        mov     esi,offset g_inbuf
        mov     edi,offset g_cbuf
        call    lx_line
        call    dc_line                 ; a typed SUB known to the lines after
        pop     esi
        cmp     [g_ui.tx.syn_on],0
        je      @uca_l
        push    esi
        mov     [g_state.cm.cchk],1
        call    cm_line
        mov     [g_state.cm.cchk],0
        mov     [g_ui.fd.uc_err],0
        mov     esi,offset g_xbuf
        mov     edx,offset ui_chkop
        call    cm_walk
        pop     esi
        mov     al,[g_ui.fd.uc_err]
        test    al,al
        jz      @uca_l
        jmp     ui_emsg
@uca_l:
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
@uca_p:
        cmp     esi,[g_ui.fd.uc_pre]
        jae     @uca_q
        lodsb
        call    io_putc
        jmp     @uca_p
@uca_q:
        mov     esi,offset g_cbuf
        call    ls_body
        and     [g_state.io.ocap],0
        clc
        ret
@uca_k:
        mov     esi,[g_ui.fd.uc_src]
        movzx   ecx,word ptr [g_ui.fd.uc_n]
        mov     [g_state.io.sbx],cl
        mov     edi,offset g_sbuf
        rep     movsb
        clc
        ret

; ::::::::::::::
; ui_chkop - cm_walk: an ERR op's code to uc_err.
;
; in:     AL  the op
;         SI  at it
; ::::::::::::::
ui_chkop:
        cmp     al,O_ERR
        jne     @uck_r
        mov     al,[esi+1]
        mov     [g_ui.fd.uc_err],al
@uck_r:
        ret

; ::::::::::::::
; ui_emsg - the message of error AL in ui_ebuf: "Syntax error".
;
; out:    CF  set
; ::::::::::::::
ui_emsg:
        push    eax
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
        call    io_msg
        mov     al,MSG_ERR
        call    io_msg
        and     [g_state.io.ocap],0
        pop     eax
        movzx   ecx,[g_state.io.sbx]
        mov     esi,offset g_sbuf
        mov     edi,offset g_ui.bf.ui_ebuf
        jcxz    @uem_e
        lodsb
        stosb
        dec     cx
        jcxz    @uem_e
@uem_l:
        lodsb
        cmp     al,'A'
        jb      @uem_s
        cmp     al,'Z'
        ja      @uem_s
        add     al,20h
@uem_s:
        stosb
        loop    @uem_l
@uem_e:
        xor     al,al
        stosb
        stc
        ret

; ::::::::::::::
; ed_goto - the cursor to line AX: this one stored first.
;
; out:    CF  set: it stayed, a syntax error shown
; ::::::::::::::
ed_goto:
        push    eax
        call    ed_commit
        pop     eax
        jc      @edg_r
        call    ed_load
        clc
@edg_r:
        ret

; ::::::::::::::
; ed_up - the cursor one line up.
; ::::::::::::::
ed_up:
        mov     ax,[g_ui.ed.ed_line]
        test    ax,ax
        jz      @edu_r
        dec     ax
        jmp     ed_goto
@edu_r:
        ret
; ::::::::::::::
; ed_down - the cursor one line down.
; ::::::::::::::
ed_down:
        mov     ax,[g_ui.ed.ed_line]
        inc     ax
        cmp     ax,[g_ui.tx.tx_n]
        jae     @edu_r
        jmp     ed_goto
; ::::::::::::::
; ed_pgup - a window of lines up.
; ::::::::::::::
ed_pgup:
        mov     ax,[g_ui.ed.ed_line]
        sub     ax,[g_ui.ed.ed_rows]
        inc     ax                      ; CF the sub's
        jae     @edp_u
        xor     ax,ax
@edp_u:
        mov     dx,[g_ui.ed.ed_top]
        sub     dx,[g_ui.ed.ed_rows]
        inc     dx
        jae     @edp_t
        xor     dx,dx
@edp_t:
        mov     [g_ui.ed.ed_top],dx
        jmp     ed_goto
; ::::::::::::::
; ed_pgdn - a window of lines down.
; ::::::::::::::
ed_pgdn:
        mov     ax,[g_ui.ed.ed_line]
        add     ax,[g_ui.ed.ed_rows]
        dec     ax
        mov     dx,[g_ui.tx.tx_n]
        dec     dx
        cmp     ax,dx
        jbe     @edp_d
        mov     ax,dx
@edp_d:
        mov     cx,[g_ui.ed.ed_rows]
        dec     cx
        add     [g_ui.ed.ed_top],cx
        jmp     ed_goto
; ::::::::::::::
; ed_first - the cursor to the first line.
; ::::::::::::::
ed_first:
        xor     ax,ax
        mov     [g_ui.bf.ed_lb.col],ax
        jmp     ed_goto
; ::::::::::::::
; ed_last - the cursor to the last line.
; ::::::::::::::
ed_last:
        mov     ax,[g_ui.tx.tx_n]
        dec     ax
        call    ed_goto
        jc      @edu_r
        mov     ax,[g_ui.bf.ed_lb.len]
        mov     [g_ui.bf.ed_lb.col],ax
        ret

; ::::::::::::::
; ed_ensure - ed_top and ed_left so that the cursor shows.
; ::::::::::::::
ed_ensure:
        mov     ax,[g_ui.ed.ed_line]
        cmp     ax,[g_ui.ed.ed_top]
        jae     @ede_b
        mov     [g_ui.ed.ed_top],ax
@ede_b:
        mov     dx,[g_ui.ed.ed_top]
        add     dx,[g_ui.ed.ed_rows]
        cmp     ax,dx
        jb      @ede_m
        sub     ax,[g_ui.ed.ed_rows]
        inc     ax
        mov     [g_ui.ed.ed_top],ax
@ede_m:
        mov     ax,[g_ui.tx.tx_n]
        cmp     [g_ui.ed.ed_top],ax
        jb      @ede_h
        dec     ax
        mov     [g_ui.ed.ed_top],ax
@ede_h:
        mov     ax,[g_ui.bf.ed_lb.col]
        cmp     ax,[g_ui.ed.ed_left]
        jae     @ede_r
        mov     [g_ui.ed.ed_left],ax
@ede_r:
        mov     dx,[g_ui.ed.ed_left]
        add     dx,ED_COLS
        cmp     ax,dx
        jb      @ede_x
        sub     ax,ED_COLS-1
        mov     [g_ui.ed.ed_left],ax
@ede_x:
        ret

; ::::::::::::::
; ed_enter - Enter: the line split at the cursor, the new line indented as
; this one.
; ::::::::::::::
ed_enter:
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_indent
        cmp     cx,[g_ui.bf.ed_lb.col]
        jbe     @een_i
        movzx   ecx,word ptr [g_ui.bf.ed_lb.col]
@een_i:
        mov     [g_ui.ed.ed_ind],cx
        mov     edi,offset g_ui.bf.sy_buf  ; the new line: the indent, the rest
        mov     al,' '
        rep     stosb
        movzx   esi,word ptr [g_ui.bf.ed_lb.col]
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        cmp     si,cx
        jae     @een_n
        mov     [g_ui.bf.ed_lb.len],si
        mov     byte ptr [g_ui.bf.ed_lb.dty],1
        sub     cx,si
        add     esi,offset g_ui.bf.ed_lb.txt
@een_s:
        cmp     byte ptr [esi],' '
        jne     @een_c
        inc     esi
        loop    @een_s
@een_c:
        rep     movsb
@een_n:
        sub     edi,offset g_ui.bf.sy_buf
        mov     [g_ui.ed.ed_nlen],di
        mov     al,[g_ui.ed.ed_vw]
        push    eax
        call    ed_commit
        pop     eax
        cmp     al,[g_ui.ed.ed_vw]      ; a SUB line: its own view, made
        jne     @een_r
        movzx   ecx,word ptr [g_ui.ed.ed_nlen]
        mov     ax,[g_ui.ed.ed_line]
        inc     ax
        mov     esi,offset g_ui.bf.sy_buf
        call    tx_insl
        jc      @een_r
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        call    ed_load
        mov     ax,[g_ui.ed.ed_ind]
        mov     [g_ui.bf.ed_lb.col],ax
@een_r:
        ret

; ::::::::::::::
; ed_bs - Backspace: at column 0 the line joins the one above.
; ::::::::::::::
ed_bs:
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_bs
        jnc     @edb_r
        mov     ax,[g_ui.ed.ed_line]
        test    ax,ax
        jz      @edb_r
        mov     esi,offset g_ui.bf.ed_lb.txt  ; this line, kept
        mov     edi,offset g_ui.bf.sy_buf
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        mov     [g_ui.ed.ed_nlen],cx
        rep     movsb
        dec     ax
        mov     edi,offset g_ui.bf.uc_line
        call    tx_get
        add     cx,[g_ui.ed.ed_nlen]
        cmp     cx,LNMAX
        ja      @edb_r
        mov     ax,[g_ui.ed.ed_line]
        call    tx_dell
        dec     ax
        call    ed_load
        mov     ax,[g_ui.bf.ed_lb.len]
        mov     [g_ui.bf.ed_lb.col],ax
        mov     esi,offset g_ui.bf.sy_buf
        movzx   ecx,word ptr [g_ui.ed.ed_nlen]
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_insn
        mov     ax,[g_ui.bf.ed_lb.len]
        sub     ax,[g_ui.ed.ed_nlen]
        mov     [g_ui.bf.ed_lb.col],ax
        mov     byte ptr [g_ui.bf.ed_lb.dty],1
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
@edb_r:
        ret

; ::::::::::::::
; ed_del - Del: past the end the next line joins this one.
; ::::::::::::::
ed_del:
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_del
        jnc     @edd_r
        mov     ax,[g_ui.ed.ed_line]
        inc     ax
        cmp     ax,[g_ui.tx.tx_n]
        jae     @edd_r
        mov     edi,offset g_ui.bf.sy_buf
        call    tx_get
        mov     [g_ui.ed.ed_nlen],cx
        call    le_pad
        mov     ax,[g_ui.bf.ed_lb.len]
        add     ax,cx
        cmp     ax,LNMAX
        ja      @edd_r
        mov     ax,[g_ui.ed.ed_line]
        inc     ax
        call    tx_dell
        push    word ptr [g_ui.bf.ed_lb.col]
        mov     esi,offset g_ui.bf.sy_buf
        movzx   ecx,word ptr [g_ui.ed.ed_nlen]
        call    le_insn
        pop     word ptr [g_ui.bf.ed_lb.col]
        mov     byte ptr [g_ui.bf.ed_lb.dty],1
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
@edd_r:
        ret

; ::::::::::::::
; ed_tab - Tab: spaces to the next column of 8.
; ::::::::::::::
ed_tab:
        mov     ebx,offset g_ui.bf.ed_lb
@edt_l:
        mov     al,' '
        mov     dx,LNMAX
        call    le_ins
        jc      @edt_r
        test    word ptr [g_ui.bf.ed_lb.col],7
        jnz     @edt_l
@edt_r:
        ret

; ::::::::::::::
; ed_ctrly - Ctrl+Y: the line to the clipboard, out of the text.
; ::::::::::::::
ed_ctrly:
        cmp     byte ptr [g_ui.bf.ed_lb.dty],0  ; kept as typed: no syntax check
        je      @edy_c
        and     byte ptr [g_ui.bf.ed_lb.dty],0
        mov     ax,[g_ui.ed.ed_line]
        mov     esi,offset g_ui.bf.ed_lb.txt
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        call    tx_repl
@edy_c:
        mov     ax,[g_ui.ed.ed_line]
        mov     dx,ax
        call    cb_lines
        call    tx_dell
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        cmp     ax,[g_ui.tx.tx_n]
        jb      @edy_l
        mov     ax,[g_ui.tx.tx_n]
        dec     ax
@edy_l:
        call    ed_load
        and     [g_ui.bf.ed_lb.col],0
        ret

; ::::::::::::::
; sel_mark - a shifted move: the selection starts here if not yet on.
; ::::::::::::::
sel_mark:
        cmp     [g_ui.ed.sel_on],0
        jne     @slm_r
        mov     [g_ui.ed.sel_on],1
        mov     ax,[g_ui.ed.ed_line]
        mov     [g_ui.ed.sel_l],ax
        mov     ax,[g_ui.bf.ed_lb.col]
        mov     [g_ui.ed.sel_c],ax
@slm_r:
        ret

; ::::::::::::::
; sel_get - the selection: sel_a to sel_b, columns of the cursor's line
; (a piece) or lines (whole lines).
;
; out:    AL  0 none, 1 a piece, 2 lines
; keeps:  all but AX
; ::::::::::::::
sel_get:
        xor     al,al
        cmp     [g_ui.ed.sel_on],0
        je      @slg_r
        push    ebx
        push    edx
        mov     ax,[g_ui.ed.sel_l]
        cmp     ax,[g_ui.ed.ed_line]
        jne     @slg_l
        mov     ax,[g_ui.ed.sel_c]
        mov     dx,[g_ui.bf.ed_lb.col]
        cmp     ax,dx
        jbe     @slg_o
        xchg    ax,dx
@slg_o:
        mov     [g_ui.ed.sel_a],ax
        mov     [g_ui.ed.sel_b],dx
        cmp     ax,dx
        mov     al,0
        je      @slg_x
        mov     al,1
        jmp     @slg_x
@slg_l:
        mov     dx,[g_ui.ed.ed_line]
        movzx   ebx,word ptr [g_ui.bf.ed_lb.col]  ; the lower end's column
        cmp     ax,dx
        jb      @slg_u
        xchg    ax,dx
        movzx   ebx,word ptr [g_ui.ed.sel_c]
@slg_u:
        test    bx,bx
        jnz     @slg_k
        dec     dx
@slg_k:
        mov     [g_ui.ed.sel_a],ax
        mov     [g_ui.ed.sel_b],dx
        mov     al,2
@slg_x:
        pop     edx
        pop     ebx
@slg_r:
        ret

; ::::::::::::::
; ed_copy - the selection to the clipboard.
;
; out:    AL  as sel_get
; ::::::::::::::
ed_copy:
        call    sel_get
        cmp     al,1
        jb      @edy_r
        ja      @edo_l
        movzx   esi,word ptr [g_ui.ed.sel_a]
        add     esi,offset g_ui.bf.ed_lb.txt
        movzx   ecx,word ptr [g_ui.ed.sel_b]
        mov     dx,[g_ui.bf.ed_lb.len]
        cmp     cx,dx
        jbe     @edo_p
        movzx   ecx,dx
@edo_p:
        sub     cx,[g_ui.ed.sel_a]
        jae     @edo_q
        xor     ecx,ecx
@edo_q:
        xor     al,al
        call    cb_put
        mov     al,1
        ret
@edo_l:
        call    ed_commit
        mov     ax,[g_ui.ed.sel_a]
        mov     dx,[g_ui.ed.sel_b]
        call    cb_lines
        mov     al,2
@edy_r:
        ret

; ::::::::::::::
; ed_clear - the selection out of the text; off.
; ::::::::::::::
ed_clear:
        call    sel_get
        and     [g_ui.ed.sel_on],0
        cmp     al,1
        jb      @edl_r
        ja      @edl_l
        movzx   ecx,word ptr [g_ui.ed.sel_a]
        mov     dx,[g_ui.ed.sel_b]
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_cut
        mov     [g_ui.bf.ed_lb.col],cx
        ret
@edl_l:
        call    ed_commit
        mov     ax,[g_ui.ed.sel_a]
        movzx   ecx,word ptr [g_ui.ed.sel_b]
        sub     cx,ax
        inc     cx
@edl_d:
        call    tx_dell
        loop    @edl_d
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        cmp     ax,[g_ui.tx.tx_n]
        jb      @edl_g
        mov     ax,[g_ui.tx.tx_n]
        dec     ax
@edl_g:
        call    ed_load
        and     [g_ui.bf.ed_lb.col],0
@edl_r:
        ret

; ::::::::::::::
; ed_cut - the selection to the clipboard, out of the text.
; ::::::::::::::
ed_cut:
        call    ed_copy
        jmp     ed_clear

; ::::::::::::::
; ed_paste - the clipboard at the cursor, over the selection: a piece into
; the line, lines before it.
; ::::::::::::::
ed_paste:
        call    ed_clear
        movzx   ecx,word ptr [g_ui.tx.cb_len]
        test    ecx,ecx
        jz      @eps_r
        cmp     [g_ui.tx.cb_ln],0
        jne     @eps_l
        mov     esi,offset g_clpb
        mov     edi,offset g_ui.bf.sy_buf
        rep     movsb
        movzx   ecx,word ptr [g_ui.tx.cb_len]
        mov     esi,offset g_ui.bf.sy_buf
        mov     ebx,offset g_ui.bf.ed_lb
        jmp     le_insn
@eps_l:
        call    ed_commit
        mov     esi,offset g_clpb          ; ESI in the clipboard
        mov     dx,[g_ui.ed.ed_line]             ; DX the line, AL the characters
@eps_n:
        mov     ecx,esi
        sub     ecx,offset g_clpb
        cmp     cx,[g_ui.tx.cb_len]
        jae     @eps_e
        mov     edi,offset g_ui.bf.sy_buf
@eps_c:
        lodsb
        cmp     al,NL
        je      @eps_d
        stosb
        jmp     @eps_c
@eps_d:
        mov     ecx,edi
        sub     ecx,offset g_ui.bf.sy_buf
        push    esi
        mov     esi,offset g_ui.bf.sy_buf
        mov     ax,dx
        call    tx_insl
        pop     esi
        jc      @eps_e
        inc     dx
        jmp     @eps_n
@eps_e:
        mov     ax,dx
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        call    ed_load
@eps_r:
        ret

; ::::::::::::::
; im_load - immediate line im_i into im_lb.
; ::::::::::::::
im_load:
        movzx   esi,word ptr [g_ui.im.im_i]
        imul    esi,esi,IMW
        add     esi,offset g_ui.bf.im_h
        movzx   ecx,byte ptr [esi]
        inc     esi
        mov     [g_ui.bf.im_lb.len],cx
        mov     edi,offset g_ui.bf.im_lb.txt
        rep     movsb
        and     byte ptr [g_ui.bf.im_lb.dty],0
        ret

; ::::::::::::::
; im_store - im_lb back to its line, cut to the window.
; ::::::::::::::
im_store:
        movzx   edi,word ptr [g_ui.im.im_i]
        imul    edi,edi,IMW
        add     edi,offset g_ui.bf.im_h
        movzx   ecx,word ptr [g_ui.bf.im_lb.len]
        cmp     cx,IMW-1
        jbe     @ims_s
        mov     ecx,IMW-1
@ims_s:
        mov     al,cl
        stosb
        mov     esi,offset g_ui.bf.im_lb.txt
        rep     movsb
        ret

; ::::::::::::::
; im_up - the immediate line before.
; ::::::::::::::
im_up:
        cmp     [g_ui.im.im_i],0
        je      @imu_r
        call    im_store
        dec     [g_ui.im.im_i]
        jmp     im_load
@imu_r:
        ret
; ::::::::::::::
; im_down - the next immediate line.
; ::::::::::::::
im_down:
        mov     ax,[g_ui.im.im_i]
        inc     ax
        cmp     ax,[g_ui.im.im_n]
        jae     @imu_r
        call    im_store
        inc     [g_ui.im.im_i]
        jmp     im_load

; ::::::::::::::
; im_next - Enter: the line kept, its text in inbuf; the cursor to the next
; line, a new one at the end, the oldest dropped when full.
;
; out:    AX  the text's length
; ::::::::::::::
im_next:
        call    im_store
        mov     esi,offset g_ui.bf.im_lb.txt
        mov     edi,offset g_inbuf
        movzx   ecx,word ptr [g_ui.bf.im_lb.len]
        push    ecx
        rep     movsb
        mov     ax,[g_ui.im.im_i]
        inc     ax
        cmp     ax,[g_ui.im.im_n]
        jb      @imn_g
        cmp     ax,IMN
        jb      @imn_a
        mov     esi,offset g_ui.bf.im_h+IMW
        mov     edi,offset g_ui.bf.im_h
        mov     ecx,(IMN-1)*IMW
        rep     movsb
        dec     ax
        jmp     @imn_c
@imn_a:
        inc     [g_ui.im.im_n]
@imn_c:
        movzx   edi,ax
        imul    edi,edi,IMW
        mov     byte ptr [g_ui.bf.im_h+edi],0
@imn_g:
        mov     [g_ui.im.im_i],ax
        call    im_load
        and     [g_ui.bf.im_lb.col],0
        pop     eax
        ret

        end
