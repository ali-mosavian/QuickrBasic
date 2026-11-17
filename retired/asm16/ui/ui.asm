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

IMN     = 10                    ; the immediate window's lines kept
IMW     = 80                    ; their size: length, text
LSMAX   = 128                   ; a dialog list's names at most

; ::::::::::::::
; ui_start - the editor when both standard handles are the console.
; ::::::::::::::
ui_start:
        cmp     [isdev],0
        je      rpl_ready
        mov     ax,4400h
        mov     bx,1
        int     21h
        test    dl,80h
        jz      rpl_ready
        mov     ax,offset bss_e         ; the code cache: 4 KB at least
        cmp     ax,CLIM-4096
        ja      @us_mem
        mov     [ideon],1
        mov     ax,cs
        add     ax,TXSEG
        mov     [txseg],ax
        add     ax,1000h
        mov     [clpseg],ax
        add     ax,1000h
        mov     [wrkseg],ax
        mov     [ls_f+L_ITEMS],offset ls_fl
        mov     [ls_f+L_SZ],13
        mov     [ls_d+L_ITEMS],offset ls_dl
        mov     [ls_d+L_SZ],13
        mov     byte ptr ds:[ui_fn],0   ; out of the zeroed data
        mov     [im_n],1
        mov     [syn_on],1
        mov     [ed_ins],1
        mov     [ui_wrap],1
        mov     ax,cs
        add     ax,SCRBUF
        mov     [scrseg],ax
        call    ms_init
        call    hl_load
        call    tx_init
        xor     ax,ax
        call    ed_load
        call    im_load
        mov     ax,3
        int     10h
        mov     si,81h                  ; a file named on the command line,
        movzx   cx,byte ptr cs:[80h]    ; in the PSP
@ust_b:
        jcxz    @ust_w
        cmp     byte ptr cs:[si],' '
        jne     @ust_n
        inc     si
        dec     cx
        jmp     @ust_b
@ust_n:
        mov     di,offset dl_fn+LB_TXT
        xor     dx,dx
@ust_c:
        lods    byte ptr cs:[si]
        cmp     al,' '
        jbe     @ust_e
        stosb
        inc     dx
        cmp     dx,FLDMAX
        jae     @ust_e
        loop    @ust_c
@ust_e:
        mov     ds:[dl_fn+LB_LEN],dx
        call    dl_fok
        mov     [ul_new],1
        call    ui_load
        and     [ul_new],0
        jmp     ui_loop
@us_mem:
        mov     dx,offset u_cache
        mov     ah,9
        int     21h
        mov     ax,4C01h
        int     21h
@ust_w:
        mov     si,offset u_welcome
        mov     bx,offset bt_wel
        call    dl_msg

; ::::::::::::::
; ui_loop - draw, a key, its command; again.
; ::::::::::::::
ui_loop:
if AC eq 0
        and     [ui_help],0             ; else ui_key: a completion shows it
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
        mov     [alt_t],al
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
        cmp     [alt_t],0
        jne     @ugk_w
        mov     [alt_t],1
        jmp     @ugk_w
@ugk_n:
        cmp     [alt_t],1
        mov     [alt_t],0
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
        push    ax
        mov     ah,2
        int     16h
        mov     [ui_kf],al
        pop     ax
        jmp     ms_hide

; ::::::::::::::
; ui_key - the key AX: the menus, the run keys, then the window's.
; ::::::::::::::
ui_key:
if AC
        mov     [ui_help],0             ; a completion's form, shown till now
endif
        cmp     ax,0FFFFh
        je      @uk_bar
        cmp     ax,MS_KEY
        je      ms_main
        cmp     ax,260Ch                ; Ctrl+L: Repeat Last Find
        je      ui_find3
        test    al,al
        jnz     @uk_w
        cmp     ah,44h
        je      @uk_bar
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
        cmp     ah,59h                  ; Shift+F6: the other of two
        je      ui_win
        push    ax
        call    mn_alt
        pop     ax
        jnc     @uk_r
@uk_w:
        cmp     [ui_foc],0
        jne     im_key
        jmp     ed_key
@uk_bar:
        xor     ax,ax
        jmp     mn_run
@uk_r:
        ret

; ::::::::::::::
; hl_load - BASIC.HLP, from the program's own directory, at HLPSEG.
; ::::::::::::::
hl_load:
        mov     ax,cs:[2Ch]             ; the environment, the program's path
        test    ax,ax
        jz      @hll_r
        push    es
        mov     es,ax
        xor     di,di
        xor     al,al
        mov     cx,8000h
@hll_z:
        repne   scasb
        cmp     es:[di],al
        jne     @hll_z
        add     di,3                    ; past the 0 and the count
        mov     si,offset dl_fn
        mov     bx,si                   ; BX past the last '\'
@hll_c:
        mov     al,es:[di]
        inc     di
        mov     [si],al
        inc     si
        cmp     al,'\'
        jne     @hll_n
        mov     bx,si
@hll_n:
        test    al,al
        jnz     @hll_c
        pop     es
        mov     dword ptr [bx],'ISAB'
        mov     dword ptr [bx+4],'LH.C'
        mov     word ptr [bx+8],'P'
        mov     dx,offset dl_fn
        mov     ax,3D00h
        int     21h
        jc      @hll_r
        mov     bx,ax
        push    ds
        mov     ax,cs
        add     ax,HLPSEG
        mov     ds,ax
        xor     dx,dx
        mov     cx,HLPSZ
        mov     ah,3Fh
        int     21h
        pop     ds
        jc      @hll_x
        mov     [hl_len],ax
@hll_x:
        mov     ah,3Eh
        int     21h
@hll_r:
        ret

; ::::::::::::::
; ui_win - F6: the other window.
; ::::::::::::::
ui_win:
        call    ed_commit
        xor     [ui_foc],1
        ret

; ::::::::::::::
; ui_nohelp - F1.
; ::::::::::::::
ui_nohelp:
        mov     si,offset u_nohelp
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
        cmp     word ptr ds:[im_lb+LB_LEN],ED_COLS-1
        jae     @imk_r
@imk_e:
        mov     bx,offset im_lb
        jmp     ui_ledit
@imk_r:
        ret

; ::::::::::::::
; ed_key - a key in the program window: a shifted move selects.
; ::::::::::::::
ed_key:
        mov     bx,offset ed_lb
        test    al,al
        jnz     @edk_a
        mov     cx,ax
        mov     si,offset ed_moves
@edk_m:
        lodsb
        test    al,al
        jz      @edk_x
        cmp     al,ch
        je      @edk_mv
        add     si,2
        jmp     @edk_m
@edk_mv:
        test    [ui_kf],3
        jz      @edk_ns
        call    sel_mark
        jmp     @edk_go
@edk_ns:
        and     [sel_on],0
@edk_go:
        mov     bx,offset ed_lb
        jmp     word ptr [si]
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
        test    [ui_kf],3
        jnz     ed_cut
        call    sel_get
        test    al,al
        jnz     ed_clear
        and     [sel_on],0
        jmp     ed_del
@edk_ins:
        test    [ui_kf],3
        jnz     ed_paste
        xor     [ed_ins],1
        ret
@edk_a:
        cmp     [ed_pfx],0              ; WordStar keys (COW edittbls.inc)
        jne     @edk_q
        cmp     al,11h                  ; Ctrl+Q: a second key follows
        je      @edk_qp
        cmp     al,1Ah
        ja      @edk_a2
        movzx   si,al
        mov     ah,[ed_ws+si-1]
        test    ah,ah
        jz      @edk_a2
        xor     al,al
        jmp     ed_key
@edk_qp:
        mov     [ed_pfx],al
        ret
@edk_q:
        mov     [ed_pfx],0
        and     al,1Fh
        mov     si,offset ed_wsq
@edk_ql:
        mov     cx,[si]
        add     si,2
        test    cl,cl
        jz      @edk_r
        cmp     cl,al
        jne     @edk_ql
        mov     ah,ch
        xor     al,al
        test    ah,ah                   ; 0: Ctrl+Q,Y, to the line's end out
        jnz     ed_key
        and     [sel_on],0
        mov     bx,offset ed_lb
        mov     cx,ds:[ed_lb+LB_COL]
        mov     dx,ds:[ed_lb+LB_LEN]
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
        push    ax
        call    sel_get
        cmp     al,1
        jne     @edk_t
        call    ed_clear
@edk_t:
        and     [sel_on],0
        pop     ax
        mov     bx,offset ed_lb
        mov     dx,LNMAX
        jmp     le_ins
@edk_ent:
        and     [sel_on],0
        jmp     ed_enter
@edk_bs:
        call    sel_get
        test    al,al
        jnz     ed_clear
        jmp     ed_bs
@edk_tab:
        and     [sel_on],0
if AC
        call    ac_tab
        jnc     @edk_r
endif
        jmp     ed_tab
@edk_y:
        and     [sel_on],0
        jmp     ed_ctrly
@edk_esc:
        and     [sel_on],0
@edk_r:
        ret

; Ctrl+A to Ctrl+Z as WordStar's: the scan code of the key they stand for
DSEG    segment
ed_ws   db      73h,0,51h,4Dh,48h,74h,53h,0,0,0,0,0,0,0,0,0
        db      0,49h,4Bh,0,0,52h,0,50h,0,0
; Ctrl+Q's second key: the letter's code, the scan code (0 erases)
ed_wsq  db      13h,47h,04h,4Fh,12h,77h,03h,75h,19h,0,0
DSEG    ends

; the keys that move the cursor: scan code, routine
DSEG    segment
ed_moves db     48h
        dw      ed_up
        db      50h
        dw      ed_down
        db      4Bh
        dw      le_left
        db      4Dh
        dw      le_right
        db      47h
        dw      le_home
        db      4Fh
        dw      le_end
        db      49h
        dw      ed_pgup
        db      51h
        dw      ed_pgdn
        db      77h
        dw      ed_first
        db      75h
        dw      ed_last
        db      73h
        dw      le_wleft
        db      74h
        dw      le_wright
        db      0
DSEG    ends

; ::::::::::::::
; ui_act - the menu command AL.
; ::::::::::::::
ui_act:
        movzx   bx,al
        dec     bx
        cmp     bx,A_NONE-1
        jae     @ua_r
        add     bx,bx
        jmp     word ptr [ui_acts+bx]
@ua_r:
        ret
DSEG    segment
ui_acts dw      ui_new,ui_open,ui_save,ui_saveas,ui_exit,ed_cut,ed_copy
        dw      ed_paste,ed_clear,ui_view,ui_find,ui_find3,ui_change
        dw      ui_runp,ui_contp,ui_syntax,ui_about
DSEG    ends

; ::::::::::::::
; ui_syntax - Options, Syntax Checking: on, off.
; ::::::::::::::
ui_syntax:
        xor     [syn_on],1
        ret

; ::::::::::::::
; ui_about - Help, About.
; ::::::::::::::
ui_about:
        mov     si,offset u_about
        jmp     dl_ok

; ::::::::::::::
; ui_asksave - a changed text: save it first? Yes saves, No goes on.
;
; out:    CF  set: cancelled
; ::::::::::::::
ui_asksave:
        call    ed_commit
        cmp     [tx_chg],0
        je      @uas_n
        mov     si,offset u_notsaved
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
        mov     [ed_top],ax
        mov     [ed_left],ax
        mov     ds:[ed_lb+LB_COL],ax
        mov     [sel_on],al
        mov     [ui_foc],al
        mov     [tx_chg],al
        mov     [tx_mod],1
        call    ed_load
        jmp     prg_init

; ::::::::::::::
; ui_new - File, New.
; ::::::::::::::
ui_new:
        call    ui_asksave
        jc      @un_r
        call    tx_init
        mov     byte ptr ds:[ui_fn],0
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
        mov     dx,offset dl_name
        call    tx_read
        jnc     @ul_k
        cmp     [ul_new],0
        jne     @ul_n
        mov     si,offset u_notfound
        jmp     dl_ok
@ul_n:
        call    tx_init
@ul_k:
        xor     ax,ax                   ; each line as the editor shows it
@ul_l:
        cmp     ax,[tx_n]
        jae     @ul_f
        push    ax
        mov     di,offset ed_lb+LB_TXT
        call    tx_get
        mov     si,di
        call    ui_canon
        pop     ax
        jc      @ul_x
        movzx   cx,[sbx]
        mov     si,offset sbuf
        call    tx_repl
@ul_x:
        inc     ax
        jmp     @ul_l
@ul_f:
        mov     si,offset dl_name
        mov     di,offset ui_fn
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
        cmp     byte ptr ds:[ui_fn],0
        je      ui_saveas
        mov     dx,offset ui_fn
@usv_w:
        call    tx_write
        jc      @usv_e
        and     [tx_chg],0
        clc
        ret
@usv_e:
        mov     si,offset u_access
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
        mov     si,offset dl_name       ; another file there: Overwrite?
        mov     di,offset ui_fn
@usa_s:
        lodsb
        call    ui_upper
        mov     ah,al
        mov     al,[di]
        inc     di
        call    ui_upper
        cmp     al,ah
        jne     @usa_x
        test    al,al
        jnz     @usa_s
        jmp     @usa_y
@usa_x:
        mov     dx,offset dl_name
        mov     ax,4300h
        int     21h
        jc      @usa_y
        mov     si,offset u_overw
        call    dl_ync
        cmp     al,1
        je      @usa_a
        ja      @usa_n
@usa_y:
        mov     si,offset dl_name
        mov     di,offset ui_fn
@usa_c:
        lodsb
        stosb
        test    al,al
        jnz     @usa_c
        mov     dx,offset ui_fn
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
        pusha
        mov     ax,0500h
        int     10h
        popa
        ret
; ::::::::::::::
; ui_out - page 1, the output, shown.
; ::::::::::::::
ui_out:
        pusha
        mov     ax,0501h
        int     10h
        popa
        ret
; ::::::::::::::
; ui_outclr - page 1 shown and cleared.
; ::::::::::::::
ui_outclr:
        pusha
        mov     ax,0501h
        int     10h
        mov     ax,0600h
        mov     bh,07h
        xor     cx,cx
        mov     dx,184Fh
        int     10h
        mov     ah,2
        mov     bh,1
        xor     dx,dx
        int     10h
        and     [pcol],0
        popa
        ret

; ::::::::::::::
; ui_sync - the text to the program when it changed.
;
; out:    CF  set: a numbering error, the cursor on its line
; ::::::::::::::
ui_sync:
        call    ed_commit
        cmp     [tx_mod],0
        je      @usy_r
        call    sy_build
        jnc     @usy_r
        mov     ax,[sy_i]
        call    ui_atline
        stc
@usy_r:
        ret

; ::::::::::::::
; ui_atline - the cursor at text line AX's first word, the word selected,
; as QBasic shows an error's statement.
; ::::::::::::::
ui_atline:
        and     [ui_foc],0
        and     [sel_on],0
        call    ed_load
        xor     bx,bx
        mov     cx,ds:[ed_lb+LB_LEN]
@ual_b:
        cmp     bx,cx
        jae     @ual_s
        cmp     byte ptr [bx+ed_lb+LB_TXT],' '
        jne     @ual_s
        inc     bx
        jmp     @ual_b
@ual_s:
        mov     ds:[ed_lb+LB_COL],bx
        mov     dx,bx
@ual_w:
        cmp     dx,cx
        jae     @ual_e
        mov     di,dx
        mov     al,ds:[di+ed_lb+LB_TXT]
        call    ui_wordc
        jc      @ual_e
        inc     dx
        jmp     @ual_w
@ual_e:
        cmp     dx,bx
        je      @ual_r
        mov     [sel_c],dx
        mov     ax,[ed_line]
        mov     [sel_l],ax
        mov     [sel_on],1
@ual_r:
        ret

; ::::::::::::::
; ui_runp - Run, Start: the program from the text, run.
; ::::::::::::::
ui_runp:
        call    ui_sync
        jc      @urp_r
        call    ui_outclr
        mov     [ui_rf],1
        mov     dword ptr [inbuf],'NUR'
        mov     ax,3
        call    rpl_enter
        jmp     rpl_run
@urp_r:
        ret

; ::::::::::::::
; ui_contp - Run, Continue: CONT after a STOP unless the text changed.
; ::::::::::::::
ui_contp:
        call    ed_commit
        cmp     [tx_mod],0
        jne     ui_runp
        cmp     [contip],0
        je      ui_runp
        call    ui_out
        mov     [ui_rf],1
        mov     dword ptr [inbuf],'TNOC'
        mov     ax,4
        call    rpl_enter
        jmp     rpl_run

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
        push    ax
        mov     ah,3
        mov     bh,1
        int     10h
        mov     [ui_cpos],dx
        pop     ax
        mov     [ui_rf],0
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
        and     [stpflg],0
        cmp     [ui_rf],0
        jne     @urd_p
        mov     ah,3
        mov     bh,1
        int     10h
        cmp     dx,[ui_cpos]
        je      @urd_x
@urd_p:
        call    io_fresh
        mov     ah,3
        mov     bh,1
        int     10h
        mov     [ui_rpos],dx            ; output goes on here after the key
        mov     dx,1800h
        mov     ah,2
        int     10h
        mov     si,offset u_anykey
@urd_s:
        lodsb
        test    al,al
        jz      @urd_k
        call    io_putc
        jmp     @urd_s
@urd_k:
        call    plt_flush
        and     [pcol],0
        mov     ah,10h
        int     16h
        mov     dx,1800h                ; the prompt off
        mov     ah,2
        mov     bh,1
        int     10h
        mov     ax,0920h
        mov     bx,0107h
        mov     cx,80
        int     10h
        mov     dx,[ui_rpos]
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
        mov     [ui_ecode],al
        test    al,al
        jz      @uer_b
        mov     [cstk],CSTOP
        and     [contip],0
@uer_b:
        call    plt_flush
        and     [pcol],0
        and     [stpflg],0
        call    ui_in
        cmp     [sy_on],0
        je      @uer_l
        and     [sy_on],0
        mov     ax,[sy_i]
        jmp     @uer_g
@uer_l:
        mov     bx,[ln]
        cmp     bx,DLINE
        je      @uer_m
        mov     ax,gs:[bx+1]
        call    sy_find
        jc      @uer_m
@uer_g:
        call    ui_atline
@uer_m:
        mov     al,[ui_ecode]
        test    al,al
        jz      ui_loop
        call    ui_emsg
        mov     si,offset ui_ebuf
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
        mov     si,offset u_nomatch
        jmp     dl_ok

; ::::::::::::::
; ui_search - fd_what from past the cursor, wrapping: found, it is selected.
;
; in:     ui_wrap  0: to the end only
; out:    CF  set: not found
; ::::::::::::::
ui_search:
        mov     cx,ds:[fd_what+LB_LEN]
        jcxz    @usr_n
        call    ed_commit
        mov     ax,[ed_line]
        mov     [us_l],ax
        mov     ax,ds:[ed_lb+LB_COL]
        inc     ax
        mov     [us_c],ax
        mov     cx,[tx_n]
        inc     cx                      ; every line, this one twice
@usr_l:
        push    cx
        mov     ax,[us_l]
        mov     di,offset uc_line
        call    tx_get
        mov     bx,cx
        sub     bx,ds:[fd_what+LB_LEN]
        jb      @usr_x
        mov     dx,[us_c]
@usr_p:
        cmp     dx,bx
        ja      @usr_x
        call    ui_match
        jnc     @usr_y
        inc     dx
        jmp     @usr_p
@usr_x:
        pop     cx
        and     [us_c],0
        mov     ax,[us_l]
        inc     ax
        cmp     ax,[tx_n]
        jb      @usr_w
        cmp     [ui_wrap],0
        je      @usr_n
        xor     ax,ax
@usr_w:
        mov     [us_l],ax
        loop    @usr_l
@usr_n:
        stc
        ret
@usr_y:
        pop     cx
        mov     ax,[us_l]
        push    dx
        call    ed_load
        pop     dx
        mov     [sel_c],dx
        mov     ax,[us_l]
        mov     [sel_l],ax
        mov     [sel_on],1
        add     dx,ds:[fd_what+LB_LEN]
        mov     ds:[ed_lb+LB_COL],dx
        clc
        ret

; ::::::::::::::
; ui_match - fd_what at column DX of uc_line (BX past the last place)?
;
; out:    CF  clear: there
; keeps:  BX, CX, DX
; ::::::::::::::
ui_match:
        pusha
        mov     si,offset uc_line
        add     si,dx
        mov     di,offset fd_what+LB_TXT
        mov     cx,ds:[fd_what+LB_LEN]
@umt_l:
        mov     al,[si]
        mov     ah,[di]
        cmp     [fd_case],0
        jne     @umt_c
        call    ui_upper
        xchg    al,ah
        call    ui_upper
@umt_c:
        cmp     al,ah
        jne     @umt_n
        inc     si
        inc     di
        loop    @umt_l
        cmp     [fd_whole],0
        je      @umt_y
        mov     al,[si]                 ; a whole word: no letter or digit
        call    ui_wordc                ; on either side
        jnc     @umt_n
        test    dx,dx
        jz      @umt_y
        mov     si,offset uc_line-1
        add     si,dx
        mov     al,[si]
        call    ui_wordc
        jnc     @umt_n
@umt_y:
        popa
        clc
        ret
@umt_n:
        popa
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
        push    di
        push    cx
        mov     di,offset u_wordp
        mov     cx,6
        repne   scasb
        pop     cx
        pop     di
        je      @uwc_y
@uwc_n:
        stc
        ret

; ::::::::::::::
; ui_fdef - Find's text to start from: the selection on one line, else the
; word at the cursor or just before it, else the last.
; ::::::::::::::
ui_fdef:
        cmp     [ui_foc],0
        jne     @ufe_r
        push    es
        push    ds
        pop     es
        mov     si,offset ed_lb+LB_TXT
        mov     bx,ds:[ed_lb+LB_COL]
        mov     cx,ds:[ed_lb+LB_LEN]
        cmp     [sel_on],0
        je      @ufe_w
        mov     ax,[sel_l]
        cmp     ax,[ed_line]
        jne     @ufe_x
        mov     dx,[sel_c]              ; DX to BX, the columns
        cmp     dx,bx
        jbe     @ufe_c
        xchg    dx,bx
        jmp     @ufe_c
@ufe_w:
        cmp     bx,cx
        jae     @ufe_b
        mov     al,[si+bx]
        call    ui_wordc
        jnc     @ufe_f
@ufe_b:
        test    bx,bx
        jz      @ufe_x
        dec     bx
        cmp     bx,cx
        jae     @ufe_x
        mov     al,[si+bx]
        call    ui_wordc
        jc      @ufe_x
@ufe_f:
        mov     di,bx
@ufe_l:
        test    di,di
        jz      @ufe_rt
        mov     al,ds:[di+ed_lb+LB_TXT-1]
        call    ui_wordc
        jc      @ufe_rt
        dec     di
        jmp     @ufe_l
@ufe_rt:
        cmp     bx,cx
        jae     @ufe_d
        mov     al,[si+bx]
        call    ui_wordc
        jc      @ufe_d
        inc     bx
        jmp     @ufe_rt
@ufe_d:
        mov     dx,di
@ufe_c:
        mov     cx,bx
        sub     cx,dx
        jz      @ufe_x
        cmp     cx,FLDMAX
        ja      @ufe_x
        mov     ds:[fd_what+LB_LEN],cx
        mov     ds:[fd_what+LB_COL],cx
        add     si,dx
        mov     di,offset fd_what+LB_TXT
        rep     movsb
@ufe_x:
        pop     es
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
        mov     [uc_all],al
        mov     [uc_f],0
        mov     [ui_wrap],0
        call    ed_commit
        and     [sel_on],0
        cmp     [uc_all],0
        je      @uch_l
        mov     ax,[ed_line]            ; Change All: from the start, the
        mov     [uc_sl],ax              ; cursor back after
        mov     ax,ds:[ed_lb+LB_COL]
        mov     [uc_sc],ax
        xor     ax,ax
        call    ed_load
        and     ds:[ed_lb+LB_COL],0
        dec     word ptr ds:[ed_lb+LB_COL]
@uch_l:
        call    ui_search
        jc      @uch_d
        mov     [uc_f],1
        cmp     [uc_all],0
        jne     @uch_c
        call    dl_conf
        cmp     al,1
        je      @uch_l
        ja      @uch_k
@uch_c:
        mov     cx,[sel_c]
        mov     dx,ds:[ed_lb+LB_COL]
        mov     bx,offset ed_lb
        call    le_cut
        mov     si,offset fd_to+LB_TXT
        mov     cx,ds:[fd_to+LB_LEN]
        call    le_insn
        and     [sel_on],0
        dec     word ptr ds:[ed_lb+LB_COL]
        call    ed_commit
        jmp     @uch_l
@uch_d:
        mov     [ui_wrap],1
        and     [sel_on],0
        cmp     [uc_all],0
        je      @uch_m
        call    ed_commit
        mov     ax,[uc_sl]
        cmp     ax,[tx_n]
        jbe     @uch_g
        mov     ax,[tx_n]
@uch_g:
        call    ed_load
        mov     ax,[uc_sc]
        mov     ds:[ed_lb+LB_COL],ax
@uch_m:
        mov     si,offset u_chdone
        cmp     [uc_f],0
        jne     @uch_o
        mov     si,offset u_nomatch
@uch_o:
        jmp     dl_ok
@uch_k:
        mov     [ui_wrap],1             ; Cancel: no word more (uisearch.c)
        ret
@uch_r:
        ret

; ::::::::::::::
; ui_dec - AX in decimal at DI.
;
; out:    DI  past it
; ::::::::::::::
ui_dec:
        mov     bx,10
        xor     cx,cx
@udc_d:
        xor     dx,dx
        div     bx
        push    dx
        inc     cx
        test    ax,ax
        jnz     @udc_d
@udc_o:
        pop     ax
        add     al,'0'
        stosb
        loop    @udc_o
        ret

DSEG    segment
u_oom      db   'Out of memory',0
u_cache    db   'BASIC-IDE: the code cache is under 4 KB',13,10,'$'
u_range    db   'Line number out of range',0
u_order    db   'Line numbers out of order',0
u_imm      db   'Immediate',0
u_untl     db   'Untitled',0
; QBasic's status lines (qbasmsgs.txt) less the keys we lack; after
; each, its keys for the mouse: their last column, their routine
u_kedit    db   '<Shift+F1=Help> <F6=Window> <F5=Run>',0
           db   15
           dw   ui_nohelp
           db   27
           dw   ui_win
           db   36
           dw   ui_contp
           db   0
u_krun     db   '<Shift+F1=Help> <F5=Continue>',0
           db   15
           dw   ui_nohelp
           db   29
           dw   ui_contp
           db   0
u_kimm     db   '<Shift+F1=Help> <F6=Window> <Enter=Execute Line>',0
           db   15
           dw   ui_nohelp
           db   27
           dw   ui_win
           db   48
           dw   ui_exec
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
bt_ok      dw   u_ok
           db   8
           dw   u_bhelp
           db   8,0,0
bt_ync     dw   u_yes
           db   7
           dw   u_no
           db   8
           dw   u_cancel
           db   8
           dw   u_bhelp
           db   8,0,0
bt_wel     dw   u_esc
           db   0,0,0
DSEG    ends
