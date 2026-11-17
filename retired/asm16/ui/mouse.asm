; ==========================================================================
; mouse.asm - the mouse, when a driver is there: clicks in the editor
; ==========================================================================
; holds:    ms_init, ms_show, ms_hide, ms_poll, ms_main, ms_title
; uses:     INT 33h; ed_goto, sel_mark, mn_run, ui_win and the run keys
; data:     ms_poll turns the left button into events (ms_ev): MS_PRESS,
;           MS_DRAG (moved while down), MS_UP; ms_pos the cell, DH the
;           row, DL the column. ui_getkey gives one as key FFFEh.
; calls:
;   ms_main            ms_title, mn_run, ed_goto
; ==========================================================================

MS_PRESS = 1
MS_DRAG  = 2
MS_UP    = 3
MS_KEY   = 0FFFEh

; ::::::::::::::
; ms_init - ms_on set when a mouse driver answers; a null INT 33h is none.
; ::::::::::::::
ms_init:
        push    es
        xor     ax,ax
        mov     es,ax
        mov     ax,es:[33h*4]
        or      ax,es:[33h*4+2]
        pop     es
        jz      @msi_r
        xor     ax,ax
        int     33h
        cmp     ax,0FFFFh
        jne     @msi_r
        mov     [ms_on],1
@msi_r:
        ret

; ::::::::::::::
; ms_show - the mouse's cursor on.
; ::::::::::::::
ms_show:
        cmp     [ms_on],0
        je      @mss_r
        pusha
        mov     ax,1
        int     33h
        popa
@mss_r:
        ret

; ::::::::::::::
; ms_hide - the mouse's cursor off, before the screen changes under it.
; ::::::::::::::
ms_hide:
        cmp     [ms_on],0
        je      @msh_r
        pusha
        mov     ax,2
        int     33h
        popa
@msh_r:
        ret

; ::::::::::::::
; ms_poll - the left button's change since the last poll.
;
; out:    CF  set: an event in ms_ev, at ms_pos
; keeps:  all
; ::::::::::::::
ms_poll:
        cmp     [ms_on],0
        je      @msp_n
        pusha
        mov     ax,3
        int     33h
        shr     cx,3                    ; pixels to cells: 8 by 8
        shr     dx,3
        mov     dh,dl
        mov     dl,cl
        and     bl,1
        mov     al,MS_PRESS
        cmp     bl,[ms_b]
        mov     [ms_b],bl
        je      @msp_s
        test    bl,bl
        jnz     @msp_e
        mov     al,MS_UP
        jmp     @msp_e
@msp_s:
        test    bl,bl
        jz      @msp_x
        cmp     dx,[ms_pos]
        je      @msp_x
        mov     al,MS_DRAG
@msp_e:
        mov     [ms_ev],al
        mov     [ms_pos],dx
        popa
        stc
        ret
@msp_x:
        popa
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
        mov     bx,NMENU-1
@mst_l:
        mov     al,[mn_x+bx]
        dec     al
        cmp     dl,al
        jae     @mst_f
        dec     bx
        jns     @mst_l
        stc
        ret
@mst_f:
        push    bx
        add     bx,bx
        mov     si,[mn_tt+bx]
        call    sc_slen
        pop     bx
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
        mov     dx,[ms_pos]
        cmp     [ms_ev],MS_UP
        je      @msm_r
        test    dh,dh
        jnz     @msm_w
        cmp     [ms_ev],MS_PRESS
        jne     @msm_r
        call    ms_title
        jc      @msm_r
        mov     ah,1
        jmp     mn_run
@msm_w:
        cmp     dh,24
        je      @msm_st
        cmp     dh,IM_R0
        jae     @msm_im
        cmp     dh,ED_R0
        jb      @msm_r
        cmp     dh,ED_R0+ED_ROWS
        jae     @msm_r
        cmp     dl,79
        je      @msm_sb
        test    dl,dl
        jz      @msm_r
        cmp     dl,ED_COLS
        ja      @msm_r
        cmp     [ui_foc],0
        je      @msm_e
        push    dx
        call    ui_win
        pop     dx
@msm_e:
        movzx   ax,dh                   ; the line, the last at most
        sub     ax,ED_R0
        add     ax,[ed_top]
        mov     cx,[tx_n]
        cmp     ax,cx
        jb      @msm_l
        mov     ax,cx
        dec     ax
        jns     @msm_l
        xor     ax,ax
@msm_l:
        cmp     [ms_ev],MS_DRAG         ; a drag, a Shift+click select
        je      @msm_m
        test    [ui_kf],3
        jnz     @msm_m
        and     [sel_on],0
        jmp     @msm_g
@msm_m:
        push    ax
        call    sel_mark
        pop     ax
@msm_g:
        push    dx
        call    ed_goto
        pop     dx
        jc      @msm_r
        movzx   ax,dl
        dec     ax
        add     ax,[ed_left]
        mov     ds:[ed_lb+LB_COL],ax
@msm_r:
        ret
@msm_im:
        cmp     [ui_foc],0
        jne     @msm_r
        jmp     ui_win
@msm_sb:
        cmp     [ms_ev],MS_PRESS
        jne     @msm_r
        cmp     dh,ED_R0
        je      ed_up
        cmp     dh,ED_R0+ED_ROWS-1
        je      ed_down
        push    dx                      ; above the thumb or below it
        mov     ax,[ed_line]
        mov     cx,ED_ROWS-3
        mul     cx
        mov     cx,[tx_n]
        jcxz    @msm_z
        div     cx
@msm_z:
        pop     dx
        add     al,ED_R0+1
        cmp     dh,al
        jb      ed_pgup
        jmp     ed_pgdn
@msm_st:
        cmp     [ms_ev],MS_PRESS
        jne     @msm_r
        cmp     [ui_help],0
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
        lodsw
        ja      @msm_k
        jmp     ax
