; ==========================================================================
; dbg.asm - the debugger's UI: the Debug menu, F7 to F10, the lit statement
; ==========================================================================
; holds:    ud_init, ud_prep, ud_stop, the Debug menu's commands
; uses:     vm/dbg.asm (g_state.dbg, dbg_*), tx_mark, sy_walk, sy_find,
;           lx_line's lxcol, ui_contp, ui_runp
; data:     A breakpoint is a text line's mark (text.asm), so it moves with
;           the line; ud_prep gives the VM their line numbers before each
;           run. A stop (STOP, Ctrl+Break, a step, a breakpoint) or a trace
;           lights the statement: hl_ln, columns hl_c0 to hl_c1, found by
;           tokenizing the line again: the VM knows the statement by its
;           body offset, the tokenizer's lxcol has its column.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; ud_init - the VM's statements stoppable, its trace to ud_trace.
; ::::::::::::::
ud_init:
        mov     [g_state.dbg.emit],1
        mov     [g_state.dbg.hook],offset ud_trace
        ret

; ::::::::::::::
; ud_prep - before a run or CONT: the VM's breakpoints and until from the
; text, nothing lit.
; ::::::::::::::
ud_prep:
        call    dp_close
        and     [g_ui.ud.hl_on],0
        and     [g_ui.ud.stopd],0
        call    dbg_bpclr
        and     [g_state.dbg.utl],0
        mov     edx,offset ud_line
        jmp     sy_walk

; ud_line - sy_walk's: text line AX, numbered DX, to the VM.
ud_line:
        push    eax
        call    tx_mark
        test    al,al
        pop     eax
        jz      @F
        push    eax
        mov     ax,dx
        call    dbg_bpset
        pop     eax
@@:
        inc     ax
        cmp     ax,[g_ui.ud.ud_utl]
        jne     @F
        mov     [g_state.dbg.ulnum],dx
        mov     [g_state.dbg.utl],1
@@:
        ret

; ::::::::::::::
; ud_stop - a stop with CONT to go on: the debug pane, its statement lit,
; the cursor at it.
; ::::::::::::::
ud_stop:
        and     [g_ui.ud.ud_utl],0
        mov     ebx,[g_state.rn.contln]
        test    ebx,ebx
        jz      @F
        mov     [g_ui.ud.stopd],1
        call    dp_open
        mov     ebx,[g_state.rn.contln]
        mov     eax,[g_state.rn.contcode]
        add     eax,[g_state.mm.cachebase]
        call    dbg_pos
        jmp     ud_hlset
@@:
        ret

; ::::::::::::::
; ud_hlset - the statement at wnum, wb0, wb1 lit, the cursor at its start.
; ::::::::::::::
ud_hlset:
        mov     ax,[g_state.dbg.wnum]
        call    sy_find
        jc      @uhs_r
        mov     [g_ui.ud.hl_ln],ax      ; the text's line
        call    vw_goline
        mov     edi,offset g_ui.bf.uc_line
        call    tx_get
        mov     byte ptr [edi+ecx],0
        call    ui_body                 ; the body, as the editor stores it
        mov     esi,edi
        push    ecx
        push    esi
        mov     edi,offset g_cbuf
        call    lx_line
        pop     ebx
        pop     ecx
        sub     ebx,offset g_ui.bf.uc_line      ; EBX the body's column
        mov     eax,[g_state.dbg.stmtfrom]
        movzx   eax,word ptr [g_lxcol+eax*2]
        add     eax,ebx
        mov     [g_ui.ud.hl_c0],ax
        mov     edx,[g_state.dbg.stmtto]
        cmp     edx,BOFF
        je      @uhs_e
        movzx   edx,word ptr [g_lxcol+edx*2]
        add     edx,ebx
        jmp     @uhs_t
@uhs_e:
        mov     edx,ecx
@uhs_t:                                 ; back over the ':' and spaces
        cmp     edx,eax
        jbe     @uhs_c
        mov     cl,[g_ui.bf.uc_line+edx-1]
        cmp     cl,' '
        je      @F
        cmp     cl,':'
        jne     @uhs_c
@@:
        dec     edx
        jmp     @uhs_t
@uhs_c:
        mov     [g_ui.ud.hl_c1],dx
        mov     [g_ui.ud.hl_on],1
        and     [g_ui.lp.ui_foc],0
        and     [g_ui.ed.sel_on],0
        mov     ax,[g_ui.ud.hl_ln]
        sub     ax,[g_ui.tx.tx_v0]
        call    ed_load
        mov     ax,[g_ui.ud.hl_c0]
        mov     [g_ui.bf.ed_lb.col],ax
        call    ed_ensure
@uhs_r:
        ret

; ::::::::::::::
; ud_trace - the VM's hook while tracing: the statement lit on the editor's
; page a moment, then the output's again.
; ::::::::::::::
ud_trace:
        pushad
        call    ud_hlset
        call    wn_draw
        call    sc_flip
        mov     ax,0500h
        int     10h
        mov     eax,ds:[46Ch]           ; two ticks
        add     eax,2
@@:
        cmp     eax,ds:[46Ch]
        ja      @B
        mov     ax,0501h
        int     10h
        and     [g_ui.ud.hl_on],0
        popad
        ret

; ::::::::::::::
; ud_row - text row DX of the program window, line dr_ln: a breakpoint's
; colour, the lit statement's.
;
; keeps:  all
; ::::::::::::::
ud_row:
        pushad
        mov     ax,[g_ui.lp.dr_ln]
        call    tx_mark
        test    al,al
        jz      @F
        mov     ah,A_BPL
        mov     cx,[g_ui.ed.ed_tw]
        call    sc_attr
@@:
        cmp     [g_ui.ud.hl_on],0
        je      @udr_r
        cmp     [g_ui.tx.tx_mod],0
        jne     @udr_r
        mov     ax,[g_ui.lp.dr_ln]
        add     ax,[g_ui.tx.tx_v0]
        cmp     ax,[g_ui.ud.hl_ln]
        jne     @udr_r
        mov     ax,[g_ui.lp.dr_ln]
        cmp     ax,[g_ui.ed.ed_line]    ; not while it is being changed
        jne     @F
        cmp     byte ptr [g_ui.bf.ed_lb.dty],0
        jne     @udr_r
@@:
        mov     ax,[g_ui.ud.hl_c0]      ; clipped to the window
        mov     bx,[g_ui.ud.hl_c1]
        sub     ax,[g_ui.ed.ed_left]
        jae     @F
        xor     ax,ax
@@:
        sub     bx,[g_ui.ed.ed_left]
        jbe     @udr_r
        cmp     bx,[g_ui.ed.ed_tw]
        jbe     @F
        mov     bx,[g_ui.ed.ed_tw]
@@:
        cmp     ax,bx
        jae     @udr_r
        add     dl,al
        mov     cx,bx
        sub     cx,ax
        mov     ah,A_CUR
        call    sc_attr
@udr_r:
        popad
        ret

; ::::::::::::::
; ud_step - Debug, Step, F8: the next statement, into a procedure.
; ::::::::::::::
ud_step:
        mov     [g_state.dbg.mode],DM_STEP
        jmp     ui_contp

; ::::::::::::::
; ud_pstep - Debug, Step Over, F7, F10: the next statement at this depth
; or less: a call, a GOSUB run through.
; ::::::::::::::
ud_pstep:
        xor     eax,eax
        call    ui_cancont
        jc      @F
        call    dbg_depth
@@:
        mov     [g_state.dbg.pdep],eax
        mov     [g_state.dbg.mode],DM_PSTEP
        jmp     ui_contp

; ::::::::::::::
; ud_sout - Debug, Step Out, Shift+F8: on till the procedure or GOSUB
; stopped in has returned; in the module, as Step Over.
; ::::::::::::::
ud_sout:
        xor     eax,eax
        call    ui_cancont
        jc      @F
        call    dbg_depth
        dec     eax
        jns     @F
        xor     eax,eax
@@:
        mov     [g_state.dbg.pdep],eax
        mov     [g_state.dbg.mode],DM_PSTEP
        jmp     ui_contp

; ::::::::::::::
; ud_restart - Run, Restart: from the start, stopped at its first statement.
; ::::::::::::::
ud_restart:
        mov     [g_state.dbg.mode],DM_STEP
        jmp     ui_runp

; ::::::::::::::
; ud_trace1 - Debug, Trace On: on, off; TRON and TROFF the same flag.
; ::::::::::::::
ud_trace1:
        xor     [g_state.dbg.trace],1
        ret

; ::::::::::::::
; ud_bp - Debug, Toggle Breakpoint, F9: the cursor's line.
; ::::::::::::::
ud_bp:
        cmp     [g_ui.lp.ui_foc],0
        jne     @F
        mov     ax,[g_ui.ed.ed_line]
        jmp     tx_mtog
@@:
        ret

; ::::::::::::::
; ud_bpclr - Debug, Clear All Breakpoints.
; ::::::::::::::
ud_bpclr:
        jmp     tx_mclr

; ::::::::::::::
; ud_until - Ctrl+F7: on to the cursor's line.
; ::::::::::::::
ud_until:
        cmp     [g_ui.lp.ui_foc],0
        jne     @F
        mov     ax,[g_ui.ed.ed_line]
        add     ax,[g_ui.tx.tx_v0]
        inc     ax
        mov     [g_ui.ud.ud_utl],ax
        jmp     ui_contp
@@:
        ret

; ::::::::::::::
; ud_setnx - Debug, Set Next Statement: CONT goes on at the cursor's
; line, in the procedure stopped in.
;
; error:  a box: not stopped; another procedure's line
; ::::::::::::::
ud_setnx:
        call    ui_cancont
        jc      @usn_c
        mov     ax,[g_ui.ed.ed_line]
        add     ax,[g_ui.tx.tx_v0]
        mov     [g_ui.ud.ud_tgt],ax
        or      [g_ui.ud.ud_num],-1
        mov     edx,offset ud_numof
        call    sy_walk
        mov     ax,[g_ui.ud.ud_num]
        cmp     ax,NONUM
        je      @usn_r                  ; a blank line
        mov     [g_state.rn.lnum],ax
        and     [g_state.lnk.cursor],0
        call    prg_findw
        jnc     @usn_r
        movzx   ebx,bx
        call    bk_proc
        mov     al,[g_state.cm.cproc]
        push    eax
        push    ebx
        mov     ebx,[g_state.rn.contln]
        call    bk_proc
        pop     ebx
        pop     eax
        cmp     al,[g_state.cm.cproc]
        jne     @usn_p
        mov     esi,gs:[ebx].TLINE.code
        add     esi,[g_state.mm.cachebase]
        cmp     esi,[g_state.mm.cachebase]
        jne     @F
        call    cm_cache
@@:
        mov     [g_state.rn.contln],ebx
        mov     eax,esi
        sub     eax,[g_state.mm.cachebase]
        mov     [g_state.rn.contcode],eax
        jmp     ud_stop
@usn_p:
        mov     esi,offset u_bnext
        jmp     dl_ok
@usn_c:
        mov     esi,offset u_cncont
        jmp     dl_ok
@usn_r:
        ret

; ud_numof - sy_walk's: the number of text line ud_tgt to ud_num.
ud_numof:
        cmp     ax,[g_ui.ud.ud_tgt]
        jne     @F
        mov     [g_ui.ud.ud_num],dx
@@:
        ret

        .data
u_bnext db      'Cannot cross module/procedure boundary',0
u_cncont db     'Cannot continue',0
        .code

        end
