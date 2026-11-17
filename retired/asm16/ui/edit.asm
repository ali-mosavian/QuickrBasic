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

LB_COL  = 0
LB_LEN  = 2
LB_DTY  = 4
LB_TXT  = 5

; ::::::::::::::
; le_pad - the text of LB BX padded with spaces to its cursor.
;
; keeps:  all
; ::::::::::::::
le_pad:
        pusha
        mov     cx,[bx+LB_COL]
        mov     di,[bx+LB_LEN]
        cmp     cx,di
        jbe     @lep_r
        mov     [bx+LB_LEN],cx
        sub     cx,di
        lea     di,[bx+di+LB_TXT]
        mov     al,' '
        rep     stosb
@lep_r:
        popa
        ret

; ::::::::::::::
; le_ins - AL typed into LB BX: inserted, or over the character there.
;
; in:     DX  the most characters
; out:    CF  set: full
; keeps:  all
; ::::::::::::::
le_ins:
        pusha
        mov     si,[bx+LB_COL]
        cmp     si,dx
        jae     @lei_f
        call    le_pad
        cmp     [ed_ins],0
        jne     @lei_i
        cmp     si,[bx+LB_LEN]
        jb      @lei_s
@lei_i:
        mov     cx,[bx+LB_LEN]
        cmp     cx,dx
        jae     @lei_f
        sub     cx,si                   ; the characters from the cursor
        lea     di,[bx+LB_TXT]
        add     di,[bx+LB_LEN]
        lea     si,[di-1]
        std
        rep     movsb
        cld
        inc     word ptr [bx+LB_LEN]
        mov     si,[bx+LB_COL]
@lei_s:
        mov     [bx+si+LB_TXT],al
        inc     word ptr [bx+LB_COL]
        mov     byte ptr [bx+LB_DTY],1
        popa
        clc
        ret
@lei_f:
        popa
        stc
        ret

; ::::::::::::::
; le_cut - characters CX to DX of LB BX out; the cursor to CX.
;
; keeps:  all
; ::::::::::::::
le_cut:
        pusha
        mov     ax,[bx+LB_LEN]
        cmp     dx,ax
        jbe     @lec_a
        mov     dx,ax
@lec_a:
        mov     [bx+LB_COL],cx
        cmp     cx,dx
        jae     @lec_r
        lea     di,[bx+LB_TXT]
        add     di,cx
        lea     si,[bx+LB_TXT]
        add     si,dx
        sub     ax,dx                   ; what follows
        sub     dx,cx
        sub     [bx+LB_LEN],dx
        mov     cx,ax
        rep     movsb
        mov     byte ptr [bx+LB_DTY],1
@lec_r:
        popa
        ret

; ::::::::::::::
; le_bs - Backspace in LB BX.
;
; out:    CF  set: at column 0, nothing done
; keeps:  all
; ::::::::::::::
le_bs:
        cmp     word ptr [bx+LB_COL],0
        je      @leb_n
        push    cx
        push    dx
        mov     cx,[bx+LB_COL]
        dec     cx
        mov     dx,cx
        inc     dx
        call    le_cut
        mov     [bx+LB_COL],cx
        pop     dx
        pop     cx
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
        push    cx
        push    dx
        mov     cx,[bx+LB_COL]
        cmp     cx,[bx+LB_LEN]
        jae     @led_n
        mov     dx,cx
        inc     dx
        call    le_cut
        pop     dx
        pop     cx
        clc
        ret
@led_n:
        pop     dx
        pop     cx
        stc
        ret

; ::::::::::::::
; le_indent - the spaces LB BX starts with.
;
; out:    CX  their count
; keeps:  all but CX
; ::::::::::::::
le_indent:
        push    si
        xor     cx,cx
        lea     si,[bx+LB_TXT]
@lin_l:
        cmp     cx,[bx+LB_LEN]
        jae     @lin_r
        cmp     byte ptr [si],' '
        jne     @lin_r
        inc     si
        inc     cx
        jmp     @lin_l
@lin_r:
        pop     si
        ret

; ::::::::::::::
; le_home - to the first character, or to column 0 when there.
; ::::::::::::::
le_home:
        call    le_indent
        cmp     cx,[bx+LB_COL]
        jne     @lh_s
        xor     cx,cx
@lh_s:
        mov     [bx+LB_COL],cx
        ret

; ::::::::::::::
; le_end - past the last character.
; ::::::::::::::
le_end:
        mov     ax,[bx+LB_LEN]
        mov     [bx+LB_COL],ax
        ret

; ::::::::::::::
; le_left - the cursor one column left.
; ::::::::::::::
le_left:
        cmp     word ptr [bx+LB_COL],0
        je      @lel_r
        dec     word ptr [bx+LB_COL]
@lel_r:
        ret
; ::::::::::::::
; le_right - the cursor one column right.
; ::::::::::::::
le_right:
        cmp     word ptr [bx+LB_COL],LNMAX
        jae     @lel_r
        inc     word ptr [bx+LB_COL]
        ret

; ::::::::::::::
; le_wleft - the cursor to the word before.
; ::::::::::::::
le_wleft:
        mov     si,[bx+LB_COL]
        cmp     si,[bx+LB_LEN]
        jbe     @lwl_s
        mov     si,[bx+LB_LEN]
@lwl_s:
        test    si,si
        jz      @lwl_r
        dec     si
        cmp     byte ptr [bx+si+LB_TXT],' '
        je      @lwl_s
@lwl_w:
        test    si,si
        jz      @lwl_r
        cmp     byte ptr [bx+si+LB_TXT-1],' '
        je      @lwl_r
        dec     si
        jmp     @lwl_w
@lwl_r:
        mov     [bx+LB_COL],si
        ret
; ::::::::::::::
; le_wright - the cursor to the next word.
; ::::::::::::::
le_wright:
        mov     si,[bx+LB_COL]
@lwr_w:
        cmp     si,[bx+LB_LEN]
        jae     @lwr_r
        cmp     byte ptr [bx+si+LB_TXT],' '
        je      @lwr_s
        inc     si
        jmp     @lwr_w
@lwr_s:
        cmp     si,[bx+LB_LEN]
        jae     @lwr_r
        cmp     byte ptr [bx+si+LB_TXT],' '
        jne     @lwr_r
        inc     si
        jmp     @lwr_s
@lwr_r:
        mov     [bx+LB_COL],si
        ret

; ::::::::::::::
; le_insn - CX characters from SI into LB BX at its cursor.
;
; out:    CF  set: they do not fit, nothing done
; ::::::::::::::
le_insn:
        call    le_pad
        mov     ax,[bx+LB_LEN]
        add     ax,cx
        cmp     ax,LNMAX
        ja      @lis_f
        push    si
        push    cx
        mov     dx,[bx+LB_LEN]
        sub     dx,[bx+LB_COL]          ; the tail
        lea     si,[bx+LB_TXT-1]
        add     si,[bx+LB_LEN]
        mov     di,si
        add     di,cx
        mov     cx,dx
        std
        rep     movsb
        cld
        pop     cx
        pop     si
        lea     di,[bx+LB_TXT]
        add     di,[bx+LB_COL]
        add     [bx+LB_LEN],cx
        add     [bx+LB_COL],cx
        rep     movsb
        mov     byte ptr [bx+LB_DTY],1
        clc
        ret
@lis_f:
        stc
        ret

; ::::::::::::::
; ed_load - line AX into ed_lb; the column stays.
; ::::::::::::::
ed_load:
        mov     [ed_line],ax
        mov     di,offset ed_lb+LB_TXT
        call    tx_get
        mov     ds:[ed_lb+LB_LEN],cx
        and     byte ptr ds:[ed_lb+LB_DTY],0
        ret

; ::::::::::::::
; ed_commit - a changed line back to the text: spaces at its end cut, its
; keywords capitalised, its syntax checked.
;
; out:    CF  set: a syntax error was shown; the line is stored as typed
; ::::::::::::::
ed_commit:
        cmp     byte ptr ds:[ed_lb+LB_DTY],0
        je      @edc_r
        and     byte ptr ds:[ed_lb+LB_DTY],0
        mov     cx,ds:[ed_lb+LB_LEN]
@edc_t:
        jcxz    @edc_s
        mov     bx,cx
        cmp     byte ptr [ed_lb+LB_TXT-1+bx],' '
        jne     @edc_s
        dec     cx
        jmp     @edc_t
@edc_s:
        mov     ds:[ed_lb+LB_LEN],cx
        mov     [tx_mod],1
        mov     [tx_chg],1
        mov     si,offset ed_lb+LB_TXT
        call    ui_canon
        pushf
        jc      @edc_w
        movzx   cx,[sbx]
        mov     ds:[ed_lb+LB_LEN],cx
        mov     si,offset sbuf
        mov     di,offset ed_lb+LB_TXT
        rep     movsb
@edc_w:
        mov     ax,[ed_line]
        mov     si,offset ed_lb+LB_TXT
        mov     cx,ds:[ed_lb+LB_LEN]
        call    tx_repl
        popf
        jnc     @edc_r
        mov     si,offset ui_ebuf
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
        mov     [uc_src],si
        mov     [uc_n],cx
        cmp     cx,200
        ja      @uca_k
        mov     bx,si
        add     bx,cx
        mov     byte ptr [bx],0
        mov     di,si                   ; DI past the label and the indent
@uca_b:
        cmp     byte ptr [di],' '
        jne     @uca_d
        inc     di
        jmp     @uca_b
@uca_d:
        mov     al,[di]
        cmp     al,'0'
        jb      @uca_e
        cmp     al,'9'
        ja      @uca_e
        inc     di
        jmp     @uca_d
@uca_e:
        cmp     byte ptr [di],' '
        jne     @uca_f
        inc     di
        jmp     @uca_e
@uca_f:
        cmp     byte ptr [di],0
        je      @uca_k
        mov     [uc_pre],di
        push    si
        mov     si,di
        mov     di,offset inbuf
@uca_c:
        lodsb
        stosb
        test    al,al
        jnz     @uca_c
        mov     [lxd],0
        mov     [ln],DLINE
        mov     si,offset inbuf
        mov     di,offset cbuf
        call    lx_line
        pop     si
        cmp     [syn_on],0
        je      @uca_l
        push    si
        call    cm_line
        mov     [uc_err],0
        mov     si,offset xbuf
        mov     dx,offset ui_chkop
        call    cm_walk
        pop     si
        mov     al,[uc_err]
        test    al,al
        jz      @uca_l
        jmp     ui_emsg
@uca_l:
        mov     [ocap],80h
        and     [sbx],0
@uca_p:
        cmp     si,[uc_pre]
        jae     @uca_q
        lodsb
        call    io_putc
        jmp     @uca_p
@uca_q:
        mov     si,offset cbuf
        call    ls_body
        and     [ocap],0
        clc
        ret
@uca_k:
        mov     si,[uc_src]
        mov     cx,[uc_n]
        mov     [sbx],cl
        mov     di,offset sbuf
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
        mov     al,[si+1]
        mov     [uc_err],al
@uck_r:
        ret

; ::::::::::::::
; ui_emsg - the message of error AL in ui_ebuf: "Syntax error".
;
; out:    CF  set
; ::::::::::::::
ui_emsg:
        push    ax
        mov     [ocap],80h
        and     [sbx],0
        call    io_msg
        mov     al,MSG_ERR
        call    io_msg
        and     [ocap],0
        pop     ax
        movzx   cx,[sbx]
        mov     si,offset sbuf
        mov     di,offset ui_ebuf
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
        push    ax
        call    ed_commit
        pop     ax
        jc      @edg_r
        call    ed_load
        clc
@edg_r:
        ret

; ::::::::::::::
; ed_up - the cursor one line up.
; ::::::::::::::
ed_up:
        mov     ax,[ed_line]
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
        mov     ax,[ed_line]
        inc     ax
        cmp     ax,[tx_n]
        jae     @edu_r
        jmp     ed_goto
; ::::::::::::::
; ed_pgup - a window of lines up.
; ::::::::::::::
ed_pgup:
        mov     ax,[ed_line]
        sub     ax,ED_ROWS-1
        jae     @edp_u
        xor     ax,ax
@edp_u:
        mov     dx,[ed_top]
        sub     dx,ED_ROWS-1
        jae     @edp_t
        xor     dx,dx
@edp_t:
        mov     [ed_top],dx
        jmp     ed_goto
; ::::::::::::::
; ed_pgdn - a window of lines down.
; ::::::::::::::
ed_pgdn:
        mov     ax,[ed_line]
        add     ax,ED_ROWS-1
        mov     dx,[tx_n]
        dec     dx
        cmp     ax,dx
        jbe     @edp_d
        mov     ax,dx
@edp_d:
        add     [ed_top],ED_ROWS-1
        jmp     ed_goto
; ::::::::::::::
; ed_first - the cursor to the first line.
; ::::::::::::::
ed_first:
        xor     ax,ax
        mov     ds:[ed_lb+LB_COL],ax
        jmp     ed_goto
; ::::::::::::::
; ed_last - the cursor to the last line.
; ::::::::::::::
ed_last:
        mov     ax,[tx_n]
        dec     ax
        call    ed_goto
        jc      @edu_r
        mov     ax,ds:[ed_lb+LB_LEN]
        mov     ds:[ed_lb+LB_COL],ax
        ret

; ::::::::::::::
; ed_ensure - ed_top and ed_left so that the cursor shows.
; ::::::::::::::
ed_ensure:
        mov     ax,[ed_line]
        cmp     ax,[ed_top]
        jae     @ede_b
        mov     [ed_top],ax
@ede_b:
        mov     dx,[ed_top]
        add     dx,ED_ROWS
        cmp     ax,dx
        jb      @ede_m
        sub     ax,ED_ROWS-1
        mov     [ed_top],ax
@ede_m:
        mov     ax,[tx_n]
        cmp     [ed_top],ax
        jb      @ede_h
        dec     ax
        mov     [ed_top],ax
@ede_h:
        mov     ax,ds:[ed_lb+LB_COL]
        cmp     ax,[ed_left]
        jae     @ede_r
        mov     [ed_left],ax
@ede_r:
        mov     dx,[ed_left]
        add     dx,ED_COLS
        cmp     ax,dx
        jb      @ede_x
        sub     ax,ED_COLS-1
        mov     [ed_left],ax
@ede_x:
        ret

; ::::::::::::::
; ed_enter - Enter: the line split at the cursor, the new line indented as
; this one.
; ::::::::::::::
ed_enter:
        mov     bx,offset ed_lb
        call    le_indent
        cmp     cx,ds:[ed_lb+LB_COL]
        jbe     @een_i
        mov     cx,ds:[ed_lb+LB_COL]
@een_i:
        mov     [ed_ind],cx
        mov     di,offset sy_buf        ; the new line: the indent, the rest
        mov     al,' '
        rep     stosb
        mov     si,ds:[ed_lb+LB_COL]
        mov     cx,ds:[ed_lb+LB_LEN]
        cmp     si,cx
        jae     @een_n
        mov     ds:[ed_lb+LB_LEN],si
        mov     byte ptr ds:[ed_lb+LB_DTY],1
        sub     cx,si
        add     si,offset ed_lb+LB_TXT
@een_s:
        cmp     byte ptr [si],' '
        jne     @een_c
        inc     si
        loop    @een_s
@een_c:
        rep     movsb
@een_n:
        mov     [ed_nlen],di
        call    ed_commit
        mov     cx,[ed_nlen]
        sub     cx,offset sy_buf
        mov     ax,[ed_line]
        inc     ax
        mov     si,offset sy_buf
        call    tx_insl
        jc      @een_r
        mov     [tx_mod],1
        mov     [tx_chg],1
        call    ed_load
        mov     ax,[ed_ind]
        mov     ds:[ed_lb+LB_COL],ax
@een_r:
        ret

; ::::::::::::::
; ed_bs - Backspace: at column 0 the line joins the one above.
; ::::::::::::::
ed_bs:
        mov     bx,offset ed_lb
        call    le_bs
        jnc     @edb_r
        mov     ax,[ed_line]
        test    ax,ax
        jz      @edb_r
        mov     si,offset ed_lb+LB_TXT  ; this line, kept
        mov     di,offset sy_buf
        mov     cx,ds:[ed_lb+LB_LEN]
        mov     [ed_nlen],cx
        rep     movsb
        dec     ax
        mov     di,offset uc_line
        call    tx_get
        add     cx,[ed_nlen]
        cmp     cx,LNMAX
        ja      @edb_r
        mov     ax,[ed_line]
        call    tx_dell
        dec     ax
        call    ed_load
        mov     ax,ds:[ed_lb+LB_LEN]
        mov     ds:[ed_lb+LB_COL],ax
        mov     si,offset sy_buf
        mov     cx,[ed_nlen]
        mov     bx,offset ed_lb
        call    le_insn
        mov     ax,ds:[ed_lb+LB_LEN]
        sub     ax,[ed_nlen]
        mov     ds:[ed_lb+LB_COL],ax
        mov     byte ptr ds:[ed_lb+LB_DTY],1
        mov     [tx_mod],1
        mov     [tx_chg],1
@edb_r:
        ret

; ::::::::::::::
; ed_del - Del: past the end the next line joins this one.
; ::::::::::::::
ed_del:
        mov     bx,offset ed_lb
        call    le_del
        jnc     @edd_r
        mov     ax,[ed_line]
        inc     ax
        cmp     ax,[tx_n]
        jae     @edd_r
        mov     di,offset sy_buf
        call    tx_get
        mov     [ed_nlen],cx
        call    le_pad
        mov     ax,ds:[ed_lb+LB_LEN]
        add     ax,cx
        cmp     ax,LNMAX
        ja      @edd_r
        mov     ax,[ed_line]
        inc     ax
        call    tx_dell
        push    word ptr ds:[ed_lb+LB_COL]
        mov     si,offset sy_buf
        mov     cx,[ed_nlen]
        call    le_insn
        pop     word ptr ds:[ed_lb+LB_COL]
        mov     byte ptr ds:[ed_lb+LB_DTY],1
        mov     [tx_mod],1
        mov     [tx_chg],1
@edd_r:
        ret

; ::::::::::::::
; ed_tab - Tab: spaces to the next column of 8.
; ::::::::::::::
ed_tab:
        mov     bx,offset ed_lb
@edt_l:
        mov     al,' '
        mov     dx,LNMAX
        call    le_ins
        jc      @edt_r
        test    word ptr ds:[ed_lb+LB_COL],7
        jnz     @edt_l
@edt_r:
        ret

; ::::::::::::::
; ed_ctrly - Ctrl+Y: the line to the clipboard, out of the text.
; ::::::::::::::
ed_ctrly:
        cmp     byte ptr ds:[ed_lb+LB_DTY],0 ; kept as typed: no syntax check
        je      @edy_c
        and     byte ptr ds:[ed_lb+LB_DTY],0
        mov     ax,[ed_line]
        mov     si,offset ed_lb+LB_TXT
        mov     cx,ds:[ed_lb+LB_LEN]
        call    tx_repl
@edy_c:
        mov     ax,[ed_line]
        mov     dx,ax
        call    cb_lines
        call    tx_dell
        mov     [tx_mod],1
        mov     [tx_chg],1
        cmp     ax,[tx_n]
        jb      @edy_l
        mov     ax,[tx_n]
        dec     ax
@edy_l:
        call    ed_load
        and     ds:[ed_lb+LB_COL],0
        ret

; ::::::::::::::
; sel_mark - a shifted move: the selection starts here if not yet on.
; ::::::::::::::
sel_mark:
        cmp     [sel_on],0
        jne     @slm_r
        mov     [sel_on],1
        mov     ax,[ed_line]
        mov     [sel_l],ax
        mov     ax,ds:[ed_lb+LB_COL]
        mov     [sel_c],ax
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
        cmp     [sel_on],0
        je      @slg_r
        push    bx
        push    dx
        mov     ax,[sel_l]
        cmp     ax,[ed_line]
        jne     @slg_l
        mov     ax,[sel_c]
        mov     dx,ds:[ed_lb+LB_COL]
        cmp     ax,dx
        jbe     @slg_o
        xchg    ax,dx
@slg_o:
        mov     [sel_a],ax
        mov     [sel_b],dx
        cmp     ax,dx
        mov     al,0
        je      @slg_x
        mov     al,1
        jmp     @slg_x
@slg_l:
        mov     dx,[ed_line]
        mov     bx,ds:[ed_lb+LB_COL]       ; the lower end's column
        cmp     ax,dx
        jb      @slg_u
        xchg    ax,dx
        mov     bx,[sel_c]
@slg_u:
        test    bx,bx
        jnz     @slg_k
        dec     dx
@slg_k:
        mov     [sel_a],ax
        mov     [sel_b],dx
        mov     al,2
@slg_x:
        pop     dx
        pop     bx
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
        mov     si,offset ed_lb+LB_TXT
        add     si,[sel_a]
        mov     cx,[sel_b]
        mov     dx,ds:[ed_lb+LB_LEN]
        cmp     cx,dx
        jbe     @edo_p
        mov     cx,dx
@edo_p:
        sub     cx,[sel_a]
        jae     @edo_q
        xor     cx,cx
@edo_q:
        xor     al,al
        call    cb_put
        mov     al,1
        ret
@edo_l:
        call    ed_commit
        mov     ax,[sel_a]
        mov     dx,[sel_b]
        call    cb_lines
        mov     al,2
@edy_r:
        ret

; ::::::::::::::
; ed_clear - the selection out of the text; off.
; ::::::::::::::
ed_clear:
        call    sel_get
        and     [sel_on],0
        cmp     al,1
        jb      @edl_r
        ja      @edl_l
        mov     cx,[sel_a]
        mov     dx,[sel_b]
        mov     bx,offset ed_lb
        call    le_cut
        mov     ds:[ed_lb+LB_COL],cx
        ret
@edl_l:
        call    ed_commit
        mov     ax,[sel_a]
        mov     cx,[sel_b]
        sub     cx,ax
        inc     cx
@edl_d:
        call    tx_dell
        loop    @edl_d
        mov     [tx_mod],1
        mov     [tx_chg],1
        cmp     ax,[tx_n]
        jb      @edl_g
        mov     ax,[tx_n]
        dec     ax
@edl_g:
        call    ed_load
        and     ds:[ed_lb+LB_COL],0
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
        mov     cx,[cb_len]
        jcxz    @eps_r
        cmp     [cb_ln],0
        jne     @eps_l
        push    ds
        mov     si,0
        mov     di,offset sy_buf
        mov     ds,[clpseg]
        rep     movsb
        pop     ds
        mov     cx,[cb_len]
        mov     si,offset sy_buf
        mov     bx,offset ed_lb
        jmp     le_insn
@eps_l:
        call    ed_commit
        xor     si,si                   ; SI in the clipboard
        mov     dx,[ed_line]            ; DX the line, AL the characters
@eps_n:
        cmp     si,[cb_len]
        jae     @eps_e
        mov     di,offset sy_buf
        push    ds
        mov     ds,[clpseg]
@eps_c:
        lodsb
        cmp     al,NL
        je      @eps_d
        stosb
        jmp     @eps_c
@eps_d:
        pop     ds
        mov     cx,di
        sub     cx,offset sy_buf
        push    si
        mov     si,offset sy_buf
        mov     ax,dx
        call    tx_insl
        pop     si
        jc      @eps_e
        inc     dx
        jmp     @eps_n
@eps_e:
        mov     ax,dx
        mov     [tx_mod],1
        mov     [tx_chg],1
        call    ed_load
@eps_r:
        ret

; ::::::::::::::
; im_load - immediate line im_i into im_lb.
; ::::::::::::::
im_load:
        mov     si,[im_i]
        imul    si,si,IMW
        add     si,offset im_h
        movzx   cx,byte ptr [si]
        inc     si
        mov     ds:[im_lb+LB_LEN],cx
        mov     di,offset im_lb+LB_TXT
        rep     movsb
        and     byte ptr ds:[im_lb+LB_DTY],0
        ret

; ::::::::::::::
; im_store - im_lb back to its line, cut to the window.
; ::::::::::::::
im_store:
        mov     di,[im_i]
        imul    di,di,IMW
        add     di,offset im_h
        mov     cx,ds:[im_lb+LB_LEN]
        cmp     cx,IMW-1
        jbe     @ims_s
        mov     cx,IMW-1
@ims_s:
        mov     al,cl
        stosb
        mov     si,offset im_lb+LB_TXT
        rep     movsb
        ret

; ::::::::::::::
; im_up - the immediate line before.
; ::::::::::::::
im_up:
        cmp     [im_i],0
        je      @imu_r
        call    im_store
        dec     [im_i]
        jmp     im_load
@imu_r:
        ret
; ::::::::::::::
; im_down - the next immediate line.
; ::::::::::::::
im_down:
        mov     ax,[im_i]
        inc     ax
        cmp     ax,[im_n]
        jae     @imu_r
        call    im_store
        inc     [im_i]
        jmp     im_load

; ::::::::::::::
; im_next - Enter: the line kept, its text in inbuf; the cursor to the next
; line, a new one at the end, the oldest dropped when full.
;
; out:    AX  the text's length
; ::::::::::::::
im_next:
        call    im_store
        mov     si,offset im_lb+LB_TXT
        mov     di,offset inbuf
        mov     cx,ds:[im_lb+LB_LEN]
        push    cx
        rep     movsb
        mov     ax,[im_i]
        inc     ax
        cmp     ax,[im_n]
        jb      @imn_g
        cmp     ax,IMN
        jb      @imn_a
        mov     si,offset im_h+IMW
        mov     di,offset im_h
        mov     cx,(IMN-1)*IMW
        rep     movsb
        dec     ax
        jmp     @imn_c
@imn_a:
        inc     [im_n]
@imn_c:
        mov     di,ax
        imul    di,di,IMW
        mov     byte ptr [im_h+di],0
@imn_g:
        mov     [im_i],ax
        call    im_load
        and     ds:[im_lb+LB_COL],0
        pop     ax
        ret
