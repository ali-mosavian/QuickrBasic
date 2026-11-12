; ==========================================================================
; view.asm - views: the module and each SUB or FUNCTION on its own
; ==========================================================================
; holds:    vw_reset, vw_scan, vw_apply, vw_show, vw_goline, vw_title,
;           vw_commit, vw_next, vw_prev, vw_subs, vw_newsub, vw_newfn
; uses:     tx_* (the text and its view), ed_load, ed_commit, wn_*, dl_run
; data:     The text keeps the module's lines first, then each procedure
;           from its SUB or FUNCTION line to the next one's (vw_order makes
;           it so on loading). vw: a view per part, from vfirst, vn lines;
;           a window's ed_vw its view, applied to the text by vw_apply. An
;           added or removed line (tx_hook) moves the views after the
;           window's. The cursor's place in a view is kept when another is
;           shown.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; vw_reset - a new or loaded text: its module first, the views, both
; windows on the module.
; ::::::::::::::
vw_reset:
        mov     [g_ui.tx.tx_hook],offset vw_shift
        call    vw_order
        call    vw_scan
        xor     eax,eax
        mov     ecx,NVIEW               ; no kept places
        mov     edi,offset g_ui.vw.vw_tab
@vrs_l:
        mov     [edi].TVIEW.vline,ax
        mov     [edi].TVIEW.vtop,ax
        mov     [edi].TVIEW.vcol,ax
        add     edi,size TVIEW
        loop    @vrs_l
        mov     [g_ui.ed.ed_vw],al
        mov     [g_ui.wn.win+W_ED1*size TWIN+TWIN.wed.ed_vw],al
        mov     [g_ui.wn.win+W_ED2*size TWIN+TWIN.wed.ed_vw],al
        jmp     vw_apply

; ::::::::::::::
; vw_kind - text line AX: a procedure's first line, its end, or neither.
;
; out:    AL  VK_SUB, VK_FN, VK_END, else 0
;         ESI past the keyword, at the name, for VK_SUB, VK_FN
; keeps:  all but EAX, ESI
; ::::::::::::::
vw_kind:
        push    ebx
        push    ecx
        push    edi
        call    tx_offa
        call    tx_len
        lea     esi,[g_txb+ebx]
        mov     [g_ui.vw.vw_src],esi
        mov     edi,offset g_ui.bf.vw_ln        ; 0-ended, upper case
        mov     ebx,edi
@vk_c:
        jecxz   @vk_z
        lodsb
        call    ui_upper
        stosb
        dec     ecx
        jmp     @vk_c
@vk_z:
        mov     byte ptr [edi],0
        mov     esi,ebx
        call    vw_sp
@vk_d:                                  ; a line number
        mov     al,[esi]
        sub     al,'0'
        cmp     al,9
        ja      @F
        inc     esi
        jmp     @vk_d
@@:
        call    vw_sp
        mov     edi,offset u_kwsub
        mov     ah,VK_SUB
        call    vw_word
        je      @vk_r
        mov     edi,offset u_kwfn
        mov     ah,VK_FN
        call    vw_word
        je      @vk_r
        mov     edi,offset u_kwend
        call    vw_word
        jne     @vk_n
        mov     edi,offset u_kwsub
        mov     ah,VK_END
        call    vw_word
        je      @vk_r
        mov     edi,offset u_kwfn
        call    vw_word
        je      @vk_r
@vk_n:
        xor     ah,ah
@vk_r:
        mov     al,ah
        pop     edi
        pop     ecx
        pop     ebx
        ret

; vw_sp - ESI past spaces. Keeps all but ESI.
vw_sp:
        cmp     byte ptr [esi],' '
        jne     @F
        inc     esi
        jmp     vw_sp
@@:
        ret

; vw_word - the word at ESI is the one at EDI (0-ended), a whole word: ZF
; set, ESI past it and its spaces. Keeps all but ESI, flags.
vw_word:
        push    eax
        push    esi
        push    edi
@vw_l:
        mov     al,[edi]
        test    al,al
        jz      @vw_e
        cmp     al,[esi]
        jne     @vw_n
        inc     esi
        inc     edi
        jmp     @vw_l
@vw_e:
        mov     al,[esi]                ; the word ends here
        cmp     al,' '
        je      @F
        test    al,al
        jz      @F
        cmp     al,'('
        je      @F
        cmp     al,"'"
        jne     @vw_n
@@:
        pop     edi
        add     esp,4
        pop     eax
        call    vw_sp
        cmp     al,al                   ; ZF set
        ret
@vw_n:
        pop     edi
        pop     esi
        pop     eax
        or      esp,esp                 ; ZF clear
        ret

; ::::::::::::::
; vw_order - the text's module lines first, then the procedures', each
; from its SUB or FUNCTION line to its END; in their order.
; ::::::::::::::
vw_order:
        call    tx_whole
        mov     edi,offset g_wrkb
        xor     edx,edx                 ; DL 0: the module's lines, 1 the rest
@vor_p:
        xor     eax,eax
        xor     dh,dh                   ; DH 1: in a procedure
@vor_l:
        cmp     ax,[g_ui.tx.tx_all]
        jae     @vor_n
        push    eax
        call    vw_kind
        mov     cl,dh
        cmp     al,VK_END
        je      @vor_e
        cmp     al,VK_SUB
        jb      @vor_k
        mov     dh,1
        mov     cl,1
        jmp     @vor_k
@vor_e:
        xor     dh,dh                   ; the END is the procedure's
@vor_k:
        pop     eax
        cmp     cl,dl
        jne     @vor_s
        push    esi
        call    tx_offa                 ; its bytes with their CR
        call    tx_len
        inc     ecx
        lea     esi,[g_txb+ebx]
        rep     movsb
        pop     esi
@vor_s:
        inc     eax
        jmp     @vor_l
@vor_n:
        inc     dl
        cmp     dl,2
        jb      @vor_p
        mov     esi,offset g_wrkb
        mov     ecx,edi
        sub     ecx,esi
        mov     edi,offset g_txb
        rep     movsb
        mov     [g_ui.tx.tx_ci],0
        mov     [g_ui.tx.tx_co],0
        ret

; ::::::::::::::
; vw_scan - the views from the text: the module, then a view from each
; SUB or FUNCTION line; the module never empty.
; ::::::::::::::
vw_scan:
        call    tx_whole
        and     [g_ui.tx.tx_hook],0     ; not told of the module's line
        xor     eax,eax
        call    vw_kind
        cmp     al,VK_SUB
        jb      @vws_s
        cmp     al,VK_END
        jae     @vws_s
        xor     eax,eax                 ; a blank line before
        xor     ecx,ecx
        call    tx_insl
@vws_s:
        mov     [g_ui.tx.tx_hook],offset vw_shift
        mov     edi,offset g_ui.vw.vw_tab
        mov     [edi].TVIEW.vfirst,0
        mov     [edi].TVIEW.vkind,0
        mov     byte ptr [edi].TVIEW.vname,0
        mov     ebx,1                   ; EBX the views
        mov     eax,1
@vws_l:
        cmp     ax,[g_ui.tx.tx_all]
        jae     @vws_d
        push    eax
        call    vw_kind
        cmp     al,VK_SUB
        jb      @vws_n
        cmp     al,VK_END
        jae     @vws_n
        cmp     ebx,NVIEW
        jae     @vws_n
        add     edi,size TVIEW
        mov     [edi].TVIEW.vkind,al
        mov     eax,[esp]
        mov     [edi].TVIEW.vfirst,ax
        push    edi                     ; its name as typed, to a space or a (
        sub     esi,offset g_ui.bf.vw_ln
        add     esi,[g_ui.vw.vw_src]
        lea     edi,[edi].TVIEW.vname
        mov     ecx,VNAME
@vws_c:
        lodsb
        cmp     al,NL
        je      @vws_e
        cmp     al,' '
        je      @vws_e
        cmp     al,'('
        je      @vws_e
        stosb
        loop    @vws_c
@vws_e:
        mov     byte ptr [edi],0
        pop     edi
        inc     ebx
@vws_n:
        pop     eax
        inc     eax
        jmp     @vws_l
@vws_d:
        mov     [g_ui.vw.vw_n],bl
        mov     edi,offset g_ui.vw.vw_tab       ; each one's lines: to the next
        mov     ecx,ebx
@vws_m:
        mov     ax,[g_ui.tx.tx_all]
        cmp     ecx,1
        je      @F
        mov     ax,[edi+size TVIEW].TVIEW.vfirst
@@:
        sub     ax,[edi].TVIEW.vfirst
        mov     [edi].TVIEW.vn,ax
        add     edi,size TVIEW
        loop    @vws_m
        ret

; ::::::::::::::
; vw_shift - tx_hook: line EAX of the text added (EDX 1) or removed (-1)
; in the active window's view: it and the views after it move.
; ::::::::::::::
vw_shift:
        movzx   ebx,[g_ui.ed.ed_vw]
        imul    edi,ebx,size TVIEW
        add     edi,offset g_ui.vw.vw_tab
        add     [edi].TVIEW.vn,dx
@vsh_l:
        inc     ebx
        cmp     bl,[g_ui.vw.vw_n]
        jae     @vsh_r
        add     edi,size TVIEW
        add     [edi].TVIEW.vfirst,dx
        jmp     @vsh_l
@vsh_r:
        ret

; ::::::::::::::
; vw_apply - the active window's view, the text's.
;
; keeps:  all
; ::::::::::::::
vw_apply:
        push    eax
        push    edx
        push    edi
        movzx   eax,[g_ui.ed.ed_vw]
        cmp     al,[g_ui.vw.vw_n]
        jb      @F
        xor     eax,eax
        mov     [g_ui.ed.ed_vw],al
@@:
        imul    edi,eax,size TVIEW
        add     edi,offset g_ui.vw.vw_tab
        mov     ax,[edi].TVIEW.vfirst
        mov     dx,[edi].TVIEW.vn
        call    tx_view
        pop     edi
        pop     edx
        pop     eax
        ret

; vw_ptr - EDI = view EBX's TVIEW. Keeps all but EDI.
vw_ptr:
        imul    edi,ebx,size TVIEW
        add     edi,offset g_ui.vw.vw_tab
        ret

; ::::::::::::::
; vw_show - view EBX in the active window, the cursor where it was left.
; ::::::::::::::
vw_show:
        push    ebx
        call    ed_commit
        movzx   ebx,[g_ui.ed.ed_vw]     ; the place left
        call    vw_ptr
        mov     ax,[g_ui.ed.ed_line]
        mov     [edi].TVIEW.vline,ax
        mov     ax,[g_ui.ed.ed_top]
        mov     [edi].TVIEW.vtop,ax
        mov     ax,[g_ui.bf.ed_lb.col]
        mov     [edi].TVIEW.vcol,ax
        pop     ebx
        mov     [g_ui.ed.ed_vw],bl
        call    vw_apply
        call    vw_ptr
        and     [g_ui.ed.sel_on],0
        and     [g_ui.ed.ed_left],0
        and     [g_ui.lp.ui_foc],0
        mov     ax,[edi].TVIEW.vtop
        mov     [g_ui.ed.ed_top],ax
        mov     ax,[edi].TVIEW.vcol
        mov     [g_ui.bf.ed_lb.col],ax
        mov     ax,[edi].TVIEW.vline
        cmp     ax,[g_ui.tx.tx_n]
        jb      @F
        xor     ax,ax
        mov     [g_ui.ed.ed_top],ax
@@:
        call    ed_load
        jmp     ed_ensure

; ::::::::::::::
; vw_goline - text line AX in the active window, its view shown.
;
; out:    AX  its line in the view
; ::::::::::::::
vw_goline:
        push    eax
        xor     ebx,ebx                 ; the last view from at or before it
@vgl_l:
        inc     ebx
        cmp     bl,[g_ui.vw.vw_n]
        jae     @vgl_f
        call    vw_ptr
        cmp     ax,[edi].TVIEW.vfirst
        jae     @vgl_l
@vgl_f:
        dec     ebx
        cmp     bl,[g_ui.ed.ed_vw]
        je      @F
        call    vw_show
@@:
        call    vw_apply
        pop     eax
        sub     ax,[g_ui.tx.tx_v0]
        ret

; ::::::::::::::
; vw_title - the active window's view's name after the file's at EDI.
;
; in:     EDI  past the file's name's 0
; ::::::::::::::
vw_title:
        push    ebx
        movzx   ebx,[g_ui.ed.ed_vw]
        test    ebx,ebx
        jz      @vtt_r
        push    edi
        call    vw_ptr
        lea     esi,[edi].TVIEW.vname
        pop     edi
        mov     byte ptr [edi-1],':'
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
@vtt_r:
        pop     ebx
        ret

; ::::::::::::::
; vw_next - Shift+F2: the next view; vw_prev - Ctrl+F2: the one before.
; ::::::::::::::
vw_next:
        movzx   ebx,[g_ui.ed.ed_vw]
        inc     ebx
        cmp     bl,[g_ui.vw.vw_n]
        jb      vw_show
        xor     ebx,ebx
        jmp     vw_show
vw_prev:
        movzx   ebx,[g_ui.ed.ed_vw]
        dec     ebx
        jns     vw_show
        movzx   ebx,[g_ui.vw.vw_n]
        dec     ebx
        jmp     vw_show

; ::::::::::::::
; vw_commit - the line just stored: a SUB or FUNCTION typed in the module
; becomes a procedure of its own, shown, as QBasic does.
; ::::::::::::::
vw_commit:
        cmp     [g_ui.ed.ed_vw],0
        jne     @vcm_r
        mov     ax,[g_ui.ed.ed_line]
        add     ax,[g_ui.tx.tx_v0]
        push    eax
        call    vw_kind
        mov     dl,al
        pop     eax
        cmp     dl,VK_SUB
        jb      @vcm_r
        cmp     dl,VK_END
        jae     @vcm_r
        push    edx
        mov     edi,offset g_ui.bf.vw_new       ; its text
        mov     ax,[g_ui.ed.ed_line]
        call    tx_get
        mov     [g_ui.vw.vw_nlen],cx
        mov     ax,[g_ui.ed.ed_line]    ; out of the module
        call    tx_dell
        pop     edx
        jmp     vw_add
@vcm_r:
        ret

; ::::::::::::::
; vw_newsub - Edit, New SUB; vw_newfn - Edit, New FUNCTION: its name asked,
; then its view, the name's own if it has one.
; ::::::::::::::
vw_newsub:
        mov     dl,VK_SUB
        mov     esi,offset u_nsub
        jmp     @vns_g
vw_newfn:
        mov     dl,VK_FN
        mov     esi,offset u_nfn
@vns_g:
        push    edx
        mov     ebx,offset dg_vnew
        mov     [ebx].TDLG.ttl,esi
        mov     byte ptr [ebx].TDLG.foc,0
        and     [g_ui.bf.vw_fld.len],0
        and     [g_ui.bf.vw_fld.col],0
        call    dl_run
        pop     edx
        cmp     al,1
        jne     @vns_r
        movzx   ecx,[g_ui.bf.vw_fld.len]
        jecxz   @vns_r
        push    edx
        call    vw_find
        pop     edx
        jnc     vw_show
        mov     esi,offset u_kwsub      ; "SUB name"
        cmp     dl,VK_SUB
        je      @F
        mov     esi,offset u_kwfn
@@:
        mov     edi,offset g_ui.bf.vw_new
        call    vw_cpy
        mov     al,' '
        stosb
        mov     esi,offset g_ui.bf.vw_fld.txt
        movzx   ecx,[g_ui.bf.vw_fld.len]
        rep     movsb
        sub     edi,offset g_ui.bf.vw_new
        mov     [g_ui.vw.vw_nlen],di
        jmp     vw_add
@vns_r:
        ret

; vw_cpy - the 0-ended text at ESI to EDI, its 0 not. Keeps all but ESI, EDI.
vw_cpy:
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

; vw_find - the view named as the field: EBX it, CF clear; CF set: none.
vw_find:
        mov     ebx,1
@vfd_l:
        cmp     bl,[g_ui.vw.vw_n]
        jae     @vfd_n
        call    vw_ptr
        lea     esi,[edi].TVIEW.vname
        mov     edi,offset g_ui.bf.vw_fld.txt
        movzx   ecx,[g_ui.bf.vw_fld.len]
@vfd_c:
        lodsb
        call    ui_upper
        mov     ah,al
        mov     al,[edi]
        call    ui_upper
        cmp     al,ah
        jne     @vfd_x
        inc     edi
        loop    @vfd_c
        cmp     byte ptr [esi],0
        jne     @vfd_x
        clc
        ret
@vfd_x:
        inc     ebx
        jmp     @vfd_l
@vfd_n:
        stc
        ret

; ::::::::::::::
; vw_add - a procedure at the text's end: vw_new's line (vw_nlen), a
; blank, its END (DL VK_SUB, VK_FN); shown, the cursor on the blank.
; ::::::::::::::
vw_add:
        push    edx
        call    tx_whole
        and     [g_ui.tx.tx_hook],0
        mov     ax,[g_ui.tx.tx_all]
        mov     esi,offset g_ui.bf.vw_new
        mov     cx,[g_ui.vw.vw_nlen]
        call    tx_insl
        inc     ax
        xor     ecx,ecx
        call    tx_insl
        inc     ax
        pop     edx
        mov     esi,offset u_endsub
        mov     cx,7
        cmp     dl,VK_SUB
        je      @F
        mov     esi,offset u_endfn
        mov     cx,12
@@:
        call    tx_insl
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        call    vw_scan
        movzx   ebx,[g_ui.vw.vw_n]      ; the last view, at its blank
        dec     ebx
        call    vw_ptr
        mov     [edi].TVIEW.vline,1
        mov     [edi].TVIEW.vtop,0
        mov     [edi].TVIEW.vcol,0
        call    vw_apply                ; the old view's place kept
        jmp     vw_show

; ::::::::::::::
; vw_subs - View, SUBs, F2: the module and the procedures listed; Edit in
; Active, Edit in Split, Delete.
; ::::::::::::::
vw_subs:
        call    ed_commit
        movzx   ecx,[g_ui.vw.vw_n]      ; the list: the file's name, then
        mov     [g_ui.dg.ls_v+TLIST.n],cx       ; each procedure's indented
        xor     ebx,ebx
        mov     edi,offset g_ui.bf.vw_list
@vsb_l:
        push    edi
        test    ebx,ebx
        jnz     @vsb_p
        mov     esi,offset g_ui.bf.ui_fn        ; the module: the file's name
        cmp     byte ptr [esi],0
        jne     @vsb_c
        mov     esi,offset u_untl
        jmp     @vsb_c
@vsb_p:
        mov     ax,'  '
        stosw
        push    edi
        call    vw_ptr
        lea     esi,[edi].TVIEW.vname
        pop     edi
@vsb_c:
        lodsb
        stosb
        test    al,al
        jnz     @vsb_c
        pop     edi
        add     edi,VLSZ
        inc     ebx
        cmp     ebx,ecx
        jb      @vsb_l
        mov     al,[g_ui.ed.ed_vw]
        movzx   eax,al
        mov     [g_ui.dg.ls_v+TLIST.sel],ax
        and     [g_ui.dg.ls_v+TLIST.top],0
        mov     [g_ui.dg.ls_v+TLIST.items],offset g_ui.bf.vw_list
        mov     [g_ui.dg.ls_v+TLIST.isz],VLSZ
        mov     ebx,offset dg_subs
        mov     byte ptr [ebx].TDLG.foc,0
        call    dl_run
        movzx   ebx,[g_ui.dg.ls_v+TLIST.sel]
        cmp     al,1
        je      vw_show
        cmp     al,2
        je      @vsb_s
        cmp     al,3
        je      vw_del
        ret
@vsb_s:                                 ; Edit in Split: the split's other
        push    ebx                     ; window shows it
        cmp     [g_ui.wn.win+W_ED2*size TWIN].wh,0
        jne     @F
        call    wn_split
@@:
        mov     ebx,W_ED2
        cmp     [g_ui.wn.wact],bl
        jne     @F
        mov     ebx,W_ED1
@@:
        call    wn_act
        pop     ebx
        jmp     vw_show

; ::::::::::::::
; vw_del - view EBX's procedure out of the text, after asking; the module
; stays.
; ::::::::::::::
vw_del:
        test    ebx,ebx
        jz      @vdl_r
        push    ebx
        mov     esi,offset u_delp
        call    dl_ync
        pop     ebx
        cmp     al,0
        jne     @vdl_r
        call    vw_ptr
        movzx   eax,[edi].TVIEW.vfirst
        movzx   ecx,[edi].TVIEW.vn
        call    tx_whole
        and     [g_ui.tx.tx_hook],0
@@:
        call    tx_dell
        loop    @B
        mov     [g_ui.tx.tx_mod],1
        mov     [g_ui.tx.tx_chg],1
        call    vw_scan
        xor     al,al                   ; the windows on it: the module
        mov     [g_ui.ed.ed_vw],al
        mov     [g_ui.wn.win+W_ED1*size TWIN+TWIN.wed.ed_vw],al
        mov     [g_ui.wn.win+W_ED2*size TWIN+TWIN.wed.ed_vw],al
        call    vw_apply
        xor     ax,ax
        mov     [g_ui.ed.ed_top],ax
        call    ed_load
@vdl_r:
        ret

        .data
u_kwsub db      'SUB',0
u_kwfn  db      'FUNCTION',0
u_kwend db      'END',0
u_endsub db     'END SUB'
u_endfn db      'END FUNCTION'
u_nsub  db      'New SUB',0
u_nfn   db      'New FUNCTION',0
u_vname db      '&Name:',0
u_subs  db      'SUBs',0
u_vpick db      'Choose program item to edit',0
u_vact  db      'Edit in &Active',0
u_vspl  db      'Edit in &Split',0
u_vdel  db      '&Delete',0
u_delp  db      'Delete the procedure?',0
; the New SUB, New FUNCTION box (uiview.c)
dg_vnew TDLG    <8,14,8,52,u_nsub,4,0,5,1>
        TCTL    <CT_FLD,1,10,38,1,0,g_ui.bf.vw_fld,0,u_vname,2,2>
        TCTL    <CT_BTN,6,8,6,0,0,u_ok,0,0,0,0>
        TCTL    <CT_BTN,6,20,10,0,0,u_cancel,0,0,0,0>
        TCTL    <CT_BTN,6,36,8,0,0,u_bhelp,dl_bhelp,0,0,0>
; the SUBs box
dg_subs TDLG    <3,9,18,62,u_subs,5,0,15,1>
        TCTL    <CT_LST,2,2,58,12,0,g_ui.dg.ls_v,0,u_vpick,1,2>
        TCTL    <CT_BTN,16,2,17,0,0,u_vact,0,0,0,0>
        TCTL    <CT_BTN,16,20,16,0,0,u_vspl,0,0,0,0>
        TCTL    <CT_BTN,16,37,10,0,0,u_vdel,0,0,0,0>
        TCTL    <CT_BTN,16,48,10,0,0,u_cancel,0,0,0,0>
        .code

        end
