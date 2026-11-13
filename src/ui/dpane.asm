; ==========================================================================
; dpane.asm - the debug pane: at a stop, right of the code, its windows
; Watch, Locals and Call stack
; ==========================================================================
; holds:    dp_open, dp_close, dp_draw, dp_key, dp_mouse, dp_hover, dp_tip
; uses:     cstk's frames, sy_find, vw_of, vw_goline, dr_ttl, sc_*
; data:     g_ui.dp. The frames (TDFRM): the stop's, then each procedure's
;           caller, from cstk's M_PC frames, each with its text line and its
;           row: the procedure's name, or module, and its line in its view.
;           Locals: the selected frame's procedure's variables (vscp), its
;           parameters first, as the frame sees them: a newer call of the
;           same procedure saved them on the save stack (F_LSP0 the
;           parameters' cells, F_LSP1 the locals' values, in g_plh's order).
;           Watch: expressions, each PRINTed by dbg_eval in the stop's
;           procedure's scope at each stop; kept across runs.
;           The tooltip: the Locals row of the variable under the cursor, or
;           the mouse (tt_ms), in the selected frame's view; a module
;           variable's the procedure sees (DIM SHARED, SHARED) too.
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
        call    dp_vars
        call    dp_weval
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
        mov     ebx,[g_state.rn.contip]
        mov     edx,[g_state.rn.cstk]
@dpf_l:
        call    dp_pc
        call    dp_frm1
        inc     ebp
        test    edx,edx
        jz      @dpf_e
        cmp     ebp,NDFRM
        jae     @dpf_e
        mov     ebx,[edx].TFRAME.ln
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
        mov     [edi].TDFRM.df_vw,0FFh
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
        mov     ax,gs:[ebx].TLINE.num
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
        mov     [edi].TDFRM.df_vw,bl
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
        movzx   eax,ax
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

; dp_dec - EAX in decimal at EDI, a sign before it when negative. Keeps
; all but EDI.
dp_dec:
        pushad
        test    eax,eax
        jns     @F
        neg     eax
        mov     byte ptr [edi],'-'
        inc     edi
@@:
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
        movzx   eax,[g_ui.dp.dp_nw]
        cmp     ebp,1
        jb      @F
        mov     al,[g_ui.dp.dp_nv]
        je      @F
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
        imul    esi,eax,DP_TW
        add     esi,offset g_ui.dp.dp_wv
        test    ebp,ebp
        jz      @F
        add     esi,offset g_ui.dp.dp_vt-offset g_ui.dp.dp_wv
        cmp     ebp,DP_STK
        jne     @F
        imul    esi,eax,size TDFRM
        add     esi,offset g_ui.dp.dp_fr+TDFRM.df_txt
@@:
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
        test    ebp,ebp
        jnz     @F
        cmp     ah,52h
        je      dp_wadd
        cmp     ah,53h
        je      dp_wdel
@@:
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
        call    dp_vars
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

; ::::::::::::::
; dp_wadd - Ins in Watch: an expression asked, added, evaluated.
; ::::::::::::::
dp_wadd:
        cmp     [g_ui.dp.dp_nw],NWATCH
        jae     @dwa_r
        mov     ebx,offset dg_dpw
        mov     byte ptr [ebx].TDLG.foc,0
        and     [g_ui.dp.dp_fld.len],0
        and     [g_ui.dp.dp_fld.col],0
        call    dl_run
        cmp     al,1
        jne     @dwa_r
        movzx   ecx,[g_ui.dp.dp_fld.len]
        jecxz   @dwa_r
        cmp     ecx,WXLEN-1
        jbe     @F
        mov     ecx,WXLEN-1
@@:
        movzx   eax,[g_ui.dp.dp_nw]
        mov     [g_ui.dp.dp_sel],al
        imul    edi,eax,WXLEN
        add     edi,offset g_ui.dp.dp_wx
        mov     esi,offset g_ui.dp.dp_fld.txt
        rep     movsb
        mov     byte ptr [edi],0
        inc     [g_ui.dp.dp_nw]
        call    dp_weval
        xor     ebp,ebp
        jmp     dp_vis
@dwa_r:
        ret

; ::::::::::::::
; dp_wdel - Del in Watch: the selected watch dropped.
; ::::::::::::::
dp_wdel:
        movzx   eax,[g_ui.dp.dp_sel]
        movzx   ecx,[g_ui.dp.dp_nw]
        cmp     eax,ecx
        jae     @dwd_r
        dec     ecx
        mov     [g_ui.dp.dp_nw],cl
        sub     ecx,eax                 ; those after it, down one
        push    ecx
        imul    edi,eax,WXLEN
        add     edi,offset g_ui.dp.dp_wx
        lea     esi,[edi+WXLEN]
        imul    ecx,ecx,WXLEN
        rep     movsb
        pop     ecx
        imul    edi,eax,DP_TW
        add     edi,offset g_ui.dp.dp_wv
        lea     esi,[edi+DP_TW]
        imul    ecx,ecx,DP_TW
        rep     movsb
        cmp     al,[g_ui.dp.dp_nw]
        jb      @dwd_r
        dec     al
        js      @dwd_r
        mov     [g_ui.dp.dp_sel],al
@dwd_r:
        ret

; ::::::::::::::
; dp_weval - each watch's row: its expression, " = ", what PRINT shows of
; it in the stop's procedure, or its error's message.
;
; keeps:  all
; ::::::::::::::
dp_weval:
        pushad
        mov     edx,[g_ui.dp.dp_fr].TDFRM.df_fp
        xor     eax,eax
        test    edx,edx
        jz      @F
        mov     al,[edx+F_PRC]
@@:
        mov     [g_dpp],eax
        xor     ebx,ebx
@dwe_l:
        cmp     bl,[g_ui.dp.dp_nw]
        jae     @dwe_e
        imul    esi,ebx,WXLEN
        add     esi,offset g_ui.dp.dp_wx
        mov     edi,offset g_dprow
        push    esi
        call    dp_cpy
        mov     esi,offset u_dpeq
        call    dp_cpy
        pop     esi
        push    ebx
        call    dp_wval
        pop     ebx
        mov     byte ptr [edi],0
        imul    edi,ebx,DP_TW
        add     edi,offset g_ui.dp.dp_wv
        call    dp_rowcut
        inc     ebx
        jmp     @dwe_l
@dwe_e:
        popad
        ret

; ::::::::::::::
; dp_wval - expression ESI's value as text at EDI: dbg_eval's output, its
; spaces and end of line left out; an error's message. A ':' (another
; statement) or a direct line's frame on cstk (its code would go): none.
;
; out:    EDI  past it
; ::::::::::::::
dp_wval:
        xor     ecx,ecx
@@:
        mov     al,[esi+ecx]
        inc     ecx
        test    al,al
        jz      @F
        cmp     al,':'
        jne     @B
        mov     al,E_SN+1
        jmp     @dwv_x
@@:
        mov     ebx,[g_state.rn.cstk]   ; a direct line's frame: none
@@:
        cmp     ebx,CSTOP
        jae     @F
        cmp     [ebx].TFRAME.ln,DLINE
        je      @dwv_n
        movzx   ecx,byte ptr [ebx+F_MARK]
        add     ebx,ecx
        jmp     @B
@@:
        push    edi
        mov     eax,[g_dpp]
        mov     edi,offset g_dpout
        call    dbg_eval
        pop     edi
        test    al,al
        jnz     @dwv_x
        mov     esi,offset g_dpout
@@:
        lodsb
        cmp     al,' '
        je      @B
        dec     esi
@dwv_c:
        lodsb
        cmp     al,' '
        jb      @dwv_t
        stosb
        jmp     @dwv_c
@dwv_t:
        cmp     byte ptr [edi-1],' '
        jne     @dwv_r
        dec     edi
        jmp     @dwv_t
@dwv_x:
        dec     al
        call    er_text
        jmp     dp_cpy
@dwv_n:
        mov     esi,offset u_dpna
        jmp     dp_cpy
@dwv_r:
        ret

; dp_rowcut - the row at g_dprow to EDI, cut to DP_TW-1 characters,
; 0-ended. Keeps all but ESI, EDI, ECX.
dp_rowcut:
        mov     esi,offset g_dprow
        mov     ecx,DP_TW-1
@@:
        lodsb
        test    al,al
        jz      @F
        stosb
        loop    @B
@@:
        mov     byte ptr [edi],0
        ret

; ::::::::::::::
; dp_vars - Locals: the selected frame's procedure's variables, the
; module's at the module; its parameters first, then by id. A DEF FN's are
; left out.
;
; keeps:  all
; ::::::::::::::
dp_vars:
        pushad
        movzx   eax,[g_ui.dp.dp_frm]
        imul    eax,eax,size TDFRM
        mov     edx,[g_ui.dp.dp_fr+eax].TDFRM.df_fp
        xor     eax,eax
        test    edx,edx
        jz      @F
        mov     al,[edx+F_PRC]
@@:
        mov     [g_dpp],eax
        call    dp_newer
        mov     edi,offset g_ui.dp.dp_vt
        xor     ebp,ebp
        test    eax,eax                 ; the parameters
        jz      @dpv_o
        shl     eax,5
        movzx   ecx,word ptr [g_prc+eax+P_NPAR]
        movzx   esi,word ptr [g_prc+eax+P_PAR]
        xor     edx,edx
@dpv_p:
        cmp     edx,ecx
        jae     @dpv_o
        movzx   ebx,word ptr [g_ppar+esi*2]
        push    ecx
        mov     ecx,edx
        call    dp_var1
        pop     ecx
        inc     esi
        inc     edx
        jmp     @dpv_p
@dpv_o:
        mov     ebx,1                   ; the others
@dpv_l:
        cmp     bx,[g_state.lx.nsyms]
        jae     @dpv_e
        mov     al,[g_vscp+ebx]
        cmp     al,byte ptr [g_dpp]
        jne     @dpv_n
        mov     al,[g_sinfo+ebx]
        and     al,83h
        cmp     al,3
        je      @dpv_n
        call    dp_ispar
        jnc     @dpv_n
        or      ecx,-1
        call    dp_var1
@dpv_n:
        inc     ebx
        jmp     @dpv_l
@dpv_e:
        mov     eax,ebp
        mov     [g_ui.dp.dp_nv],al
        popad
        ret

; dp_ispar - CF clear: EBX is a parameter of procedure dpp. Keeps all.
dp_ispar:
        pushad
        mov     eax,[g_dpp]
        test    eax,eax
        jz      @dip_n
        shl     eax,5
        movzx   ecx,word ptr [g_prc+eax+P_NPAR]
        jecxz   @dip_n
        movzx   esi,word ptr [g_prc+eax+P_PAR]
@@:
        cmp     bx,[g_ppar+esi*2]
        je      @dip_y
        inc     esi
        loop    @B
@dip_n:
        popad
        stc
        ret
@dip_y:
        popad
        clc
        ret

; ::::::::::::::
; dp_newer - dpnw: the oldest call of procedure dpp newer than the frame at
; EDX, which saved what that frame sees; 0 none, or the module.
;
; keeps:  all
; ::::::::::::::
dp_newer:
        pushad
        xor     esi,esi
        test    edx,edx
        jz      @dpn_r
        mov     ebx,[g_state.rn.cstk]
        mov     eax,[g_dpp]
@dpn_l:
        cmp     ebx,edx
        jae     @dpn_r
        movzx   ecx,byte ptr [ebx+F_MARK]
        cmp     cl,M_PC
        jne     @F
        cmp     al,[ebx+F_PRC]
        jne     @F
        mov     esi,ebx
@@:
        add     ebx,ecx
        jmp     @dpn_l
@dpn_r:
        mov     [g_dpnw],esi
        popad
        ret

; ::::::::::::::
; dp_var1 - Locals' row of variable EBX, parameter ECX of its procedure or
; -1: its name, " = ", its value as the frame sees it.
;
; in:     EDI  the row; EBP the rows so far
; out:    EDI  the next row; EBP one more
; ::::::::::::::
dp_var1:
        cmp     ebp,NDVAR
        jae     @dv1_r
        mov     [g_ui.dp.dp_vid+ebp*2],bx
        call    dp_vtext
        push    ecx
        push    esi
        push    edi
        call    dp_rowcut
        pop     edi
        pop     esi
        pop     ecx
        add     edi,DP_TW
        inc     ebp
@dv1_r:
        ret

; ::::::::::::::
; dp_vtext - variable EBX's row at dprow, 0-ended: its name, " = ", its
; value as the frame sees it; ECX its parameter's place or -1.
;
; keeps:  all
; ::::::::::::::
dp_vtext:
        pushad
        mov     edi,offset g_dprow
        movzx   eax,word ptr [g_vnam+ebx*2]
        mov     esi,[g_nname+eax*4]
        movzx   eax,byte ptr [esi]
        inc     esi
        push    ecx
        mov     ecx,eax
        rep     movsb
        pop     ecx
        movzx   eax,[g_vtyp+ebx]
        cmp     eax,ET_SD
        ja      @F
        mov     al,[g_dpsfx+eax]
        test    al,al
        jz      @F
        stosb
@@:
        mov     esi,offset u_dpeq
        call    dp_cpy
        mov     esi,offset u_dparr
        test    [g_sinfo+ebx],80h
        jnz     @dv1_t
        mov     esi,offset u_dprec
        cmp     [g_vtyp+ebx],ET_REC
        jae     @dv1_t
        call    dp_cell
        call    dp_val
        jmp     @dv1_c
@dv1_t:
        call    dp_cpy
@dv1_c:
        mov     byte ptr [edi],0
        popad
        ret

; ::::::::::::::
; dp_cell - variable EBX's cell as the frame sees it: vt's, else a newer
; call's saved one.
;
; in:     ECX  its parameter's place, -1 none
; out:    EDX  the cell
; keeps:  all but EDX
; ::::::::::::::
dp_cell:
        mov     edx,[g_vt+ebx*4]
        push    esi
        mov     esi,[g_dpnw]
        test    esi,esi
        jz      @dpc_x
        push    eax
        push    ecx
        test    ecx,ecx
        js      @F
        shl     ecx,4                   ; LSSZ
        add     ecx,[esi+F_LSP0]
        mov     edx,[ecx+12]            ; the cell before the newer call
@@:
        call    var_cell
        cmp     edx,ecx
        jne     @dpc_p
        mov     eax,[g_dpp]             ; its own: the value saved
        movzx   eax,word ptr [g_plh+eax*2]
        xor     ecx,ecx
@dpc_l:
        test    eax,eax
        jz      @dpc_p
        cmp     eax,ebx
        je      @F
        movzx   eax,word ptr [g_vnloc+eax*2]
        inc     ecx
        jmp     @dpc_l
@@:
        shl     ecx,4
        mov     edx,[esi+F_LSP1]
        lea     edx,[edx+ecx+4]
@dpc_p:
        pop     ecx
        pop     eax
@dpc_x:
        pop     esi
        ret

; ::::::::::::::
; dp_val - the value of variable EBX's kind in cell EDX, as text at EDI: a
; number as PRINT's without its sign's space, a string in quotes.
;
; out:    EDI  past it
; ::::::::::::::
dp_val:
        mov     al,[g_sinfo+ebx]
        test    al,SK_DBL
        jnz     @dvl_d
        and     al,3
        cmp     al,2
        je      @dvl_s
        mov     al,[g_vtyp+ebx]
        cmp     al,ET_R4
        je      @dvl_f
        cmp     al,ET_IMP
        je      @dvl_f
        mov     eax,[edx]
        jmp     dp_dec
@dvl_f:
        push    edi
        mov     eax,edx
        call    flt_fmt
        jmp     @dvl_n
@dvl_d:
        push    edi
        mov     eax,edx
        call    dbl_fmt
@dvl_n:
        pop     edi
        mov     esi,offset g_dbuf
        cmp     byte ptr [esi],' '
        jne     @F
        inc     esi
        dec     ecx
@@:
        rep     movsb
        ret
@dvl_s:
        mov     al,'"'
        stosb
        movzx   ecx,word ptr [edx+D_LEN]
        cmp     ecx,DP_TW
        jbe     @F
        mov     ecx,DP_TW
@@:
        mov     esi,[edx+D_PTR]
        rep     movsb
        stosb
        ret

; ::::::::::::::
; dp_hover - the mouse moved to DX: the tooltip by it.
; ::::::::::::::
dp_hover:
        mov     [g_ui.dp.tt_ms],1
        mov     [g_ui.dp.tt_pos],dx
        ret

; ::::::::::::::
; dp_tip - stopped, the program as it ran: the Locals row of the variable
; under the mouse or the cursor, under it (over it on the last row).
; ::::::::::::::
dp_tip:
        cmp     [g_ui.dp.dp_on],0
        je      @dpt_r
        call    ui_stopped
        jnz     @dpt_r
        pushad
        movzx   eax,[g_ui.dp.dp_frm]    ; in the frame's view
        imul    eax,eax,size TDFRM
        mov     al,[g_ui.dp.dp_fr+eax].TDFRM.df_vw
        cmp     al,[g_ui.ed.ed_vw]
        jne     @dpt_x
        cmp     [g_ui.dp.tt_ms],0
        jne     @dpt_m
        cmp     [g_ui.lp.ui_foc],0      ; the cursor's
        jne     @dpt_x
        movzx   eax,[g_ui.bf.ed_lb.col]
        mov     esi,offset g_ui.bf.ed_lb.txt
        movzx   ecx,[g_ui.bf.ed_lb.len]
        movzx   edx,[g_ui.ed.ed_line]
        jmp     @dpt_w
@dpt_m:
        mov     dx,[g_ui.dp.tt_pos]     ; the mouse's, in the active window
        call    wn_at
        jc      @dpt_x
        cmp     bl,[g_ui.wn.wact]
        jne     @dpt_x
        movzx   eax,dh
        sub     ax,[g_ui.ed.ed_r0]
        jb      @dpt_x
        cmp     ax,[g_ui.ed.ed_rows]
        jae     @dpt_x
        add     ax,[g_ui.ed.ed_top]
        cmp     ax,[g_ui.tx.tx_n]
        jae     @dpt_x
        movzx   ecx,dl
        sub     cl,[g_ui.ed.ed_x]
        jbe     @dpt_x
        dec     ecx
        cmp     cx,[g_ui.ed.ed_tw]
        jae     @dpt_x
        add     cx,[g_ui.ed.ed_left]
        movzx   edx,ax
        push    ecx
        mov     edi,offset g_ui.bf.uc_line
        call    tx_get
        mov     esi,edi
        pop     eax
@dpt_w:                                 ; ESI the line, ECX its length, EAX
        push    edx                     ; the column, EDX the line
        call    dp_word
        jc      @dpt_p
        call    dp_find
        jc      @dpt_p
        pop     edx
        sub     dx,[g_ui.ed.ed_top]     ; its row: under the name, else over
        add     dx,[g_ui.ed.ed_r0]
        inc     edx
        mov     ax,[g_ui.ed.ed_r0]
        add     ax,[g_ui.ed.ed_rows]
        cmp     dx,ax
        jb      @F
        sub     edx,2
@@:
        mov     dh,dl
        xor     ecx,ecx                 ; ECX the row's length
@@:
        cmp     byte ptr [esi+ecx],0
        je      @F
        inc     ecx
        jmp     @B
@@:
        movzx   eax,[g_ui.ed.ed_x]      ; its column: the name's, in the
        inc     eax                     ; window
        sub     bx,[g_ui.ed.ed_left]
        jae     @F
        xor     ebx,ebx
@@:
        add     eax,ebx
        movzx   ebx,[g_ui.ed.ed_x]
        inc     ebx
        add     bx,[g_ui.ed.ed_tw]      ; past the window
        lea     edi,[eax+ecx+2]
        cmp     edi,ebx
        jbe     @F
        mov     eax,ebx
        sub     eax,ecx
        sub     eax,2
@@:
        mov     dl,al
        mov     ax,A_BAR*256+' '
        push    ecx
        mov     ecx,1
        call    sc_fill
        pop     ecx
        call    sc_strn
        mov     ecx,1
        call    sc_fill
        jmp     @dpt_x
@dpt_p:
        pop     edx
@dpt_x:
        popad
@dpt_r:
        ret

; ::::::::::::::
; dp_word - the name at column EAX of the line ESI, ECX long: not in a
; string or after a ' .
;
; out:    EBX  its first column
;         ECX  its length, its suffix left out
;         DL   its suffix, 0 none
;         CF   set: none
; keeps:  ESI
; ::::::::::::::
dp_word:
        cmp     eax,ecx
        jae     @dpw_n
        mov     edi,ecx                 ; EDI the line's length
        mov     dl,[esi+eax]
        call    dp_issfx
        jc      @F
        test    eax,eax                 ; on a suffix: the name before it
        jz      @dpw_n
        dec     eax
@@:
        mov     dl,[esi+eax]
        call    dp_isnm
        jc      @dpw_n
        mov     ebx,eax
@@:
        test    ebx,ebx                 ; back to its start, a letter
        jz      @F
        mov     dl,[esi+ebx-1]
        call    dp_isnm
        jc      @F
        dec     ebx
        jmp     @B
@@:
        mov     dl,[esi+ebx]
        or      dl,20h
        cmp     dl,'a'
        jb      @dpw_n
        cmp     dl,'z'
        ja      @dpw_n
@@:
        inc     eax                     ; on to its end
        cmp     eax,edi
        jae     @F
        mov     dl,[esi+eax]
        call    dp_isnm
        jnc     @B
@@:
        mov     ecx,eax
        sub     ecx,ebx
        xor     dl,dl
        cmp     eax,edi
        jae     @F
        mov     dl,[esi+eax]
        call    dp_issfx
        jnc     @F
        xor     dl,dl
@@:
        xor     eax,eax                 ; not in a string, a comment: EDI
        xor     edi,edi                 ; 1 in a string
@dpw_q:
        cmp     eax,ebx
        jae     @dpw_y
        mov     dh,[esi+eax]
        inc     eax
        cmp     dh,'"'
        jne     @F
        xor     edi,1
        jmp     @dpw_q
@@:
        cmp     dh,"'"
        jne     @dpw_q
        test    edi,edi
        jz      @dpw_n
        jmp     @dpw_q
@dpw_y:
        clc
        ret
@dpw_n:
        stc
        ret

; dp_isnm - CF clear: DL a name's character. Keeps all.
dp_isnm:
        push    edx
        cmp     dl,'.'
        je      @dpi_y
        cmp     dl,'0'
        jb      @dpi_x
        cmp     dl,'9'
        jbe     @dpi_y
        or      dl,20h
        cmp     dl,'a'
        jb      @dpi_x
        cmp     dl,'z'
        jbe     @dpi_y
@dpi_x:
        pop     edx
        stc
        ret
@dpi_y:
        pop     edx
        clc
        ret

; dp_issfx - CF clear: DL a type's suffix. Keeps all.
dp_issfx:
        push    edi
        push    ecx
        push    eax
        mov     al,dl
        mov     edi,offset g_dpsfn
        mov     ecx,5
        repne   scasb
        pop     eax
        pop     ecx
        pop     edi
        je      @F
        stc
        ret
@@:
        clc
        ret

; ::::::::::::::
; dp_find - the row of the name at ESI+EBX, ECX long, suffix DL: Locals'
; variable of it, else a module one the frame's procedure sees, SHARED.
;
; out:    ESI  the row, 0-ended
;         CF   set: none
; keeps:  EBX
; ::::::::::::::
dp_find:
        push    ebx
        add     esi,ebx
        xor     eax,eax
@dfd_l:
        cmp     al,[g_ui.dp.dp_nv]
        jae     @dfd_m
        movzx   ebx,[g_ui.dp.dp_vid+eax*2]
        call    dp_match
        je      @F
        inc     eax
        jmp     @dfd_l
@@:
        imul    esi,eax,DP_TW
        add     esi,offset g_ui.dp.dp_vt
        jmp     @dfd_y
@dfd_m:
        cmp     [g_dpp],0               ; the module's: all in Locals
        je      @dfd_n
        mov     ebx,1
@dfd_v:
        cmp     bx,[g_state.lx.nsyms]
        jae     @dfd_n
        cmp     [g_vscp+ebx],0
        jne     @dfd_x
        mov     al,[g_sinfo+ebx]
        and     al,83h
        cmp     al,3
        je      @dfd_x
        call    dp_match
        jne     @dfd_x
        call    dp_seen
        jc      @dfd_x
        or      ecx,-1
        call    dp_vtext
        mov     edi,offset g_dptip
        call    dp_rowcut
        mov     esi,offset g_dptip
@dfd_y:
        pop     ebx
        clc
        ret
@dfd_x:
        inc     ebx
        jmp     @dfd_v
@dfd_n:
        pop     ebx
        stc
        ret

; ::::::::::::::
; dp_match - variable EBX is the name at ESI, ECX long, suffix DL: its
; letters the name's, the suffix, if any, its type's.
;
; out:    ZF  set: so
; keeps:  all
; ::::::::::::::
dp_match:
        pushad
        test    dl,dl
        jz      @F
        mov     al,dl
        mov     edi,offset g_dpsfn
        push    ecx
        mov     ecx,5
        repne   scasb
        pop     ecx
        mov     al,[edi-1+g_dpsft-g_dpsfn]
        cmp     al,[g_vtyp+ebx]
        jne     @dmt_r
@@:
        movzx   eax,word ptr [g_vnam+ebx*2]
        mov     edi,[g_nname+eax*4]
        cmp     cl,[edi]
        jne     @dmt_r
@@:
        inc     edi
        lodsb
        cmp     al,'a'
        jb      @dpm_u
        cmp     al,'z'
        ja      @dpm_u
        sub     al,20h
@dpm_u:
        cmp     al,[edi]
        jne     @dmt_r
        loop    @B
@dmt_r:
        popad
        ret

; dp_seen - CF clear: module variable EBX is seen in procedure dpp: DIM
; SHARED, or SHARED there. Keeps all.
dp_seen:
        pushad
        movzx   ebx,word ptr [g_vnam+ebx*2]
        xor     eax,eax
        call    scp_get
        jc      @F
        test    byte ptr [edi+B_FLG],BF_SHR
        jnz     @dps_y
@@:
        mov     eax,[g_dpp]
        shl     eax,5
        movzx   eax,word ptr [g_prc+eax+P_NAM]
        call    scp_get
        jc      @dps_n
        test    byte ptr [edi+B_FLG],BF_REF
        jnz     @dps_y
@dps_n:
        popad
        stc
        ret
@dps_y:
        popad
        clc
        ret

        .data
g_dpsfn db      '%&!#$'                 ; a suffix, its type
g_dpsft db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD
g_dpsfx db      0,'%','&',0,'#','$'     ; by ET_*: the suffix shown
u_dpeq  db      ' = ',0
u_dparr db      '(array)',0
u_dprec db      '(record)',0
g_dpttl dd      u_dpwat,u_dploc,u_dpstk
u_dpwat db      'Watch',0
u_dploc db      'Locals',0
u_dpstk db      'Call stack',0
u_dpmod db      'module',0
u_dpimm db      'Immediate',0
u_dpline db     'line ',0
u_dpna  db      '(not here)',0
u_dpadd db      'Add Watch',0
u_dpexp db      'Expression:',0
dg_dpw  TDLG    <8,14,8,52,u_dpadd,3,0,5,1>
        TCTL    <CT_FLD,1,14,34,1,0,g_ui.dp.dp_fld,0,u_dpexp,2,2>
        TCTL    <CT_BTN,6,12,6,0,0,u_ok,0,0,0,0>
        TCTL    <CT_BTN,6,30,10,0,0,u_cancel,0,0,0,0>
        .data?
g_dpout db      80 dup(?)               ; dbg_eval's output
g_dptip db      DP_TW dup(?)            ; the tooltip of a module variable
g_dpp   dd      ?                       ; Locals' procedure, 0 the module
g_dpnw  dd      ?                       ; its newer call, dp_newer's
g_dprow db      80 dup(?)               ; a row being made
        .code

        end
