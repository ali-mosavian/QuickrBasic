; ==========================================================================
; menu.asm - the menu bar, its menus, their commands
; ==========================================================================
; holds:    mn_bar, mn_run, mn_alt, mn_drop, mn_item, the menu tables
; uses:     sc_*, dr_all, ui_getkey, ui_act
; data:     A menu: its title, its column, its items. An item: the command
;           (A_*), flags (MF_*), its text with '&' before the hotkey, its
;           key's name, its help line, each 0-ended; MF_SEP alone is a
;           separator; a 0 command ends the menu.
; calls:
;   mn_run             the bar lit, a menu open, until a command or Esc
;     mn_drop          the open menu's box
; ==========================================================================

MF_GRAY = 1
MF_SEP  = 2
MF_CHK  = 4                     ; a mark when its flag (mn_chk) is on
MF_SEL  = 8                     ; gray without a selection
MF_CLP  = 16                    ; gray with the Clipboard empty
MF_ED   = 32                    ; gray in the immediate window
NMENU   = 8
A_MSEL  = 07h
A_MHOT  = 0Fh
A_GRAY  = 78h

; ::::::::::::::
; mn_bar - the menu bar; the open menu's title lit (mn_on).
; ::::::::::::::
mn_bar:
        xor     dx,dx
        mov     ax,A_BAR*256+' '
        mov     cx,80
        call    sc_fill
        xor     bx,bx
@mnb_l:
        push    bx
        mov     si,[mn_tt+bx]
        shr     bx,1
        mov     dl,[mn_x+bx]
        xor     dh,dh
        mov     ah,A_BAR
        mov     bh,A_HOT
        cmp     [mn_on],0
        je      @mnb_s
        cmp     bl,[mn_c]
        jne     @mnb_s
        dec     dl
        mov     ax,A_MSEL*256+' '
        mov     cx,1
        call    sc_fill
        mov     bh,A_MHOT
        call    sc_str
        mov     al,' '
        call    sc_fill
        jmp     @mnb_n
@mnb_s:
        call    sc_str
@mnb_n:
        pop     bx
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
        mov     cx,2
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
        pusha
        push    es
        push    ds
        pop     es
        movzx   di,[mn_c]
        add     di,di
        xor     bx,bx
        xor     dx,dx                   ; DX the line
@mnh_m:
        mov     si,[mn_it+bx]
        xor     cl,cl                   ; CL the item in this menu
@mnh_i:
        cmp     bx,di
        jne     @mnh_a
        cmp     cl,al
        je      @mnh_f
@mnh_a:
        cmp     byte ptr [si],0
        je      @mnh_n
        cmp     byte ptr [si+1],MF_SEP
        je      @mnh_s
        inc     dx
@mnh_s:
        push    ax
        push    cx
        call    mn_skip
        pop     cx
        pop     ax
        inc     cl
        jmp     @mnh_i
@mnh_n:
        add     bx,2
        jmp     @mnh_m
@mnh_f:
        mov     cx,[hl_len]
        mov     di,offset ui_hbuf
        mov     bx,76
        mov     ax,cs
        add     ax,HLPSEG
        push    ds
        mov     ds,ax
        xor     si,si
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
        pop     ds
        mov     byte ptr es:[di],0
        mov     [ui_help],offset ui_hbuf
        pop     es
        popa
        ret

; ::::::::::::::
; mn_off - whether item SI is gray: always, or for want of what it needs.
;
; out:    ZF  clear: gray
; keeps:  all
; ::::::::::::::
mn_off:
        push    ax
        mov     al,[si+1]
        test    al,MF_GRAY
        jnz     @mno_r
        test    al,MF_SEL
        jz      @mno_c
        cmp     [sel_on],0
        je      @mno_x
@mno_c:
        test    al,MF_CLP
        jz      @mno_e
        cmp     [cb_len],0
        je      @mno_x
@mno_e:
        test    al,MF_ED
        jz      @mno_y
        cmp     [ui_foc],0
        jne     @mno_x
@mno_y:
        cmp     al,al
        pop     ax
        ret
@mno_x:
        or      al,1
@mno_r:
        pop     ax
        ret

; ::::::::::::::
; mn_item - item AL of menu mn_c.
;
; out:    SI  it
; ::::::::::::::
mn_item:
        movzx   bx,[mn_c]
        add     bx,bx
        mov     si,[mn_it+bx]
        movzx   cx,al
        jcxz    @mni_r
@mni_l:
        push    cx
        call    mn_skip
        pop     cx
        loop    @mni_l
@mni_r:
        ret

; ::::::::::::::
; mn_size - menu mn_c's items (mn_cnt) and its box's width (mn_w).
; ::::::::::::::
mn_size:
        mov     al,0
        call    mn_item
        xor     dx,dx                   ; DL the count, DH the widest
@mnz_l:
        cmp     byte ptr [si],0
        je      @mnz_e
        inc     dl
        cmp     byte ptr [si+1],MF_SEP
        je      @mnz_s
        push    si
        add     si,2
        call    sc_slen
        mov     bx,cx
@mnz_t:
        lodsb
        test    al,al
        jnz     @mnz_t
        call    sc_slen
        jcxz    @mnz_w
        add     bx,cx
        add     bx,3
@mnz_w:
        cmp     bl,dh
        jbe     @mnz_p
        mov     dh,bl
@mnz_p:
        pop     si
@mnz_s:
        call    mn_skip
        jmp     @mnz_l
@mnz_e:
        mov     [mn_cnt],dl
        add     dh,4
        mov     [mn_w],dh
        ret

; ::::::::::::::
; mn_drop - menu mn_c's box under its title, item mn_i lit, its help on the
; status bar.
; ::::::::::::::
mn_drop:
        call    mn_size
        movzx   bx,[mn_c]
        mov     dl,[mn_x+bx]
        sub     dl,2
        mov     al,dl
        add     al,[mn_w]
        cmp     al,79
        jbe     @mnd_x
        mov     dl,79
        sub     dl,[mn_w]
@mnd_x:
        mov     [mn_l],dl
        mov     dh,1
        mov     ch,[mn_cnt]
        add     ch,2
        mov     cl,[mn_w]
        mov     ah,A_BAR
        xor     si,si
        call    sc_box
        call    sc_shadow
        xor     al,al
@mnd_l:
        cmp     al,[mn_cnt]
        jae     @mnd_e
        push    ax
        mov     dh,al
        add     dh,2
        mov     dl,[mn_l]
        push    ax
        call    mn_item
        pop     ax
        cmp     byte ptr [si+1],MF_SEP
        jne     @mnd_i
        mov     cl,[mn_w]
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
        cmp     al,[mn_i]
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
        movzx   cx,[mn_w]
        sub     cx,2
        mov     al,' '
        push    dx
        call    sc_fill
        pop     dx
        test    byte ptr [si+1],MF_CHK
        jz      @mnd_t
        cmp     [syn_on],0
        je      @mnd_t
        mov     al,07h
        mov     cx,1
        push    dx
        call    sc_fill
        pop     dx
@mnd_t:
        inc     dl
        add     si,2
        call    sc_str
        push    bx
        call    sc_slen
        pop     bx
        jcxz    @mnd_n
        mov     dl,[mn_l]
        add     dl,[mn_w]
        sub     dl,2
        sub     dl,cl
        mov     bh,ah
        call    sc_str
@mnd_n:
        pop     ax
        inc     al
        jmp     @mnd_l
@mnd_e:
        ret

; ::::::::::::::
; mn_move - mn_i stepped by AL (1 or -1) past separators.
; ::::::::::::::
mn_move:
        mov     [mn_d],al
@mnm_l:
        mov     al,[mn_i]
        add     al,[mn_d]
        jns     @mnm_a
        mov     al,[mn_cnt]
        dec     al
@mnm_a:
        cmp     al,[mn_cnt]
        jb      @mnm_b
        xor     al,al
@mnm_b:
        mov     [mn_i],al
        call    mn_item
        cmp     byte ptr [si+1],MF_SEP
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
        push    bx
        mov     bx,offset mn_alta
@mnu_a:
        cmp     byte ptr [bx],0
        je      @mnu_l
        cmp     ah,[bx]
        je      @mnu_y
        add     bx,3
        jmp     @mnu_a
@mnu_y:
        mov     ax,[bx+1]
        xchg    al,ah
        jmp     @mnu_x
@mnu_l:
        cmp     ah,10h                  ; Alt+letter: its letter
        jb      @mnu_x
        cmp     ah,32h
        ja      @mnu_x
        movzx   bx,ah
        mov     al,[mn_altl+bx-10h]
        cmp     al,' '
        jne     @mnu_x
        xor     al,al
@mnu_x:
        pop     bx
@mnu_r:
        ret

; Alt+key's scan code: the plain key's scan code and character
DSEG    segment
mn_alta db      98h,48h,0,0A0h,50h,0,9Bh,4Bh,0,9Dh,4Dh,0
        db      1Ch,1Ch,0Dh,0A6h,1Ch,0Dh,01h,01h,1Bh,0
mn_altl db      'QWERTYUIOP    ASDFGHJKL     ZXCVBNM'
DSEG    ends

; ::::::::::::::
; mn_run - the menus, from menu AL: AH 0 the bar lit, 1 its menu open.
; ::::::::::::::
mn_run:
        push    ax
        call    ed_commit
        pop     ax
        mov     [mn_c],al
        mov     [mn_open],ah
        mov     [mn_on],1
        and     [mn_i],0
        call    mn_size
@mnr_l:
        mov     [ui_help],offset u_mbar
        call    dr_all
        cmp     [mn_open],0
        je      @mnr_k
        call    mn_drop
        call    dr_status
@mnr_k:
        mov     cx,2000h
        call    sc_shape
        call    ui_getkey
        cmp     ax,0FFFFh
        je      @mnr_x
        cmp     ax,MS_KEY
        je      @mnr_ms
        call    mn_unalt
        cmp     al,1Bh
        je      @mnr_x
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
        cmp     [mn_open],0
        jne     @mnr_ik
        call    mn_find                 ; a title's key opens it
        jc      @mnr_l
        mov     [mn_c],al
        jmp     @mnr_o
@mnr_ik:
        mov     [mn_q],0                ; an item's key runs it
@mnr_q:
        mov     al,[mn_q]
        cmp     al,[mn_cnt]
        jae     @mnr_l
        call    mn_item
        cmp     byte ptr [si+1],MF_SEP
        je      @mnr_qn
        push    si
        add     si,2
@mnr_qa:
        lodsb
        test    al,al
        jz      @mnr_qz
        cmp     al,'&'
        jne     @mnr_qa
        lodsb
@mnr_qz:
        pop     si
        call    ui_upper
        cmp     al,dl
        jne     @mnr_qn
        call    mn_off                  ; gray: lit, a beep (COW menu.c)
        jz      @mnr_go
        mov     al,[mn_q]
        mov     [mn_i],al
@mnr_bp:
        mov     ax,0E07h
        int     10h
        jmp     @mnr_l
@mnr_qn:
        inc     [mn_q]
        jmp     @mnr_q
@mnr_x:
        and     [mn_on],0
        and     [ui_help],0
        ret
@mnr_ms:                                ; the mouse: a title, an item
        mov     dx,[ms_pos]
        test    dh,dh
        jnz     @mnr_mi
        cmp     [ms_ev],MS_UP
        je      @mnr_l
        call    ms_title
        jc      @mnr_l
        cmp     [mn_open],0
        je      @mnr_mt
        cmp     al,[mn_c]
        je      @mnr_l
@mnr_mt:
        mov     [mn_c],al
        jmp     @mnr_o
@mnr_mi:
        cmp     [mn_open],0
        je      @mnr_mo
        mov     al,dh
        sub     al,2
        jb      @mnr_mo
        cmp     al,[mn_cnt]
        jae     @mnr_mo
        mov     ah,dl
        sub     ah,[mn_l]
        jbe     @mnr_mo
        inc     ah
        cmp     ah,[mn_w]
        jae     @mnr_mo
        push    ax
        call    mn_item
        pop     ax
        test    byte ptr [si+1],MF_SEP
        jnz     @mnr_l
        mov     [mn_i],al
        cmp     [ms_ev],MS_UP
        jne     @mnr_l
        call    mn_off
        jnz     @mnr_l
        jmp     @mnr_go
@mnr_mo:
        cmp     [ms_ev],MS_PRESS
        jne     @mnr_l
        jmp     @mnr_x
@mnr_e:
        cmp     [mn_open],0
        je      @mnr_o
        mov     al,[mn_i]
        call    mn_item
        call    mn_off
        jnz     @mnr_bp
@mnr_go:
        and     [mn_on],0
        and     [ui_help],0
        mov     al,[si]
        jmp     ui_act
@mnr_o:
        mov     [mn_open],1
        and     [mn_i],0
        call    mn_size
        jmp     @mnr_l
@mnr_lf:
        mov     al,[mn_c]
        dec     al
        jns     @mnr_m
        mov     al,NMENU-1
        jmp     @mnr_m
@mnr_rt:
        mov     al,[mn_c]
        inc     al
        cmp     al,NMENU
        jb      @mnr_m
        xor     al,al
@mnr_m:
        mov     [mn_c],al
        and     [mn_i],0
        call    mn_size
        jmp     @mnr_l
@mnr_up:
        cmp     [mn_open],0
        je      @mnr_o
        mov     al,-1
        call    mn_move
        jmp     @mnr_l
@mnr_dn:
        cmp     [mn_open],0
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
        xor     bx,bx
@mnf_l:
        mov     si,[mn_tt+bx]
@mnf_c:
        lodsb
        test    al,al
        jz      @mnf_n
        cmp     al,'&'
        jne     @mnf_c
        mov     ah,[si]
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
        mov     bx,offset mn_scan
@mna_l:
        cmp     ah,[bx]
        je      @mna_y
        inc     bx
        cmp     bx,offset mn_scan+NMENU
        jb      @mna_l
        stc
        ret
@mna_y:
        sub     bx,offset mn_scan
        mov     al,bl
        mov     ah,1
        call    mn_run
        clc
        ret

A_NEW   = 1
A_OPEN  = 2
A_SAVE  = 3
A_SAVEAS = 4
A_EXIT  = 5
A_CUT   = 6
A_COPY  = 7
A_PASTE = 8
A_CLEAR = 9
A_OUT   = 10
A_FIND  = 11
A_FIND3 = 12
A_CHANGE = 13
A_START = 14
A_CONT  = 15
A_SYNTAX = 16
A_ABOUT = 17
A_NONE  = 18

DSEG    segment
mn_scan db      21h,12h,2Fh,1Fh,13h,20h,18h,23h ; Alt+F E V S R D O H
mn_x    db      3,9,15,21,29,34,41,74   ; QBasic's (uirsrcc.c)
mn_tt   dw      t_file,t_edit,t_view,t_srch,t_run,t_dbg,t_opt,t_help
mn_it   dw      m_file,m_edit,m_view,m_srch,m_run,m_dbg,m_opt,m_help
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
        db      A_NONE,MF_GRAY,'New &SUB...',0,0
        db      A_NONE,MF_GRAY,'New &FUNCTION...',0,0
        db      0
m_view  db      A_NONE,MF_GRAY,'&SUBs...',0,'F2',0
        db      A_NONE,MF_GRAY,'S&plit',0,0
        db      A_OUT,0,'Output &Screen',0,'F4',0
        db      0
m_srch  db      A_FIND,MF_ED,'&Find...',0,0
        db      A_FIND3,0,'&Repeat Last Find',0,'F3',0
        db      A_CHANGE,MF_ED,'&Change...',0,0
        db      0
m_run   db      A_START,0,'&Start',0,'Shift+F5',0
        db      A_NONE,MF_GRAY,'&Restart',0,0
        db      A_CONT,0,'Co&ntinue',0,'F5',0
        db      0
m_dbg   db      A_NONE,MF_GRAY,'&Step',0,'F8',0
        db      A_NONE,MF_GRAY,'&Procedure Step',0,'F10',0
        db      A_NONE,MF_SEP
        db      A_NONE,MF_GRAY,'&Trace On',0,0
        db      A_NONE,MF_SEP
        db      A_NONE,MF_GRAY,'Toggle &Breakpoint',0,'F9',0
        db      A_NONE,MF_GRAY,'&Clear All Breakpoints',0,0
        db      A_NONE,MF_GRAY,'Set &Next Statement',0,0
        db      0
m_opt   db      A_NONE,MF_GRAY,'&Display...',0,0
        db      A_NONE,MF_GRAY,'Help &Path...',0,0
        db      A_SYNTAX,MF_CHK,'&Syntax Checking',0,0
        db      0
m_help  db      A_NONE,MF_GRAY,'&Index',0,0
        db      A_NONE,MF_GRAY,'&Contents',0,0
        db      A_NONE,MF_GRAY,'&Topic:',0,'F1',0
        db      A_NONE,MF_GRAY,'&Using Help',0,'Shift+F1',0
        db      A_NONE,MF_SEP
        db      A_ABOUT,0,'&About...',0,0
        db      0
DSEG    ends
