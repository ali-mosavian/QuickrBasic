; ==========================================================================
; draw.asm - the editor's screen: the windows, the scroll bars, the status
; ==========================================================================
; holds:    dr_all, dr_edit, dr_imm, dr_status, dr_cursor
; uses:     sc_*, tx_get, sel_get, mn_bar
; data:     Row 0 the menu bar; 1 the program window's title; 2 to 19 its
;           text, column 79 its scroll bar; 20 the sideways bar; 21 the
;           immediate window's title, 22 and 23 its lines; 24 the status.
; calls:
;   dr_all             mn_bar, dr_edit, dr_imm, dr_status, dr_cursor
;     dr_edit          dr_row a line at a time
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; dr_all - the whole screen and the cursor.
; ::::::::::::::
dr_all:
        call    mn_bar
        call    dr_edit
        call    dr_imm
        call    dr_status
        jmp     dr_cursor

; ::::::::::::::
; dr_edit - the program window: its frame, its lines, its scroll bars.
; ::::::::::::::
dr_edit:
        mov     dx,0100h
        mov     cx,1300h+80
        mov     ah,A_ED
        xor     esi,esi
        call    sc_box
        call    ui_title
        mov     esi,offset g_ui.bf.ui_ttl
        mov     dh,1
        mov     cx,0BFDAh
        mov     bl,[g_ui.lp.ui_foc]
        xor     bl,1
        mov     bh,1
        call    dr_ttl
        call    sel_get
        mov     [g_ui.lp.dr_sk],al
        xor     cx,cx                   ; CX the row
@dre_r:
        push    ecx
        call    dr_row
        pop     ecx
        inc     cx
        cmp     cx,ED_ROWS
        jb      @dre_r
        cmp     [g_ui.lp.ui_foc],0               ; the bars: the active window's
        jne     @dre_n
        call    dr_vbar
        jmp     dr_hbar
@dre_n:
        mov     dx,(ED_R0+ED_ROWS)*256+1
        mov     ax,A_ED*256+' '
        mov     cx,ED_COLS
        jmp     sc_fill

; ::::::::::::::
; dr_ttl - a window's title row DH, as QBasic's (uiwind.c): CL, CH its
; corners; the title at ESI centred, lit when BL is 1; BH 1: the maximise
; arrow.
; ::::::::::::::
dr_ttl:
        xor     dl,dl
        push    ecx
        mov     ah,A_ED
        mov     al,cl
        mov     cx,1
        call    sc_fill
        mov     al,0C4h
        mov     cx,78
        call    sc_fill
        pop     ecx
        mov     al,ch
        mov     cx,1
        call    sc_fill
        test    bh,bh
        jz      @drt_t
        mov     dl,75
        mov     ax,A_ED*256+0B4h
        call    sc_fill
        mov     ax,A_TTL*256+18h
        call    sc_fill
        mov     ax,A_ED*256+0C3h
        call    sc_fill
@drt_t:
        call    sc_slen                 ; at 1 + (80 - its length - 5) / 2
        mov     dl,75
        sub     dl,cl
        shr     dl,1
        inc     dl
        mov     ah,A_ED
        test    bl,bl
        jz      @drt_c
        mov     ah,A_TTL
@drt_c:
        mov     bh,ah
        mov     al,' '
        mov     cx,1
        call    sc_fill
        call    sc_str
        jmp     sc_fill

; ::::::::::::::
; dr_row - text row CX of the program window.
; ::::::::::::::
dr_row:
        mov     dh,cl
        add     dh,ED_R0
        mov     dl,1
        mov     ax,[g_ui.ed.ed_top]
        add     ax,cx
        mov     [g_ui.lp.dr_ln],ax
        cmp     ax,[g_ui.tx.tx_n]
        jb      @drr_t
        mov     ax,A_ED*256+' '
        mov     cx,ED_COLS
        jmp     sc_fill
@drr_t:
        mov     esi,offset g_ui.bf.ed_lb.txt
        mov     cx,[g_ui.bf.ed_lb.len]
        cmp     ax,[g_ui.ed.ed_line]
        je      @drr_g
        mov     edi,offset g_ui.bf.uc_line
        call    tx_get
        mov     esi,offset g_ui.bf.uc_line
@drr_g:
        movzx   ebx,[g_ui.ed.ed_left]            ; the part from ed_left
        sub     cx,bx
        jae     @drr_c
        xor     cx,cx
@drr_c:
        add     esi,ebx
        cmp     cx,ED_COLS
        jbe     @drr_w
        mov     cx,ED_COLS
@drr_w:
        mov     ah,A_ED
        push    edx
        call    sc_strn
        mov     bx,ED_COLS
        sub     bx,cx
        mov     cx,bx
        mov     al,' '
        call    sc_fill
        pop     edx
        call    ud_row
        mov     ax,[g_ui.lp.dr_ln]               ; the selection
        cmp     [g_ui.lp.dr_sk],2
        jne     @drr_p
        cmp     ax,[g_ui.ed.sel_a]
        jb      @drr_x
        cmp     ax,[g_ui.ed.sel_b]
        ja      @drr_x
        mov     ah,A_SEL
        mov     cx,ED_COLS
        jmp     sc_attr
@drr_p:
        cmp     [g_ui.lp.dr_sk],1
        jne     @drr_x
        cmp     ax,[g_ui.ed.ed_line]
        jne     @drr_x
        mov     ax,[g_ui.ed.sel_a]               ; the piece, clipped to the window
        mov     bx,[g_ui.ed.sel_b]
        sub     ax,[g_ui.ed.ed_left]
        jae     @drr_1
        xor     ax,ax
@drr_1:
        sub     bx,[g_ui.ed.ed_left]
        jbe     @drr_x
        cmp     bx,ED_COLS
        jbe     @drr_2
        mov     bx,ED_COLS
@drr_2:
        cmp     ax,bx
        jae     @drr_x
        add     dl,al
        mov     cx,bx
        sub     cx,ax
        mov     ah,A_SEL
        jmp     sc_attr
@drr_x:
        ret

; ::::::::::::::
; dr_vbar - the scroll bar at column 79: arrows, the track, the thumb at
; the cursor's line.
; ::::::::::::::
dr_vbar:
        mov     dx,ED_R0*256+79
        mov     ax,A_BAR*256+18h
        mov     cx,1
        call    sc_fill
        mov     dx,(ED_R0+1)*256+79
        mov     ecx,ED_ROWS-2
@drv_t:
        push    ecx
        mov     ax,A_BAR*256+0B0h
        mov     cx,1
        call    sc_fill
        dec     dl
        inc     dh
        pop     ecx
        loop    @drv_t
        mov     ax,A_BAR*256+19h
        mov     cx,1
        call    sc_fill
        mov     ax,[g_ui.ed.ed_line]             ; the thumb
        mov     cx,ED_ROWS-3
        mul     cx
        mov     cx,[g_ui.tx.tx_n]
        div     cx
        mov     dh,al
        add     dh,ED_R0+1
        mov     dl,79
        mov     ax,0000h+' '
        mov     cx,1
        jmp     sc_fill

; ::::::::::::::
; dr_hbar - the sideways bar on row 20, its thumb at ed_left.
; ::::::::::::::
dr_hbar:
        mov     dx,(ED_R0+ED_ROWS)*256+0
        mov     ax,A_ED*256+0B3h
        mov     cx,1
        call    sc_fill
        mov     ax,A_BAR*256+1Bh
        call    sc_fill
        mov     ax,A_BAR*256+0B0h
        mov     cx,ED_COLS-2
        call    sc_fill
        mov     ax,A_BAR*256+1Ah
        mov     cx,1
        call    sc_fill
        mov     ax,A_ED*256+0B3h
        call    sc_fill
        mov     ax,[g_ui.ed.ed_left]
        mov     cx,ED_COLS-3
        mul     cx
        mov     cx,LNMAX
        div     cx
        mov     dl,al
        add     dl,2
        mov     dh,ED_R0+ED_ROWS
        mov     ax,0000h+' '
        mov     cx,1
        jmp     sc_fill

; ::::::::::::::
; dr_imm - the immediate window: its title, its two lines.
; ::::::::::::::
dr_imm:
        mov     dh,IM_R0-1
        mov     esi,offset u_imm
        mov     cx,0B4C3h
        mov     bl,[g_ui.lp.ui_foc]
        xor     bh,bh
        call    dr_ttl
        mov     ax,[g_ui.im.im_i]                ; the window shows im_top and the next
        cmp     ax,[g_ui.im.im_top]
        jae     @dri_a
        mov     [g_ui.im.im_top],ax
@dri_a:
        mov     dx,[g_ui.im.im_top]
        inc     dx
        cmp     ax,dx
        jbe     @dri_b
        dec     ax
        mov     [g_ui.im.im_top],ax
@dri_b:
        xor     bx,bx
@dri_r:
        mov     dh,bl
        add     dh,IM_R0
        xor     dl,dl
        mov     ax,A_ED*256+0B3h
        mov     cx,1
        call    sc_fill
        mov     ax,[g_ui.im.im_top]
        add     ax,bx
        xor     cx,cx
        cmp     ax,[g_ui.im.im_n]
        jae     @dri_w
        mov     esi,offset g_ui.bf.im_lb.txt
        mov     cx,[g_ui.bf.im_lb.len]
        cmp     ax,[g_ui.im.im_i]
        je      @dri_w
        movzx   esi,ax
        imul    esi,esi,IMW
        add     esi,offset g_ui.bf.im_h
        movzx   cx,byte ptr [esi]
        inc     esi
@dri_w:
        cmp     cx,ED_COLS
        jbe     @dri_c
        mov     cx,ED_COLS
@dri_c:
        mov     ah,A_ED
        push    ecx
        call    sc_strn
        pop     ecx
        neg     cx
        add     cx,ED_COLS
        mov     al,' '
        call    sc_fill
        mov     ax,A_ED*256+0B3h
        mov     cx,1
        call    sc_fill
        inc     bx
        cmp     bx,IM_ROWS
        jb      @dri_r
        ret

; ::::::::::::::
; dr_skeys - the status bar's keys for the state: the editor's, Continue's
; (a STOP to go on from), the immediate window's.
;
; out:    ESI  the text; past its 0, its keys' last columns and routines
; ::::::::::::::
dr_skeys:
        mov     esi,offset u_kimm
        cmp     [g_ui.lp.ui_foc],0
        jne     @drk_r
        mov     esi,offset u_kedit
        call    ui_stopped
        jnz     @drk_r
        mov     esi,offset u_krun
@drk_r:
        ret

; ::::::::::::::
; dr_status - the status bar: the keys, or a help line after F1's; the
; cursor's line and column when they fit, from column 62, as QBasic.
; ::::::::::::::
dr_status:
        mov     dx,1800h
        mov     ax,A_STAT*256+' '
        mov     cx,80
        call    sc_fill
        mov     dx,1801h
        mov     ah,A_STAT
        mov     bh,ah
        mov     esi,[g_ui.lp.ui_help]
        test    esi,esi
        jnz     @drs_h
        call    dr_skeys
        call    sc_str
        jmp     @drs_p
@drs_h:
        push    esi
        mov     esi,offset u_f1         ; a menu item's: F1=Help and a bar
        cmp     dword ptr [esp],offset u_mbar
        je      @drs_b
        cmp     dword ptr [esp],offset u_dlg
        jne     @drs_f
@drs_b:
        mov     esi,offset u_f1s
@drs_f:
        call    sc_str
        pop     esi
        call    sc_str
        cmp     dl,62
        jbe     @drs_p
        ret
@drs_p:
        mov     dx,183Eh
        mov     ax,A_STAT*256+0B3h
        mov     cx,1
        call    sc_fill
        mov     dl,64                   ; ^Q pending
        mov     al,' '
        mov     cx,2
        cmp     [g_ui.ed.ed_pfx],0
        je      @drs_q
        mov     cx,1
        mov     al,'^'
        call    sc_fill
        mov     al,'Q'
@drs_q:
        call    sc_fill
        mov     cx,1
        mov     dl,67                   ; C Caps Lock, N Num Lock
        mov     al,'C'
        test    [g_ui.lp.ui_kf],40h
        jnz     @drs_c
        mov     al,' '
@drs_c:
        call    sc_fill
        mov     al,'N'
        test    [g_ui.lp.ui_kf],20h
        jnz     @drs_n
        mov     al,' '
@drs_n:
        call    sc_fill
        mov     edi,offset g_ui.lp.ui_pos
        mov     ax,[g_ui.ed.ed_line]
        inc     ax
        mov     cx,5
        call    ui_decn
        mov     al,':'
        stosb
        mov     ax,[g_ui.bf.ed_lb.col]
        inc     ax
        mov     cx,3
        call    ui_decn
        xor     al,al
        stosb
        mov     dx,1846h
        mov     esi,offset g_ui.lp.ui_pos
        mov     ah,A_STAT
        mov     bh,ah
        jmp     sc_str

; ::::::::::::::
; ui_decn - AX in CX decimal digits at EDI, zeros before.
;
; out:    EDI  past them
; ::::::::::::::
ui_decn:
        push    ebx
        push    edx
        movzx   ecx,cx
        add     edi,ecx
        push    edi
        mov     bx,10
@udn_l:
        xor     dx,dx
        div     bx
        add     dl,'0'
        dec     edi
        mov     [edi],dl
        loop    @udn_l
        pop     edi
        pop     edx
        pop     ebx
        ret

; ::::::::::::::
; dr_cursor - the cursor in the window that has it, its shape the mode's.
; ::::::::::::::
dr_cursor:
        mov     cx,0607h
        cmp     [g_ui.ed.ed_ins],0
        jne     @drc_s
        mov     cx,0407h
@drc_s:
        call    sc_shape
        cmp     [g_ui.lp.ui_foc],0
        jne     @drc_i
        call    ed_ensure
        mov     ax,[g_ui.ed.ed_line]
        sub     ax,[g_ui.ed.ed_top]
        add     al,ED_R0
        mov     dh,al
        mov     ax,[g_ui.bf.ed_lb.col]
        sub     ax,[g_ui.ed.ed_left]
        inc     al
        mov     dl,al
        jmp     sc_cur
@drc_i:
        mov     ax,[g_ui.im.im_i]
        sub     ax,[g_ui.im.im_top]
        add     al,IM_R0
        mov     dh,al
        mov     ax,[g_ui.bf.im_lb.col]
        cmp     ax,ED_COLS-1
        jbe     @drc_c
        mov     ax,ED_COLS-1
@drc_c:
        inc     al
        mov     dl,al
        jmp     sc_cur

; ::::::::::::::
; ui_title - the window's title in ui_ttl: the file's name or Untitled.
; ::::::::::::::
ui_title:
        push    esi
        push    edi
        push    eax
        mov     esi,offset g_ui.bf.ui_fn
        cmp     byte ptr [esi],0
        jne     @utt_c
        mov     esi,offset u_untl
@utt_c:
        mov     edi,offset g_ui.bf.ui_ttl
@utt_l:
        lodsb
        stosb
        test    al,al
        jnz     @utt_l
        pop     eax
        pop     edi
        pop     esi
        ret

        end
