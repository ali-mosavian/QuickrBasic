; ==========================================================================
; mouse.asm - the mouse, when a driver is there: clicks in the editor
; ==========================================================================
; holds:    ms_init, ms_show, ms_hide, ms_poll, ms_main, ms_title
; uses:     INT 33h; ed_goto, sel_mark, mn_run, wn_at, wn_focus and the run keys
; data:     ms_poll turns the left button into events (ms_ev): MS_PRESS,
;           MS_DRAG (moved while down), MS_UP, MS_MOVE (moved with none,
;           while the key loop waits: ms_hov); ms_pos the cell, DH the
;           row, DL the column. ui_getkey gives one as key FFFEh.
; calls:
;   ms_main            ms_title, mn_run, ed_goto
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; ms_init - ms_on set when a mouse driver answers; a null INT 33h is none.
; ::::::::::::::
ms_init:
        cmp     dword ptr ds:[33h*4],0  ; the real-mode vector, linear
        je      @msi_r
        xor     ax,ax
        int     33h
        cmp     ax,0FFFFh
        jne     @msi_r
        mov     [g_ui.ms.ms_on],1
@msi_r:
        ret

; ::::::::::::::
; ms_show - the mouse's cursor on.
; ::::::::::::::
ms_show:
        cmp     [g_ui.ms.ms_on],0
        je      @mss_r
        pushad
        mov     ax,1
        int     33h
        popad
@mss_r:
        ret

; ::::::::::::::
; ms_hide - the mouse's cursor off, before the screen changes under it.
; ::::::::::::::
ms_hide:
        cmp     [g_ui.ms.ms_on],0
        je      @msh_r
        pushad
        mov     ax,2
        int     33h
        popad
@msh_r:
        ret

; ::::::::::::::
; ms_poll - the left button's change since the last poll.
;
; out:    CF  set: an event in ms_ev, at ms_pos
; keeps:  all
; ::::::::::::::
ms_poll:
        cmp     [g_ui.ms.ms_on],0
        je      @msp_n
        pushad
        mov     ax,3
        int     33h
        shr     cx,3                    ; pixels to cells: 8 by 8
        shr     dx,3
        mov     dh,dl
        mov     dl,cl
        and     bl,1
        mov     al,MS_PRESS
        cmp     bl,[g_ui.ms.ms_b]
        mov     [g_ui.ms.ms_b],bl
        je      @msp_s
        test    bl,bl
        jnz     @msp_e
        mov     al,MS_UP
        jmp     @msp_e
@msp_s:
        test    bl,bl
        jz      @msp_m
        cmp     dx,[g_ui.ms.ms_pos]
        je      @msp_x
        mov     al,MS_DRAG
        jmp     @msp_e
@msp_m:                                 ; no button: a move, if the key
        cmp     [g_ui.ms.ms_hov],0      ; loop takes one
        je      @msp_x
        cmp     dx,[g_ui.ms.ms_pos]
        je      @msp_x
        mov     al,MS_MOVE
@msp_e:
        mov     [g_ui.ms.ms_ev],al
        mov     [g_ui.ms.ms_pos],dx
        popad
        stc
        ret
@msp_x:
        popad
@msp_n:
        clc
        ret

; ::::::::::::::
; ms_title - the menu whose title is at column DL of the bar.
;
; out:    CF  set: none
;         AL  the menu
; ::::::::::::::
ms_title:
        mov     ebx,NMENU-1
@mst_l:
        mov     al,[mn_x+ebx]
        dec     al
        cmp     dl,al
        jae     @mst_f
        dec     ebx
        jns     @mst_l
        stc
        ret
@mst_f:
        push    ebx
        mov     esi,[mn_tt+ebx*4]
        call    sc_slen
        pop     ebx
        add     al,cl
        inc     al
        cmp     al,dl
        jb      @mst_r
        mov     al,bl
@mst_r:
        ret

; ::::::::::::::
; ms_main - a mouse event in the editor: a title opens its menu; a click
; puts the cursor there, a drag selects; the scroll bar; the window; the
; keys on the status bar.
; ::::::::::::::
ms_main:
        mov     dx,[g_ui.ms.ms_pos]
        cmp     [g_ui.ms.ms_ev],MS_MOVE
        je      dp_hover
        cmp     [g_ui.ms.ms_ev],MS_UP
        je      @msm_r
        test    dh,dh
        jnz     @msm_w
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @msm_r
        call    ms_title
        jc      @msm_r
        mov     ah,1
        jmp     mn_run
@msm_w:
        cmp     dh,24
        je      @msm_st
        call    wn_at
        jc      @msm_r
        cmp     ebx,W_DBG
        je      dp_mouse
        cmp     ebx,W_ED1
        jb      @msm_r
        cmp     [g_ui.ms.ms_ev],MS_PRESS        ; a press: the focus there; a
        jne     @F                              ; drag stays in its window
        push    edx
        call    wn_focus
        pop     edx
        jmp     @msm_x
@@:
        call    wn_cur
        cmp     bl,[g_ui.wn.wact]
        je      @msm_x
        cmp     ebx,W_IMM
        jne     @msm_r
@msm_x:
        cmp     [g_ui.lp.ui_foc],0
        jne     @msm_r
        cmp     dh,byte ptr [g_ui.ed.ed_r0]     ; its text rows
        jb      @msm_r
        mov     al,byte ptr [g_ui.ed.ed_r0]
        add     al,byte ptr [g_ui.ed.ed_rows]
        cmp     dh,al
        jae     @msm_r
        mov     al,dl                   ; AL the text's column + 1
        sub     al,[g_ui.ed.ed_x]
        jbe     @msm_r
        dec     al
        cmp     al,byte ptr [g_ui.ed.ed_tw]
        je      @msm_sb
        ja      @msm_r
        movzx   ax,dh                   ; the line, the last at most
        sub     ax,[g_ui.ed.ed_r0]
        add     ax,[g_ui.ed.ed_top]
        mov     cx,[g_ui.tx.tx_n]
        cmp     ax,cx
        jb      @msm_l
        mov     ax,cx
        dec     ax
        jns     @msm_l
        xor     ax,ax
@msm_l:
        cmp     [g_ui.ms.ms_ev],MS_DRAG          ; a drag, a Shift+click select
        je      @msm_m
        test    [g_ui.lp.ui_kf],3
        jnz     @msm_m
        and     [g_ui.ed.sel_on],0
        jmp     @msm_g
@msm_m:
        push    eax
        call    sel_mark
        pop     eax
@msm_g:
        push    edx
        call    ed_goto
        pop     edx
        jc      @msm_r
        movzx   ax,dl
        sub     al,[g_ui.ed.ed_x]
        dec     ax
        add     ax,[g_ui.ed.ed_left]
        mov     [g_ui.bf.ed_lb.col],ax
@msm_r:
        ret
@msm_sb:
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @msm_r
        cmp     dh,byte ptr [g_ui.ed.ed_r0]
        je      ed_up
        mov     al,byte ptr [g_ui.ed.ed_r0]
        add     al,byte ptr [g_ui.ed.ed_rows]
        dec     al
        cmp     dh,al
        je      ed_down
        push    edx                      ; above the thumb or below it
        mov     ax,[g_ui.ed.ed_line]
        mov     cx,[g_ui.ed.ed_rows]
        sub     cx,3
        mul     cx
        mov     cx,[g_ui.tx.tx_n]
        jcxz    @msm_z
        div     cx
@msm_z:
        pop     edx
        add     al,byte ptr [g_ui.ed.ed_r0]
        inc     al
        cmp     dh,al
        jb      ed_pgup
        jmp     ed_pgdn
@msm_st:
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @msm_r
        cmp     [g_ui.lp.ui_help],0
        jne     @msm_r
        call    dr_skeys
@msm_y:
        lodsb
        test    al,al
        jnz     @msm_y
@msm_k:
        lodsb
        test    al,al
        jz      @msm_r
        cmp     dl,al
        lodsd
        ja      @msm_k
        jmp     eax

        end
