; ==========================================================================
; win.asm - the windows: their rows, focus, Split, grow, shrink, maximise
; ==========================================================================
; holds:    wn_init, wn_layout, wn_draw, wn_next, wn_prev, wn_at, wn_split,
;           wn_grow, wn_shrink, wn_max, wn_act
; uses:     dr_edit, dr_imm, ed_commit, ed_load
; data:     The windows stack from row 1 to row 23, W_ED1, W_ED2 (Split),
;           W_IMM, each wh rows with its title, 0 hidden. An edit window
;           has a bottom row, the scroll bar's when it has the focus. The
;           active edit window's state is g_ui.ed and ed_lb's column; the
;           other's waits in its wed, wcol. ui_foc 1: the immediate window
;           has the focus, else the active edit window.
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
        mov     dl,1                    ; DL the next window's title row
        xor     ebx,ebx
@wnl_l:
        call    wn_ptr
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
        mov     ax,[g_ui.ed.ed_line]
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
        cmp     ebx,W_IMM
        jne     wn_act
        call    ed_commit
        mov     [g_ui.lp.ui_foc],1
        ret

; wn_cur - EBX the window with the focus.
wn_cur:
        mov     ebx,W_IMM
        cmp     [g_ui.lp.ui_foc],0
        jne     @F
        movzx   ebx,[g_ui.wn.wact]
@@:
        ret

; ::::::::::::::
; wn_next - F6: the focus to the next window shown; wn_prev - Shift+F6:
; the one before.
; ::::::::::::::
wn_next:
        mov     edx,1
        jmp     @wnn_g
wn_prev:
        mov     edx,-1
@wnn_g:
        call    wn_cur
@wnn_l:
        add     ebx,edx
        cmp     ebx,W_ED1
        jge     @F
        mov     ebx,W_IMM
@@:
        cmp     ebx,W_IMM
        jle     @F
        mov     ebx,W_ED1
@@:
        call    wn_ptr
        cmp     [edi].TWIN.wh,0
        je      @wnn_l
        jmp     wn_focus

; ::::::::::::::
; wn_at - the window over screen row DH.
;
; out:    EBX  it
;         CF   set: none
; keeps:  all but EBX
; ::::::::::::::
wn_at:
        push    eax
        push    edi
        mov     al,1                    ; AL past window EBX
        xor     ebx,ebx
@wat_l:
        call    wn_ptr
        add     al,[edi].TWIN.wh
        cmp     dh,al
        jb      @wat_r                  ; CF clear: a hidden one never ends
        inc     ebx                     ; under the row before it
        cmp     ebx,NWIN
        jb      @wat_l
        clc                             ; none: CF set after the cmc
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
        add     [g_ui.wn.win+W_ED1*size TWIN].wh,al
        mov     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        jmp     wn_layout
@wsp_r:
        ret

; ::::::::::::::
; wn_grow - Alt+Plus: the window with the focus a row taller, from the
; one under it, else over it; wn_shrink - Alt+Minus: a row shorter, to
; the same.
; ::::::::::::::
wn_grow:
        call    wn_unmax
        call    wn_cur
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
        call    wn_cur
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
        xor     ebx,ebx                 ; the heights kept, all hidden
@wmx_l:
        call    wn_ptr
        mov     al,[edi].TWIN.wh
        mov     [g_ui.wn.wsav+ebx],al
        mov     [edi].TWIN.wh,0
        inc     ebx
        cmp     ebx,NWIN
        jb      @wmx_l
        call    wn_cur
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
        call    wn_layout
@wum_r:
        ret

        end
