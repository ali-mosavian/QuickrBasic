; ==========================================================================
; ui.asm - the editor: start, the key loop, the commands, running
; ==========================================================================
; holds:    ui_start, ui_loop, ui_key, ui_act, ui_ready, ui_err, ui_getkey
; uses:     every ui/ module; rpl_enter, rpl_run, sy_build
; data:     Page 0 the editor, page 1 the program's output. The text is
;           edited free-form; at a run it becomes the program (sync.asm).
;           An error, BREAK or STOP comes back to the editor at its line.
;           Only when standard input and output are both the console.
; calls:
;   ui_start           the console? else the READY loop; a file named on
;                      the command line, else the welcome box
;   ui_loop            dr_all, a key, ui_key
;     ui_key           the menus, the windows' keys
;   ui_ready           the program ended: its page until a key
;   ui_err             an error: the editor at its line, a box
; ==========================================================================

include x86f.inc

        .code


; ::::::::::::::
; ui_start - the editor when both standard handles are the console.
; ::::::::::::::
ui_start:
        assume  fs:nothing
        cmp     [g_state.io.isdev],0
        je      rpl_ready
        mov     ax,4400h
        mov     ebx,1
        int     21h
        test    dl,80h
        jz      rpl_ready
        mov     [g_ui.lp.ideon],1
        mov     [g_ui.dg.ls_f+TLIST.items],offset g_ui.bf.ls_fl
        mov     [g_ui.dg.ls_f+TLIST.isz],13
        mov     [g_ui.dg.ls_d+TLIST.items],offset g_ui.bf.ls_dl
        mov     [g_ui.dg.ls_d+TLIST.isz],13
        mov     byte ptr [g_ui.bf.ui_fn],0    ; out of the zeroed data
        mov     [g_ui.im.im_n],1
        mov     [g_ui.tx.syn_on],1
        mov     [g_ui.ed.ed_ins],1
        mov     [g_ui.lp.ui_wrap],1
        call    ud_init
        call    wn_init
        call    ms_init
        call    hl_load
        call    tx_init
        call    vw_reset
        xor     ax,ax
        call    ed_load
        call    im_load
        mov     ax,3
        int     10h
        call    th_qb
        mov     fs,[g_plat.pspsel]              ; a file named on the command line,
        mov     esi,81h                 ; in the PSP
        movzx   ecx,byte ptr fs:[80h]
@ust_b:
        jcxz    @ust_w
        cmp     byte ptr fs:[esi],' '
        jne     @ust_n
        inc     esi
        dec     cx
        jmp     @ust_b
@ust_n:
        mov     edi,offset g_ui.bf.dl_fn.txt
        xor     dx,dx
@ust_c:
        lods    byte ptr fs:[esi]
        cmp     al,' '
        jbe     @ust_e
        stosb
        inc     dx
        cmp     dx,FLDMAX
        jae     @ust_e
        loop    @ust_c
@ust_e:
        mov     [g_ui.bf.dl_fn.len],dx
        call    dl_fok
        mov     [g_ui.lp.ul_new],1
        call    ui_load
        and     [g_ui.lp.ul_new],0
        jmp     ui_loop
@ust_w:
        mov     esi,offset u_welcome
        mov     ebx,offset bt_wel
        call    dl_msg

; ::::::::::::::
; ui_loop - draw, a key, its command; again.
; ::::::::::::::
ui_loop:
if AC eq 0
        and     [g_ui.lp.ui_help],0              ; else ui_key: a completion shows it
endif
        call    dr_all
        call    ui_getkey
        call    ui_key
        jmp     ui_loop

; ::::::::::::::
; ui_getkey - a key, the screen shown first; Alt pressed and let go alone
; is FFFFh, a mouse event MS_KEY. An Alt already down at the call, as
; after Alt+F, is not a tap when it is let go.
;
; out:    AX  the key: AL its character, 0 or E0h for none; AH its scan
;         ui_kf  the shift keys' flags then
; ::::::::::::::
ui_getkey:
        call    sc_flip
        call    ms_show
        mov     ah,2
        int     16h
        and     al,8
        shr     al,2                    ; 2: the Alt held from before
        mov     [g_ui.lp.alt_t],al
@ugk_w:
        mov     ah,11h
        int     16h
        jnz     @ugk_k
        call    ms_poll
        mov     ax,MS_KEY
        jc      @ugk_f
        mov     ah,2
        int     16h
        test    al,8
        jz      @ugk_n
        cmp     [g_ui.lp.alt_t],0
        jne     @ugk_w
        mov     [g_ui.lp.alt_t],1
        jmp     @ugk_w
@ugk_n:
        cmp     [g_ui.lp.alt_t],1
        mov     [g_ui.lp.alt_t],0
        jne     @ugk_w
        mov     ax,0FFFFh
        jmp     @ugk_f
@ugk_k:
        mov     ah,10h
        int     16h
        cmp     al,0E0h
        jne     @ugk_f
        test    ah,ah
        jz      @ugk_f
        xor     al,al
@ugk_f:
        push    eax
        mov     ah,2
        int     16h
        mov     [g_ui.lp.ui_kf],al
        pop     eax
        jmp     ms_hide

; ::::::::::::::
; ui_key - the key AX: the menus, the run keys, then the window's.
; ::::::::::::::
ui_key:
if AC
        mov     [g_ui.lp.ui_help],0              ; a completion's form, shown till now
endif
        cmp     ax,0FFFFh
        je      @uk_bar
        cmp     ax,MS_KEY
        je      ms_main
        cmp     ax,260Ch                ; Ctrl+L: Repeat Last Find
        je      ui_find3
        test    al,al
        jnz     @uk_w
        cmp     ah,41h
        je      ud_until
        cmp     ah,42h
        je      ud_step
        cmp     ah,43h
        je      ud_bp
        cmp     ah,44h
        je      ud_pstep
        cmp     ah,3Eh
        je      ui_view
        cmp     ah,3Fh
        je      ui_contp
        cmp     ah,58h
        je      ui_runp
        cmp     ah,40h
        je      ui_win
        cmp     ah,3Dh
        je      ui_find3
        cmp     ah,3Bh
        je      ui_nohelp
        cmp     ah,54h                  ; Shift+F1
        je      ui_nohelp
        cmp     ah,59h                  ; Shift+F6: the window before
        je      wn_prev
        cmp     ah,83h                  ; Alt+=, Alt+grey +: a row more
        je      wn_grow
        cmp     ah,4Eh
        je      wn_grow
        cmp     ah,82h                  ; Alt+-, Alt+grey -: a row less
        je      wn_shrink
        cmp     ah,4Ah
        je      wn_shrink
        cmp     ah,67h                  ; Ctrl+F10
        je      wn_max
        cmp     ah,3Ch                  ; F2, Shift+F2, Ctrl+F2: the views
        je      vw_subs
        cmp     ah,55h
        je      vw_next
        cmp     ah,5Fh
        je      vw_prev
        push    eax
        call    mn_alt
        pop     eax
        jnc     @uk_r
@uk_w:
        cmp     [g_ui.lp.ui_foc],0
        jne     im_key
        jmp     ed_key
@uk_bar:
        xor     ax,ax
        jmp     mn_run
@uk_r:
        ret

; ::::::::::::::
; ui_lin - the linear address of selector BX; not one, of real-mode segment
; BX (DOS/32A's PSP may hold either at 2Ch).
;
; out:    EAX  it
; keeps:  all but EAX
; ::::::::::::::
ui_lin:
        push    ecx
        push    edx
        mov     ax,0006h                ; DPMI: the selector's base
        int     31h
        jc      @uln_s
        shl     ecx,16
        mov     cx,dx
        mov     eax,ecx
        jmp     @uln_r
@uln_s:
        movzx   eax,bx
        shl     eax,4
@uln_r:
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; hl_load - BASIC.HLP, from the program's own directory, at hlpb.
; ::::::::::::::
hl_load:
        mov     fs,[g_plat.pspsel]
        movzx   ebx,word ptr fs:[2Ch]   ; the environment, the program's path
        test    ebx,ebx
        jz      @hll_r
        call    ui_lin
        mov     edi,eax
        xor     al,al
        mov     ecx,8000h
@hll_z:
        repne   scasb
        cmp     [edi],al
        jne     @hll_z
        add     edi,3                   ; past the 0 and the count
        mov     esi,offset g_ui.bf.dl_fn
        mov     ebx,esi                 ; EBX past the last '\'
@hll_c:
        mov     al,[edi]
        inc     edi
        mov     [esi],al
        inc     esi
        cmp     al,'\'
        jne     @hll_n
        mov     ebx,esi
@hll_n:
        test    al,al
        jnz     @hll_c
        mov     dword ptr [ebx],'ISAB'
        mov     dword ptr [ebx+4],'LH.C'
        mov     word ptr [ebx+8],'P'
        mov     edx,offset g_ui.bf.dl_fn
        mov     ax,3D00h
        int     21h
        jc      @hll_r
        movzx   ebx,ax
        mov     edx,offset g_hlpb
        mov     ecx,HLPSZ
        mov     ah,3Fh
        int     21h
        jc      @hll_x
        mov     [g_ui.tx.hl_len],ax
@hll_x:
        mov     ah,3Eh
        int     21h
@hll_r:
        ret

; ::::::::::::::
; ui_win - F6: the next window.
; ::::::::::::::
ui_win:
        jmp     wn_next

; ::::::::::::::
; ui_nohelp - F1.
; ::::::::::::::
ui_nohelp:
        mov     esi,offset u_nohelp
        jmp     dl_ok

; ::::::::::::::
; im_key - a key in the immediate window.
; ::::::::::::::
im_key:
        cmp     al,0Dh
        je      ui_exec
        test    al,al
        jnz     @imk_c
        cmp     ah,48h
        je      im_up
        cmp     ah,50h
        je      im_down
@imk_c:
        cmp     al,20h
        jb      @imk_e
        cmp     word ptr [g_ui.bf.im_lb.len],ED_COLS-1
        jae     @imk_r
@imk_e:
        mov     ebx,offset g_ui.bf.im_lb
        jmp     ui_ledit
@imk_r:
        ret

; ::::::::::::::
; ed_key - a key in the program window: a shifted move selects.
; ::::::::::::::
ed_key:
        mov     ebx,offset g_ui.bf.ed_lb
        test    al,al
        jnz     @edk_a
        movzx   ecx,ax
        mov     esi,offset ed_moves
@edk_m:
        lodsb
        test    al,al
        jz      @edk_x
        cmp     al,ch
        je      @edk_mv
        add     esi,4
        jmp     @edk_m
@edk_mv:
        test    [g_ui.lp.ui_kf],3
        jz      @edk_ns
        call    sel_mark
        jmp     @edk_go
@edk_ns:
        and     [g_ui.ed.sel_on],0
@edk_go:
        mov     ebx,offset g_ui.bf.ed_lb
        jmp     dword ptr [esi]
@edk_x:
        mov     ax,cx
        cmp     ah,53h
        je      @edk_del
        cmp     ah,52h
        je      @edk_ins
        cmp     ah,92h
        je      ed_copy
        ret
@edk_del:
        test    [g_ui.lp.ui_kf],3
        jnz     ed_cut
        call    sel_get
        test    al,al
        jnz     ed_clear
        and     [g_ui.ed.sel_on],0
        jmp     ed_del
@edk_ins:
        test    [g_ui.lp.ui_kf],3
        jnz     ed_paste
        xor     [g_ui.ed.ed_ins],1
        ret
@edk_a:
        cmp     [g_ui.ed.ed_pfx],0               ; WordStar keys (COW edittbls.inc)
        jne     @edk_q
        cmp     al,11h                  ; Ctrl+Q: a second key follows
        je      @edk_qp
        cmp     al,1Ah
        ja      @edk_a2
        movzx   esi,al
        mov     ah,[ed_ws+esi-1]
        test    ah,ah
        jz      @edk_a2
        xor     al,al
        jmp     ed_key
@edk_qp:
        mov     [g_ui.ed.ed_pfx],al
        ret
@edk_q:
        mov     [g_ui.ed.ed_pfx],0
        and     al,1Fh
        mov     esi,offset ed_wsq
@edk_ql:
        movzx   ecx,word ptr [esi]
        add     esi,2
        test    cl,cl
        jz      @edk_r
        cmp     cl,al
        jne     @edk_ql
        mov     ah,ch
        xor     al,al
        test    ah,ah                   ; 0: Ctrl+Q,Y, to the line's end out
        jnz     ed_key
        and     [g_ui.ed.sel_on],0
        mov     ebx,offset g_ui.bf.ed_lb
        movzx   ecx,word ptr [g_ui.bf.ed_lb.col]
        mov     dx,[g_ui.bf.ed_lb.len]
        jmp     le_cut
@edk_a2:
        cmp     al,0Dh
        je      @edk_ent
        cmp     al,08h
        je      @edk_bs
        cmp     al,09h
        je      @edk_tab
        cmp     al,19h
        je      @edk_y
        cmp     al,1Bh
        je      @edk_esc
        cmp     al,20h
        jb      @edk_r
        push    eax
        call    sel_get
        cmp     al,1
        jne     @edk_t
        call    ed_clear
@edk_t:
        and     [g_ui.ed.sel_on],0
        pop     eax
        mov     ebx,offset g_ui.bf.ed_lb
        mov     dx,LNMAX
        jmp     le_ins
@edk_ent:
        and     [g_ui.ed.sel_on],0
        jmp     ed_enter
@edk_bs:
        call    sel_get
        test    al,al
        jnz     ed_clear
        jmp     ed_bs
@edk_tab:
        and     [g_ui.ed.sel_on],0
if AC
        call    ac_tab
        jnc     @edk_r
endif
        jmp     ed_tab
@edk_y:
        and     [g_ui.ed.sel_on],0
        jmp     ed_ctrly
@edk_esc:
        and     [g_ui.ed.sel_on],0
@edk_r:
        ret

; Ctrl+A to Ctrl+Z as WordStar's: the scan code of the key they stand for
        .data
ed_ws   db      73h,0,51h,4Dh,48h,74h,53h,0,0,0,0,0,0,0,0,0
        db      0,49h,4Bh,0,0,52h,0,50h,0,0
; Ctrl+Q's second key: the letter's code, the scan code (0 erases)
ed_wsq  db      13h,47h,04h,4Fh,12h,77h,03h,75h,19h,0,0
        .code

; the keys that move the cursor: scan code, routine
        .data
ed_moves db     48h
        dd      ed_up
        db      50h
        dd      ed_down
        db      4Bh
        dd      le_left
        db      4Dh
        dd      le_right
        db      47h
        dd      le_home
        db      4Fh
        dd      le_end
        db      49h
        dd      ed_pgup
        db      51h
        dd      ed_pgdn
        db      77h
        dd      ed_first
        db      75h
        dd      ed_last
        db      73h
        dd      le_wleft
        db      74h
        dd      le_wright
        db      0
        .code

; ::::::::::::::
; ui_act - the menu command AL.
; ::::::::::::::
ui_act:
        movzx   ebx,al
        dec     bx
        cmp     bx,A_NONE-1
        jae     @ua_r
        add     bx,bx
        jmp     dword ptr [ui_acts+ebx*2]
@ua_r:
        ret
        .data
ui_acts dd      ui_new,ui_open,ui_save,ui_saveas,ui_exit,ed_cut,ed_copy
        dd      ed_paste,ed_clear,ui_view,ui_find,ui_find3,ui_change
        dd      ui_runp,ui_contp,ui_syntax,ui_about,ud_step,ud_pstep
        dd      ud_trace1,ud_bp,ud_bpclr,ud_setnx,ud_restart,wn_split
        dd      vw_subs,vw_newsub,vw_newfn,th_qb,th_dark,hl_toggle,wn_vsplit
        .code

; ::::::::::::::
; ui_syntax - Options, Syntax Checking: on, off.
; ::::::::::::::
ui_syntax:
        xor     [g_ui.tx.syn_on],1
        ret

; ::::::::::::::
; ui_about - Help, About.
; ::::::::::::::
ui_about:
        mov     esi,offset u_about
        jmp     dl_ok

; ::::::::::::::
; ui_asksave - a changed text: save it first? Yes saves, No goes on.
;
; out:    CF  set: cancelled
; ::::::::::::::
ui_asksave:
        call    ed_commit
        cmp     [g_ui.tx.tx_chg],0
        je      @uas_n
        mov     esi,offset u_notsaved
        call    dl_ync
        cmp     al,1
        je      @uas_n
        ja      @uas_c
        jmp     ui_save
@uas_n:
        clc
        ret
@uas_c:
        stc
        ret

; ::::::::::::::
; ui_reset - the editor at the text's start; the program gone.
; ::::::::::::::
ui_reset:
        xor     ax,ax
        mov     [g_ui.ed.ed_top],ax
        mov     [g_ui.ed.ed_left],ax
        mov     [g_ui.bf.ed_lb.col],ax
        mov     [g_ui.ed.sel_on],al
        mov     [g_ui.lp.ui_foc],al
        mov     [g_ui.tx.tx_chg],al
        mov     [g_ui.tx.tx_mod],1
        call    vw_reset
        call    ed_load
        jmp     prg_init

; ::::::::::::::
; ui_new - File, New.
; ::::::::::::::
ui_new:
        call    ui_asksave
        jc      @un_r
        call    tx_init
        mov     byte ptr [g_ui.bf.ui_fn],0
        jmp     ui_reset
@un_r:
        ret

; ::::::::::::::
; ui_open - File, Open.
; ::::::::::::::
ui_open:
        call    ui_asksave
        jc      @un_r
        call    dl_open
        jc      @un_r

; ::::::::::::::
; ui_load - the file dl_name into the text; not found: a new one of
; that name, from the command line, else a box.
; ::::::::::::::
ui_load:
        mov     edx,offset g_ui.bf.dl_name
        call    tx_read
        jnc     @ul_k
        cmp     [g_ui.lp.ul_new],0
        jne     @ul_n
        mov     esi,offset u_notfound
        jmp     dl_ok
@ul_n:
        call    tx_init
@ul_k:
        xor     ax,ax                   ; each line as the editor shows it
@ul_l:
        cmp     ax,[g_ui.tx.tx_n]
        jae     @ul_f
        push    eax
        mov     edi,offset g_ui.bf.ed_lb.txt
        call    tx_get
        mov     esi,edi
        call    ui_canon
        pop     eax
        jc      @ul_x
        movzx   ecx,[g_state.io.sbx]
        mov     esi,offset g_sbuf
        call    tx_repl
@ul_x:
        inc     ax
        jmp     @ul_l
@ul_f:
        mov     esi,offset g_ui.bf.dl_name
        mov     edi,offset g_ui.bf.ui_fn
@ul_c:
        lodsb
        stosb
        test    al,al
        jnz     @ul_c
        jmp     ui_reset

; ::::::::::::::
; ui_save - File, Save: to its name, else Save As.
;
; out:    CF  set: not saved
; ::::::::::::::
ui_save:
        call    ed_commit
        cmp     byte ptr [g_ui.bf.ui_fn],0
        je      ui_saveas
        mov     edx,offset g_ui.bf.ui_fn
@usv_w:
        call    tx_write
        jc      @usv_e
        and     [g_ui.tx.tx_chg],0
        clc
        ret
@usv_e:
        mov     esi,offset u_access
        call    dl_ok
        stc
        ret

; ::::::::::::::
; ui_saveas - File, Save As.
;
; out:    CF  set: not saved
; ::::::::::::::
ui_saveas:
        call    ed_commit
@usa_a:
        call    dl_saveas
        jc      @usa_r
        mov     esi,offset g_ui.bf.dl_name  ; another file there: Overwrite?
        mov     edi,offset g_ui.bf.ui_fn
@usa_s:
        lodsb
        call    ui_upper
        mov     ah,al
        mov     al,[edi]
        inc     edi
        call    ui_upper
        cmp     al,ah
        jne     @usa_x
        test    al,al
        jnz     @usa_s
        jmp     @usa_y
@usa_x:
        mov     edx,offset g_ui.bf.dl_name
        mov     ax,4300h
        int     21h
        jc      @usa_y
        mov     esi,offset u_overw
        call    dl_ync
        cmp     al,1
        je      @usa_a
        ja      @usa_n
@usa_y:
        mov     esi,offset g_ui.bf.dl_name
        mov     edi,offset g_ui.bf.ui_fn
@usa_c:
        lodsb
        stosb
        test    al,al
        jnz     @usa_c
        mov     edx,offset g_ui.bf.ui_fn
        jmp     @usv_w
@usa_n:
        stc
@usa_r:
        ret

; ::::::::::::::
; ui_exit - File, Exit: back to DOS.
; ::::::::::::::
ui_exit:
        call    ui_asksave
        jc      @un_r
        mov     ax,3
        int     10h
        xor     al,al
        jmp     plt_exit

; ::::::::::::::
; ui_view - F4: the output page until a key.
; ::::::::::::::
ui_view:
        call    ui_out
        call    ui_getkey
        jmp     ui_in

; ::::::::::::::
; ui_in - page 0, the editor, shown.
; ::::::::::::::
ui_in:
        pushad
        mov     ax,0500h
        int     10h
        popad
        jmp     th_in
; ::::::::::::::
; ui_out - page 1, the output, shown.
; ::::::::::::::
ui_out:
        pushad
        mov     ax,0501h
        int     10h
        popad
        jmp     th_out
; ::::::::::::::
; ui_outclr - page 1 shown and cleared.
; ::::::::::::::
ui_outclr:
        call    th_out
        pushad
        mov     ax,0501h
        int     10h
        mov     ax,0600h
        mov     bh,07h
        xor     ecx,ecx
        mov     dx,184Fh
        int     10h
        mov     ah,2
        mov     bh,1
        xor     dx,dx
        int     10h
        and     [g_state.io.pcol],0
        popad
        ret

; ::::::::::::::
; ui_sync - the text to the program when it changed.
;
; out:    CF  set: a numbering error, the cursor on its line
; ::::::::::::::
ui_sync:
        call    ed_commit
        cmp     [g_ui.tx.tx_mod],0
        je      @usy_r
        call    sy_build
        jnc     @usy_r
        mov     ax,[g_ui.sy.sy_i]
        call    ui_atline
        stc
@usy_r:
        ret

; ::::::::::::::
; ui_atline - the cursor at text line AX's first word, the word selected,
; as QBasic shows an error's statement.
; ::::::::::::::
ui_atline:
        and     [g_ui.lp.ui_foc],0
        and     [g_ui.ed.sel_on],0
        call    ed_load
        xor     ebx,ebx
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
@ual_b:
        cmp     bx,cx
        jae     @ual_s
        cmp     byte ptr [ebx+g_ui.bf.ed_lb.txt],' '
        jne     @ual_s
        inc     bx
        jmp     @ual_b
@ual_s:
        mov     [g_ui.bf.ed_lb.col],bx
        mov     dx,bx
@ual_w:
        cmp     dx,cx
        jae     @ual_e
        movzx   edi,dx
        mov     al,ds:[edi+g_ui.bf.ed_lb.txt]
        call    ui_wordc
        jc      @ual_e
        inc     dx
        jmp     @ual_w
@ual_e:
        cmp     dx,bx
        je      @ual_r
        mov     [g_ui.ed.sel_c],dx
        mov     ax,[g_ui.ed.ed_line]
        mov     [g_ui.ed.sel_l],ax
        mov     [g_ui.ed.sel_on],1
@ual_r:
        ret

; ::::::::::::::
; ui_runp - Run, Start: the program from the text, run.
; ::::::::::::::
ui_runp:
        call    ui_sync
        jc      @urp_r
        call    ud_prep
        call    ui_outclr
        mov     [g_ui.lp.ui_rf],1
        mov     dword ptr [g_inbuf],'NUR'
        mov     ax,3
        call    rpl_enter
        jmp     rpl_run
@urp_r:
        ret

; ::::::::::::::
; ui_cancont - CONT can go on: stopped, the text as it was. After END or
; an error QBasic runs again.
;
; out:    CF  set: it cannot
; ::::::::::::::
ui_cancont:
        call    ed_commit
        call    ui_stopped
        jnz     @F
        clc
        ret
@@:
        stc
        ret

; ::::::::::::::
; ui_stopped - a stop to go on from, the text as it was.
;
; out:    ZF  set: so
; keeps:  all
; ::::::::::::::
ui_stopped:
        cmp     [g_ui.ud.stopd],0
        je      @F
        cmp     [g_ui.tx.tx_mod],0
        ret
@@:
        cmp     [g_ui.ud.stopd],1       ; ZF clear
        ret

; ::::::::::::::
; ui_contp - Run, Continue: CONT after a STOP unless the text changed.
; ::::::::::::::
ui_contp:
        call    ui_cancont
        jc      ui_runp
        call    ud_prep
        call    ui_out
        mov     [g_ui.lp.ui_rf],1
        jmp     rpl_cont

; ::::::::::::::
; ui_exec - Enter in the immediate window: its line run on the output page.
; ::::::::::::::
ui_exec:
        call    ui_sync
        jc      @uex_r
        call    im_next
        test    ax,ax
        jz      @uex_r
        call    ui_out
        push    eax
        mov     ah,3
        mov     bh,1
        int     10h
        mov     [g_ui.lp.ui_cpos],dx
        pop     eax
        mov     [g_ui.lp.ui_rf],0
        call    rpl_enter
        test    al,al
        jz      rpl_run
        jmp     ui_in
@uex_r:
        ret

; ::::::::::::::
; ui_ready - the READY loop's place: the output page until a key, unless
; an immediate line printed nothing.
; ::::::::::::::
ui_ready:
        call    plt_flush
        and     [g_state.rn.stpflg],0
        cmp     [g_ui.lp.ui_rf],0
        jne     @urd_p
        mov     ah,3
        mov     bh,1
        int     10h
        cmp     dx,[g_ui.lp.ui_cpos]
        je      @urd_x
@urd_p:
        call    io_fresh
        mov     ah,3
        mov     bh,1
        int     10h
        mov     [g_ui.lp.ui_rpos],dx             ; output goes on here after the key
        mov     dx,1800h
        mov     ah,2
        int     10h
        mov     esi,offset u_anykey
@urd_s:
        lodsb
        test    al,al
        jz      @urd_k
        call    io_putc
        jmp     @urd_s
@urd_k:
        call    plt_flush
        and     [g_state.io.pcol],0
        mov     ah,10h
        int     16h
        mov     dx,1800h                ; the prompt off
        mov     ah,2
        mov     bh,1
        int     10h
        mov     ax,0920h
        mov     ebx,0107h
        mov     ecx,80
        int     10h
        mov     dx,[g_ui.lp.ui_rpos]
        mov     ah,2
        mov     bh,1
        int     10h
@urd_x:
        call    ui_in
        jmp     ui_loop

; ::::::::::::::
; ui_err - an error, BREAK or STOP in the editor: the cursor on its line,
; the error in a box. err_raise's stacks are reset already.
;
; in:     AL  the error, 0 BREAK
; ::::::::::::::
ui_err:
        mov     [g_ui.lp.ui_ecode],al
        test    al,al
        jz      @uer_b
        mov     [g_state.rn.cstk],CSTOP
        and     [g_state.rn.contip],0
@uer_b:
        call    plt_flush
        and     [g_state.io.pcol],0
        and     [g_state.rn.stpflg],0
        call    ui_in
        cmp     [g_ui.sy.sy_on],0
        je      @uer_l
        and     [g_ui.sy.sy_on],0
        and     [g_state.io.lmode],0    ; sy_build's, the error ended
        mov     ax,[g_ui.sy.sy_i]
        jmp     @uer_g
@uer_l:
        movzx   ebx,word ptr [g_state.rn.ln]
        cmp     bx,DLINE
        je      @uer_m
        mov     ax,word ptr [g_progb+ebx+1]
        call    sy_find
        jc      @uer_m
@uer_g:
        call    vw_goline
        call    ui_atline
@uer_m:
        mov     al,[g_ui.lp.ui_ecode]
        test    al,al
        jnz     @F
        call    ud_stop
        jmp     ui_loop
@@:
        call    ui_emsg
        mov     esi,offset g_ui.bf.ui_ebuf
        call    dl_err
        jmp     ui_loop

; ::::::::::::::
; ui_find - Search, Find: the box, then the search.
; ::::::::::::::
ui_find:
        call    ui_fdef
        call    dl_find
        jc      @ufd_r
        jmp     ui_find3
@ufd_r:
        ret

; ::::::::::::::
; ui_find3 - F3: the text found again, from past the cursor; it wraps.
; ::::::::::::::
ui_find3:
        call    ui_search
        jnc     @ufd_r
        mov     esi,offset u_nomatch
        jmp     dl_ok

; ::::::::::::::
; ui_search - fd_what from past the cursor, wrapping: found, it is selected.
;
; in:     ui_wrap  0: to the end only
; out:    CF  set: not found
; ::::::::::::::
ui_search:
        movzx   ecx,word ptr [g_ui.bf.fd_what.len]
        test    ecx,ecx
        jz      @usr_n
        call    ed_commit
        mov     ax,[g_ui.ed.ed_line]
        mov     [g_ui.fd.us_l],ax
        mov     ax,[g_ui.bf.ed_lb.col]
        inc     ax
        mov     [g_ui.fd.us_c],ax
        movzx   ecx,word ptr [g_ui.tx.tx_n]
        inc     cx                      ; every line, this one twice
@usr_l:
        push    ecx
        mov     ax,[g_ui.fd.us_l]
        mov     edi,offset g_ui.bf.uc_line
        call    tx_get
        movzx   ebx,cx
        sub     bx,[g_ui.bf.fd_what.len]
        jb      @usr_x
        mov     dx,[g_ui.fd.us_c]
@usr_p:
        cmp     dx,bx
        ja      @usr_x
        call    ui_match
        jnc     @usr_y
        inc     dx
        jmp     @usr_p
@usr_x:
        pop     ecx
        and     [g_ui.fd.us_c],0
        mov     ax,[g_ui.fd.us_l]
        inc     ax
        cmp     ax,[g_ui.tx.tx_n]
        jb      @usr_w
        cmp     [g_ui.lp.ui_wrap],0
        je      @usr_n
        xor     ax,ax
@usr_w:
        mov     [g_ui.fd.us_l],ax
        loop    @usr_l
@usr_n:
        stc
        ret
@usr_y:
        pop     ecx
        mov     ax,[g_ui.fd.us_l]
        push    edx
        call    ed_load
        pop     edx
        mov     [g_ui.ed.sel_c],dx
        mov     ax,[g_ui.fd.us_l]
        mov     [g_ui.ed.sel_l],ax
        mov     [g_ui.ed.sel_on],1
        add     dx,[g_ui.bf.fd_what.len]
        mov     [g_ui.bf.ed_lb.col],dx
        clc
        ret

; ::::::::::::::
; ui_match - fd_what at column DX of uc_line (BX past the last place)?
;
; out:    CF  clear: there
; keeps:  BX, CX, DX
; ::::::::::::::
ui_match:
        pushad
        movzx   esi,dx
        add     esi,offset g_ui.bf.uc_line
        mov     edi,offset g_ui.bf.fd_what.txt
        movzx   ecx,word ptr [g_ui.bf.fd_what.len]
@umt_l:
        mov     al,[esi]
        mov     ah,[edi]
        cmp     [g_ui.fd.fd_case],0
        jne     @umt_c
        call    ui_upper
        xchg    al,ah
        call    ui_upper
@umt_c:
        cmp     al,ah
        jne     @umt_n
        inc     esi
        inc     edi
        loop    @umt_l
        cmp     [g_ui.fd.fd_whole],0
        je      @umt_y
        mov     al,[esi] ; a whole word: no letter or digit
        call    ui_wordc                ; on either side
        jnc     @umt_n
        test    dx,dx
        jz      @umt_y
        movzx   esi,dx
        add     esi,offset g_ui.bf.uc_line-1
        mov     al,[esi]
        call    ui_wordc
        jnc     @umt_n
@umt_y:
        popad
        clc
        ret
@umt_n:
        popad
        stc
        ret

; ::::::::::::::
; ui_wordc - CF clear when AL is in a word: a letter, a digit, or one of
; "!#$%&." (QBasic's wordCharBitMask, uiword.asm).
; ::::::::::::::
ui_wordc:
        call    ui_upper
        cmp     al,'0'
        jb      @uwc_p
        cmp     al,'9'
        jbe     @uwc_y
        cmp     al,'A'
        jb      @uwc_n
        cmp     al,'Z'
        ja      @uwc_n
@uwc_y:
        clc
        ret
@uwc_p:
        push    edi
        push    ecx
        mov     edi,offset u_wordp
        mov     ecx,6
        repne   scasb
        pop     ecx
        pop     edi
        je      @uwc_y
@uwc_n:
        stc
        ret

; ::::::::::::::
; ui_fdef - Find's text to start from: the selection on one line, else the
; word at the cursor or just before it, else the last.
; ::::::::::::::
ui_fdef:
        cmp     [g_ui.lp.ui_foc],0
        jne     @ufe_r
        mov     esi,offset g_ui.bf.ed_lb.txt
        movzx   ebx,word ptr [g_ui.bf.ed_lb.col]
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        cmp     [g_ui.ed.sel_on],0
        je      @ufe_w
        mov     ax,[g_ui.ed.sel_l]
        cmp     ax,[g_ui.ed.ed_line]
        jne     @ufe_x
        mov     dx,[g_ui.ed.sel_c]               ; DX to BX, the columns
        cmp     dx,bx
        jbe     @ufe_c
        xchg    dx,bx
        jmp     @ufe_c
@ufe_w:
        cmp     bx,cx
        jae     @ufe_b
        mov     al,[esi+ebx]
        call    ui_wordc
        jnc     @ufe_f
@ufe_b:
        test    bx,bx
        jz      @ufe_x
        dec     bx
        cmp     bx,cx
        jae     @ufe_x
        mov     al,[esi+ebx]
        call    ui_wordc
        jc      @ufe_x
@ufe_f:
        movzx   edi,bx
@ufe_l:
        test    edi,edi
        jz      @ufe_rt
        mov     al,ds:[edi+g_ui.bf.ed_lb.txt-1]
        call    ui_wordc
        jc      @ufe_rt
        dec     edi
        jmp     @ufe_l
@ufe_rt:
        cmp     bx,cx
        jae     @ufe_d
        mov     al,[esi+ebx]
        call    ui_wordc
        jc      @ufe_d
        inc     bx
        jmp     @ufe_rt
@ufe_d:
        mov     edx,edi
@ufe_c:
        movzx   ecx,bx
        sub     cx,dx
        jz      @ufe_x
        cmp     cx,FLDMAX
        ja      @ufe_x
        mov     [g_ui.bf.fd_what.len],cx
        mov     [g_ui.bf.fd_what.col],cx
        movzx   edx,dx
        add     esi,edx
        mov     edi,offset g_ui.bf.fd_what.txt
        rep     movsb
@ufe_x:
@ufe_r:
        ret

; ::::::::::::::
; ui_change - Search, Change: Change All, or each one asked.
; ::::::::::::::
ui_change:
        call    ui_fdef
        call    dl_change
        cmp     al,1
        ja      @uch_r
        mov     [g_ui.fd.uc_all],al
        mov     [g_ui.fd.uc_f],0
        mov     [g_ui.lp.ui_wrap],0
        call    ed_commit
        and     [g_ui.ed.sel_on],0
        cmp     [g_ui.fd.uc_all],0
        je      @uch_l
        mov     ax,[g_ui.ed.ed_line]             ; Change All: from the start, the
        mov     [g_ui.fd.uc_sl],ax               ; cursor back after
        mov     ax,[g_ui.bf.ed_lb.col]
        mov     [g_ui.fd.uc_sc],ax
        xor     ax,ax
        call    ed_load
        and     [g_ui.bf.ed_lb.col],0
        dec     word ptr [g_ui.bf.ed_lb.col]
@uch_l:
        call    ui_search
        jc      @uch_d
        mov     [g_ui.fd.uc_f],1
        cmp     [g_ui.fd.uc_all],0
        jne     @uch_c
        call    dl_conf
        cmp     al,1
        je      @uch_l
        ja      @uch_k
@uch_c:
        movzx   ecx,word ptr [g_ui.ed.sel_c]
        mov     dx,[g_ui.bf.ed_lb.col]
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_cut
        mov     esi,offset g_ui.bf.fd_to.txt
        movzx   ecx,word ptr [g_ui.bf.fd_to.len]
        call    le_insn
        and     [g_ui.ed.sel_on],0
        dec     word ptr [g_ui.bf.ed_lb.col]
        call    ed_commit
        jmp     @uch_l
@uch_d:
        mov     [g_ui.lp.ui_wrap],1
        and     [g_ui.ed.sel_on],0
        cmp     [g_ui.fd.uc_all],0
        je      @uch_m
        call    ed_commit
        mov     ax,[g_ui.fd.uc_sl]
        cmp     ax,[g_ui.tx.tx_n]
        jbe     @uch_g
        mov     ax,[g_ui.tx.tx_n]
@uch_g:
        call    ed_load
        mov     ax,[g_ui.fd.uc_sc]
        mov     [g_ui.bf.ed_lb.col],ax
@uch_m:
        mov     esi,offset u_chdone
        cmp     [g_ui.fd.uc_f],0
        jne     @uch_o
        mov     esi,offset u_nomatch
@uch_o:
        jmp     dl_ok
@uch_k:
        mov     [g_ui.lp.ui_wrap],1              ; Cancel: no word more (uisearch.c)
        ret
@uch_r:
        ret

; ::::::::::::::
; ui_dec - AX in decimal at DI.
;
; out:    DI  past it
; ::::::::::::::
ui_dec:
        mov     ebx,10
        xor     ecx,ecx
@udc_d:
        xor     dx,dx
        div     bx
        push    edx
        inc     cx
        test    ax,ax
        jnz     @udc_d
@udc_o:
        pop     eax
        add     al,'0'
        stosb
        loop    @udc_o
        ret

        .data
u_oom      db   'Out of memory',0
u_range    db   'Line number out of range',0
u_order    db   'Line numbers out of order',0
u_imm      db   'Immediate',0
u_untl     db   'Untitled',0
; QBasic's status lines (qbasmsgs.txt) less the keys we lack; after
; each, its keys for the mouse: their last column, their routine
u_kedit    db   '<Shift+F1=Help> <F6=Window> <F5=Run> <F8=Step>',0
           db   15
           dd   ui_nohelp
           db   27
           dd   ui_win
           db   36
           dd   ui_contp
           db   46
           dd   ud_step
           db   0
u_krun     db   '<Shift+F1=Help> <F5=Continue> <F9=Toggle Bkpt> <F8=Step>',0
           db   15
           dd   ui_nohelp
           db   29
           dd   ui_contp
           db   46
           dd   ud_bp
           db   56
           dd   ud_step
           db   0
u_kimm     db   '<Shift+F1=Help> <F6=Window> <Enter=Execute Line>',0
           db   15
           dd   ui_nohelp
           db   27
           dd   ui_win
           db   48
           dd   ui_exec
           db   0
u_f1       db   'F1=Help ',0B3h,' ',0
u_f1s      db   'F1=Help   ',0
u_mbar     db   'Enter=Display Menu   Esc=Cancel   Arrow=Next Item',0
u_dlg      db   'Enter=Execute   Esc=Cancel   Tab=Next Field   Arrow=Next Item',0
u_open     db   'Open',0
u_saveas   db   'Save As',0
u_find     db   'Find',0
u_change   db   'Change',0
u_fname    db   'File &Name:',0
u_files    db   '&Files',0
u_dirs     db   '&Dirs/Drives',0
u_ok       db   'OK',0
u_cancel   db   'Cancel',0
u_bhelp    db   '&Help',0
u_yes      db   '&Yes',0
u_no       db   '&No',0
u_skip     db   '&Skip',0
u_chg1     db   '&Change',0
u_esc      db   'Press Esc to clear this dialog box',0
u_fwhat    db   '&Find What:',0
u_cto      db   'Change &To:',0
u_case     db   '&Match Upper/Lowercase',0
u_whole    db   '&Whole Word',0
u_fverify  db   'Find and &Verify',0
u_chall    db   '&Change All',0
u_mbas     db   '*.BAS',0
u_bas      db   '.BAS'
u_star     db   '*.*',0
u_notfound db   'File not found',0
u_overw    db   'File already exists. Overwrite?',0
u_nomatch  db   'Match not found',0
u_chdone   db   'Change complete',0
u_notsaved db   'Loaded file is not saved. Save it now?',0
u_access   db   'Path/File access error',0
u_wordp    db   '!#$%&.'
u_nohelp   db   'Help is not available',0
u_about    db   'CBM BASIC V2 for DOS||A full-screen editor',0
u_welcome  db   'Welcome to CBM BASIC V2 for DOS||'
           db   'Lines may have numbers or not; GOTO and|'
           db   'GOSUB need them. F5 runs the program.',0
u_anykey   db   'Press any key to continue',0
; a message box's buttons: each its text, its width (COW's itl.h)
bt_ok      TBTN <u_ok,8>
           TBTN <u_bhelp,8>
           dd   0                ; the list's end
bt_ync     TBTN <u_yes,7>
           TBTN <u_no,8>
           TBTN <u_cancel,8>
           TBTN <u_bhelp,8>
           dd   0                ; the list's end
bt_wel     TBTN <u_esc,0>
           dd   0                ; the list's end
        .code

        .data?
g_ui    TUI     <>                      ; the editor's state
g_hlpb  db      HLPSZ dup(?)            ; BASIC.HLP
        .code

        end
