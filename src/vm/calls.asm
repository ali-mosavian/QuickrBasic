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
        pop     eax
        call    io_int
        jmp     ex_lp
; ::::::::::::::
; sc_fpr - PRINT a value ( x -- ).
; ::::::::::::::
sc_fpr:
        pop     eax
        call    flt_out
        jmp     ex_lp
; ::::::::::::::
; sc_spr - PRINT a value ( x -- ).
; ::::::::::::::
sc_spr:
        pop     eax
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
; sc_pzone - PR.ZONE: to the next 10-column zone.
;
; in:     pcol  the column
; ::::::::::::::
sc_pzone:
        call    io_space
        mov     al,[pcol]
        mov     ah,0
        mov     bl,10
        div     bl
        test    ah,ah
        jnz     sc_pzone
        jmp     ex_lp

; ::::::::::::::
; sc_ptab - PR.TAB ( i -- ).
; ::::::::::::::
sc_ptab:
        pop     eax
        sub     al,[pcol]
        jbe     @jt_call1
        mov     cl,al
        call    io_spaces
@jt_call1:
        jmp     ex_lp

; ::::::::::::::
; sc_pspc - PR.SPC ( i -- ).
; ::::::::::::::
sc_pspc:
        pop     eax
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
        mov     [iredo],0
        lodsw
        movzx   eax,ax
        add     eax,offset cachb
        mov     [inend],eax
        mov     [iny0],esi
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
        or      [infl],80h
        call    io_read
        jmp     ex_lp

; ::::::::::::::
; io_redo - after a bad field's store: ?REDO FROM START, the INPUT again.
;
; in:     iredo  set by a bad field
; ::::::::::::::
io_redo:
        cmp     [iredo],0
        jne     @F
        ret
@@:
        mov     [iredo],0
        mov     al,MSG_REDO
        call    io_msg
        mov     esi,[iny0]
        mov     esp,[rsp]
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
        mov     [lk],0
        jmp     @inv_g
; ::::::::::::::
; sc_sin - F.INPUT S.INPUT ( -- x ): the next field. An empty line
; ::::::::::::::
sc_sin:
        call    io_redo
        mov     [lk],2
@inv_g:
        cmp     [inlen],0
        je      @inv_skip
        call    io_field
        mov     [isw_si],esi
        movzx   esi,[inpos]
        add     esi,offset inbuf
        call    io_item
        pushf
        mov     ebx,esi
        sub     ebx,offset inbuf
        mov     [inpos],bl
        mov     esi,[isw_si]
        popf
        jc      @inv_bad
        push    eax
        jmp     ex_lp
@inv_bad:
        test    [infl],80h
        jnz     @inv_fd
        push    eax                     ; stored as far as it went, then REDO
        mov     [iredo],1
        jmp     ex_lp
@inv_fd:
        mov     al,E_FD
        jmp     err_raise
@inv_skip:
        mov     esp,[rsp]
        mov     esi,[inend]
        jmp     ex_lp

; ::::::::::::::
; sc_inend - IN.END: ?EXTRA IGNORED for fields left.
;
; in:     inpos  the last field's end
; ::::::::::::::
sc_inend:
        call    io_redo
        test    [infl],80h
        jnz     @jt_call2
        mov     bl,[inpos]
        inc     bl
        jz      @jt_call2
        movzx   ebx,bl
        cmp     byte ptr [inbuf+ebx-1],0
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
        cmp     [lk],2
        jne     @F
        call    str_item
        jmp     @ii_t
@@:
        call    flt_in
@ii_t:
        mov     [fsv],eax
        call    st_eos
        jz      @ii_ok
        cmp     al,','
        je      @ii_ok
        mov     eax,[fsv]
        stc
        ret
@ii_ok:
        mov     eax,[fsv]
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
        push    eax
        jmp     ex_lp
; ::::::::::::::
; sc_sget - F.GET S.GET ( -- x ): the key waiting, or none.
; ::::::::::::::
sc_sget:
        call    plt_getc
        movzx   cx,al
        mov     [sln],cx
        test    al,al
        jnz     @F
        call    str_push
        push    eax
        jmp     ex_lp
@@:
        mov     [sbuf],al
        mov     ax,1
        call    str_buftmp
        push    eax
        jmp     ex_lp

; ::::::::::::::
; sc_fread - F.READ S.READ ( -- x ): the next DATA item.
;
; error:  ?SYNTAX ERROR  a bad item, in the DATA line
;         ?OUT OF DATA   none left
; ::::::::::::::
sc_fread:
        mov     [lk],0
        jmp     @rdv_g
; ::::::::::::::
; sc_sread - F.READ S.READ ( -- x ): the next DATA item.
; ::::::::::::::
sc_sread:
        mov     [lk],2
@rdv_g:
        push    esi
        call    rd_item
        call    io_item
        jc      @rdv_sn
        mov     [dlp],esi
        pop     esi
        push    eax
        jmp     ex_lp
@rdv_sn:
        mov     ax,[dcp]
        mov     ebx,PROGBASE
@rdv_l:
        movzx   ecx,byte ptr [progb+ebx]
        add     ebx,ecx
        cmp     ax,bx
        jae     @rdv_l
        sub     ebx,ecx
        mov     [ln],bx
        jmp     err_sn

; ::::::::::::::
; rd_item - the next DATA item.
;
; in:     dlp, dcp  the place in rdbuf, its DATA op (0 none)
; out:    SI  at the item
; error:  ?OUT OF DATA  no item left
; ::::::::::::::
rd_item:
        mov     esi,[dlp]
        test    esi,esi
        jnz     @ri_c
        mov     ebx,PROGBASE
        call    rd_seek
        cmp     [dcp],0
        je      @st_call1
        mov     esi,[dlp]
@ri_c:
        cmp     [dyo],0
        je      @ri_i
        mov     [dyo],0
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
        movzx   ebx,[dcp]
        test    ebx,ebx
        jz      @st_call1
        call    rd_after
        jnc     @st_call1
        call    rd_load
        mov     esi,offset rdbuf
        ret

; ::::::::::::::
; rd_load - the text of the DATA at BX to rdbuf, up to a ':' outside quotes.
;
; in:     BX  its first byte, in the program
; out:    dcp  BX
; keeps:  SI
; ::::::::::::::
rd_load:
        mov     [dcp],bx
        push    esi
        xor     cl,cl
        mov     esi,offset rdbuf
@rdl_l:
        mov     al,[progb+ebx]
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
        cmp     byte ptr [progb+ebx],' '
        jne     @rds_t
        inc     ebx
        jmp     @rds_s
@rds_t:
        mov     al,[progb+ebx]
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
        cmp     byte ptr [progb+ebx],0      ; the next line, or the end
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
        mov     al,[progb+ebx]
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
        add     ebx,2
        jmp     @rsk_l
@rsk_v:
        cmp     al,M_VAR
        jne     @rsk_l
        movzx   eax,byte ptr [progb+ebx]
        add     ebx,eax
        add     ebx,2
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
; sc_resl - RESTORE.L ( line -- ).
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
sc_resl:
        pop     eax
        mov     [lnum],ax
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
        and     [dlp],0
        ret

; ::::::::::::::
; rd_seek - READ goes on at the first DATA at or after line BX.
;
; in:     BX  the line
; out:    dcp, dlp  the DATA, 0 when there is none; its text in rdbuf
; ::::::::::::::
rd_seek:
        cmp     byte ptr [progb+ebx],0
        je      @rsk_n
        add     ebx,HDR
        call    rd_scan
        jnc     @rsk_n
        call    rd_load
        jmp     @rsk_y
@rsk_n:
        mov     [dcp],0
        mov     [rdbuf],0
@rsk_y:
        mov     [dlp],offset rdbuf
        mov     [dyo],1
        ret

; ::::::::::::::
; sc_dim - DIM ( i1..in id n -- ).
;
; error:  as arr_dim
; ::::::::::::::
sc_dim:
        pop     eax
        mov     [narr],al
        pop     edx
        movzx   ebx,al
        add     ebx,ebx
@sdm_l:
        pop     eax
        mov     [dimbuf+ebx-2],ax
        sub     ebx,2
        jnz     @sdm_l
        movzx   ebx,dl
        call    arr_dim
        jmp     ex_lp

; ::::::::::::::
; sc_open - OPEN ( lfn [dev [sa [name]]] n -- ).
; ::::::::::::::
sc_open:
        call    fil_def
        pop     eax
        mov     [cnarg],al
        cmp     al,4
        jb      @sop_3
        pop     eax
        call    str_get
@sop_3:
        cmp     [cnarg],3
        jb      @sop_2
        pop     eax
        mov     [fsa],al
@sop_2:
        cmp     [cnarg],2
        jb      @sop_1
        pop     eax
        mov     [fdv],al
@sop_1:
        pop     eax
        mov     [fln],al
        call    fil_open
        jmp     ex_lp

; ::::::::::::::
; fil_args - LOAD's, SAVE's, VERIFY's ( [name [dev [sa]]] n -- ), then aret.
;
; out:    fdv, fsa, sln, spt  the file, the defaults for the rest
; ::::::::::::::
fil_args:
        call    fil_def
        pop     eax
        mov     [cnarg],al
        cmp     al,3
        jb      @fa_2
        pop     eax
        mov     [fsa],al
@fa_2:
        cmp     [cnarg],2
        jb      @fa_1
        pop     eax
        mov     [fdv],al
@fa_1:
        cmp     [cnarg],1
        jb      @fa_0
        pop     eax
        call    str_get
@fa_0:
        jmp     [aret]

; ::::::::::::::
; sc_load - LOAD ( [name [dev [sa]]] n -- ).
; ::::::::::::::
sc_load:
        mov     [aret],offset fil_load
        jmp     fil_args
; ::::::::::::::
; sc_save - SAVE ( [name [dev [sa]]] n -- ).
; ::::::::::::::
sc_save:
        mov     [aret],offset fil_save
        jmp     fil_args
; ::::::::::::::
; sc_vfy - VERIFY ( [name [dev [sa]]] n -- ).
; ::::::::::::::
sc_vfy:
        mov     [aret],offset fil_verify
        jmp     fil_args

; ::::::::::::::
; sc_close - CLOSE ( lfn -- ).
; ::::::::::::::
sc_close:
        pop     eax
        call    plt_close
        jmp     ex_lp

; ::::::::::::::
; sc_fiout - FI.OUT ( lfn -- ): output to the file.
; ::::::::::::::
sc_fiout:
        pop     eax
        call    plt_chkout
        call    fil_chk
        jmp     ex_lp

; ::::::::::::::
; sc_fiin - FI.IN ( lfn -- ): input from the file.
; ::::::::::::::
sc_fiin:
        pop     eax
        call    plt_chkin
        call    fil_chk
        jmp     ex_lp

; ::::::::::::::
; sc_ficlr - FI.CLR: back to the screen and keyboard.
; ::::::::::::::
sc_ficlr:
        call    fil_clrch
        jmp     ex_lp

; ::::::::::::::
; sc_runl - RUN.L ( line -- ).
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
sc_runl:
        pop     eax
        mov     [runl],ax
        call    var_clr
        call    cm_link
        mov     ax,[runl]
        mov     [lnum],ax
        call    prg_findw
        jnc     err_us

; ::::::::::::::
; ex_rungo - both stacks empty, run the line at BX.
;
; in:     BX  the line
; ::::::::::::::
ex_rungo:
        mov     esp,[rsp]
        mov     [cstk],CSTOP
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
        mov     ebx,PROGBASE
        cmp     byte ptr [progb+ebx],0
        jne     ex_rungo
        jmp     ex_end

; ::::::::::::::
; sc_list - LIST ( from to -- ), then READY.
; ::::::::::::::
sc_list:
        pop     eax
        mov     [lto],ax
        pop     eax
        mov     [lfrom],ax
        call    ls_range
        jmp     ex_ready

; ::::::::::::::
; sc_cont - CONT.
;
; in:     contip, contsi  where it stopped
; error:  ?CAN'T CONTINUE  no place
; ::::::::::::::
sc_cont:
        movzx   ebx,[contip]
        test    ebx,ebx
        jz      err_cn
        mov     ax,[contsi]
        test    ax,ax
        jz      ex_ready
        mov     [ln],bx
        mov     dx,bx
        ipfix
        jmp     ex_lp

; ::::::::::::::
; sc_clr - CLR: the variables, and the frames.
; ::::::::::::::
sc_clr:
        call    var_clr
        mov     [cstk],CSTOP
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
        call    ex_setcont

; ::::::::::::::
; ex_ready - back to READY, the frames kept.
; ::::::::::::::
ex_ready:
        jmp     rpl_ready

; ::::::::::::::
; sc_stop - STOP.
;
; in:     ln, SI  where CONT goes on
; error:  BREAK  always
; ::::::::::::::
sc_stop:
        mov     [stpflg],0
        call    ex_setcont
        mov     al,E_BRK
        jmp     err_raise

; ::::::::::::::
; sc_poke - POKE ( addr byte -- ), in its own segment.
; ::::::::::::::
sc_poke:
        pop     eax
        pop     ebx
        movzx   ebx,bx
        mov     [pokeb+ebx],al
        jmp     ex_lp

; ::::::::::::::
; sc_wait - WAIT ( addr mask xor -- ), ( addr mask -- ): until
; (PEEK EOR xor) AND mask.
; ::::::::::::::
sc_wait2:
        xor     eax,eax
        jmp     @swt_g
; ::::::::::::::
; sc_wait - WAIT ( addr mask xor -- ), ( addr mask -- ): until
; ::::::::::::::
sc_wait:
        pop     eax
@swt_g:
        pop     edx
        pop     ebx
        movzx   ebx,bx
@@:
        mov     cl,[pokeb+ebx]
        xor     cl,al
        and     cl,dl
        jz      @B
        jmp     ex_lp

; ::::::::::::::
; sc_sys - SYS ( addr -- ): a call into the POKE memory.
; ::::::::::::::
sc_sys:
        pop     eax
        movzx   ebx,ax
        cmp     [pokeb+ebx],0           ; nothing poked there: a no-op
        je      @jt_call3
        add     ebx,offset pokeb
        pushad
        call    ebx
        popad
@jt_call3:
        jmp     ex_lp

; ::::::::::::::
; sc_stti - S.ST.TI ( s -- ): TI$ = "HHMMSS".
; ::::::::::::::
sc_stti:
        pop     eax
        mov     cl,2
        call    sys_write
        jmp     ex_lp

; ::::::::::::::
; sc_ipow - I.POW ( base power -- r ): truncated for a negative power.
;
; error:  ?OVERFLOW  past an INT
; ::::::::::::::
sc_ipow:
        pop     ecx
        movsx   ecx,cx
        pop     edx
        mov     ax,1
        jecxz    @sip_r
        cmp     dx,1
        je      @sip_r
        xor     ax,ax
        test    dx,dx
        jz      @sip_r
        cmp     dx,-1
        jne     @sip_b
        mov     ax,1                    ; (-1)^n
        test    cl,1
        jz      @sip_r
        neg     ax
        jmp     @sip_r
@sip_b:
        test    ecx,ecx
        js      @sip_r                  ; |base| over 1, a negative power: 0
        mov     ax,1
@sip_l:
        imul    ax,dx
        jo      err_ov
        loop    @sip_l
@sip_r:
        push    eax
        dispatch

; ::::::::::::::
; sc_fpow - F.POW ( base power -- r ).
; ::::::::::::::
sc_fpow:
        pop     eax
        pop     edx
        call    fn_pow
        push    eax
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
        push    eax
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
        mov     word ptr [sbuf+ebx],ax
        pop     eax
        sub     ebx,2
        jns     @sti_l
        mov     ax,6
        call    str_buftmp
        push    eax
        jmp     ex_lp

; ::::::::::::::
; sc_ist - I.LD.ST ( -- i ): the I/O status.
; ::::::::::::::
sc_ist:
        movzx   eax,[iost]
        push    eax
        jmp     ex_lp

; ::::::::::::::
; sys_write - TI$ = "HHMMSS" sets the clock.
;
; in:     CL   lk, the kind
;         AX   the string
; error:  ?SYNTAX ERROR      TI or ST
;         ?TYPE MISMATCH     a number
;         ?ILLEGAL QUANTITY  not 6 digits, or past 23:59:59
; ::::::::::::::
sys_write:
        test    cl,3
        jz      err_sn
        call    str_get
        cmp     cx,6
        jne     err_iq
        push    esi
        call    str_near
        xor     ebx,ebx
        xor     edi,edi
@syw_d:
        lodsb
        xor     al,'0'
        cmp     al,[wlim+edi]
        jae     @syw_q
        movzx   eax,al
        mov     ecx,edi
        add     edi,ecx
        add     edi,ecx
        add     edi,ecx
        imul    eax,[wtab+edi]
        mov     edi,ecx
        add     ebx,eax
        inc     edi
        cmp     edi,6
        jne     @syw_d
        cmp     ebx,5184000
        jae     @syw_q
        pop     esi
        mov     eax,ebx
        jmp     plt_settim
@syw_q:
        pop     esi
        jmp     err_iq

        .data
wtab    dd      2160000,216000,36000,3600,600,60
wlim    db      10,10,6,10,6,10
relbit  db      1,2,4
        .code

        end
