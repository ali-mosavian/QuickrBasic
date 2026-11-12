; ==========================================================================
; dlg.asm - dialog boxes: the engine, message boxes, Open, Save As, Find
; ==========================================================================
; holds:    dl_run, dl_msg, dl_ok, dl_ync, dl_open, dl_saveas, dl_find,
;           dl_change, dl_files
; uses:     sc_*, le_*, dr_all, ui_getkey; INT 21h for the directory
; data:     A dialog (D_*): row, column, rows, columns, title, its controls'
;           count, the one with the focus, the separator's row, the default
;           button; then its controls (C_*, CSZ bytes): type, row, column
;           (from the dialog's corner), width, rows, flags, data, callback,
;           label, the label's row and column. A list (L_*): count,
;           selected, first shown, the items, an item's size.
; calls:
;   dl_run             draw, a key to the control with the focus, until a
;                      button
;     dl_draw          the frame, the controls
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; dl_ctl - control AL of the dialog at BX.
;
; out:    DI  it
; keeps:  all but DI
; ::::::::::::::
dl_ctl:
        push    eax
        movzx   edi,al
        imul    edi,edi,size TCTL
        add     edi,ebx
        add     edi,size TDLG
        pop     eax
        ret

; ::::::::::::::
; dl_pos - the screen position of row AH, column AL in the dialog at BX.
;
; out:    DX  it
; keeps:  all but DX
; ::::::::::::::
dl_pos:
        mov     dh,[ebx+TDLG.row]
        add     dh,ah
        mov     dl,[ebx+TDLG.col]
        add     dl,al
        ret

; ::::::::::::::
; dl_draw - the dialog at BX: frame, shadow, separator, controls, cursor.
; ::::::::::::::
dl_draw:
        mov     dh,[ebx+TDLG.row]
        mov     dl,[ebx+TDLG.col]
        mov     ch,[ebx+TDLG.rows]
        mov     cl,[ebx+TDLG.cols]
        mov     ah,A_DLG
        mov     esi,[ebx+TDLG.ttl]
        call    sc_box
        call    sc_shadow
        mov     al,[ebx+TDLG.sep]
        test    al,al
        jz      @dld_c
        add     dh,al
        mov     ah,A_DLG
        call    sc_sep
@dld_c:
        mov     [g_ui.dg.dl_cur],2000h
        xor     al,al
@dld_l:
        cmp     al,[ebx+TDLG.n]
        jae     @dld_e
        push    eax
        call    dl_ctl
        mov     [g_ui.dg.dl_cp],edi
        mov     cl,0
        cmp     al,[ebx+TDLG.foc]
        jne     @dld_f
        mov     cl,1
@dld_f:
        mov     [g_ui.dg.dl_hf],cl
        call    dl_one
        pop     eax
        inc     al
        jmp     @dld_l
@dld_e:
        mov     dx,[g_ui.dg.dl_cur]
        cmp     dx,2000h
        je      @dld_h
        mov     ecx,0607h
        call    sc_shape
        jmp     sc_cur
@dld_h:
        mov     ecx,2000h
        jmp     sc_shape

; ::::::::::::::
; dl_one - control dl_cp of the dialog at BX; dl_hf 1 when it has the
; focus, then dl_cur the cursor's place.
; ::::::::::::::
dl_one:
        mov     edi,[g_ui.dg.dl_cp]
        mov     esi,[edi+TCTL.lbl]
        test    esi,esi
        jz      @dlo_t
        mov     ah,[edi+TCTL.lrow]
        mov     al,[edi+TCTL.lcol]
        call    dl_pos
        push    ebx
        mov     ah,A_DLG
        call    dl_hk
        call    sc_str
        pop     ebx
        mov     edi,[g_ui.dg.dl_cp]
@dlo_t:
        mov     ah,[edi+TCTL.row]
        mov     al,[edi+TCTL.col]
        call    dl_pos
        mov     al,[edi+TCTL.kind]
        cmp     al,CT_FLD
        je      dl_dfld
        cmp     al,CT_LST
        je      dl_dlst
        cmp     al,CT_BTN
        je      dl_dbtn
        cmp     al,CT_CHK
        je      dl_dchk
        mov     esi,[edi+TCTL.data]
        push    ebx
        mov     ah,A_DLG
        mov     bh,ah
        call    sc_str
        pop     ebx
        ret

; ::::::::::::::
; dl_dfld - a text field at DX: its frame, the part of its text around the
; cursor.
; ::::::::::::::
dl_dfld:
        push    ebx
        mov     ch,3
        mov     cl,[edi+TCTL.cols]
        add     cl,2
        mov     ah,A_DLG
        xor     esi,esi
        call    sc_box
        inc     dh
        inc     dl
        mov     ebx,[edi+TCTL.data]
        movzx   ecx,byte ptr [edi+TCTL.cols]
        mov     ax,[ebx+TLBUF.col]  ; the first column shown
        sub     ax,cx
        jae     @dlf_s
        mov     ax,-1
@dlf_s:
        inc     ax
        mov     [g_ui.dg.dl_fs],ax
        movzx   esi,ax
        lea     esi,[ebx+esi+TLBUF.txt]
        movzx   ecx,word ptr [ebx+TLBUF.len]
        sub     cx,ax
        jae     @dlf_c
        xor     ecx,ecx
@dlf_c:
        movzx   ax,byte ptr [edi+TCTL.cols]
        cmp     cx,ax
        jbe     @dlf_w
        movzx   ecx,ax
@dlf_w:
        mov     ah,A_DLG
        push    edx
        push    ecx
        call    sc_strn
        pop     ecx
        pop     edx
        cmp     [g_ui.dg.dl_hf],0
        je      @dlf_r
        cmp     [g_ui.dg.dl_new],0               ; the text selected till a key
        je      @dlf_k
        mov     ah,07h
        call    sc_attr
@dlf_k:
        mov     ax,[ebx+TLBUF.col]
        sub     ax,[g_ui.dg.dl_fs]
        add     dl,al
        mov     [g_ui.dg.dl_cur],dx
@dlf_r:
        pop     ebx
        ret

; ::::::::::::::
; dl_dlst - a list at DX: its frame, the items from its first shown, the
; selected one lit when it has the focus.
; ::::::::::::::
dl_dlst:
        push    ebx
        mov     ch,[edi+TCTL.rows]
        add     ch,2
        mov     cl,[edi+TCTL.cols]
        add     cl,2
        mov     ah,A_DLG
        xor     esi,esi
        call    sc_box
        inc     dh
        inc     dl
        mov     ebx,[edi+TCTL.data]
        movzx   ecx,word ptr [ebx+TLIST.sel]  ; the first shown keeps it in view
        cmp     cx,[ebx+TLIST.top]
        jae     @dll_a
        mov     [ebx+TLIST.top],cx
@dll_a:
        movzx   ax,byte ptr [edi+TCTL.rows]
        add     ax,[ebx+TLIST.top]
        cmp     cx,ax
        jb      @dll_b
        sub     cx,ax
        inc     cx
        add     [ebx+TLIST.top],cx
@dll_b:
        xor     ecx,ecx
@dll_r:
        mov     edi,[g_ui.dg.dl_cp]
        cmp     cl,[edi+TCTL.rows]
        jae     @dll_e
        mov     ax,[ebx+TLIST.top]
        add     ax,cx
        cmp     ax,[ebx+TLIST.n]
        jae     @dll_n
        push    ecx
        push    edx
        movzx   esi,byte ptr [ebx+TLIST.isz]
        movzx   eax,ax
        imul    esi,eax
        add     esi,[ebx+TLIST.items]
        push    eax
        push    ebx
        mov     ah,A_DLG
        mov     bh,ah
        call    sc_str
        pop     ebx
        pop     eax
        pop     edx
        pop     ecx
        cmp     ax,[ebx+TLIST.sel]
        jne     @dll_n
        cmp     [g_ui.dg.dl_hf],0
        je      @dll_n
        push    ecx
        mov     edi,[g_ui.dg.dl_cp]
        movzx   ecx,byte ptr [edi+TCTL.cols]
        mov     ah,A_DSEL
        call    sc_attr
        pop     ecx
        mov     [g_ui.dg.dl_cur],dx
@dll_n:
        inc     dh
        inc     cx
        jmp     @dll_r
@dll_e:
        pop     ebx
        ret

; ::::::::::::::
; dl_dbtn - a button at DX: < text >, its brackets bright with the focus.
; ::::::::::::::
dl_dbtn:
        push    ebx
        mov     edi,[g_ui.dg.dl_cp]
        mov     esi,[edi+TCTL.data]
        call    sc_slen
        mov     al,[edi+TCTL.cols]  ; its width: the text +4 if none
        test    al,al
        jnz     @dlb_w
        mov     al,cl
        add     al,4
@dlb_w:
        sub     al,2                    ; the text centred (button.c)
        sub     al,cl
        mov     ah,al
        inc     al
        shr     al,1
        sub     ah,al
        mov     word ptr [g_ui.dg.dl_pad],ax
        mov     ah,A_DLG
        cmp     [g_ui.dg.dl_hf],0
        je      @dlb_s
        mov     ah,A_DFOC
@dlb_s:
        mov     al,'<'
        mov     ecx,1
        call    sc_fill
        push    eax
        mov     ax,A_DLG*256+' '
        movzx   ecx,byte ptr [g_ui.dg.dl_pad]
        call    sc_fill
        cmp     [g_ui.dg.dl_hf],0
        je      @dlb_t
        mov     [g_ui.dg.dl_cur],dx
@dlb_t:
        call    dl_hk
        call    sc_str
        movzx   ecx,byte ptr [g_ui.dg.dl_pad+1]
        call    sc_fill
        pop     eax
        mov     al,'>'
        mov     ecx,1
        call    sc_fill
        pop     ebx
        ret

; ::::::::::::::
; dl_hk - BH the hotkeys' colour: lit once Alt was down in the dialog, as
; COW's; else AH.
; ::::::::::::::
dl_hk:
        mov     bh,ah
        cmp     [g_ui.dg.dl_alt],0
        je      @dhk_r
        mov     bh,A_DFOC
@dhk_r:
        ret

; ::::::::::::::
; dl_acc - the hotkey of the string at SI, upper case: the letter after '&'.
;
; out:    AL  it, 0 none
; ::::::::::::::
dl_acc:
        test    esi,esi
        jz      @dac_n
@dac_l:
        lodsb
        test    al,al
        jz      @dac_r
        cmp     al,'&'
        jne     @dac_l
        lodsb
        jmp     ui_upper
@dac_n:
        xor     al,al
@dac_r:
        ret

; ::::::::::::::
; dl_dchk - a check box at DX: [X] text.
; ::::::::::::::
dl_dchk:
        push    ebx
        mov     ax,A_DLG*256+'['
        mov     ecx,1
        call    sc_fill
        mov     edi,[g_ui.dg.dl_cp]
        mov     esi,[edi+TCTL.cb]
        mov     al,' '
        cmp     byte ptr [esi],0
        je      @dlk_s
        mov     al,'X'
@dlk_s:
        cmp     [g_ui.dg.dl_hf],0
        je      @dlk_t
        mov     [g_ui.dg.dl_cur],dx
@dlk_t:
        call    sc_fill
        mov     al,']'
        call    sc_fill
        mov     al,' '
        call    sc_fill
        mov     edi,[g_ui.dg.dl_cp]
        mov     esi,[edi+TCTL.data]
        call    dl_hk
        call    sc_str
        pop     ebx
        ret

; ::::::::::::::
; dl_hit - is the mouse (ms_pos) on control DI of the dialog at BX?
;
; out:    CF  clear: it is; dl_hr the row in it
; keeps:  all
; ::::::::::::::
dl_hit:
        pushad
        mov     dx,[g_ui.ms.ms_pos]
        mov     al,dh
        sub     al,[ebx+TDLG.row]
        sub     al,[edi+TCTL.row]
        jb      @dlh_n
        mov     ah,dl
        sub     ah,[ebx+TDLG.col]
        sub     ah,[edi+TCTL.col]
        jb      @dlh_n
        mov     cl,[edi+TCTL.kind]  ; CH its rows, CL its columns
        cmp     cl,CT_FLD
        je      @dlh_f
        cmp     cl,CT_LST
        je      @dlh_l
        cmp     cl,CT_TXT
        je      @dlh_n
        push    ecx
        mov     esi,[edi+TCTL.data]
        call    sc_slen
        mov     dx,cx
        pop     ecx
        mov     cl,dl
        add     cl,4
        mov     ch,1
        jmp     @dlh_c
@dlh_f:
        mov     ch,3
        jmp     @dlh_w
@dlh_l:
        mov     ch,[edi+TCTL.rows]
        add     ch,2
@dlh_w:
        mov     cl,[edi+TCTL.cols]
        add     cl,2
@dlh_c:
        cmp     al,ch
        jae     @dlh_n
        cmp     ah,cl
        jae     @dlh_n
        mov     [g_ui.dg.dl_hr],al
        popad
        clc
        ret
@dlh_n:
        popad
        stc
        ret

; ::::::::::::::
; dl_step - the focus to the next control that takes it, AL 1, or the one
; before, AL -1.
; ::::::::::::::
dl_step:
        mov     ah,al
        mov     al,[ebx+TDLG.foc]
@dls_l:
        add     al,ah
        jns     @dls_a
        mov     al,[ebx+TDLG.n]
        dec     al
@dls_a:
        cmp     al,[ebx+TDLG.n]
        jb      @dls_b
        xor     al,al
@dls_b:
        call    dl_ctl
        test    byte ptr [edi+TCTL.flg],CF_NOF
        jnz     @dls_l
        mov     [ebx+TDLG.foc],al
        mov     [g_ui.dg.dl_new],1
        ret

; ::::::::::::::
; dl_run - the dialog at BX until a button: the editor under it.
;
; out:    AL  the button's control, FFh Esc
; ::::::::::::::
dl_run:
        mov     [g_ui.lp.ui_help],offset u_dlg
        call    dl_run0
        mov     [g_ui.lp.ui_help],0
        ret

; ::::::::::::::
; dl_run0 - dl_run, the dialog's status line set.
; ::::::::::::::
dl_run0:
        mov     [g_ui.dg.dl_bx],ebx
        mov     ah,2                    ; Alt down already: its letters lit
        int     16h
        and     al,8
        mov     [g_ui.dg.dl_alt],al
        mov     [g_ui.dg.dl_new],1
@dlr_l:
        mov     ebx,[g_ui.dg.dl_bx]
        push    ebx
        call    dr_all
        pop     ebx
        call    dl_draw
        call    ui_getkey
        mov     ebx,[g_ui.dg.dl_bx]
        movzx   ecx,ax
        mov     al,[ebx+TDLG.foc]
        call    dl_ctl
        mov     ax,cx
        cmp     ax,MS_KEY
        je      @dlr_ms
        test    [g_ui.lp.ui_kf],8                ; Alt: the letters lit; Alt+letter
        jz      @dlr_na
        mov     [g_ui.dg.dl_alt],1
        test    al,al
        jnz     @dlr_na
        sub     ah,10h
        jb      @dlr_na
        cmp     ah,22h
        ja      @dlr_na
        movzx   esi,ah
        mov     al,[mn_altl+esi]
        mov     ah,1                    ; AH 1: unmatched beeps
        jmp     @dlr_hot
@dlr_na:
        cmp     ax,0FFFFh               ; an Alt tap
        jne     @dlr_nt
        mov     [g_ui.dg.dl_alt],1
        jmp     @dlr_l
@dlr_nt:
        cmp     al,1Bh
        je      @dlr_x
        cmp     al,09h
        je      @dlr_tab
        cmp     ax,0F00h
        je      @dlr_bt
        cmp     al,0Dh
        je      @dlr_ent
        mov     cl,[edi+TCTL.kind]
        cmp     cl,CT_FLD
        je      @dlr_fld
        cmp     cl,CT_LST
        je      @dlr_lst
        cmp     al,' '
        mov     ah,0
        jne     @dlr_hot
        cmp     cl,CT_CHK
        je      @dlr_chk
        cmp     cl,CT_BTN
        je      @dlr_btn
        jmp     @dlr_l
@dlr_x:
        mov     al,0FFh
        ret
@dlr_ms:                                ; a click: the control under it
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @dlr_l
        xor     al,al
@dlr_mc:
        cmp     al,[ebx+TDLG.n]
        jae     @dlr_l
        call    dl_ctl
        call    dl_hit
        jnc     @dlr_mh
        inc     al
        jmp     @dlr_mc
@dlr_mh:
        test    byte ptr [edi+TCTL.flg],CF_NOF
        jnz     @dlr_l
        mov     cl,[edi+TCTL.kind]
        cmp     cl,CT_BTN
        je      @dlr_btn
        mov     [ebx+TDLG.foc],al
        mov     [g_ui.dg.dl_new],0
        cmp     cl,CT_CHK
        je      @dlr_chk
        cmp     cl,CT_LST
        jne     @dlr_l
        movzx   dx,[g_ui.dg.dl_hr]               ; a list's line: once selects, on the
        dec     dx                      ; selected one opens
        js      @dlr_l
        cmp     dl,[edi+TCTL.rows]
        jae     @dlr_l
        mov     esi,[edi+TCTL.data]
        add     dx,[esi+TLIST.top]
        cmp     dx,[esi+TLIST.n]
        jae     @dlr_l
        cmp     dx,[esi+TLIST.sel]
        jne     @dlr_set
        jmp     @dlr_ent
@dlr_hot:                               ; a hotkey, from the next control on
        call    ui_upper                ; (COW dlgcore.c): a button pressed,
        mov     dl,al                   ; a box checked, else the focus
        mov     dh,ah
        mov     cl,[ebx+TDLG.n]
        mov     al,[ebx+TDLG.foc]
@dlr_hn:
        inc     al
        cmp     al,[ebx+TDLG.n]
        jb      @dlr_h1
        xor     al,al
@dlr_h1:
        call    dl_ctl
        push    eax
        mov     esi,[edi+TCTL.data]
        mov     ah,[edi+TCTL.kind]
        cmp     ah,CT_BTN
        je      @dlr_h2
        cmp     ah,CT_CHK
        je      @dlr_h2
        mov     esi,[edi+TCTL.lbl]
        cmp     ah,CT_TXT
        jne     @dlr_h2
        xor     esi,esi
@dlr_h2:
        call    dl_acc
        cmp     al,dl
        pop     eax
        je      @dlr_hf
        dec     cl
        jnz     @dlr_hn
        test    dh,dh
        jz      @dlr_l
        mov     ax,0E07h
        int     10h
        jmp     @dlr_l
@dlr_hf:
        mov     cl,[edi+TCTL.kind]
        cmp     cl,CT_BTN
        je      @dlr_btn
        mov     [ebx+TDLG.foc],al
        mov     [g_ui.dg.dl_new],1
        cmp     cl,CT_CHK
        je      @dlr_chk
        jmp     @dlr_l
@dlr_tab:
        mov     al,1
        call    dl_step
        jmp     @dlr_l
@dlr_bt:
        mov     al,-1
        call    dl_step
        jmp     @dlr_l
@dlr_chk:
        mov     esi,[edi+TCTL.cb]
        xor     byte ptr [esi],1
        jmp     @dlr_l
@dlr_ent:
        cmp     byte ptr [edi+TCTL.kind],CT_BTN
        je      @dlr_btn
        cmp     byte ptr [edi+TCTL.kind],CT_LST
        jne     @dlr_def
        mov     ecx,[edi+TCTL.cb]
        jecxz   @dlr_def
        mov     al,2
        call    ecx
        jnc     @dlr_l
@dlr_def:
        mov     ebx,[g_ui.dg.dl_bx]
        mov     al,[ebx+TDLG.def]
        call    dl_ctl
@dlr_btn:
        mov     eax,edi
        sub     eax,ebx
        sub     eax,size TDLG
        mov     cl,size TCTL
        div     cl
        mov     [g_ui.dg.dl_res],al
        mov     ecx,[edi+TCTL.cb]
        jecxz   @dlr_ok
        call    ecx
        jc      @dlr_l
@dlr_ok:
        mov     al,[g_ui.dg.dl_res]
        ret
@dlr_fld:
        push    ebx
        mov     ebx,[edi+TCTL.data]
        cmp     [g_ui.dg.dl_new],0               ; a character replaces selected text
        je      @dlr_f1
        mov     [g_ui.dg.dl_new],0
        cmp     al,' '
        jb      @dlr_f1
        and     word ptr [ebx+TLBUF.len],0
        and     word ptr [ebx+TLBUF.col],0
@dlr_f1:
        call    ui_ledit
        pop     ebx
        jmp     @dlr_l
@dlr_lst:
        mov     esi,[edi+TCTL.data]
        mov     dx,[esi+TLIST.sel]
        cmp     ah,48h
        je      @dlr_up
        cmp     ah,50h
        je      @dlr_dn
        cmp     ah,49h
        je      @dlr_pu
        cmp     ah,51h
        je      @dlr_pd
        cmp     ah,47h
        je      @dlr_hm
        cmp     ah,4Fh
        je      @dlr_en
        jmp     @dlr_l
@dlr_up:
        test    dx,dx
        jz      @dlr_l
        dec     dx
        jmp     @dlr_set
@dlr_dn:
        inc     dx
        jmp     @dlr_set
@dlr_pu:
        movzx   ax,byte ptr [edi+TCTL.rows]
        sub     dx,ax
        jae     @dlr_set
@dlr_hm:
        xor     dx,dx
        jmp     @dlr_set
@dlr_pd:
        movzx   ax,byte ptr [edi+TCTL.rows]
        add     dx,ax
        jmp     @dlr_set
@dlr_en:
        mov     dx,0FFFFh
@dlr_set:
        mov     ax,[esi+TLIST.n]
        test    ax,ax
        jz      @dlr_l
        cmp     dx,ax
        jb      @dlr_s2
        mov     dx,ax
        dec     dx
@dlr_s2:
        mov     [esi+TLIST.sel],dx
        mov     ecx,[edi+TCTL.cb]
        jecxz   @dlr_l2
        mov     al,1
        call    ecx
@dlr_l2:
        jmp     @dlr_l

; ::::::::::::::
; ui_ledit - an editing key AX in the one-line buffer BX (fields, the
; immediate window).
;
; in:     DX  the most characters, for a character
; ::::::::::::::
ui_ledit:
        cmp     al,08h
        je      le_bs
        test    al,al
        jz      @ule_x
        cmp     al,0E0h
        je      @ule_x
        cmp     al,20h
        jb      @ule_r
        mov     dx,FLDMAX
        jmp     le_ins
@ule_x:
        cmp     ah,4Bh
        je      le_left
        cmp     ah,4Dh
        je      le_right
        cmp     ah,47h
        je      le_home
        cmp     ah,4Fh
        je      le_end
        cmp     ah,53h
        je      le_del
        cmp     ah,73h
        je      le_wleft
        cmp     ah,74h
        je      le_wright
@ule_r:
        ret

; ::::::::::::::
; dl_msg - a message box: the lines at SI split at '|', the buttons named
; by the 0-ended list of strings at BX.
;
; out:    AL  the button, 0 the first; FFh Esc
; ::::::::::::::
dl_msg:
        mov     edi,offset g_ui.bf.msg_txt  ; the lines, 0 each
        xor     dx,dx                   ; DL the lines, DH the widest
        xor     ecx,ecx
@dlm_c:
        lodsb
        cmp     al,'|'
        jne     @dlm_d
        xor     al,al
@dlm_d:
        stosb
        test    al,al
        jnz     @dlm_i
        inc     dl
        cmp     cl,dh
        jbe     @dlm_z
        mov     dh,cl
@dlm_z:
        xor     ecx,ecx
        cmp     byte ptr [esi-1],0
        je      @dlm_e
        jmp     @dlm_c
@dlm_i:
        inc     cx
        jmp     @dlm_c
@dlm_e:
        mov     [g_ui.dg.msg_nl],dl
        mov     [g_ui.dg.msg_bt],ebx
        mov     ecx,-3 ; the buttons' row: 3 apart
@dlm_b:
        cmp     dword ptr [ebx],0
        je      @dlm_bw
        call    dl_bw
        add     cx,ax
        add     cx,3
        add     ebx,size TBTN
        jmp     @dlm_b
@dlm_bw:
        mov     [g_ui.dg.msg_bw],cx
        movzx   ax,dh                   ; the wider, and 6 (msgbox.c)
        cmp     ax,cx
        jae     @dlm_w
        mov     ax,cx
@dlm_w:
        add     ax,6
        mov     ebx,offset g_ui.bf.msg_d
        mov     [ebx+TDLG.cols],al
        mov     ah,80
        sub     ah,al
        shr     ah,1
        mov     [ebx+TDLG.col],ah
        mov     al,[g_ui.dg.msg_nl]
        add     al,6
        mov     [ebx+TDLG.rows],al
        mov     ah,25
        sub     ah,al
        shr     ah,1
        cmp     [g_ui.dg.msg_adj],0              ; over the error's line: at the foot
        je      @dlm_p
        push    eax
        call    ed_ensure
        pop     eax
        movzx   ecx,word ptr [g_ui.ed.ed_line]
        sub     cx,[g_ui.ed.ed_top]
        add     cx,[g_ui.ed.ed_r0]
        cmp     cl,ah
        jb      @dlm_p
        mov     ch,ah
        add     ch,al
        cmp     cl,ch
        jae     @dlm_p
        mov     ah,24
        sub     ah,al
@dlm_p:
        mov     [ebx+TDLG.row],ah
        mov     dword ptr [ebx+TDLG.ttl],0
        mov     al,[g_ui.dg.msg_nl]
        add     al,3
        mov     [ebx+TDLG.sep],al
        mov     edi,ebx                 ; the lines as text controls
        add     edi,size TDLG
        mov     esi,offset g_ui.bf.msg_txt
        mov     cl,0
@dlm_t:
        cmp     cl,[g_ui.dg.msg_nl]
        jae     @dlm_u
        mov     byte ptr [edi+TCTL.kind],CT_TXT
        mov     al,cl
        add     al,2
        mov     [edi+TCTL.row],al
        push    ecx
        call    sc_slen
        mov     al,[ebx+TDLG.cols]
        sub     al,cl
        shr     al,1
        mov     [edi+TCTL.col],al
        pop     ecx
        mov     byte ptr [edi+TCTL.flg],CF_NOF
        mov     [edi+TCTL.data],esi
        xor     eax,eax
        mov     [edi+TCTL.cb],eax
        mov     [edi+TCTL.lbl],eax
@dlm_s:
        lodsb
        test    al,al
        jnz     @dlm_s
        add     edi,size TCTL
        inc     cl
        jmp     @dlm_t
@dlm_u:
        mov     [ebx+TDLG.foc],cl
        mov     [ebx+TDLG.def],cl
        mov     [g_ui.dg.msg_b0],cl
        mov     al,[ebx+TDLG.cols]  ; the buttons, centred inside
        sub     al,2
        sub     al,byte ptr [g_ui.dg.msg_bw]
        shr     al,1
        inc     al
        mov     ah,[g_ui.dg.msg_nl]
        add     ah,4
        mov     esi,[g_ui.dg.msg_bt]
@dlm_v:
        mov     edx,[esi]
        test    edx,edx
        jz      @dlm_r
        mov     byte ptr [edi+TCTL.kind],CT_BTN
        mov     [edi+TCTL.row],ah
        mov     [edi+TCTL.col],al
        mov     byte ptr [edi+TCTL.flg],0
        mov     [edi+TCTL.data],edx
        mov     dword ptr [edi+TCTL.cb],0
        cmp     edx,offset u_bhelp
        jne     @dlm_hb
        mov     dword ptr [edi+TCTL.cb],offset dl_bhelp
@dlm_hb:
        mov     dword ptr [edi+TCTL.lbl],0
        mov     dl,[esi+4]
        mov     [edi+TCTL.cols],dl
        push    eax
        push    ebx
        mov     ebx,esi
        call    dl_bw
        mov     dl,al
        pop     ebx
        pop     eax
        add     al,dl
        add     al,3
        add     edi,size TCTL
        inc     cl
        add     esi,size TBTN
        jmp     @dlm_v
@dlm_r:
        mov     [ebx+TDLG.n],cl
        call    dl_run
        cmp     al,0FFh
        je      @dlm_x
        sub     al,[g_ui.dg.msg_b0]
@dlm_x:
        push    eax
        mov     [g_ui.lp.ui_help],0
        pop     eax
        ret

; ::::::::::::::
; dl_bw - the width of the message box button at BX: its own, else its
; text + 4.
;
; out:    AX  it
; keeps:  all but AX
; ::::::::::::::
dl_bw:
        movzx   ax,byte ptr [ebx+4]
        test    ax,ax
        jnz     @dbw_r
        push    ecx
        push    esi
        mov     esi,[ebx]
        call    sc_slen
        mov     ax,cx
        add     ax,4
        pop     esi
        pop     ecx
@dbw_r:
        ret

; ::::::::::::::
; dl_bhelp - a Help button: no help to show; a beep, the box stays.
;
; out:    CF  set
; ::::::::::::::
dl_bhelp:
        mov     ax,0E07h
        int     10h
        stc
        ret

; ::::::::::::::
; dl_err - dl_ok for an error at the cursor's line: the box off that line;
; the selection off after.
; ::::::::::::::
dl_err:
        mov     [g_ui.dg.msg_adj],1
        call    dl_ok
        mov     [g_ui.dg.msg_adj],0
        and     [g_ui.ed.sel_on],0               ; the error's word, shown till now
        ret

; ::::::::::::::
; dl_ok - a message box with OK.
;
; in:     SI  the text
; ::::::::::::::
dl_ok:
        pushad
        mov     ebx,offset bt_ok
        call    dl_msg
        popad
        ret

; ::::::::::::::
; dl_ync - a message box with Yes, No, Cancel.
;
; out:    AL  0 Yes, 1 No, 2 Cancel or Esc
; ::::::::::::::
dl_ync:
        mov     ebx,offset bt_ync
        call    dl_msg
        cmp     al,0FFh
        jne     @dly_r
        mov     al,2
@dly_r:
        ret

; ::::::::::::::
; dl_files - the lists of the Open and Save As boxes: the files matching
; the mask in dl_mask, the directories, the current path.
; ::::::::::::::
dl_files:
        mov     ah,1Ah
        mov     edx,offset g_ui.bf.dl_dta
        int     21h
        mov     edi,offset g_ui.bf.ls_fl
        mov     edx,offset g_ui.bf.dl_mask
        xor     ecx,ecx
        call    @dlf_find
        mov     [g_ui.dg.ls_f+TLIST.n],ax
        mov     edi,offset g_ui.bf.ls_dl
        mov     edx,offset u_star
        mov     ecx,10h
        call    @dlf_find
        mov     [g_ui.dg.ls_d+TLIST.n],ax
        xor     ax,ax
        mov     [g_ui.dg.ls_f+TLIST.sel],ax
        mov     [g_ui.dg.ls_f+TLIST.top],ax
        mov     [g_ui.dg.ls_d+TLIST.sel],ax
        mov     [g_ui.dg.ls_d+TLIST.top],ax
        mov     ah,19h                  ; the path: drive, colon, \, directory
        int     21h
        add     al,'A'
        mov     edi,offset g_ui.bf.dl_path
        stosb
        mov     ax,'\:'
        stosw
        mov     esi,edi
        mov     ah,47h
        xor     dl,dl
        int     21h
        ret
; the names matching DX with attribute CX (10h: directories only) to DI
@dlf_find:
        xor     ebx,ebx
        push    ecx
        mov     ah,4Eh
        int     21h
        pop     ecx
        jc      @dlf_d
@dlf_l:
        cmp     cx,10h
        jne     @dlf_a
        test    byte ptr [g_ui.bf.dl_dta+15h],10h
        jz      @dlf_n
        cmp     word ptr [g_ui.bf.dl_dta+1Eh],'.'
        je      @dlf_n
@dlf_a:
        cmp     bx,LSMAX
        jae     @dlf_d
        mov     esi,offset g_ui.bf.dl_dta+1Eh
        push    edi
        push    ecx
        mov     ecx,13
        rep     movsb
        pop     ecx
        pop     edi
        add     edi,13
        inc     bx
@dlf_n:
        push    ecx
        mov     ah,4Fh
        int     21h
        pop     ecx
        jnc     @dlf_l
@dlf_d:
        mov     ax,bx
        ret

; ::::::::::::::
; dl_fsel - the Files list: the selected name to the field.
; ::::::::::::::
dl_fsel:
        cmp     [g_ui.dg.ls_f+TLIST.n],0
        je      @dfs_n
        push    eax                      ; Enter too takes the selected one
        movzx   esi,word ptr [g_ui.dg.ls_f+TLIST.sel]
        imul    esi,esi,13
        add     esi,offset g_ui.bf.ls_fl
        mov     edi,offset g_ui.bf.dl_fn.txt
        xor     ecx,ecx
@dfs_c:
        lodsb
        test    al,al
        jz      @dfs_e
        stosb
        inc     cx
        jmp     @dfs_c
@dfs_e:
        mov     [g_ui.bf.dl_fn.len],cx
        mov     [g_ui.bf.dl_fn.col],cx
        pop     eax
        cmp     al,1
        jne     @dfs_n
        clc
        ret
@dfs_n:
        stc
        ret

; ::::::::::::::
; dl_dsel - the Dirs list: Enter goes into the selected one.
;
; out:    CF  clear: done, the box stays
; ::::::::::::::
dl_dsel:
        cmp     al,2
        jne     @dds_n
        cmp     word ptr [g_ui.dg.ls_d+TLIST.n],0
        je      @dds_n
        movzx   edx,word ptr [g_ui.dg.ls_d+TLIST.sel]
        imul    edx,edx,13
        add     edx,offset g_ui.bf.ls_dl
        mov     ah,3Bh
        int     21h
        call    dl_files
        clc
        ret
@dds_n:
        stc
        ret

; ::::::::::::::
; dl_fok - OK in Open: a mask in the field lists it, the box stays;
; else the name, .BAS added without a dot, in dl_name.
;
; out:    CF  set: the box stays
; ::::::::::::::
dl_fok:
        movzx   ecx,word ptr [g_ui.bf.dl_fn.len]
        jcxz    @dfo_s
        mov     esi,offset g_ui.bf.dl_fn.txt
        mov     edi,offset g_ui.bf.dl_name
        xor     dx,dx                   ; DL a wildcard, DH a dot
@dfo_c:
        lodsb
        call    ui_upper
        cmp     al,'*'
        je      @dfo_w
        cmp     al,'?'
        jne     @dfo_p
@dfo_w:
        mov     dl,1
@dfo_p:
        cmp     al,'.'
        jne     @dfo_q
        mov     dh,1
@dfo_q:
        cmp     al,'\'
        jne     @dfo_o
        xor     dh,dh
@dfo_o:
        stosb
        loop    @dfo_c
        test    dh,dh
        jnz     @dfo_z
        mov     esi,offset u_bas
        movsd
@dfo_z:
        mov     byte ptr [edi],0
        test    dl,dl
        jz      @dfo_r
        mov     esi,offset g_ui.bf.dl_name  ; a mask: list it
        mov     edi,offset g_ui.bf.dl_mask
        mov     ecx,64
        rep     movsb
        call    dl_files
@dfo_s:
        stc
        ret
@dfo_r:
        clc
        ret

; ::::::::::::::
; dl_name1 - the field set to the 0-ended string at SI.
; ::::::::::::::
dl_name1:
        mov     edi,offset g_ui.bf.dl_fn.txt
        xor     ecx,ecx
@dn1_c:
        lodsb
        test    al,al
        jz      @dn1_e
        stosb
        inc     cx
        jmp     @dn1_c
@dn1_e:
        mov     [g_ui.bf.dl_fn.len],cx
        mov     [g_ui.bf.dl_fn.col],cx
        ret

; ::::::::::::::
; dl_open - the Open box.
;
; out:    CF  set: cancelled; else dl_name the file
; ::::::::::::::
dl_open:
        mov     esi,offset u_mbas
        mov     edi,offset g_ui.bf.dl_mask
        mov     ecx,6
        rep     movsb
        mov     esi,offset g_ui.bf.dl_mask
        call    dl_name1
        call    dl_files
        mov     ebx,offset dg_open
        mov     byte ptr [ebx+TDLG.foc],0
        call    dl_run
        cmp     al,DO_OK
        jne     @dlo_c
        clc
        ret
@dlo_c:
        stc
        ret

; ::::::::::::::
; dl_saveas - the Save As box, the field the file's name.
;
; out:    CF  set: cancelled; else dl_name the file
; ::::::::::::::
dl_saveas:
        mov     esi,offset g_ui.bf.ui_fn
        call    dl_name1
        mov     word ptr [g_ui.bf.dl_mask],'.*'
        mov     word ptr [g_ui.bf.dl_mask+2],'*'
        call    dl_files
        mov     ebx,offset dg_save
        mov     byte ptr [ebx+TDLG.foc],0
        call    dl_run
        cmp     al,DS_OK
        jne     @dlo_c
        clc
        ret

; ::::::::::::::
; dl_find - the Find box.
;
; out:    CF  set: cancelled
; ::::::::::::::
dl_find:
        mov     ebx,offset dg_find
        mov     byte ptr [ebx+TDLG.foc],0
        call    dl_run
        cmp     al,DF_OK
        jne     @dlo_c
        clc
        ret

; ::::::::::::::
; dl_conf - Find and Verify's question, QBasic's dlgConfirm: its buttons
; only, at row 19, or row 2 when the cursor is that low.
;
; out:    AL  0 Change, 1 Skip, else cancelled
; ::::::::::::::
dl_conf:
        call    ed_ensure
        mov     ebx,offset dg_conf
        mov     ax,[g_ui.ed.ed_line]
        sub     ax,[g_ui.ed.ed_top]
        add     ax,[g_ui.ed.ed_r0]
        mov     cl,19
        cmp     ax,19
        jb      @dlc_r
        mov     cl,2
@dlc_r:
        mov     [ebx+TDLG.row],cl
        mov     byte ptr [ebx+TDLG.foc],0
        jmp     dl_run

; ::::::::::::::
; dl_change - the Change box.
;
; out:    AL  0 Find and Verify, 1 Change All, else cancelled
; ::::::::::::::
dl_change:
        mov     ebx,offset dg_chg
        mov     byte ptr [ebx+TDLG.foc],0
        call    dl_run
        sub     al,DC_FV
        ret

; The boxes as QBasic's (uifile.des, uisearch.des): a place here is the
; .des one +1, from the frame; a field at its group box's, as wide as its
; EDIT; a list inside its frame.

; the Open box: the field, the path, the lists, the buttons
        .data
dg_open TDLG    <2,6,20,67,u_open,7,0,17,DO_OK>
        TCTL    <CT_FLD,1,13,50,1,0,g_ui.bf.dl_fn,0,u_fname,2,2>
        TCTL    <CT_TXT,4,2,0,0,CF_NOF,g_ui.bf.dl_path,0,0,0,0>
        TCTL    <CT_LST,6,2,43,8,0,g_ui.dg.ls_f,dl_fsel,u_files,5,22>
        TCTL    <CT_LST,6,49,14,8,0,g_ui.dg.ls_d,dl_dsel,u_dirs,5,51>
        TCTL    <CT_BTN,18,12,6,0,0,u_ok,dl_fok,0,0,0>
        TCTL    <CT_BTN,18,28,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,18,48,8,0,0,u_bhelp,dl_bhelp,0,0,0>

; the Save As box
dg_save TDLG    <3,19,18,42,u_saveas,6,0,15,DS_OK>
        TCTL    <CT_FLD,1,13,25,1,0,g_ui.bf.dl_fn,0,u_fname,2,2>
        TCTL    <CT_TXT,4,2,0,0,CF_NOF,g_ui.bf.dl_path,0,0,0,0>
        TCTL    <CT_LST,6,13,14,7,0,g_ui.dg.ls_d,dl_dsel,u_dirs,5,15>
        TCTL    <CT_BTN,16,7,6,0,0,u_ok,dl_fok,0,0,0>
        TCTL    <CT_BTN,16,17,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,16,31,8,0,0,u_bhelp,dl_bhelp,0,0,0>

; the Find box
dg_find TDLG    <7,10,11,59,u_find,6,0,8,DF_OK>
        TCTL    <CT_FLD,1,13,42,1,0,g_ui.bf.fd_what,0,u_fwhat,2,2>
        TCTL    <CT_CHK,6,5,0,0,0,u_case,g_ui.fd.fd_case,0,0,0>
        TCTL    <CT_CHK,6,38,0,0,0,u_whole,g_ui.fd.fd_whole,0,0,0>
        TCTL    <CT_BTN,9,9,6,0,0,u_ok,0,0,0,0>
        TCTL    <CT_BTN,9,24,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,9,42,8,0,0,u_bhelp,dl_bhelp,0,0,0>

; Find and Verify's question (uisearch.des dlgConfirm)
dg_conf TDLG    <19,13,5,53,u_change,4,0,2,0>
        TCTL    <CT_BTN,3,4,10,0,0,u_chg1,0,0,0,0>
        TCTL    <CT_BTN,3,16,8,0,0,u_skip,0,0,0,0>
        TCTL    <CT_BTN,3,28,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,3,41,8,0,0,u_bhelp,dl_bhelp,0,0,0>

; the Change box
dg_chg  TDLG    <5,11,14,58,u_change,8,0,11,DC_FV>
        TCTL    <CT_FLD,1,13,41,1,0,g_ui.bf.fd_what,0,u_fwhat,2,2>
        TCTL    <CT_FLD,4,13,41,1,0,g_ui.bf.fd_to,0,u_cto,5,2>
        TCTL    <CT_CHK,9,5,0,0,0,u_case,g_ui.fd.fd_case,0,0,0>
        TCTL    <CT_CHK,9,37,0,0,0,u_whole,g_ui.fd.fd_whole,0,0,0>
        TCTL    <CT_BTN,12,2,19,0,0,u_fverify,0,0,0,0>
        TCTL    <CT_BTN,12,22,14,0,0,u_chall,0,0,0,0>
        TCTL    <CT_BTN,12,37,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,12,48,8,0,0,u_bhelp,dl_bhelp,0,0,0>
        .code

        end
