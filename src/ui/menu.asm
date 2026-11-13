; ==========================================================================
; menu.asm - the menu bar, its menus, their commands
; ==========================================================================
; holds:    mn_bar, mn_run, mn_alt, mn_drop, mn_item, the menu tables
; uses:     sc_*, dr_all, ui_getkey, ui_act
; data:     A menu: its title, its column, its items. An item: the command
;           (A_*), flags (MF_*), its text with '&' before the hotkey, its
;           key's name, each 0-ended; MF_SEP alone is a separator; a 0
;           command ends the menu. An MF_SUB item's command is its sub-menu's
;           index in mn_it, past the bar's; the sub-menu's box is drawn over
;           its parent's, whose level mn_swap keeps in mn_sv.
; calls:
;   mn_run             the bar lit, a menu open, until a command or Esc
;     mn_drop          the open menu's box
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; mn_bar - the menu bar; the open menu's title lit (mn_on).
; ::::::::::::::
mn_bar:
        xor     dx,dx
        mov     ax,A_BAR*256+' '
        mov     ecx,80
        call    sc_fill
        xor     ebx,ebx
@mnb_l:
        push    ebx
        mov     esi,[mn_tt+ebx*2]
        shr     bx,1
        mov     dl,[mn_x+ebx]
        xor     dh,dh
        mov     ah,A_BAR
        mov     bh,A_HOT
        cmp     [g_ui.mn.mn_on],0
        je      @mnb_s
        cmp     bl,[g_ui.mn.mn_c]
        jne     @mnb_s
        dec     dl
        mov     ax,A_MSEL*256+' '
        mov     ecx,1
        call    sc_fill
        mov     bh,A_MHOT
        call    sc_str
        mov     al,' '
        call    sc_fill
        jmp     @mnb_n
@mnb_s:
        call    sc_str
@mnb_n:
        pop     ebx
        add     bx,2
        cmp     bx,NMENU*2
        jb      @mnb_l
        ret

; ::::::::::::::
; mn_skip - SI past an item.
; ::::::::::::::
mn_skip:
        lodsw
        cmp     ah,MF_SEP
        je      @mns_r
        mov     ecx,2
@mns_l:
        lodsb
        test    al,al
        jnz     @mns_l
        loop    @mns_l
@mns_r:
        ret

; ::::::::::::::
; mn_help - item AL's help on the status bar: its line in BASIC.HLP, the
; lines in item order, separators not counted.
; ::::::::::::::
mn_help:
        pushad
        movzx   edi,[g_ui.mn.mn_m]
        add     edi,edi
        xor     ebx,ebx
        xor     dx,dx                   ; DX the line
@mnh_m:
        mov     esi,[mn_it+ebx*2]
        xor     cl,cl                   ; CL the item in this menu
@mnh_i:
        cmp     ebx,edi
        jne     @mnh_a
        cmp     cl,al
        je      @mnh_f
@mnh_a:
        cmp     byte ptr [esi],0
        je      @mnh_n
        cmp     byte ptr [esi+1],MF_SEP
        je      @mnh_s
        inc     dx
@mnh_s:
        push    eax
        push    ecx
        call    mn_skip
        pop     ecx
        pop     eax
        inc     cl
        jmp     @mnh_i
@mnh_n:
        add     bx,2
        jmp     @mnh_m
@mnh_f:
        movzx   ecx,word ptr [g_ui.tx.hl_len]
        mov     edi,offset g_ui.bf.ui_hbuf
        mov     ebx,76
        mov     esi,offset g_hlpb
@mnh_l:
        test    dx,dx
        jz      @mnh_c
        jcxz    @mnh_e
        lodsb
        dec     cx
        cmp     al,10
        jne     @mnh_l
        dec     dx
        jmp     @mnh_l
@mnh_c:
        jcxz    @mnh_e
        lodsb
        dec     cx
        cmp     al,13
        je      @mnh_e
        cmp     al,10
        je      @mnh_e
        stosb
        dec     bx
        jnz     @mnh_c
@mnh_e:
        mov     byte ptr [edi],0
        mov     [g_ui.lp.ui_help],offset g_ui.bf.ui_hbuf
        popad
        ret

; ::::::::::::::
; mn_off - whether item SI is gray: always, or for want of what it needs.
;
; out:    ZF  clear: gray
; keeps:  all
; ::::::::::::::
mn_off:
        push    eax
        mov     al,[esi+1]
        test    al,MF_GRAY
        jnz     @mno_r
        test    al,MF_SEL
        jz      @mno_c
        cmp     [g_ui.ed.sel_on],0
        je      @mno_x
@mno_c:
        test    al,MF_CLP
        jz      @mno_e
        cmp     [g_ui.tx.cb_len],0
        je      @mno_x
@mno_e:
        test    al,MF_ED
        jz      @mno_y
        cmp     [g_ui.lp.ui_foc],0
        jne     @mno_x
@mno_y:
        cmp     al,al
        pop     eax
        ret
@mno_x:
        or      al,1
@mno_r:
        pop     eax
        ret

; ::::::::::::::
; mn_item - item AL of menu mn_c.
;
; out:    SI  it
; ::::::::::::::
mn_item:
        movzx   ebx,[g_ui.mn.mn_m]
        add     bx,bx
        mov     esi,[mn_it+ebx*2]
        movzx   ecx,al
        jcxz    @mni_r
@mni_l:
        push    ecx
        call    mn_skip
        pop     ecx
        loop    @mni_l
@mni_r:
        ret

; ::::::::::::::
; mn_size - menu mn_m's items (mn_cnt) and its box's width (mn_w); no
; sub-menu open, mn_m the bar's mn_c.
; ::::::::::::::
mn_size:
        cmp     [g_ui.mn.mn_sub],0
        jne     @F
        mov     al,[g_ui.mn.mn_c]
        mov     [g_ui.mn.mn_m],al
@@:
        mov     al,0
        call    mn_item
        xor     dx,dx                   ; DL the count, DH the widest
@mnz_l:
        cmp     byte ptr [esi],0
        je      @mnz_e
        inc     dl
        cmp     byte ptr [esi+1],MF_SEP
        je      @mnz_s
        push    esi
        add     esi,2
        call    sc_slen
        movzx   ebx,cx
@mnz_t:
        lodsb
        test    al,al
        jnz     @mnz_t
        call    sc_slen
        jcxz    @mnz_w
        add     bx,cx
        add     bx,3
@mnz_w:
        mov     eax,[esp]
        test    byte ptr [eax+1],MF_SUB
        jz      @F
        add     bx,2
@@:
        cmp     bl,dh
        jbe     @mnz_p
        mov     dh,bl
@mnz_p:
        pop     esi
@mnz_s:
        call    mn_skip
        jmp     @mnz_l
@mnz_e:
        mov     [g_ui.mn.mn_cnt],dl
        add     dh,4
        mov     [g_ui.mn.mn_w],dh
        ret

; ::::::::::::::
; mn_drop - menu mn_c's box under its title; then as mn_box.
; mn_box - the box at mn_l, mn_top, item mn_i lit, its help on the status
; bar.
; ::::::::::::::
mn_drop:
        call    mn_size
        movzx   ebx,[g_ui.mn.mn_c]
        mov     dl,[mn_x+ebx]
        sub     dl,2
        mov     al,dl
        add     al,[g_ui.mn.mn_w]
        cmp     al,79
        jbe     @mnd_x
        mov     dl,79
        sub     dl,[g_ui.mn.mn_w]
@mnd_x:
        mov     [g_ui.mn.mn_l],dl
        mov     [g_ui.mn.mn_top],1
mn_box:
        mov     dl,[g_ui.mn.mn_l]
        mov     dh,[g_ui.mn.mn_top]
        mov     ch,[g_ui.mn.mn_cnt]
        add     ch,2
        mov     cl,[g_ui.mn.mn_w]
        mov     ah,A_BAR
        xor     esi,esi
        call    sc_box
        call    sc_shadow
        xor     al,al
@mnd_l:
        cmp     al,[g_ui.mn.mn_cnt]
        jae     @mnd_e
        push    eax
        mov     dh,al
        add     dh,[g_ui.mn.mn_top]
        inc     dh
        mov     dl,[g_ui.mn.mn_l]
        push    eax
        call    mn_item
        pop     eax
        cmp     byte ptr [esi+1],MF_SEP
        jne     @mnd_i
        mov     cl,[g_ui.mn.mn_w]
        mov     ah,A_BAR
        call    sc_sep
        jmp     @mnd_n
@mnd_i:
        mov     ah,A_BAR                ; AH the item's colour, BH its key's
        mov     bh,A_HOT
        call    mn_off
        jz      @mnd_c
        mov     ah,A_GRAY
        mov     bh,ah
@mnd_c:
        cmp     al,[g_ui.mn.mn_i]
        jne     @mnd_d
        mov     ah,A_MSEL
        mov     bh,A_MHOT
        call    mn_off
        jz      @mnd_h
        mov     bh,ah
@mnd_h:
        call    mn_help
@mnd_d:
        inc     dl
        movzx   ecx,[g_ui.mn.mn_w]
        sub     cx,2
        mov     al,' '
        push    edx
        call    sc_fill
        pop     edx
        test    byte ptr [esi+1],MF_CHK
        jz      @mnd_t
        call    mn_chk
        jz      @mnd_t
        mov     al,07h
        mov     ecx,1
        push    edx
        call    sc_fill
        pop     edx
@mnd_t:
        inc     dl
        push    esi
        add     esi,2
        call    sc_str
        push    ebx
        call    sc_slen
        pop     ebx
        jcxz    @mnd_a
        mov     dl,[g_ui.mn.mn_l]
        add     dl,[g_ui.mn.mn_w]
        sub     dl,2
        sub     dl,cl
        mov     bh,ah
        call    sc_str
@mnd_a:
        pop     esi
        test    byte ptr [esi+1],MF_SUB
        jz      @mnd_n
        mov     dl,[g_ui.mn.mn_l]
        add     dl,[g_ui.mn.mn_w]
        sub     dl,3
        mov     al,10h
        mov     ecx,1
        call    sc_fill
@mnd_n:
        pop     eax
        inc     al
        jmp     @mnd_l
@mnd_e:
        ret

; ::::::::::::::
; mn_move - mn_i stepped by AL (1 or -1) past separators.
; ::::::::::::::
mn_move:
        mov     [g_ui.mn.mn_d],al
@mnm_l:
        mov     al,[g_ui.mn.mn_i]
        add     al,[g_ui.mn.mn_d]
        jns     @mnm_a
        mov     al,[g_ui.mn.mn_cnt]
        dec     al
@mnm_a:
        cmp     al,[g_ui.mn.mn_cnt]
        jb      @mnm_b
        xor     al,al
@mnm_b:
        mov     [g_ui.mn.mn_i],al
        call    mn_item
        cmp     byte ptr [esi+1],MF_SEP
        je      @mnm_l
        ret

; ::::::::::::::
; mn_unalt - Alt still held from Alt+F: its arrows and letters as plain ones.
;
; in:     AX  a key
; out:    AX  it without Alt
; ::::::::::::::
mn_unalt:
        test    al,al
        jnz     @mnu_r
        push    ebx
        mov     ebx,offset mn_alta
@mnu_a:
        cmp     byte ptr [ebx],0
        je      @mnu_l
        cmp     ah,[ebx]
        je      @mnu_y
        add     ebx,3
        jmp     @mnu_a
@mnu_y:
        mov     ax,[ebx+1]
        xchg    al,ah
        jmp     @mnu_x
@mnu_l:
        cmp     ah,10h                  ; Alt+letter: its letter
        jb      @mnu_x
        cmp     ah,32h
        ja      @mnu_x
        movzx   ebx,ah
        mov     al,[mn_altl+ebx-10h]
        cmp     al,' '
        jne     @mnu_x
        xor     al,al
@mnu_x:
        pop     ebx
@mnu_r:
        ret

; Alt+key's scan code: the plain key's scan code and character
        .data
mn_alta db      98h,48h,0,0A0h,50h,0,9Bh,4Bh,0,9Dh,4Dh,0
        db      1Ch,1Ch,0Dh,0A6h,1Ch,0Dh,01h,01h,1Bh,0
mn_altl db      'QWERTYUIOP    ASDFGHJKL     ZXCVBNM'
        .code

; ::::::::::::::
; mn_swap - the box's level and mn_sv's exchanged.
;
; keeps:  all
; ::::::::::::::
mn_swap:
        push    eax
        mov     eax,dword ptr [g_ui.mn.mn_m]
        xchg    eax,dword ptr [g_ui.mn.mn_sv]
        mov     dword ptr [g_ui.mn.mn_m],eax
        mov     ax,word ptr [g_ui.mn.mn_l]
        xchg    ax,word ptr [g_ui.mn.mn_sv+4]
        mov     word ptr [g_ui.mn.mn_l],ax
        pop     eax
        ret

; ::::::::::::::
; mn_run - the menus, from menu AL: AH 0 the bar lit, 1 its menu open.
; ::::::::::::::
mn_run:
        push    eax
        call    ed_commit
        pop     eax
        mov     [g_ui.mn.mn_c],al
        mov     [g_ui.mn.mn_open],ah
        mov     [g_ui.mn.mn_on],1
        and     [g_ui.mn.mn_sub],0
        and     [g_ui.mn.mn_i],0
        call    mn_size
@mnr_l:
        mov     [g_ui.lp.ui_help],offset u_mbar
        call    dr_all
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_k
        cmp     [g_ui.mn.mn_sub],0
        jne     @F
        call    mn_drop
        jmp     @mnr_st
@@:
        call    mn_swap
        call    mn_drop
        call    mn_swap
        call    mn_box
@mnr_st:
        call    dr_status
@mnr_k:
        mov     ecx,2000h
        call    sc_shape
        call    ui_getkey
        cmp     ax,0FFFFh
        je      @mnr_x
        cmp     ax,MS_KEY
        je      @mnr_ms
        call    mn_unalt
        cmp     al,1Bh
        je      @mnr_es
        cmp     al,0Dh
        je      @mnr_e
        cmp     ah,4Bh
        je      @mnr_lf
        cmp     ah,4Dh
        je      @mnr_rt
        cmp     ah,48h
        je      @mnr_up
        cmp     ah,50h
        je      @mnr_dn
        cmp     ah,44h
        je      @mnr_x
        test    al,al
        jz      @mnr_l
        call    ui_upper
        mov     dl,al
        cmp     [g_ui.mn.mn_open],0
        jne     @mnr_ik
        call    mn_find                 ; a title's key opens it
        jc      @mnr_l
        mov     [g_ui.mn.mn_c],al
        jmp     @mnr_o
@mnr_ik:
        mov     [g_ui.mn.mn_q],0                 ; an item's key runs it
@mnr_q:
        mov     al,[g_ui.mn.mn_q]
        cmp     al,[g_ui.mn.mn_cnt]
        jae     @mnr_l
        call    mn_item
        cmp     byte ptr [esi+1],MF_SEP
        je      @mnr_qn
        push    esi
        add     esi,2
@mnr_qa:
        lodsb
        test    al,al
        jz      @mnr_qz
        cmp     al,'&'
        jne     @mnr_qa
        lodsb
@mnr_qz:
        pop     esi
        call    ui_upper
        cmp     al,dl
        jne     @mnr_qn
        call    mn_off                  ; gray: lit, a beep (COW menu.c)
        jz      @mnr_go
        mov     al,[g_ui.mn.mn_q]
        mov     [g_ui.mn.mn_i],al
@mnr_bp:
        mov     ax,0E07h
        int     10h
        jmp     @mnr_l
@mnr_qn:
        inc     [g_ui.mn.mn_q]
        jmp     @mnr_q
@mnr_es:
        cmp     [g_ui.mn.mn_sub],0
        je      @mnr_x
@mnr_bk:                                ; back to the parent
        call    mn_swap
        and     [g_ui.mn.mn_sub],0
        jmp     @mnr_l
@mnr_x:
        and     [g_ui.mn.mn_on],0
        and     [g_ui.lp.ui_help],0
        ret
@mnr_ms:                                ; the mouse: a title, an item
        mov     dx,[g_ui.ms.ms_pos]
        test    dh,dh
        jnz     @mnr_mi
        cmp     [g_ui.ms.ms_ev],MS_UP
        je      @mnr_l
        call    ms_title
        jc      @mnr_l
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_mt
        cmp     al,[g_ui.mn.mn_c]
        je      @mnr_l
@mnr_mt:
        mov     [g_ui.mn.mn_c],al
        jmp     @mnr_o
@mnr_mi:
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_mo
        mov     al,dh
        sub     al,[g_ui.mn.mn_top]
        jbe     @mnr_mo
        dec     al
        cmp     al,[g_ui.mn.mn_cnt]
        jae     @mnr_mo
        mov     ah,dl
        sub     ah,[g_ui.mn.mn_l]
        jbe     @mnr_mo
        inc     ah
        cmp     ah,[g_ui.mn.mn_w]
        jae     @mnr_mo
        push    eax
        call    mn_item
        pop     eax
        test    byte ptr [esi+1],MF_SEP
        jnz     @mnr_l
        mov     [g_ui.mn.mn_i],al
        cmp     [g_ui.ms.ms_ev],MS_UP
        jne     @mnr_l
        call    mn_off
        jnz     @mnr_l
        jmp     @mnr_go
@mnr_mo:
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @mnr_l
        cmp     [g_ui.mn.mn_sub],0
        je      @mnr_x
        call    mn_swap                 ; off the sub-menu: the parent's click
        and     [g_ui.mn.mn_sub],0
        jmp     @mnr_ms
@mnr_e:
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_o
        mov     al,[g_ui.mn.mn_i]
        call    mn_item
        call    mn_off
        jnz     @mnr_bp
@mnr_go:
        test    byte ptr [esi+1],MF_SUB
        jnz     @mnr_so
        and     [g_ui.mn.mn_on],0
        and     [g_ui.lp.ui_help],0
        mov     al,[esi]
        jmp     ui_act
@mnr_so:                                ; a sub-menu: its first item by its parent's,
        mov     al,[esi]                ; right of the box, else left
        call    mn_swap
        mov     [g_ui.mn.mn_m],al
        mov     [g_ui.mn.mn_sub],1
        and     [g_ui.mn.mn_i],0
        call    mn_size
        mov     al,[g_ui.mn.mn_sv+1]
        add     al,[g_ui.mn.mn_sv+5]
        mov     [g_ui.mn.mn_top],al
        mov     al,[g_ui.mn.mn_sv+4]
        mov     ah,al
        add     al,[g_ui.mn.mn_sv+3]
        mov     dl,al
        add     dl,[g_ui.mn.mn_w]
        cmp     dl,79
        jbe     @F
        mov     al,ah
        sub     al,[g_ui.mn.mn_w]
@@:
        mov     [g_ui.mn.mn_l],al
        jmp     @mnr_l
@mnr_o:
        mov     [g_ui.mn.mn_open],1
        and     [g_ui.mn.mn_sub],0
        and     [g_ui.mn.mn_i],0
        call    mn_size
        jmp     @mnr_l
@mnr_lf:
        cmp     [g_ui.mn.mn_sub],0
        jne     @mnr_bk
        mov     al,[g_ui.mn.mn_c]
        dec     al
        jns     @mnr_m
        mov     al,NMENU-1
        jmp     @mnr_m
@mnr_rt:
        cmp     [g_ui.mn.mn_sub],0
        jne     @F
        cmp     [g_ui.mn.mn_open],0
        je      @F
        mov     al,[g_ui.mn.mn_i]
        call    mn_item
        test    byte ptr [esi+1],MF_SUB
        jz      @F
        call    mn_off
        jz      @mnr_so
@@:
        mov     al,[g_ui.mn.mn_c]
        inc     al
        cmp     al,NMENU
        jb      @mnr_m
        xor     al,al
@mnr_m:
        mov     [g_ui.mn.mn_c],al
        and     [g_ui.mn.mn_sub],0
        and     [g_ui.mn.mn_i],0
        call    mn_size
        jmp     @mnr_l
@mnr_up:
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_o
        mov     al,-1
        call    mn_move
        jmp     @mnr_l
@mnr_dn:
        cmp     [g_ui.mn.mn_open],0
        je      @mnr_o
        mov     al,1
        call    mn_move
        jmp     @mnr_l

; ::::::::::::::
; mn_find - the menu whose title's key is AL.
;
; out:    AL  it
;         CF  set: none
; ::::::::::::::
mn_find:
        xor     ebx,ebx
@mnf_l:
        mov     esi,[mn_tt+ebx*2]
@mnf_c:
        lodsb
        test    al,al
        jz      @mnf_n
        cmp     al,'&'
        jne     @mnf_c
        mov     ah,[esi]
        cmp     ah,dl
        je      @mnf_y
@mnf_n:
        add     bx,2
        cmp     bx,NMENU*2
        jb      @mnf_l
        stc
        ret
@mnf_y:
        mov     ax,bx
        shr     al,1
        clc
        ret

; ::::::::::::::
; mn_chk - check item ESI on: its flag's byte, by mn_flags, nonzero or
; the value given.
;
; out:    ZF  clear: on
; keeps:  all
; ::::::::::::::
mn_chk:
        push    eax
        push    ebx
        mov     ebx,offset mn_flags
@mck_l:
        mov     al,[ebx]
        test    al,al
        jz      @mck_r                  ; none: off, ZF set
        cmp     al,[esi]
        je      @F
        add     ebx,6
        jmp     @mck_l
@@:
        mov     eax,[ebx+1]
        mov     al,[eax]
        mov     ah,[ebx+5]
        cmp     ah,0FFh
        je      @mck_t
        cmp     al,ah
        sete    al
@mck_t:
        test    al,al
@mck_r:
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; ui_upper - AL in capitals.
; ::::::::::::::
ui_upper:
        cmp     al,'a'
        jb      @uup_r
        cmp     al,'z'
        ja      @uup_r
        sub     al,20h
@uup_r:
        ret

; ::::::::::::::
; mn_alt - the menu whose Alt+letter scan code is AH, open.
;
; out:    CF  set: not a menu's key
; ::::::::::::::
mn_alt:
        mov     ebx,offset mn_scan
@mna_l:
        cmp     ah,[ebx]
        je      @mna_y
        inc     ebx
        cmp     ebx,offset mn_scan+NMENU
        jb      @mna_l
        stc
        ret
@mna_y:
        sub     ebx,offset mn_scan
        mov     al,bl
        mov     ah,1
        call    mn_run
        clc
        ret


        .data
; a check item's action, its flag's byte, the value that is on (FFh any but 0)
mn_flags db     A_TRACE
        dd      offset g_state.dbg.trace
        db      0FFh, A_SYNTAX
        dd      offset g_ui.tx.syn_on
        db      0FFh, A_THQB
        dd      offset g_thn
        db      0, A_THDK
        dd      offset g_thn
        db      1
mf_th = 2
        rept    NTHEME-2
        db      A_TH2+mf_th-2
        dd      offset g_thn
        db      mf_th
mf_th = mf_th+1
        endm
        db      A_SYNHL
        dd      offset g_hlon
        db      0FFh, 0
mn_scan db      21h,12h,2Fh,1Fh,13h,20h,18h,23h ; Alt+F E V S R D O H
mn_x    db      3,9,15,21,29,34,41,74   ; QBasic's (uirsrcc.c)
mn_tt   dd      t_file,t_edit,t_view,t_srch,t_run,t_dbg,t_opt,t_help
mn_it   dd      m_file,m_edit,m_view,m_srch,m_run,m_dbg,m_opt,m_help
        dd      m_theme                 ; the sub-menus, from NMENU
t_file  db      '&File',0
t_edit  db      '&Edit',0
t_view  db      '&View',0
t_srch  db      '&Search',0
t_run   db      '&Run',0
t_dbg   db      '&Debug',0
t_opt   db      '&Options',0
t_help  db      '&Help',0

m_file  db      A_NEW,0,'&New',0,0
        db      A_OPEN,0,'&Open...',0,0
        db      A_SAVE,0,'&Save',0,0
        db      A_SAVEAS,0,'Save &As...',0,0
        db      A_NONE,MF_SEP
        db      A_NONE,MF_GRAY,'&Print...',0,0
        db      A_NONE,MF_SEP
        db      A_EXIT,0,'E&xit',0,0
        db      0
m_edit  db      A_CUT,MF_SEL,'Cu&t',0,'Shift+Del',0
        db      A_COPY,MF_SEL,'&Copy',0,'Ctrl+Ins',0
        db      A_PASTE,MF_CLP,'&Paste',0,'Shift+Ins',0
        db      A_CLEAR,MF_SEL,'Cl&ear',0,'Del',0
        db      A_NONE,MF_SEP
        db      A_NSUB,0,'New &SUB...',0,0
        db      A_NFN,0,'New &FUNCTION...',0,0
        db      0
m_view  db      A_SUBS,0,'&SUBs...',0,'F2',0
        db      A_SPLIT,0,'S&plit',0,0
        db      A_VSPLIT,0,'Split &Vertically',0,0
        db      A_OUT,0,'Output &Screen',0,'F4',0
        db      0
m_srch  db      A_FIND,MF_ED,'&Find...',0,0
        db      A_FIND3,0,'&Repeat Last Find',0,'F3',0
        db      A_CHANGE,MF_ED,'&Change...',0,0
        db      0
m_run   db      A_START,0,'&Start',0,'Shift+F5',0
        db      A_RESTART,0,'&Restart',0,0
        db      A_CONT,0,'Co&ntinue',0,'F5',0
        db      0
m_dbg   db      A_STEP,0,'&Step',0,'F8',0
        db      A_PSTEP,0,'&Procedure Step',0,'F10',0
        db      A_NONE,MF_SEP
        db      A_TRACE,MF_CHK,'&Trace On',0,0
        db      A_NONE,MF_SEP
        db      A_BP,MF_ED,'Toggle &Breakpoint',0,'F9',0
        db      A_BPCLR,0,'&Clear All Breakpoints',0,0
        db      A_SETNX,MF_ED,'Set &Next Statement',0,0
        db      0
m_opt   db      NMENU,MF_SUB,'&Theme',0,0
        db      A_SYNHL,MF_CHK,'Syntax &Highlighting',0,0
        db      A_NONE,MF_SEP
        db      A_NONE,MF_GRAY,'Help &Path...',0,0
        db      A_SYNTAX,MF_CHK,'&Syntax Checking',0,0
        db      0
m_theme db      A_THQB,MF_CHK,'&QBasic Colors',0,0
        db      A_THDK,MF_CHK,'One &Dark',0,0
        db      A_TH2,MF_CHK,'One &Light',0,0
        db      A_TH2+1,MF_CHK,'&Ayu Mirage',0,0
        db      A_TH2+2,MF_CHK,'Ay&u Light',0,0
        db      A_TH2+3,MF_CHK,'&Gruvbox Dark Soft',0,0
        db      A_TH2+4,MF_CHK,'Gruv&box Light',0,0
        db      A_TH2+5,MF_CHK,'&JetBrains Dark',0,0
        db      A_TH2+6,MF_CHK,'JetBrains Ligh&t',0,0
        db      0
m_help  db      A_NONE,MF_GRAY,'&Index',0,0
        db      A_NONE,MF_GRAY,'&Contents',0,0
        db      A_NONE,MF_GRAY,'&Topic:',0,'F1',0
        db      A_NONE,MF_GRAY,'&Using Help',0,'Shift+F1',0
        db      A_NONE,MF_SEP
        db      A_ABOUT,0,'&About...',0,0
        db      0
        .code

        end
