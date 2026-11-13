; ==========================================================================
; dpane.asm - the debug pane: at a stop, right of the code, its windows
; Watch, Locals and Call stack
; ==========================================================================
; holds:    dp_open, dp_close, dp_draw, dp_key, dp_mouse
; uses:     cstk's frames, sy_find, vw_of, vw_goline, dr_ttl, sc_*
; data:     g_ui.dp. The frames (TDFRM): the stop's, then each procedure's
;           caller, from cstk's M_PC frames, each with its text line and its
;           row: the procedure's name, or module, and its line in its view.
;           The windows share the edit windows' rows (dp_geo).
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; dp_open - a stop: the pane shown, its frames, its first window's focus.
; ::::::::::::::
dp_open:
        pushad
        mov     [g_ui.dp.dp_on],1
        xor     eax,eax
        mov     [g_ui.dp.dp_foc],al
        mov     [g_ui.dp.dp_frm],al
        mov     edi,offset g_ui.dp.dp_sel
        mov     ecx,2*NDP
        rep     stosb
        call    dp_frames
        call    wn_layout
        popad
        ret

; ::::::::::::::
; dp_close - no pane; its focus to the edit window.
;
; keeps:  all
; ::::::::::::::
dp_close:
        cmp     [g_ui.dp.dp_on],0
        je      @dpc_r
        mov     [g_ui.dp.dp_on],0
        cmp     [g_ui.lp.ui_foc],2
        jne     @F
        mov     [g_ui.lp.ui_foc],0
@@:
        call    wn_layout
@dpc_r:
        ret

; ::::::::::::::
; dp_frames - the frames: the stop's line, then each M_PC frame's caller.
; ::::::::::::::
dp_frames:
        mov     edi,offset g_ui.dp.dp_fr
        xor     ebp,ebp
        movzx   ebx,[g_state.rn.contip]
        mov     edx,[g_state.rn.cstk]
@dpf_l:
        call    dp_pc
        call    dp_frm1
        inc     ebp
        test    edx,edx
        jz      @dpf_e
        cmp     ebp,NDFRM
        jae     @dpf_e
        movzx   ebx,word ptr [edx+F_LN]
        add     edx,M_PC
        jmp     @dpf_l
@dpf_e:
        mov     eax,ebp
        mov     [g_ui.dp.dp_nf],al
        ret

; dp_pc - EDX the first M_PC frame from EDX on cstk, 0 none. Keeps all but
; EDX.
dp_pc:
        push    ecx
@dpp_l:
        cmp     edx,CSTOP
        jae     @dpp_n
        movzx   ecx,byte ptr [edx+F_MARK]
        cmp     cl,M_PC
        je      @dpp_r
        add     edx,ecx
        jmp     @dpp_l
@dpp_n:
        xor     edx,edx
@dpp_r:
        pop     ecx
        ret

; ::::::::::::::
; dp_frm1 - the frame at EDI: program line EBX, its procedure's frame EDX.
;
; out:    EDI  past it
; keeps:  EBX, EDX, EBP
; ::::::::::::::
dp_frm1:
        push    ebx
        push    edx
        mov     [edi].TDFRM.df_fp,edx
        mov     [edi].TDFRM.df_ln,0FFFFh
        lea     edx,[edi].TDFRM.df_txt
        push    edi
        mov     edi,edx
        mov     ecx,DP_TW
        mov     al,' '
        rep     stosb
        mov     byte ptr [edi],0
        pop     edi
        mov     esi,offset u_dpimm
        cmp     ebx,DLINE
        je      @dfr_n
        mov     ax,word ptr [g_progb+ebx+L_NUM]
        push    ebp
        push    edx
        push    edi
        call    sy_find
        pop     edi
        pop     edx
        pop     ebp
        mov     esi,offset u_dpimm
        jc      @dfr_n
        mov     [edi].TDFRM.df_ln,ax
        push    edi
        call    vw_of
        sub     ax,[edi].TVIEW.vfirst
        inc     eax
        lea     esi,[edi].TVIEW.vname
        pop     edi
        test    ebx,ebx
        jnz     @F
        mov     esi,offset u_dpmod
@@:
        push    edi
        push    eax
        mov     edi,edx                 ; the name, 15 characters at most
        mov     ecx,15
@dfr_c:
        lodsb
        test    al,al
        jz      @F
        stosb
        loop    @dfr_c
@@:
        lea     edi,[edx+16]
        mov     esi,offset u_dpline
        call    dp_cpy
        pop     eax
        call    dp_dec
        pop     edi
        jmp     @dfr_r
@dfr_n:
        push    edi
        mov     edi,edx
        call    dp_cpy
        pop     edi
@dfr_r:
        add     edi,size TDFRM
        pop     edx
        pop     ebx
        ret

; dp_cpy - the 0-ended text at ESI to EDI, its 0 left out. Keeps all but
; ESI, EDI.
dp_cpy:
        push    eax
@@:
        lodsb
        test    al,al
        jz      @F
        stosb
        jmp     @B
@@:
        pop     eax
        ret

; dp_dec - AX in decimal at EDI. Keeps all but EDI.
dp_dec:
        pushad
        movzx   eax,ax
        xor     ecx,ecx
        mov     ebx,10
@@:
        xor     edx,edx
        div     ebx
        push    edx
        inc     ecx
        test    eax,eax
        jnz     @B
@@:
        pop     eax
        add     al,'0'
        stosb
        loop    @B
        mov     [esp],edi
        popad
        ret

; ::::::::::::::
; dp_geo - each window's title row and rows: the edit windows' rows,
; three tenths to Watch and to Call stack, the rest to Locals.
;
; keeps:  all
; ::::::::::::::
dp_geo:
        pushad
        mov     al,23
        sub     al,[g_ui.wn.win+W_IMM*size TWIN].wh
        mov     cl,al
        mov     ah,3
        mul     ah
        mov     bl,10
        div     bl                      ; AL a tenth's three
        mov     [g_ui.dp.dp_h],al
        mov     [g_ui.dp.dp_h+DP_STK],al
        sub     cl,al
        sub     cl,al
        mov     [g_ui.dp.dp_h+1],cl
        mov     [g_ui.dp.dp_row],1
        inc     al
        mov     [g_ui.dp.dp_row+1],al
        add     al,cl
        mov     [g_ui.dp.dp_row+DP_STK],al
        popad
        ret

; ::::::::::::::
; dp_cnt - window EBP's rows of items.
;
; out:    EAX  their count
; keeps:  all but EAX
; ::::::::::::::
dp_cnt:
        xor     eax,eax
        cmp     ebp,DP_STK
        jne     @F
        mov     al,[g_ui.dp.dp_nf]
@@:
        ret

; ::::::::::::::
; dp_item - item EAX of window EBP: its text.
;
; out:    ESI  it, 0-ended
;         CF   set: none
; keeps:  all but ESI
; ::::::::::::::
dp_item:
        push    eax
        push    eax
        call    dp_cnt
        cmp     [esp],eax
        pop     eax
        jae     @dpi_n
        imul    esi,eax,size TDFRM
        add     esi,offset g_ui.dp.dp_fr+TDFRM.df_txt
        pop     eax
        clc
        ret
@dpi_n:
        pop     eax
        stc
        ret

; ::::::::::::::
; dp_draw - the pane, when shown: each window's title, its rows; the one
; with the focus lit, its row selected; the frame selected marked.
; ::::::::::::::
dp_draw:
        cmp     [g_ui.dp.dp_on],0
        je      @dpd_r
        pushad
        call    dp_geo
        mov     [g_ttlx],DP_COL
        mov     [g_ttlw],DP_W
        xor     ebp,ebp
@dpd_w:
        call    dp_hasfoc
        mov     bl,al
        xor     bh,bh
        mov     dh,[g_ui.dp.dp_row+ebp]
        mov     esi,[g_dpttl+ebp*4]
        mov     cx,0B4C3h
        test    ebp,ebp
        jnz     @F
        mov     cx,0BFDAh
@@:
        push    ebp
        call    dr_ttl
        pop     ebp
        xor     ebx,ebx                 ; EBX the row
@dpd_r1:
        movzx   eax,[g_ui.dp.dp_h+ebp]
        dec     eax
        cmp     ebx,eax
        jae     @dpd_n
        call    dp_line
        inc     ebx
        jmp     @dpd_r1
@dpd_n:
        inc     ebp
        cmp     ebp,NDP
        jb      @dpd_w
        popad
@dpd_r:
        ret

; dp_hasfoc - AL 1: window EBP has the focus. Keeps all but EAX.
dp_hasfoc:
        xor     eax,eax
        cmp     [g_ui.lp.ui_foc],2
        jne     @F
        push    ecx
        movzx   ecx,[g_ui.dp.dp_foc]
        cmp     ecx,ebp
        pop     ecx
        jne     @F
        inc     eax
@@:
        ret

; ::::::::::::::
; dp_line - row EBX of window EBP: its borders, its item.
;
; keeps:  EBX, EBP
; ::::::::::::::
dp_line:
        mov     dh,[g_ui.dp.dp_row+ebp]
        inc     dh
        add     dh,bl
        mov     dl,DP_COL
        mov     ax,A_ED*256+0B3h
        mov     ecx,1
        call    sc_fill
        movzx   ecx,[g_ui.dp.dp_top+ebp]
        add     ecx,ebx                 ; ECX the item
        call    dp_hasfoc
        mov     ah,A_ED                 ; AH the row's colour: lit, selected
        test    al,al                   ; in the window with the focus
        jz      @F
        cmp     cl,[g_ui.dp.dp_sel+ebp]
        jne     @F
        mov     ah,A_SEL
@@:
        mov     al,' '                  ; the selected frame's mark
        cmp     ebp,DP_STK
        jne     @F
        cmp     cl,[g_ui.dp.dp_frm]
        jne     @F
        cmp     cl,[g_ui.dp.dp_nf]
        jae     @F
        mov     al,10h
@@:
        push    ecx
        push    eax
        mov     ecx,1
        call    sc_fill
        mov     al,' '
        mov     ecx,DP_TW-1
        push    edx
        call    sc_fill
        pop     edx
        pop     eax
        pop     ecx
        push    eax
        mov     eax,ecx
        call    dp_item
        pop     eax
        jc      @dpl_e
        push    esi
        xor     ecx,ecx
@@:
        cmp     byte ptr [esi+ecx],0
        je      @F
        inc     ecx
        cmp     ecx,DP_TW-1
        jb      @B
@@:
        pop     esi
        call    sc_strn
@dpl_e:
        mov     dl,DP_COL+DP_W-1
        mov     ax,A_ED*256+0B3h
        mov     ecx,1
        jmp     sc_fill

; ::::::::::::::
; dp_key - a key in the pane: Tab, Shift+Tab its windows; Up, Down a row;
; Enter a frame; Esc back to the edit window.
; ::::::::::::::
dp_key:
        movzx   ebp,[g_ui.dp.dp_foc]
        cmp     al,9
        je      @dpk_t
        cmp     ax,0F00h
        je      @dpk_b
        cmp     al,1Bh
        je      @dpk_x
        cmp     al,0Dh
        je      @dpk_e
        test    al,al
        jnz     @dpk_r
        movzx   ecx,[g_ui.dp.dp_sel+ebp]
        cmp     ah,48h
        je      @dpk_u
        cmp     ah,50h
        jne     @dpk_r
        inc     ecx
        call    dp_cnt
        cmp     ecx,eax
        jae     @dpk_r
        jmp     @dpk_s
@dpk_u:
        dec     ecx
        js      @dpk_r
@dpk_s:
        mov     [g_ui.dp.dp_sel+ebp],cl
        jmp     dp_vis
@dpk_t:
        inc     ebp
        cmp     ebp,NDP
        jb      @dpk_f
        xor     ebp,ebp
        jmp     @dpk_f
@dpk_b:
        dec     ebp
        jns     @dpk_f
        mov     ebp,NDP-1
@dpk_f:
        mov     eax,ebp
        mov     [g_ui.dp.dp_foc],al
@dpk_r:
        ret
@dpk_x:
        movzx   ebx,[g_ui.wn.wact]
        jmp     wn_focus
@dpk_e:
        cmp     ebp,DP_STK
        jne     @dpk_r
        movzx   eax,[g_ui.dp.dp_sel+ebp]
        cmp     al,[g_ui.dp.dp_nf]
        jae     @dpk_r
        jmp     dp_go

; dp_vis - window EBP's first row shown: its selected row in view.
dp_vis:
        call    dp_geo
        mov     al,[g_ui.dp.dp_sel+ebp]
        cmp     al,[g_ui.dp.dp_top+ebp]
        jb      @dpv_s
        mov     cl,[g_ui.dp.dp_h+ebp]
        dec     cl                      ; the rows
        jz      @dpv_r
        mov     ah,al
        sub     ah,[g_ui.dp.dp_top+ebp]
        cmp     ah,cl
        jb      @dpv_r
        sub     al,cl
        inc     al
@dpv_s:
        mov     [g_ui.dp.dp_top+ebp],al
@dpv_r:
        ret

; ::::::::::::::
; dp_go - frame EAX selected: the cursor at the start of its line.
; ::::::::::::::
dp_go:
        mov     [g_ui.dp.dp_frm],al
        imul    ebx,eax,size TDFRM
        mov     ax,[g_ui.dp.dp_fr+ebx].TDFRM.df_ln
        cmp     ax,0FFFFh
        je      @dpg_r
        push    eax
        call    ed_commit
        mov     eax,[esp]
        call    vw_goline
        pop     eax
        sub     ax,[g_ui.tx.tx_v0]
        and     [g_ui.ed.sel_on],0
        call    ed_load
        and     [g_ui.bf.ed_lb.col],0
        jmp     ed_ensure
@dpg_r:
        ret

; ::::::::::::::
; dp_mouse - a press in the pane: its window's focus, a row selected; a
; frame's row selects the frame.
;
; in:     DX  the cell: DH the row, DL the column
; ::::::::::::::
dp_mouse:
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @dpm_r
        push    edx
        mov     ebx,W_DBG
        call    wn_focus
        pop     edx
        call    dp_geo
        mov     ebp,NDP-1
@@:
        cmp     dh,[g_ui.dp.dp_row+ebp]
        jae     @F
        dec     ebp
        jns     @B
        ret
@@:
        mov     eax,ebp
        mov     [g_ui.dp.dp_foc],al
        movzx   eax,dh
        sub     al,[g_ui.dp.dp_row+ebp]
        jz      @dpm_r
        dec     eax
        add     al,[g_ui.dp.dp_top+ebp]
        mov     ecx,eax
        call    dp_cnt
        cmp     ecx,eax
        jae     @dpm_r
        mov     [g_ui.dp.dp_sel+ebp],cl
        cmp     ebp,DP_STK
        jne     @dpm_r
        mov     eax,ecx
        jmp     dp_go
@dpm_r:
        ret

        .data
g_dpttl dd      u_dpwat,u_dploc,u_dpstk
u_dpwat db      'Watch',0
u_dploc db      'Locals',0
u_dpstk db      'Call stack',0
u_dpmod db      'module',0
u_dpimm db      'Immediate',0
u_dpline db     'line ',0
        .code

        end
