; ==========================================================================
; calls.asm - the CALL routines of statements
; ==========================================================================
; holds:    sc_*: PRINT, INPUT, GET, READ, DIM, files, RUN, LIST, CONT,
;           POKE, WAIT, SYS, ^, TI, TI$, ST
; uses:     io_*, str_*, arr_dim, fil_*, prg_findw, cm_link, flt_*
; calls:
;   sc_fin, sc_sin     io_field, io_item; a bad field: io_redo
;   sc_fread           rd_item: rd_seek, rd_load
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; sc_ipr - PRINT a value ( x -- ).
; ::::::::::::::
sc_ipr:
        dpop    eax
        call    io_int
        jmp     ex_lp
; ::::::::::::::
; sc_fpr - PRINT a value ( x -- ).
; ::::::::::::::
sc_fpr:
        dpop    eax
        call    flt_out
        jmp     ex_lp
; ::::::::::::::
; sc_spr - PRINT a value ( x -- ).
; ::::::::::::::
sc_spr:
        dpop    eax
        call    str_get
        call    str_print
        jmp     ex_lp

; ::::::::::::::
; sc_pnl - PR.NL.
; ::::::::::::::
sc_pnl:
        call    io_nl
        jmp     ex_lp

; ::::::::::::::
; sc_pzone - PR.ZONE: to the next 14-column zone.
;
; in:     pcol  the column
; ::::::::::::::
sc_pzone:
        call    io_space
        mov     al,[g_state.io.pcol]
        mov     ah,0
        mov     bl,14
        div     bl
        test    ah,ah
        jnz     sc_pzone
        jmp     ex_lp

; ::::::::::::::
; sc_ptab - PR.TAB ( i -- ): to column i, from 1; past it, on the next line.
; ::::::::::::::
sc_ptab:
        dpop    eax
        dec     eax
        jg      @F
        xor     eax,eax
@@:
        cmp     al,[g_state.io.pcol]
        jae     @F
        push    eax
        call    io_nl
        pop     eax
@@:
        sub     al,[g_state.io.pcol]
        jbe     @jt_call1
        mov     cl,al
        call    io_spaces
@jt_call1:
        jmp     ex_lp

; ::::::::::::::
; sc_pspc - PR.SPC ( i -- ).
; ::::::::::::::
sc_pspc:
        dpop    eax
        mov     cl,al
        call    io_spaces
        jmp     ex_lp

; ::::::::::::::
; sc_inbeg - IN.BEGIN end:2: not direct; REDO comes back here. The end is
; where an empty line goes on.
;
; out:    iny0  SI
; error:  ?ILLEGAL DIRECT  in the direct line
; ::::::::::::::
sc_inbeg:
        call    ex_nodir
        mov     [g_state.rn.iredo],0
        lodsw
        movzx   eax,ax
        add     eax,[g_state.mm.cachebase]
        mov     [g_state.rn.inend],eax
        mov     [g_state.rn.iny0],esi
        jmp     ex_lp

; ::::::::::::::
; sc_inpr - IN.PROMPT: ? and a line.
; ::::::::::::::
sc_inpr:
        call    io_prompt
        jmp     ex_lp

; ::::::::::::::
; sc_infile - IN.FILE: INPUT#'s line.
;
; out:    infl  bit 7 set
; ::::::::::::::
sc_infile:
        or      [g_state.io.infl],80h
@@:
        call    io_read                 ; empty lines skipped
        cmp     [g_state.io.inlen],0
        je      @B
        jmp     ex_lp

; ::::::::::::::
; io_redo - after a bad field's store: ?REDO FROM START, the INPUT again.
;
; in:     iredo  set by a bad field
; ::::::::::::::
io_redo:
        cmp     [g_state.rn.iredo],0
        jne     @F
        ret
@@:
        mov     [g_state.rn.iredo],0
        mov     al,MSG_REDO
        call    io_msg
        mov     esi,[g_state.rn.iny0]
        mov     esp,[g_state.rn.rsp]
        jmp     ex_lp

; ::::::::::::::
; sc_fin - F.INPUT S.INPUT ( -- x ): the next field. An empty line
; leaves the rest of the INPUT.
;
; in:     infl  bit 7: INPUT#
; error:  ?FILE DATA  a bad number in INPUT#
; ::::::::::::::
sc_fin:
        call    io_redo
        mov     [g_state.rn.lk],0
        jmp     @inv_g
; ::::::::::::::
; sc_sin - F.INPUT S.INPUT ( -- x ): the next field. An empty line
; ::::::::::::::
sc_sin:
        call    io_redo
        mov     [g_state.rn.lk],2
        jmp     @inv_g
; ::::::::::::::
; sc_din - D.INPUT ( -- d ).
; ::::::::::::::
sc_din:
        call    io_redo
        mov     [g_state.rn.lk],3
@inv_g:
        cmp     [g_state.io.inlen],0
        je      @inv_skip
        call    io_field
        mov     [g_state.rn.isw_si],esi
        mov     esi,[g_state.io.inpos]
        add     esi,offset g_inbuf
        call    io_item
        pushf
        mov     ebx,esi
        sub     ebx,offset g_inbuf
        mov     [g_state.io.inpos],ebx
        mov     esi,[g_state.rn.isw_si]
        popf
        jc      @inv_bad
        jmp     @vpush
@inv_bad:
        test    [g_state.io.infl],80h
        jnz     @inv_fd
        mov     [g_state.rn.iredo],1    ; stored as far as it went, then REDO
@vpush:                                 ; EAX, or lk 3's DOUBLE
        cmp     [g_state.rn.lk],3
        jne     @F
        push    dword ptr [g_dval+4]
        push    dword ptr [g_dval]
        jmp     ex_lp
@@:
        dpush   eax
        jmp     ex_lp
@inv_fd:
        mov     al,E_FD
        jmp     err_raise
@inv_skip:
        mov     esp,[g_state.rn.rsp]
        mov     esi,[g_state.rn.inend]
        jmp     ex_lp

; ::::::::::::::
; sc_inend - IN.END: ?EXTRA IGNORED for fields left.
;
; in:     inpos  the last field's end
; ::::::::::::::
sc_inend:
        call    io_redo
        test    [g_state.io.infl],80h
        jnz     @jt_call2
        mov     ebx,[g_state.io.inpos]
        inc     ebx
        jz      @jt_call2
        cmp     byte ptr [g_inbuf+ebx-1],0
        je      @jt_call2
        mov     al,MSG_EXTRA
        call    io_msg
@jt_call2:
        jmp     ex_lp

; ::::::::::::::
; io_item - an item, ending at 0, ',' or ':'.
;
; in:     SI  at it
;         lk  2 reads a string
; out:    EAX  what was read
;         C    set when bad
; ::::::::::::::
io_item:
        cmp     [g_state.rn.lk],2
        jne     @F
        call    str_item
        jmp     @ii_t
@@:
        cmp     [g_state.rn.lk],3
        jne     @F
        call    dbl_in
        jmp     @ii_t
@@:
        call    flt_in
@ii_t:
        mov     [g_state.rn.fsv],eax
        call    st_eos
        jz      @ii_ok
        cmp     al,','
        je      @ii_ok
        mov     eax,[g_state.rn.fsv]
        stc
        ret
@ii_ok:
        mov     eax,[g_state.rn.fsv]
        clc
        ret

; ::::::::::::::
; sc_fget - F.GET S.GET ( -- x ): the key waiting, or none.
;
; error:  ?SYNTAX ERROR  not a digit, for a number
; ::::::::::::::
sc_fget:
        call    plt_getc
        test    al,al
        jz      @F
        xor     al,'0'
        cmp     al,10
        jae     err_sn
@@:
        xor     ah,ah
        call    flt_from16
        dpush   eax
        jmp     ex_lp
; ::::::::::::::
; sc_sget - F.GET S.GET ( -- x ): the key waiting, or none.
; ::::::::::::::
sc_sget:
        call    plt_getc
        movzx   cx,al
        mov     [g_state.sg.sln],cx
        test    al,al
        jnz     @F
        call    str_push
        dpush   eax
        jmp     ex_lp
@@:
        mov     [g_sbuf],al
        mov     ax,1
        call    str_buftmp
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_fread - F.READ S.READ ( -- x ): the next DATA item.
;
; error:  ?SYNTAX ERROR  a bad item, in the DATA line
;         ?OUT OF DATA   none left
; ::::::::::::::
sc_fread:
        mov     [g_state.rn.lk],0
        jmp     @rdv_g
; ::::::::::::::
; sc_sread - F.READ S.READ ( -- x ): the next DATA item.
; ::::::::::::::
sc_sread:
        mov     [g_state.rn.lk],2
        jmp     @rdv_g
; ::::::::::::::
; sc_dread - D.READ ( -- d ).
; ::::::::::::::
sc_dread:
        mov     [g_state.rn.lk],3
@rdv_g:
        push    esi
        call    rd_item
        call    io_item
        jc      @rdv_sn
        mov     [g_state.rn.dlp],esi
        pop     esi
        jmp     @vpush
@rdv_sn:
        mov     eax,[g_state.rn.dataat]
        mov     ebx,PROGBASE
@rdv_l:
        mov     ecx,gs:[ebx].TLINE.len
        add     ebx,ecx
        cmp     eax,ebx
        jae     @rdv_l
        sub     ebx,ecx
        mov     [g_state.rn.line],ebx
        jmp     err_sn

; ::::::::::::::
; rd_item - the next DATA item.
;
; in:     dlp, dcp  the place in rdbuf, its DATA op (0 none)
; out:    SI  at the item
; error:  ?OUT OF DATA  no item left
; ::::::::::::::
rd_item:
        mov     esi,[g_state.rn.dlp]
        test    esi,esi
        jnz     @ri_c
        mov     ebx,PROGBASE
        call    rd_seek
        cmp     [g_state.rn.dataat],0
        je      @st_call1
        mov     esi,[g_state.rn.dlp]
@ri_c:
        cmp     [g_state.rn.dyo],0
        je      @ri_i
        mov     [g_state.rn.dyo],0
        ret
@st_call1:
        jmp     err_od
@ri_i:
        call    st_get
        cmp     al,','
        jne     @ri_nx
        inc     esi
        ret
@ri_nx:
        mov     ebx,[g_state.rn.dataat]
        test    ebx,ebx
        jz      @st_call1
        call    rd_after
        jnc     @st_call1
        call    rd_load
        mov     esi,offset g_rdbuf
        ret

; ::::::::::::::
; rd_load - the text of the DATA at BX to rdbuf, up to a ':' outside quotes.
;
; in:     BX  its first byte, in the program
; out:    dcp  BX
; keeps:  SI
; ::::::::::::::
rd_load:
        mov     [g_state.rn.dataat],ebx
        push    esi
        xor     cl,cl
        mov     esi,offset g_rdbuf
@rdl_l:
        mov     al,gs:[ebx]
        inc     ebx
        test    al,al
        jz      @rdl_e
        cmp     al,'"'
        jne     @rdl_c
        xor     cl,1
        jmp     @rdl_s
@rdl_c:
        cmp     al,':'
        jne     @rdl_s
        test    cl,cl
        jz      @rdl_e
@rdl_s:
        mov     [esi],al
        inc     esi
        jmp     @rdl_l
@rdl_e:
        mov     byte ptr [esi],0
        pop     esi
        ret

; ::::::::::::::
; rd_scan - the first DATA at or after the statement starting at BX in the
; program; rd_after: after the statement BX is in.
;
; in:     BX  the statement, or a place in it
; out:    BX  the DATA's first byte
;         CF  set when there is one
; ::::::::::::::
rd_after:
        call    rd_sk
        test    al,al
        jz      @rds_n
rd_scan:
@rds_s:
        cmp     byte ptr gs:[ebx],' '
        jne     @rds_t
        inc     ebx
        jmp     @rds_s
@rds_t:
        mov     al,gs:[ebx]
        cmp     al,TK_DATA
        je      @rds_y
        test    al,al
        jz      @rds_z
        call    rd_sk
        test    al,al
        jnz     @rds_s
        jmp     @rds_n
@rds_z:
        inc     ebx
@rds_n:
        cmp     gs:[ebx].TLINE.len,0       ; the next line, or the end
        je      @rds_x
        add     ebx,HDR
        jmp     @rds_s
@rds_y:
        inc     ebx
        stc
        ret
@rds_x:
        clc
        ret

; ::::::::::::::
; rd_sk - skip a statement: BX past its ':' or its 0 (in AL).
;
; in:     BX  in the statement
; out:    AL  the ':' or 0
; ::::::::::::::
rd_sk:
        xor     cl,cl
@rsk_l:
        mov     al,gs:[ebx]
        inc     ebx
        test    al,al
        jz      @rsk_r
        cmp     al,'"'
        jne     @rsk_q
        xor     cl,1
        jmp     @rsk_l
@rsk_q:
        test    cl,cl
        jnz     @rsk_l
        cmp     al,':'
        je      @rsk_r
        cmp     al,M_INT
        jne     @rsk_v
        add     ebx,4
        jmp     @rsk_l
@rsk_v:
        cmp     al,M_VAR
        jne     @rsk_l
        movzx   eax,byte ptr gs:[ebx]
        add     ebx,eax
        add     ebx,3
        jmp     @rsk_l
@rsk_r:
        ret

; ::::::::::::::
; sc_res - RESTORE: the first DATA.
; ::::::::::::::
sc_res:
        call    rd_first
        jmp     ex_lp

; ::::::::::::::
; sc_resl - RESTORE.L ( line -- ): a number, or 10000h and a label's line.
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
sc_resl:
        dpop    eax
        cmp     eax,10000h
        jb      @srl_n
        lea     ebx,[eax-10000h]
        test    ebx,ebx
        jz      err_us
        call    rd_seek
        jmp     ex_lp
@srl_n:
        mov     [g_state.rn.lnum],ax
        call    prg_findw
        jnc     err_us
        call    rd_seek
        jmp     ex_lp

; ::::::::::::::
; rd_first - the next READ starts at the first DATA.
;
; out:    dlp  0
; ::::::::::::::
rd_first:
        and     [g_state.rn.dlp],0
        ret

; ::::::::::::::
; rd_seek - READ goes on at the first DATA at or after line BX.
;
; in:     BX  the line
; out:    dcp, dlp  the DATA, 0 when there is none; its text in rdbuf
; ::::::::::::::
rd_seek:
        cmp     gs:[ebx].TLINE.len,0
        je      @rsk_n
        add     ebx,HDR
        call    rd_scan
        jnc     @rsk_n
        call    rd_load
        jmp     @rsk_y
@rsk_n:
        mov     [g_state.rn.dataat],0
        mov     [g_rdbuf],0
@rsk_y:
        mov     [g_state.rn.dlp],offset g_rdbuf
        mov     [g_state.rn.dyo],1
        ret

; ::::::::::::::
; sc_dim - DIM ( i1..in mlo mhi id n -- ): m, bit i: subscript i is hi, base.
;
; error:  as arr_dim
; ::::::::::::::
sc_dim:
        dpop    eax
        mov     [g_dimst],eax
        and     al,3Fh
        mov     [g_state.rn.narr],al
        dpop    edx
        dpop    dword ptr [g_dmsk+4]
        dpop    dword ptr [g_dmsk]
        movzx   ecx,al
@sdm_l:
        dpop    eax                     ; lo TO hi, or hi and the base
        dpop    ebx
        push    edx
        lea     edx,[ecx-1]
        bt      dword ptr [g_dmsk],edx
        pop     edx
        jnc     @F
        xchg    eax,ebx
@@:
        sub     eax,ebx
        mov     [g_dimbuf+ecx*2-2],ax
        mov     [g_lbbuf+ecx*2-2],bx
        loop    @sdm_l
        movzx   ebx,dx
        test    [g_dimst],40h           ; REDIM: the old one goes
        jnz     @sdm_r
        test    [g_dimst],80h           ; a STATIC's, dimensioned: kept
        jz      @F
        cmp     dword ptr [g_vt+ebx*4],0
        jne     ex_lp
@@:
        call    arr_dim
        jmp     ex_lp
@sdm_r:
        mov     [g_dyn],1
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        je      @F
        test    byte ptr [g_dimst+1],1
        jnz     @sdm_p
        mov     [g_afrep],1
        call    arr_free
@@:
        push    ebx
        call    arr_dim
        pop     ebx
        call    arr_rep
        mov     [g_dyn],0
        jmp     ex_lp
@sdm_p:                                 ; PRESERVE: the new one made, the old
        mov     al,[g_state.rn.narr]    ; one's elements copied, then freed
        cmp     al,[edi]
        jne     arr_bs
        mov     [g_pold],edi
        and     dword ptr [g_vt+ebx*4],0
        push    ebx
        call    arr_dim
        pop     ebx
        mov     edi,[g_vt+ebx*4]
        mov     [g_pnew],edi
        call    arr_keep
        mov     eax,[g_pold]
        mov     [g_vt+ebx*4],eax
        mov     [g_afrep],1
        call    arr_free
        mov     eax,[g_pnew]            ; its header was after the old one's
        sub     eax,[g_afhs]
        mov     [g_vt+ebx*4],eax
        call    arr_rep
        mov     [g_dyn],0
        jmp     ex_lp

; ::::::::::::::
; sc_erase - ERASE ( id -- ): REDIM's array freed, another's elements zeroed.
; ::::::::::::::
sc_erase:
        dpop    ebx
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        jz      ex_lp
        cmp     [g_adyn+ebx],0
        je      @ser_z
        and     [g_afrep],0
        call    arr_free
        jmp     ex_lp
@ser_z:
        call    arr_size
        mov     ecx,eax
        mov     eax,[edi+AH_PTR]
        call    mem_zero
        jmp     ex_lp
        .data?
g_dimst dd      ?                       ; DIM's count: bit 6 REDIM, bit 7 a STATIC's,
                                        ; bit 8 PRESERVE
g_dmsk  dq      ?                       ; bit i: subscript i is hi, base
        .code

; ::::::::::::::
; sc_runl - RUN.L ( line -- ).
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
sc_runl:
        dpop    eax
        cmp     eax,10000h
        jae     err_us                  ; RUN takes a number, not a label
        mov     [g_state.rn.runl],ax
        call    var_clr
        call    cm_link
        mov     ax,[g_state.rn.runl]
        mov     [g_state.rn.lnum],ax
        call    prg_findw
        jnc     err_us

; ::::::::::::::
; ex_rungo - both stacks empty, run the line at BX.
;
; in:     BX  the line
; ::::::::::::::
ex_rungo:
        mov     esp,[g_state.rn.rsp]
        call    ev_clr
        call    chn_start
        and     [g_state.rn.onerr],0
        and     [g_state.rn.inerr],0
        and     [g_state.rn.errn],0
        and     [g_state.rn.erl],0
        mov     [g_state.rn.cstk],CSTOP
        mov     [g_state.rn.lsp],offset g_lsav
        and     [g_state.sg.tspb],0
        jmp     ex_jump

; ::::::::::::::
; sc_run - RUN.
; ::::::::::::::
sc_run:
        call    var_clr
        call    cm_link

; ::::::::::::::
; ex_runfirst - run from the first line.
; ::::::::::::::
ex_runfirst:
        call    ch_clall
        mov     ebx,PROGBASE
        cmp     gs:[ebx].TLINE.len,0
        jne     ex_rungo
        jmp     ex_end

; ::::::::::::::
; sc_list - LIST ( from to -- ), then READY.
; ::::::::::::::
sc_list:
        dpop    eax
        mov     [g_state.rn.lto],ax
        dpop    eax
        mov     [g_state.rn.lfrom],ax
        call    ls_range
        jmp     ex_ready

; ::::::::::::::
; sc_cont - CONT.
;
; in:     contip, contsi  where it stopped
; error:  ?CAN'T CONTINUE  no place
; ::::::::::::::
sc_cont:
        mov     ebx,[g_state.rn.contln]
        test    ebx,ebx
        jz      err_cn
        mov     eax,[g_state.rn.contcode]
        test    eax,eax
        jz      ex_ready
        mov     [g_state.rn.line],ebx
        mov     edx,ebx
        ipload
        jmp     ex_lp

; ::::::::::::::
; sc_clr - CLR: the variables, and the frames.
; ::::::::::::::
sc_clr:
        call    var_clr
        mov     [g_state.rn.cstk],CSTOP
        jmp     ex_lp

; ::::::::::::::
; sc_new - NEW.
; ::::::::::::::
sc_new:
        call    prg_init
        jmp     ex_end

; ::::::::::::::
; sc_nodir - NODIR: ?ILLEGAL DIRECT in the direct line.
; ::::::::::::::
sc_nodir:
        call    ex_nodir
        jmp     ex_lp

; ::::::::::::::
; sc_end - END: CONT goes on after it.
;
; in:     ln, SI  where CONT goes on
; ::::::::::::::
sc_end:
        xor     al,al
        cmp     [g_dbev],0              ; the probe's line ended
        jne     dbg_evx
        call    ch_clall
        call    ex_setcont

; ::::::::::::::
; ex_ready - back to READY, the frames kept.
; ::::::::::::::
ex_ready:
        call    ev_clr
        and     [g_state.rn.stpflg],not SF_EV
        jmp     rpl_ready

; ::::::::::::::
; sc_stop - STOP.
;
; in:     ln, SI  where CONT goes on
; error:  BREAK  always
; ::::::::::::::
sc_stop:
        mov     [g_state.rn.stpflg],0
        call    ex_setcont
        mov     al,E_BRK
        jmp     err_raise

; ::::::::::::::
; sc_poke - POKE ( addr byte -- ).
; ::::::::::::::
sc_poke:
        dpop    eax
        dpop    ebx
        mov     [ebx],al                ; a flat address
        jmp     ex_lp

; ::::::::::::::
; sc_wait - WAIT ( port mask xor -- ); sc_wait2 ( port mask -- ): until
; (INP XOR xor) AND mask.
; ::::::::::::::
sc_wait2:
        xor     eax,eax
        jmp     @swt_g
sc_wait:
        dpop    eax
@swt_g:
        mov     ecx,eax
        dpop    ebx
        dpop    edx
@@:
        in      al,dx
        xor     al,cl
        and     al,bl
        jz      @B
        jmp     ex_lp

; ::::::::::::::
; sc_inp - INP ( port -- b ); sc_out - OUT ( port b -- ).
; ::::::::::::::
sc_inp:
        dpop    edx
        in      al,dx
        movzx   eax,al
        dpush   eax
        jmp     ex_lp
sc_out:
        dpop    eax
        dpop    edx
        out     dx,al
        jmp     ex_lp

; ::::::::::::::
; sc_sys - SYS ( addr -- ): a call into the POKE memory.
; ::::::::::::::
sc_sys:
        dpop    eax
        movzx   ebx,ax
        cmp     [g_pokeb+ebx],0            ; nothing poked there: a no-op
        je      @jt_call3
        add     ebx,offset g_pokeb
        pushad
        call    ebx
        popad
@jt_call3:
        jmp     ex_lp

; ::::::::::::::
; sc_inln - IN.LINE: a line, no ?; INPUT "prompt", ..
; ::::::::::::::
sc_inln:
        call    io_read
        jmp     ex_lp

; ::::::::::::::
; sc_ipow - I.POW ( base power -- r ): truncated for a negative power.
;
; error:  ?OVERFLOW  past an INT
; ::::::::::::::
sc_ipow:
        dpop    ecx
        dpop    edx
        mov     eax,1
        test    ecx,ecx
        jz      @sip_r
        cmp     edx,1
        je      @sip_r
        xor     eax,eax
        test    edx,edx
        jz      @sip_r
        cmp     edx,-1
        jne     @sip_b
        mov     eax,1                   ; (-1)^n
        test    cl,1
        jz      @sip_r
        neg     eax
        jmp     @sip_r
@sip_b:
        test    ecx,ecx
        js      @sip_r                  ; |base| over 1, a negative power: 0
        mov     eax,1
@sip_l:
        imul    eax,edx
        jo      err_ov
        loop    @sip_l
@sip_r:
        dpush   eax
        dispatch

; ::::::::::::::
; sc_fpow - F.POW ( base power -- r ).
; ::::::::::::::
sc_fpow:
        dpop    eax
        dpop    edx
        call    fn_pow
        dpush   eax
        dispatch

; ::::::::::::::
; sc_fti - F.LD.TI ( -- f ): the jiffies.
; ::::::::::::::
sc_fti:
        call    plt_rdtim
        test    eax,eax
        jz      @F
        xor     ebp,ebp
        call    flt_norm
@@:
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_sti - S.LD.TI ( -- s ): "HHMMSS".
; ::::::::::::::
sc_sti:
        call    plt_rdtim
        mov     ebx,6
@sti_l:
        mov     ecx,60
        test    ebx,ebx
        jnz     @sti_d
        mov     cl,100
@sti_d:
        xor     edx,edx
        div     ecx
        push    eax
        mov     ax,dx
        mov     cl,10
        div     cl
        or      ax,3030h
        mov     word ptr [g_sbuf+ebx],ax
        pop     eax
        sub     ebx,2
        jns     @sti_l
        mov     ax,6
        call    str_buftmp
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_ist - I.LD.ST ( -- i ): the I/O status.
; ::::::::::::::
sc_ist:
        movzx   eax,[g_state.io.iost]
        dpush   eax
        jmp     ex_lp

        .data
g_relbit db      1,2,4
        .code

        .data?
g_rdbuf db      256 dup(?)
        .code

        end
