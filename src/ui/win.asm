; ==========================================================================
; win.asm - the windows: their rows, focus, Split, grow, shrink, maximise
; ==========================================================================
; holds:    wn_init, wn_layout, wn_draw, wn_next, wn_prev, wn_at, wn_split,
;           wn_grow, wn_shrink, wn_max, wn_act
; uses:     dr_edit, dr_imm, ed_commit, ed_load
; data:     The windows stack from row 1 to row 23, W_ED1, W_ED2 (Split),
;           W_IMM, each wh rows with its title, 0 hidden. wvert: ED2 sits
;           beside ED1 instead, in its rows, each half the columns. An edit window
;           has a bottom row, the scroll bar's when it has the focus. The
;           active edit window's state is g_ui.ed and ed_lb's column; the
;           other's waits in its wed, wcol. ui_foc 1: the immediate window
;           has the focus, 2 the debug pane (dpane.asm), else the active edit
;           window. With the pane (a stop) the edit windows have wcols 50
;           columns, the pane the rest, in their rows.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; wn_init - one edit window over the immediate window, as QBasic starts.
; ::::::::::::::
wn_init:
        mov     [g_ui.wn.win+W_ED1*size TWIN].wh,20
        mov     [g_ui.wn.win+W_IMM*size TWIN].wh,3
        mov     [g_ui.wn.wact],W_ED1
        jmp     wn_layout

; ::::::::::::::
; wn_layout - each window's rows from the heights, top down.
;
; keeps:  all
; ::::::::::::::
wn_layout:
        pushad
        mov     al,80
        cmp     [g_ui.dp.dp_on],0
        je      @F
        mov     al,DP_COL
@@:
        mov     [g_ui.wn.wcols],al
        mov     dl,1                    ; DL the next window's title row
        xor     ebx,ebx
@wnl_l:
        call    wn_ptr
        call    wn_beside
        jnc     @F
        mov     al,[g_ui.wn.win+W_ED1*size TWIN].wh
        mov     [edi].TWIN.wh,al
        sub     dl,al                   ; ED1's rows again
@@:
        movzx   ecx,[edi].TWIN.wh
        movzx   eax,dl
        inc     eax                     ; the first row under the title
        cmp     ebx,W_IMM
        je      @wnl_i
        lea     esi,[edi].TWIN.wed      ; the active one's is g_ui.ed
        cmp     bl,[g_ui.wn.wact]
        jne     @F
        mov     esi,offset g_ui.ed
@@:
        push    eax                     ; the columns: all, or a half
        mov     [esi].TEDIT.ed_x,0
        movzx   eax,[g_ui.wn.wcols]
        cmp     [g_ui.wn.wvert],0
        je      @F
        cmp     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        je      @F
        shr     eax,1
        cmp     ebx,W_ED2
        jne     @F
        mov     [esi].TEDIT.ed_x,al
@@:
        sub     eax,2
        mov     [esi].TEDIT.ed_tw,ax
        pop     eax
        mov     [esi].TEDIT.ed_r0,ax
        sub     ecx,2                   ; the title, the bottom row
        jae     @F
        xor     ecx,ecx
@@:
        mov     [esi].TEDIT.ed_rows,cx
        jmp     @wnl_n
@wnl_i:
        mov     [g_ui.im.im_r0],ax
        dec     ecx
        jns     @F
        xor     ecx,ecx
@@:
        mov     [g_ui.im.im_rows],cx
@wnl_n:
        add     dl,[edi].TWIN.wh
        inc     ebx
        cmp     ebx,NWIN
        jb      @wnl_l
        popad
        ret

; wn_beside - CF set: window EBX is ED2 beside ED1, shown. Keeps all.
wn_beside:
        cmp     ebx,W_ED2
        jne     @wbs_n
        cmp     [g_ui.wn.wvert],0
        je      @wbs_n
        cmp     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        je      @wbs_n
        stc
        ret
@wbs_n:
        clc
        ret

; wn_ptr - EDI = window EBX's TWIN. Keeps all but EDI.
wn_ptr:
        imul    edi,ebx,size TWIN
        add     edi,offset g_ui.wn.win
        ret

; ::::::::::::::
; wn_draw - each window shown: the edit windows, the immediate window.
; ::::::::::::::
wn_draw:
        mov     ebx,W_ED1
@wnd_l:
        call    wn_ptr
        cmp     [edi].TWIN.wh,0
        je      @wnd_n
        cmp     bl,[g_ui.wn.wact]
        jne     @wnd_o
        push    ebx
        call    dr_edit
        pop     ebx
        jmp     @wnd_n
@wnd_o:                                 ; the other edit window: its state
        push    ebx                     ; in g_ui.ed while it draws
        call    wn_swap
        call    vw_apply
        mov     [g_ui.lp.dr_ina],1
        call    dr_edit
        mov     [g_ui.lp.dr_ina],0
        pop     ebx
        call    wn_swap
        call    vw_apply
@wnd_n:
        inc     ebx
        cmp     ebx,W_IMM
        jb      @wnd_l
        call    dp_draw
        cmp     [g_ui.wn.win+W_IMM*size TWIN].wh,0
        je      @F
        jmp     dr_imm
@@:
        ret

; wn_swap - g_ui.ed and edit window EBX's wed exchanged. Keeps all.
wn_swap:
        pushad
        call    wn_ptr
        lea     esi,[edi].TWIN.wed
        mov     edi,offset g_ui.ed
        mov     ecx,size TEDIT
@@:
        mov     al,[esi]
        xchg    al,[edi]
        mov     [esi],al
        inc     esi
        inc     edi
        loop    @B
        popad
        ret

; ::::::::::::::
; wn_act - edit window EBX the active one, the focus in it.
; ::::::::::::::
wn_act:
        push    ebx
        call    ed_commit
        pop     ebx
        and     [g_ui.lp.ui_foc],0
        cmp     bl,[g_ui.wn.wact]
        je      @wna_r
        push    ebx
        movzx   ebx,[g_ui.wn.wact]      ; the old one's state to its wed
        call    wn_ptr
        mov     ax,[g_ui.bf.ed_lb.col]
        mov     [edi].TWIN.wcol,ax
        call    wn_swap
        pop     ebx
        mov     [g_ui.wn.wact],bl
        call    wn_swap                 ; the new one's from its wed
        call    vw_apply
        call    wn_ptr
        push    [edi].TWIN.wcol
        mov     eax,[g_ui.ed.ed_line]
        call    ed_load
        pop     ax
        mov     [g_ui.bf.ed_lb.col],ax
@wna_r:
        ret

; ::::::::::::::
; wn_focus - window EBX has the focus: an edit window active, or the
; immediate window.
; ::::::::::::::
wn_focus:
        mov     al,1
        cmp     ebx,W_IMM
        je      @F
        cmp     ebx,W_DBG
        jne     wn_act
        inc     al
@@:
        push    eax
        call    ed_commit
        pop     eax
        mov     [g_ui.lp.ui_foc],al
        ret

; wn_cur - EBX the window with the focus.
wn_cur:
        movzx   ebx,[g_ui.wn.wact]
        cmp     [g_ui.lp.ui_foc],1
        jb      @F
        mov     ebx,W_IMM
        je      @F
        mov     ebx,W_DBG
@@:
        ret

; ::::::::::::::
; wn_next - F6: the focus to the next window shown, by wnord; wn_prev -
; Shift+F6: the one before.
; ::::::::::::::
wn_next:
        mov     edx,1
        jmp     @wnn_g
wn_prev:
        mov     edx,NWORD-1
@wnn_g:
        call    wn_cur
        xor     ecx,ecx                 ; ECX its place in wnord
@@:
        cmp     bl,[g_wnord+ecx]
        je      @wnn_l
        inc     ecx
        jmp     @B
@wnn_l:
        add     ecx,edx
        cmp     ecx,NWORD
        jb      @F
        sub     ecx,NWORD
@@:
        movzx   ebx,[g_wnord+ecx]
        cmp     ebx,W_DBG
        jne     @F
        cmp     [g_ui.dp.dp_on],0
        je      @wnn_l
        jmp     wn_focus
@@:
        call    wn_ptr
        cmp     [edi].TWIN.wh,0
        je      @wnn_l
        jmp     wn_focus
        .data
g_wnord db      W_ED1,W_ED2,W_DBG,W_IMM
NWORD   = $ - g_wnord
        .code

; ::::::::::::::
; wn_at - the window at screen row DH, column DL.
;
; out:    EBX  it
;         CF   set: none
; keeps:  all but EBX
; ::::::::::::::
wn_at:
        push    eax
        push    edi
        mov     ebx,W_DBG               ; the pane: its columns, the edit
        cmp     [g_ui.dp.dp_on],0       ; windows' rows
        je      @F
        cmp     dl,DP_COL
        jb      @F
        test    dh,dh
        jz      @F
        mov     al,23
        sub     al,[g_ui.wn.win+W_IMM*size TWIN].wh
        cmp     dh,al
        jbe     @wat_y
@@:
        mov     al,1                    ; AL past window EBX
        xor     ebx,ebx
@wat_l:
        call    wn_ptr
        call    wn_beside               ; in ED1's rows, counted there
        jc      @wat_n
        add     al,[edi].TWIN.wh
        cmp     dh,al
        jb      @wat_f                  ; CF clear: a hidden one never ends
@wat_n:                                 ; under the row before it
        inc     ebx
        cmp     ebx,NWIN
        jb      @wat_l
        clc                             ; none: CF set after the cmc
        jmp     @wat_r
@wat_f:
        cmp     ebx,W_ED1               ; ED1's rows, ED2's columns
        jne     @F
        push    ebx
        mov     ebx,W_ED2
        call    wn_beside
        pop     ebx
        jnc     @F
        mov     al,[g_ui.wn.wcols]
        shr     al,1
        cmp     dl,al
        jb      @F
        mov     ebx,W_ED2
@@:
@wat_y:
        stc                             ; found: CF clear after the cmc
@wat_r:
        cmc
        pop     edi
        pop     eax
        ret

; ::::::::::::::
; wn_split - View, Split: the edit window in two, the same text in both;
; again, one.
; ::::::::::::::
wn_split:
        call    wn_unmax
        call    ed_commit
        cmp     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        jne     @wsp_o
        movzx   eax,[g_ui.wn.win+W_ED1*size TWIN].wh
        cmp     eax,6                   ; ED2 hidden: ED1 is the active one
        jb      @wsp_r
        shr     eax,1
        mov     [g_ui.wn.win+W_ED2*size TWIN].wh,al
        sub     [g_ui.wn.win+W_ED1*size TWIN].wh,al
@wsp_c:
        mov     esi,offset g_ui.ed      ; ED2 starts as ED1
        mov     edi,offset g_ui.wn.win+W_ED2*size TWIN+TWIN.wed
        mov     ecx,size TEDIT
        rep     movsb
        mov     ax,[g_ui.bf.ed_lb.col]
        mov     [g_ui.wn.win+W_ED2*size TWIN].wcol,ax
        jmp     wn_layout
@wsp_o:                                 ; one again, with the active one's
        mov     [g_ui.wn.wact],W_ED1    ; state, which g_ui.ed holds
        mov     al,[g_ui.wn.win+W_ED2*size TWIN].wh
        cmp     [g_ui.wn.wvert],0
        jne     @F
        add     [g_ui.wn.win+W_ED1*size TWIN].wh,al
@@:
        mov     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        mov     [g_ui.wn.wvert],0
        jmp     wn_layout
@wsp_r:
        ret

; ::::::::::::::
; wn_vsplit - View, Split Vertically: the edit window in two side by
; side, the same text in both; again, one.
; ::::::::::::::
wn_vsplit:
        call    wn_unmax
        call    ed_commit
        cmp     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        jne     @wsp_o
        mov     [g_ui.wn.wvert],1
        mov     al,[g_ui.wn.win+W_ED1*size TWIN].wh
        mov     [g_ui.wn.win+W_ED2*size TWIN].wh,al
        jmp     @wsp_c

; ::::::::::::::
; wn_grow - Alt+Plus: the window with the focus a row taller, from the
; one under it, else over it; wn_shrink - Alt+Minus: a row shorter, to
; the same.
; ::::::::::::::
wn_grow:
        call    wn_unmax
        call    wn_curh
        mov     ecx,ebx                 ; ECX gains, a neighbour that can
        mov     edx,1                   ; spare a row loses: under it first
        call    wn_spare
        jnc     wn_move
        mov     edx,-1
        call    wn_spare
        jnc     wn_move
        ret
wn_shrink:
        call    wn_unmax
        call    wn_curh
        call    wn_ptr
        call    wn_min
        cmp     [edi].TWIN.wh,al
        jbe     @wgr_r
        mov     ecx,ebx                 ; EBX loses, the neighbour under it,
        mov     edx,1                   ; else over it, gains
        call    wn_shown
        jnc     @F
        mov     edx,-1
        call    wn_shown
        jc      @wgr_r
@@:
        mov     ecx,eax
        jmp     wn_move
@wgr_r:
        ret

; wn_curh - EBX the window with the focus, ED1 for ED2 beside it or the
; pane: whose rows change.
wn_curh:
        call    wn_cur
        cmp     ebx,W_DBG
        je      @F
        call    wn_beside
        jnc     @wch_r
@@:
        mov     ebx,W_ED1
@wch_r:
        ret

; wn_move - a row from window EBX to window ECX.
wn_move:
        call    wn_ptr
        dec     [edi].TWIN.wh
        mov     ebx,ecx
        call    wn_ptr
        inc     [edi].TWIN.wh
        jmp     wn_layout

; wn_shown - EAX = the window shown next to window ECX, EDX 1 under, -1
; over. CF set: none. Keeps all but EAX.
wn_shown:
        push    ebx
        push    edi
        mov     ebx,ecx
@wsh_l:
        add     ebx,edx
        cmp     ebx,NWIN
        jae     @wsh_n                  ; past either end
        call    wn_ptr
        cmp     [edi].TWIN.wh,0
        je      @wsh_l
        call    wn_beside
        jc      @wsh_l
        mov     eax,ebx
        clc
        jmp     @wsh_r
@wsh_n:
        stc
@wsh_r:
        pop     edi
        pop     ebx
        ret

; wn_spare - EBX = wn_shown's window when it has a row to spare. CF set:
; none.
wn_spare:
        call    wn_shown
        jc      @wsp_x
        mov     ebx,eax
        call    wn_ptr
        call    wn_min
        cmp     [edi].TWIN.wh,al
        ja      @wsp_x                  ; CF clear
        stc
@wsp_x:
        ret

; wn_min - AL = window EBX's least height: its title and a row, an edit
; window's bottom row too.
wn_min:
        mov     al,3
        cmp     ebx,W_IMM
        jne     @F
        mov     al,2
@@:
        ret

; ::::::::::::::
; wn_max - Ctrl+F10: the window with the focus the whole screen; again,
; the windows as they were.
; ::::::::::::::
wn_max:
        cmp     [g_ui.wn.wmax],0
        jne     wn_unmax
        mov     al,[g_ui.wn.wvert]
        mov     [g_ui.wn.wvsav],al
        mov     [g_ui.wn.wvert],0
        xor     ebx,ebx                 ; the heights kept, all hidden
@wmx_l:
        call    wn_ptr
        mov     al,[edi].TWIN.wh
        mov     [g_ui.wn.wsav+ebx],al
        mov     [edi].TWIN.wh,0
        inc     ebx
        cmp     ebx,NWIN
        jb      @wmx_l
        call    wn_curh
        call    wn_ptr
        mov     [edi].TWIN.wh,23
        mov     [g_ui.wn.wmax],1
        jmp     wn_layout

; wn_unmax - the heights before Ctrl+F10, if maximised.
wn_unmax:
        cmp     [g_ui.wn.wmax],0
        je      @wum_r
        xor     ebx,ebx
@wum_l:
        call    wn_ptr
        mov     al,[g_ui.wn.wsav+ebx]
        mov     [edi].TWIN.wh,al
        inc     ebx
        cmp     ebx,NWIN
        jb      @wum_l
        mov     [g_ui.wn.wmax],0
        mov     al,[g_ui.wn.wvsav]
        mov     [g_ui.wn.wvert],al
        call    wn_layout
@wum_r:
        ret

        end
