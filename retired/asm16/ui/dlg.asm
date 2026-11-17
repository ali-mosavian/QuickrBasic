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

D_ROW   = 0
D_COL   = 1
D_H     = 2
D_W     = 3
D_TTL   = 4
D_N     = 6
D_FOC   = 7
D_SEP   = 8
D_DEF   = 9
D_CTL   = 12
CSZ     = 14
C_TYPE  = 0
C_ROW   = 1
C_COL   = 2
C_W     = 3
C_H     = 4
C_FLG   = 5
C_DATA  = 6
C_CB    = 8
C_LBL   = 10
C_LR    = 12
C_LC    = 13
CT_FLD  = 1
CT_LST  = 2
CT_BTN  = 3
CT_CHK  = 4
CT_TXT  = 5
CF_NOF  = 1                     ; takes no focus
L_N     = 0
L_SEL   = 2
L_TOP   = 4
L_ITEMS = 6
L_SZ    = 8
A_DLG   = 70h
A_DSEL  = 07h
A_DFOC  = 7Fh

; ::::::::::::::
; dl_ctl - control AL of the dialog at BX.
;
; out:    DI  it
; keeps:  all but DI
; ::::::::::::::
dl_ctl:
        push    ax
        movzx   ax,al
        imul    di,ax,CSZ
        add     di,bx
        add     di,D_CTL
        pop     ax
        ret

; ::::::::::::::
; dl_pos - the screen position of row AH, column AL in the dialog at BX.
;
; out:    DX  it
; keeps:  all but DX
; ::::::::::::::
dl_pos:
        mov     dh,[bx+D_ROW]
        add     dh,ah
        mov     dl,[bx+D_COL]
        add     dl,al
        ret

; ::::::::::::::
; dl_draw - the dialog at BX: frame, shadow, separator, controls, cursor.
; ::::::::::::::
dl_draw:
        mov     dh,[bx+D_ROW]
        mov     dl,[bx+D_COL]
        mov     ch,[bx+D_H]
        mov     cl,[bx+D_W]
        mov     ah,A_DLG
        mov     si,[bx+D_TTL]
        call    sc_box
        call    sc_shadow
        mov     al,[bx+D_SEP]
        test    al,al
        jz      @dld_c
        add     dh,al
        mov     ah,A_DLG
        call    sc_sep
@dld_c:
        mov     [dl_cur],2000h
        xor     al,al
@dld_l:
        cmp     al,[bx+D_N]
        jae     @dld_e
        push    ax
        call    dl_ctl
        mov     [dl_cp],di
        mov     cl,0
        cmp     al,[bx+D_FOC]
        jne     @dld_f
        mov     cl,1
@dld_f:
        mov     [dl_hf],cl
        call    dl_one
        pop     ax
        inc     al
        jmp     @dld_l
@dld_e:
        mov     dx,[dl_cur]
        cmp     dx,2000h
        je      @dld_h
        mov     cx,0607h
        call    sc_shape
        jmp     sc_cur
@dld_h:
        mov     cx,2000h
        jmp     sc_shape

; ::::::::::::::
; dl_one - control dl_cp of the dialog at BX; dl_hf 1 when it has the
; focus, then dl_cur the cursor's place.
; ::::::::::::::
dl_one:
        mov     di,[dl_cp]
        mov     si,[di+C_LBL]
        test    si,si
        jz      @dlo_t
        mov     ah,[di+C_LR]
        mov     al,[di+C_LC]
        call    dl_pos
        push    bx
        mov     ah,A_DLG
        call    dl_hk
        call    sc_str
        pop     bx
        mov     di,[dl_cp]
@dlo_t:
        mov     ah,[di+C_ROW]
        mov     al,[di+C_COL]
        call    dl_pos
        mov     al,[di+C_TYPE]
        cmp     al,CT_FLD
        je      dl_dfld
        cmp     al,CT_LST
        je      dl_dlst
        cmp     al,CT_BTN
        je      dl_dbtn
        cmp     al,CT_CHK
        je      dl_dchk
        mov     si,[di+C_DATA]
        push    bx
        mov     ah,A_DLG
        mov     bh,ah
        call    sc_str
        pop     bx
        ret

; ::::::::::::::
; dl_dfld - a text field at DX: its frame, the part of its text around the
; cursor.
; ::::::::::::::
dl_dfld:
        push    bx
        mov     ch,3
        mov     cl,[di+C_W]
        add     cl,2
        mov     ah,A_DLG
        xor     si,si
        call    sc_box
        inc     dh
        inc     dl
        mov     bx,[di+C_DATA]
        movzx   cx,byte ptr [di+C_W]
        mov     ax,[bx+LB_COL]          ; the first column shown
        sub     ax,cx
        jae     @dlf_s
        mov     ax,-1
@dlf_s:
        inc     ax
        mov     [dl_fs],ax
        lea     si,[bx+LB_TXT]
        add     si,ax
        mov     cx,[bx+LB_LEN]
        sub     cx,ax
        jae     @dlf_c
        xor     cx,cx
@dlf_c:
        movzx   ax,byte ptr [di+C_W]
        cmp     cx,ax
        jbe     @dlf_w
        mov     cx,ax
@dlf_w:
        mov     ah,A_DLG
        push    dx
        push    cx
        call    sc_strn
        pop     cx
        pop     dx
        cmp     [dl_hf],0
        je      @dlf_r
        cmp     [dl_new],0              ; the text selected till a key
        je      @dlf_k
        mov     ah,07h
        call    sc_attr
@dlf_k:
        mov     ax,[bx+LB_COL]
        sub     ax,[dl_fs]
        add     dl,al
        mov     [dl_cur],dx
@dlf_r:
        pop     bx
        ret

; ::::::::::::::
; dl_dlst - a list at DX: its frame, the items from its first shown, the
; selected one lit when it has the focus.
; ::::::::::::::
dl_dlst:
        push    bx
        mov     ch,[di+C_H]
        add     ch,2
        mov     cl,[di+C_W]
        add     cl,2
        mov     ah,A_DLG
        xor     si,si
        call    sc_box
        inc     dh
        inc     dl
        mov     bx,[di+C_DATA]
        mov     cx,[bx+L_SEL]           ; the first shown keeps it in view
        cmp     cx,[bx+L_TOP]
        jae     @dll_a
        mov     [bx+L_TOP],cx
@dll_a:
        movzx   ax,byte ptr [di+C_H]
        add     ax,[bx+L_TOP]
        cmp     cx,ax
        jb      @dll_b
        sub     cx,ax
        inc     cx
        add     [bx+L_TOP],cx
@dll_b:
        xor     cx,cx
@dll_r:
        mov     di,[dl_cp]
        cmp     cl,[di+C_H]
        jae     @dll_e
        mov     ax,[bx+L_TOP]
        add     ax,cx
        cmp     ax,[bx+L_N]
        jae     @dll_n
        push    cx
        push    dx
        movzx   si,byte ptr [bx+L_SZ]
        imul    si,ax
        add     si,[bx+L_ITEMS]
        push    ax
        push    bx
        mov     ah,A_DLG
        mov     bh,ah
        call    sc_str
        pop     bx
        pop     ax
        pop     dx
        pop     cx
        cmp     ax,[bx+L_SEL]
        jne     @dll_n
        cmp     [dl_hf],0
        je      @dll_n
        push    cx
        mov     di,[dl_cp]
        movzx   cx,byte ptr [di+C_W]
        mov     ah,A_DSEL
        call    sc_attr
        pop     cx
        mov     [dl_cur],dx
@dll_n:
        inc     dh
        inc     cx
        jmp     @dll_r
@dll_e:
        pop     bx
        ret

; ::::::::::::::
; dl_dbtn - a button at DX: < text >, its brackets bright with the focus.
; ::::::::::::::
dl_dbtn:
        push    bx
        mov     di,[dl_cp]
        mov     si,[di+C_DATA]
        call    sc_slen
        mov     al,[di+C_W]             ; its width: the text +4 if none
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
        mov     word ptr [dl_pad],ax
        mov     ah,A_DLG
        cmp     [dl_hf],0
        je      @dlb_s
        mov     ah,A_DFOC
@dlb_s:
        mov     al,'<'
        mov     cx,1
        call    sc_fill
        push    ax
        mov     ax,A_DLG*256+' '
        movzx   cx,byte ptr [dl_pad]
        call    sc_fill
        cmp     [dl_hf],0
        je      @dlb_t
        mov     [dl_cur],dx
@dlb_t:
        call    dl_hk
        call    sc_str
        movzx   cx,byte ptr [dl_pad+1]
        call    sc_fill
        pop     ax
        mov     al,'>'
        mov     cx,1
        call    sc_fill
        pop     bx
        ret

; ::::::::::::::
; dl_hk - BH the hotkeys' colour: lit once Alt was down in the dialog, as
; COW's; else AH.
; ::::::::::::::
dl_hk:
        mov     bh,ah
        cmp     [dl_alt],0
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
        test    si,si
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
        push    bx
        mov     ax,A_DLG*256+'['
        mov     cx,1
        call    sc_fill
        mov     di,[dl_cp]
        mov     si,[di+C_CB]
        mov     al,' '
        cmp     byte ptr [si],0
        je      @dlk_s
        mov     al,'X'
@dlk_s:
        cmp     [dl_hf],0
        je      @dlk_t
        mov     [dl_cur],dx
@dlk_t:
        call    sc_fill
        mov     al,']'
        call    sc_fill
        mov     al,' '
        call    sc_fill
        mov     di,[dl_cp]
        mov     si,[di+C_DATA]
        call    dl_hk
        call    sc_str
        pop     bx
        ret

; ::::::::::::::
; dl_hit - is the mouse (ms_pos) on control DI of the dialog at BX?
;
; out:    CF  clear: it is; dl_hr the row in it
; keeps:  all
; ::::::::::::::
dl_hit:
        pusha
        mov     dx,[ms_pos]
        mov     al,dh
        sub     al,[bx+D_ROW]
        sub     al,[di+C_ROW]
        jb      @dlh_n
        mov     ah,dl
        sub     ah,[bx+D_COL]
        sub     ah,[di+C_COL]
        jb      @dlh_n
        mov     cl,[di+C_TYPE]          ; CH its rows, CL its columns
        cmp     cl,CT_FLD
        je      @dlh_f
        cmp     cl,CT_LST
        je      @dlh_l
        cmp     cl,CT_TXT
        je      @dlh_n
        push    cx
        mov     si,[di+C_DATA]
        call    sc_slen
        mov     dx,cx
        pop     cx
        mov     cl,dl
        add     cl,4
        mov     ch,1
        jmp     @dlh_c
@dlh_f:
        mov     ch,3
        jmp     @dlh_w
@dlh_l:
        mov     ch,[di+C_H]
        add     ch,2
@dlh_w:
        mov     cl,[di+C_W]
        add     cl,2
@dlh_c:
        cmp     al,ch
        jae     @dlh_n
        cmp     ah,cl
        jae     @dlh_n
        mov     [dl_hr],al
        popa
        clc
        ret
@dlh_n:
        popa
        stc
        ret

; ::::::::::::::
; dl_step - the focus to the next control that takes it, AL 1, or the one
; before, AL -1.
; ::::::::::::::
dl_step:
        mov     ah,al
        mov     al,[bx+D_FOC]
@dls_l:
        add     al,ah
        jns     @dls_a
        mov     al,[bx+D_N]
        dec     al
@dls_a:
        cmp     al,[bx+D_N]
        jb      @dls_b
        xor     al,al
@dls_b:
        call    dl_ctl
        test    byte ptr [di+C_FLG],CF_NOF
        jnz     @dls_l
        mov     [bx+D_FOC],al
        mov     [dl_new],1
        ret

; ::::::::::::::
; dl_run - the dialog at BX until a button: the editor under it.
;
; out:    AL  the button's control, FFh Esc
; ::::::::::::::
dl_run:
        mov     [ui_help],offset u_dlg
        call    dl_run0
        mov     [ui_help],0
        ret

; ::::::::::::::
; dl_run0 - dl_run, the dialog's status line set.
; ::::::::::::::
dl_run0:
        mov     [dl_bx],bx
        mov     ah,2                    ; Alt down already: its letters lit
        int     16h
        and     al,8
        mov     [dl_alt],al
        mov     [dl_new],1
@dlr_l:
        mov     bx,[dl_bx]
        push    bx
        call    dr_all
        pop     bx
        call    dl_draw
        call    ui_getkey
        mov     bx,[dl_bx]
        mov     cx,ax
        mov     al,[bx+D_FOC]
        call    dl_ctl
        mov     ax,cx
        cmp     ax,MS_KEY
        je      @dlr_ms
        test    [ui_kf],8               ; Alt: the letters lit; Alt+letter
        jz      @dlr_na
        mov     [dl_alt],1
        test    al,al
        jnz     @dlr_na
        sub     ah,10h
        jb      @dlr_na
        cmp     ah,22h
        ja      @dlr_na
        movzx   si,ah
        mov     al,[mn_altl+si]
        mov     ah,1                    ; AH 1: unmatched beeps
        jmp     @dlr_hot
@dlr_na:
        cmp     ax,0FFFFh               ; an Alt tap
        jne     @dlr_nt
        mov     [dl_alt],1
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
        mov     cl,[di+C_TYPE]
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
        cmp     [ms_ev],MS_PRESS
        jne     @dlr_l
        xor     al,al
@dlr_mc:
        cmp     al,[bx+D_N]
        jae     @dlr_l
        call    dl_ctl
        call    dl_hit
        jnc     @dlr_mh
        inc     al
        jmp     @dlr_mc
@dlr_mh:
        test    byte ptr [di+C_FLG],CF_NOF
        jnz     @dlr_l
        mov     cl,[di+C_TYPE]
        cmp     cl,CT_BTN
        je      @dlr_btn
        mov     [bx+D_FOC],al
        mov     [dl_new],0
        cmp     cl,CT_CHK
        je      @dlr_chk
        cmp     cl,CT_LST
        jne     @dlr_l
        movzx   dx,[dl_hr]              ; a list's line: once selects, on the
        dec     dx                      ; selected one opens
        js      @dlr_l
        cmp     dl,[di+C_H]
        jae     @dlr_l
        mov     si,[di+C_DATA]
        add     dx,[si+L_TOP]
        cmp     dx,[si+L_N]
        jae     @dlr_l
        cmp     dx,[si+L_SEL]
        jne     @dlr_set
        jmp     @dlr_ent
@dlr_hot:                               ; a hotkey, from the next control on
        call    ui_upper                ; (COW dlgcore.c): a button pressed,
        mov     dl,al                   ; a box checked, else the focus
        mov     dh,ah
        mov     cl,[bx+D_N]
        mov     al,[bx+D_FOC]
@dlr_hn:
        inc     al
        cmp     al,[bx+D_N]
        jb      @dlr_h1
        xor     al,al
@dlr_h1:
        call    dl_ctl
        push    ax
        mov     si,[di+C_DATA]
        mov     ah,[di+C_TYPE]
        cmp     ah,CT_BTN
        je      @dlr_h2
        cmp     ah,CT_CHK
        je      @dlr_h2
        mov     si,[di+C_LBL]
        cmp     ah,CT_TXT
        jne     @dlr_h2
        xor     si,si
@dlr_h2:
        call    dl_acc
        cmp     al,dl
        pop     ax
        je      @dlr_hf
        dec     cl
        jnz     @dlr_hn
        test    dh,dh
        jz      @dlr_l
        mov     ax,0E07h
        int     10h
        jmp     @dlr_l
@dlr_hf:
        mov     cl,[di+C_TYPE]
        cmp     cl,CT_BTN
        je      @dlr_btn
        mov     [bx+D_FOC],al
        mov     [dl_new],1
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
        mov     si,[di+C_CB]
        xor     byte ptr [si],1
        jmp     @dlr_l
@dlr_ent:
        cmp     byte ptr [di+C_TYPE],CT_BTN
        je      @dlr_btn
        cmp     byte ptr [di+C_TYPE],CT_LST
        jne     @dlr_def
        mov     cx,[di+C_CB]
        jcxz    @dlr_def
        mov     al,2
        call    cx
        jnc     @dlr_l
@dlr_def:
        mov     bx,[dl_bx]
        mov     al,[bx+D_DEF]
        call    dl_ctl
@dlr_btn:
        mov     ax,di
        sub     ax,bx
        sub     ax,D_CTL
        mov     cl,CSZ
        div     cl
        mov     [dl_res],al
        mov     cx,[di+C_CB]
        jcxz    @dlr_ok
        call    cx
        jc      @dlr_l
@dlr_ok:
        mov     al,[dl_res]
        ret
@dlr_fld:
        push    bx
        mov     bx,[di+C_DATA]
        cmp     [dl_new],0              ; a character replaces selected text
        je      @dlr_f1
        mov     [dl_new],0
        cmp     al,' '
        jb      @dlr_f1
        and     word ptr [bx+LB_LEN],0
        and     word ptr [bx+LB_COL],0
@dlr_f1:
        call    ui_ledit
        pop     bx
        jmp     @dlr_l
@dlr_lst:
        mov     si,[di+C_DATA]
        mov     dx,[si+L_SEL]
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
        movzx   ax,byte ptr [di+C_H]
        sub     dx,ax
        jae     @dlr_set
@dlr_hm:
        xor     dx,dx
        jmp     @dlr_set
@dlr_pd:
        movzx   ax,byte ptr [di+C_H]
        add     dx,ax
        jmp     @dlr_set
@dlr_en:
        mov     dx,0FFFFh
@dlr_set:
        mov     ax,[si+L_N]
        test    ax,ax
        jz      @dlr_l
        cmp     dx,ax
        jb      @dlr_s2
        mov     dx,ax
        dec     dx
@dlr_s2:
        mov     [si+L_SEL],dx
        mov     cx,[di+C_CB]
        jcxz    @dlr_l2
        mov     al,1
        call    cx
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
        mov     di,offset msg_txt       ; the lines, 0 each
        xor     dx,dx                   ; DL the lines, DH the widest
        xor     cx,cx
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
        xor     cx,cx
        cmp     byte ptr [si-1],0
        je      @dlm_e
        jmp     @dlm_c
@dlm_i:
        inc     cx
        jmp     @dlm_c
@dlm_e:
        mov     [msg_nl],dl
        mov     [msg_bt],bx
        mov     cx,-3                   ; the buttons' row: 3 apart
@dlm_b:
        cmp     word ptr [bx],0
        je      @dlm_bw
        call    dl_bw
        add     cx,ax
        add     cx,3
        add     bx,3
        jmp     @dlm_b
@dlm_bw:
        mov     [msg_bw],cx
        movzx   ax,dh                   ; the wider, and 6 (msgbox.c)
        cmp     ax,cx
        jae     @dlm_w
        mov     ax,cx
@dlm_w:
        add     ax,6
        mov     bx,offset msg_d
        mov     [bx+D_W],al
        mov     ah,80
        sub     ah,al
        shr     ah,1
        mov     [bx+D_COL],ah
        mov     al,[msg_nl]
        add     al,6
        mov     [bx+D_H],al
        mov     ah,25
        sub     ah,al
        shr     ah,1
        cmp     [msg_adj],0             ; over the error's line: at the foot
        je      @dlm_p
        push    ax
        call    ed_ensure
        pop     ax
        mov     cx,[ed_line]
        sub     cx,[ed_top]
        add     cx,ED_R0
        cmp     cl,ah
        jb      @dlm_p
        mov     ch,ah
        add     ch,al
        cmp     cl,ch
        jae     @dlm_p
        mov     ah,24
        sub     ah,al
@dlm_p:
        mov     [bx+D_ROW],ah
        mov     word ptr [bx+D_TTL],0
        mov     al,[msg_nl]
        add     al,3
        mov     [bx+D_SEP],al
        mov     di,bx                   ; the lines as text controls
        add     di,D_CTL
        mov     si,offset msg_txt
        mov     cl,0
@dlm_t:
        cmp     cl,[msg_nl]
        jae     @dlm_u
        mov     byte ptr [di+C_TYPE],CT_TXT
        mov     al,cl
        add     al,2
        mov     [di+C_ROW],al
        push    cx
        call    sc_slen
        mov     al,[bx+D_W]
        sub     al,cl
        shr     al,1
        mov     [di+C_COL],al
        pop     cx
        mov     byte ptr [di+C_FLG],CF_NOF
        mov     [di+C_DATA],si
        xor     ax,ax
        mov     [di+C_CB],ax
        mov     [di+C_LBL],ax
@dlm_s:
        lodsb
        test    al,al
        jnz     @dlm_s
        add     di,CSZ
        inc     cl
        jmp     @dlm_t
@dlm_u:
        mov     [bx+D_FOC],cl
        mov     [bx+D_DEF],cl
        mov     [msg_b0],cl
        mov     al,[bx+D_W]             ; the buttons, centred inside
        sub     al,2
        sub     al,byte ptr [msg_bw]
        shr     al,1
        inc     al
        mov     ah,[msg_nl]
        add     ah,4
        mov     si,[msg_bt]
@dlm_v:
        mov     dx,[si]
        test    dx,dx
        jz      @dlm_r
        mov     byte ptr [di+C_TYPE],CT_BTN
        mov     [di+C_ROW],ah
        mov     [di+C_COL],al
        mov     byte ptr [di+C_FLG],0
        mov     [di+C_DATA],dx
        mov     word ptr [di+C_CB],0
        cmp     dx,offset u_bhelp
        jne     @dlm_hb
        mov     word ptr [di+C_CB],offset dl_bhelp
@dlm_hb:
        mov     word ptr [di+C_LBL],0
        mov     dl,[si+2]
        mov     [di+C_W],dl
        push    ax
        push    bx
        mov     bx,si
        call    dl_bw
        mov     dl,al
        pop     bx
        pop     ax
        add     al,dl
        add     al,3
        add     di,CSZ
        inc     cl
        add     si,3
        jmp     @dlm_v
@dlm_r:
        mov     [bx+D_N],cl
        call    dl_run
        cmp     al,0FFh
        je      @dlm_x
        sub     al,[msg_b0]
@dlm_x:
        push    ax
        mov     [ui_help],0
        pop     ax
        ret

; ::::::::::::::
; dl_bw - the width of the message box button at BX: its own, else its
; text + 4.
;
; out:    AX  it
; keeps:  all but AX
; ::::::::::::::
dl_bw:
        movzx   ax,byte ptr [bx+2]
        test    ax,ax
        jnz     @dbw_r
        push    cx
        push    si
        mov     si,[bx]
        call    sc_slen
        mov     ax,cx
        add     ax,4
        pop     si
        pop     cx
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
        mov     [msg_adj],1
        call    dl_ok
        mov     [msg_adj],0
        and     [sel_on],0              ; the error's word, shown till now
        ret

; ::::::::::::::
; dl_ok - a message box with OK.
;
; in:     SI  the text
; ::::::::::::::
dl_ok:
        pusha
        mov     bx,offset bt_ok
        call    dl_msg
        popa
        ret

; ::::::::::::::
; dl_ync - a message box with Yes, No, Cancel.
;
; out:    AL  0 Yes, 1 No, 2 Cancel or Esc
; ::::::::::::::
dl_ync:
        mov     bx,offset bt_ync
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
        mov     dx,offset dl_dta
        int     21h
        mov     di,offset ls_fl
        mov     dx,offset dl_mask
        xor     cx,cx
        call    @dlf_find
        mov     [ls_f+L_N],ax
        mov     di,offset ls_dl
        mov     dx,offset u_star
        mov     cx,10h
        call    @dlf_find
        mov     [ls_d+L_N],ax
        xor     ax,ax
        mov     [ls_f+L_SEL],ax
        mov     [ls_f+L_TOP],ax
        mov     [ls_d+L_SEL],ax
        mov     [ls_d+L_TOP],ax
        mov     ah,19h                  ; the path: drive, colon, \, directory
        int     21h
        add     al,'A'
        mov     di,offset dl_path
        stosb
        mov     ax,'\:'
        stosw
        mov     si,di
        mov     ah,47h
        xor     dl,dl
        int     21h
        ret
; the names matching DX with attribute CX (10h: directories only) to DI
@dlf_find:
        xor     bx,bx
        push    cx
        mov     ah,4Eh
        int     21h
        pop     cx
        jc      @dlf_d
@dlf_l:
        cmp     cx,10h
        jne     @dlf_a
        test    byte ptr ds:[dl_dta+15h],10h
        jz      @dlf_n
        cmp     word ptr ds:[dl_dta+1Eh],'.'
        je      @dlf_n
@dlf_a:
        cmp     bx,LSMAX
        jae     @dlf_d
        mov     si,offset dl_dta+1Eh
        push    di
        push    cx
        mov     cx,13
        rep     movsb
        pop     cx
        pop     di
        add     di,13
        inc     bx
@dlf_n:
        push    cx
        mov     ah,4Fh
        int     21h
        pop     cx
        jnc     @dlf_l
@dlf_d:
        mov     ax,bx
        ret

; ::::::::::::::
; dl_fsel - the Files list: the selected name to the field.
; ::::::::::::::
dl_fsel:
        cmp     [ls_f+L_N],0
        je      @dfs_n
        push    ax                      ; Enter too takes the selected one
        mov     ax,[ls_f+L_SEL]
        imul    si,ax,13
        add     si,offset ls_fl
        mov     di,offset dl_fn+LB_TXT
        xor     cx,cx
@dfs_c:
        lodsb
        test    al,al
        jz      @dfs_e
        stosb
        inc     cx
        jmp     @dfs_c
@dfs_e:
        mov     ds:[dl_fn+LB_LEN],cx
        mov     ds:[dl_fn+LB_COL],cx
        pop     ax
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
        cmp     word ptr [ls_d+L_N],0
        je      @dds_n
        mov     ax,[ls_d+L_SEL]
        imul    dx,ax,13
        add     dx,offset ls_dl
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
        mov     cx,ds:[dl_fn+LB_LEN]
        jcxz    @dfo_s
        mov     si,offset dl_fn+LB_TXT
        mov     di,offset dl_name
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
        mov     si,offset u_bas
        movsd
@dfo_z:
        mov     byte ptr [di],0
        test    dl,dl
        jz      @dfo_r
        mov     si,offset dl_name       ; a mask: list it
        mov     di,offset dl_mask
        mov     cx,64
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
        mov     di,offset dl_fn+LB_TXT
        xor     cx,cx
@dn1_c:
        lodsb
        test    al,al
        jz      @dn1_e
        stosb
        inc     cx
        jmp     @dn1_c
@dn1_e:
        mov     ds:[dl_fn+LB_LEN],cx
        mov     ds:[dl_fn+LB_COL],cx
        ret

; ::::::::::::::
; dl_open - the Open box.
;
; out:    CF  set: cancelled; else dl_name the file
; ::::::::::::::
dl_open:
        mov     si,offset u_mbas
        mov     di,offset dl_mask
        mov     cx,6
        rep     movsb
        mov     si,offset dl_mask
        call    dl_name1
        call    dl_files
        mov     bx,offset dg_open
        mov     byte ptr [bx+D_FOC],0
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
        mov     si,offset ui_fn
        call    dl_name1
        mov     word ptr ds:[dl_mask],'.*'
        mov     word ptr ds:[dl_mask+2],'*'
        call    dl_files
        mov     bx,offset dg_save
        mov     byte ptr [bx+D_FOC],0
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
        mov     bx,offset dg_find
        mov     byte ptr [bx+D_FOC],0
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
        mov     bx,offset dg_conf
        mov     ax,[ed_line]
        sub     ax,[ed_top]
        add     ax,ED_R0
        mov     cl,19
        cmp     ax,19
        jb      @dlc_r
        mov     cl,2
@dlc_r:
        mov     [bx+D_ROW],cl
        mov     byte ptr [bx+D_FOC],0
        jmp     dl_run

; ::::::::::::::
; dl_change - the Change box.
;
; out:    AL  0 Find and Verify, 1 Change All, else cancelled
; ::::::::::::::
dl_change:
        mov     bx,offset dg_chg
        mov     byte ptr [bx+D_FOC],0
        call    dl_run
        sub     al,DC_FV
        ret

; The boxes as QBasic's (uifile.des, uisearch.des): a place here is the
; .des one +1, from the frame; a field at its group box's, as wide as its
; EDIT; a list inside its frame.

; the Open box: the field, the path, the lists, the buttons
DSEG    segment
dg_open db      2,6,20,67
        dw      u_open
        db      7,0,17,DO_OK,0,0
        db      CT_FLD,1,13,50,1,0
        dw      dl_fn,0,u_fname
        db      2,2
        db      CT_TXT,4,2,0,0,CF_NOF
        dw      dl_path,0,0
        db      0,0
        db      CT_LST,6,2,43,8,0
        dw      ls_f,dl_fsel,u_files
        db      5,22
        db      CT_LST,6,49,14,8,0
        dw      ls_d,dl_dsel,u_dirs
        db      5,51
        db      CT_BTN,18,12,6,0,0
        dw      u_ok,dl_fok,0
        db      0,0
        db      CT_BTN,18,28,10,0,0
        dw      u_cancel,0,0
        db      0,0
        db      CT_BTN,18,48,8,0,0
        dw      u_bhelp,dl_bhelp,0
        db      0,0
DO_OK   = 4

; the Save As box
dg_save db      3,19,18,42
        dw      u_saveas
        db      6,0,15,DS_OK,0,0
        db      CT_FLD,1,13,25,1,0
        dw      dl_fn,0,u_fname
        db      2,2
        db      CT_TXT,4,2,0,0,CF_NOF
        dw      dl_path,0,0
        db      0,0
        db      CT_LST,6,13,14,7,0
        dw      ls_d,dl_dsel,u_dirs
        db      5,15
        db      CT_BTN,16,7,6,0,0
        dw      u_ok,dl_fok,0
        db      0,0
        db      CT_BTN,16,17,10,0,0
        dw      u_cancel,0,0
        db      0,0
        db      CT_BTN,16,31,8,0,0
        dw      u_bhelp,dl_bhelp,0
        db      0,0
DS_OK   = 3

; the Find box
dg_find db      7,10,11,59
        dw      u_find
        db      6,0,8,DF_OK,0,0
        db      CT_FLD,1,13,42,1,0
        dw      fd_what,0,u_fwhat
        db      2,2
        db      CT_CHK,6,5,0,0,0
        dw      u_case,fd_case,0
        db      0,0
        db      CT_CHK,6,38,0,0,0
        dw      u_whole,fd_whole,0
        db      0,0
        db      CT_BTN,9,9,6,0,0
        dw      u_ok,0,0
        db      0,0
        db      CT_BTN,9,24,10,0,0
        dw      u_cancel,0,0
        db      0,0
        db      CT_BTN,9,42,8,0,0
        dw      u_bhelp,dl_bhelp,0
        db      0,0
DF_OK   = 3

; Find and Verify's question (uisearch.des dlgConfirm)
dg_conf db      19,13,5,53
        dw      u_change
        db      4,0,2,0,0,0
        db      CT_BTN,3,4,10,0,0
        dw      u_chg1,0,0
        db      0,0
        db      CT_BTN,3,16,8,0,0
        dw      u_skip,0,0
        db      0,0
        db      CT_BTN,3,28,10,0,0
        dw      u_cancel,0,0
        db      0,0
        db      CT_BTN,3,41,8,0,0
        dw      u_bhelp,dl_bhelp,0
        db      0,0

; the Change box
dg_chg  db      5,11,14,58
        dw      u_change
        db      8,0,11,DC_FV,0,0
        db      CT_FLD,1,13,41,1,0
        dw      fd_what,0,u_fwhat
        db      2,2
        db      CT_FLD,4,13,41,1,0
        dw      fd_to,0,u_cto
        db      5,2
        db      CT_CHK,9,5,0,0,0
        dw      u_case,fd_case,0
        db      0,0
        db      CT_CHK,9,37,0,0,0
        dw      u_whole,fd_whole,0
        db      0,0
        db      CT_BTN,12,2,19,0,0
        dw      u_fverify,0,0
        db      0,0
        db      CT_BTN,12,22,14,0,0
        dw      u_chall,0,0
        db      0,0
        db      CT_BTN,12,37,10,0,0
        dw      u_cancel,0,0
        db      0,0
        db      CT_BTN,12,48,8,0,0
        dw      u_bhelp,dl_bhelp,0
        db      0,0
DC_FV   = 4
DSEG    ends
