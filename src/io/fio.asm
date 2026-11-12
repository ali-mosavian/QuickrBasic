; ==========================================================================
; fio.asm - QBasic's file statements and functions, over the channels
; ==========================================================================
; holds:    sc_open, sc_open2, sc_odef, sc_om*, sc_orec, sc_close, sc_clall,
;           sc_fiout, sc_fiin, sc_ficlr, sc_sline, sc_w*, sc_eof, sc_lof,
;           sc_loc, sc_seekf, sc_seek, sc_freef, sc_inps, sc_fattr, sc_kill,
;           sc_name, sc_mkdir, sc_rmdir, sc_chdir, sc_files, sc_get, sc_put,
;           sc_gnv, sc_fldb, sc_fld, sc_lset, sc_rset, sc_bsave, sc_bload,
;           sc_lock, sc_drop, sc_nfa
; uses:     io/chan.asm, str_*, io_*, sx_sret
; data:     A variable's place, as the compiler's va gives it: its address
;           and a kind, VA_DESC (a string's Desc; n its STRING * n), VA_RAW
;           (n bytes), VA_I2 (an INT kept as 2 bytes); n in the low word.
;           A RANDOM channel's record is g_recb's RECMAX bytes at its index.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; fx_name - the string value EAX as a 0-ended name at nbuf.
;
; out:    EDX  nbuf
; keeps:  ESI
; error:  64 Bad file name  empty
; ::::::::::::::
fx_name:
        push    esi
        call    str_get
        jz      @fxn_e
        call    str_near
        mov     edx,esi
        pop     esi
        ret
@fxn_e:
        mov     al,64
        jmp     ch_err

; ::::::::::::::
; sc_odef - OPEN's defaults: RANDOM, 128-byte records.
; sc_omi .. sc_omb - FOR INPUT, OUTPUT, APPEND, RANDOM, BINARY.
; sc_orec - LEN = ( n -- ).
; ::::::::::::::
sc_odef:
        mov     [g_omode],FM_RND
        mov     [g_orec],128
        jmp     ex_lp
sc_omi:
        mov     al,FM_IN
        jmp     @som_s
sc_omo:
        mov     al,FM_OUT
        jmp     @som_s
sc_oma:
        mov     al,FM_APP
        jmp     @som_s
sc_omr:
        mov     al,FM_RND
        jmp     @som_s
sc_omb:
        mov     al,FM_BIN
@som_s:
        mov     [g_omode],al
        jmp     ex_lp
sc_orec:
        dpop    eax
        dec     eax
        cmp     eax,RECMAX-1
        ja      @sor_e
        inc     eax
        mov     [g_orec],eax
        jmp     ex_lp
@sor_e:
        mov     al,59                   ; Bad record length
        jmp     ch_err

; ::::::::::::::
; sc_open - OPEN name FOR mode AS n ( name n -- ).
; ::::::::::::::
sc_open:
        mov     eax,[esp+CELL]
        call    fx_name
        mov     eax,[esp]
        add     esp,CELL*2
@sop_o:
        push    esi
        mov     esi,edx
        mov     dl,[g_omode]
        mov     ecx,[g_orec]
        call    ch_open
        pop     esi
        jmp     ex_lp

; ::::::::::::::
; sc_open2 - OPEN mode, n, name [, len] ( mode n name -- ): mode's first
; letter I O A R B.
;
; error:  54 Bad file mode  another
; ::::::::::::::
sc_open2:
        mov     eax,[esp]
        call    fx_name
        mov     eax,[esp+CELL*2]
        call    str_get
        jz      @so2_e
        mov     ebx,[g_state.sg.spt]
        mov     al,[ebx]
        upcase  al
        mov     ebx,offset g_omodes
@so2_l:
        cmp     byte ptr [ebx],0
        je      @so2_e
        cmp     al,[ebx]
        je      @F
        add     ebx,2
        jmp     @so2_l
@@:
        mov     al,[ebx+1]
        mov     [g_omode],al
        mov     eax,[esp+CELL]
        add     esp,CELL*3
        jmp     @sop_o
@so2_e:
        mov     al,54
        jmp     ch_err

; ::::::::::::::
; sc_close - CLOSE n ( n -- ): not open, nothing. sc_clall - CLOSE, RESET.
; ::::::::::::::
sc_close:
        dpop    eax
        call    ch_look
        jc      ex_lp
        call    ch_close
        jmp     ex_lp
sc_clall:
        call    ch_clall
        jmp     ex_lp

; ::::::::::::::
; sc_fiout - FI.OUT ( n -- ): output to file n; sc_fiin - FI.IN: input
; from it; sc_ficlr - FI.CLR: the console's again.
; ::::::::::::::
sc_fiout:
        dpop    eax
        call    ch_find
        call    ch_out
        jmp     ex_lp
sc_fiin:
        dpop    eax
        call    ch_find
        call    ch_in
        jmp     ex_lp
sc_ficlr:
        call    ch_clr
        jmp     ex_lp

; ::::::::::::::
; sc_sline - LINE INPUT's ( -- s ): the line read, whole.
; ::::::::::::::
sc_sline:
        mov     eax,[g_state.io.inlen]
        call    str_alloc
        mov     [g_state.sg.spt],offset g_inbuf
        call    str_tods
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_wi, sc_wf, sc_wd - WRITE a number ( x -- ): as PRINT, no spaces;
; sc_ws - a string ( s -- ): in quotes; sc_wcom - the comma between.
; ::::::::::::::
sc_wi:
        dpop    eax
        call    fx_cap
        call    io_int
        jmp     fx_wout
sc_wf:
        dpop    eax
        call    fx_cap
        call    flt_out
        jmp     fx_wout
sc_wd:
        mov     eax,esp
        call    dbl_fmt
        add     esp,CELL
        call    fx_cap
        mov     ebx,offset g_dbuf
@swd_l:
        mov     al,[ebx]
        call    io_putc
        inc     ebx
        loop    @swd_l
        jmp     fx_wout
sc_ws:
        dpop    eax
        call    str_get
        mov     al,'"'
        call    io_putc
        call    str_print
        mov     al,'"'
        call    io_putc
        jmp     ex_lp
sc_wcom:
        mov     al,','
        call    io_putc
        jmp     ex_lp

; fx_cap - io_putc to sbuf, from its start.
fx_cap:
        mov     [g_state.io.ocap],80h
        mov     [g_state.io.sbx],0
        ret

; fx_wout - sbuf's text, its spaces left out, to the output; the next op.
fx_wout:
        mov     [g_state.io.ocap],0
        movzx   ecx,[g_state.io.sbx]
        xor     ebx,ebx
@fwo_l:
        cmp     ebx,ecx
        jae     ex_lp
        mov     al,[g_sbuf+ebx]
        inc     ebx
        cmp     al,' '
        je      @fwo_l
        call    io_putc
        jmp     @fwo_l

; ::::::::::::::
; sc_eof - EOF ( n -- i ): a file read to its end, or a GET past it.
; ::::::::::::::
sc_eof:
        dpop    eax
        call    ch_find
        xor     eax,eax
        test    [edi].TCHAN.cmode,FM_RND+FM_BIN
        jz      @F
        cmp     [edi].TCHAN.ceof,0
        je      @sef_r
        dec     eax
        jmp     @sef_r
@@:
        test    [edi].TCHAN.cmode,FM_IN
        jz      @sef_t                  ; written: at its end
        call    ch_peek
        mov     eax,0
        jnc     @sef_r
@sef_t:
        dec     eax
@sef_r:
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_lof - LOF ( n -- i ): the file's length.
; ::::::::::::::
sc_lof:
        dpop    eax
        call    ch_find
        call    ch_lof
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_loc - LOC ( n -- i ): RANDOM's last record; BINARY's last byte; a
; sequential file's place in 128-byte blocks.
; ::::::::::::::
sc_loc:
        dpop    eax
        call    ch_find
        call    ch_tell
        test    [edi].TCHAN.cmode,FM_RND
        jz      @F
        mov     eax,[edi].TCHAN.clast
        jmp     @slc_r
@@:
        test    [edi].TCHAN.cmode,FM_BIN
        jnz     @slc_r
        add     eax,127
        shr     eax,7
@slc_r:
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_seekf - SEEK ( n -- i ): the next record, or byte, from 1.
; ::::::::::::::
sc_seekf:
        dpop    eax
        call    ch_find
        call    ch_tell
        test    [edi].TCHAN.cmode,FM_RND
        jz      @F
        movzx   ecx,[edi].TCHAN.creclen
        xor     edx,edx
        div     ecx
@@:
        inc     eax
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_seek - SEEK #n, p ( n p -- ): to record, or byte, p.
;
; error:  63 Bad record number  p under 1
; ::::::::::::::
sc_seek:
        mov     eax,[esp+CELL]
        call    ch_find
        dpop    eax
        add     esp,CELL
        call    fx_place
        call    ch_seek
        mov     [edi].TCHAN.ceof,0
        jmp     ex_lp

; fx_place - record or byte EAX of channel EDI, from 1, as an offset.
fx_place:
        dec     eax
        js      @fxp_e
        test    [edi].TCHAN.cmode,FM_RND
        jz      @F
        push    edx
        movzx   ecx,[edi].TCHAN.creclen
        mul     ecx
        pop     edx
@@:
        ret
@fxp_e:
        mov     al,63
        jmp     ch_err

; ::::::::::::::
; sc_freef - FREEFILE ( -- i ): the first number not open.
; ::::::::::::::
sc_freef:
        mov     eax,1
@sff_l:
        push    eax
        call    ch_look
        pop     eax
        jc      @F
        inc     eax
        cmp     eax,255
        jbe     @sff_l
@@:
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_inps - INPUT$ ( n [f] count -- s ): n chars from file f, else n keys,
; not shown.
;
; error:  62 Input past end of file
; ::::::::::::::
sc_inps:
        dpop    eax
        xor     edi,edi                 ; EDI the channel, 0 the keyboard
        cmp     eax,1
        je      @F
        dpop    eax
        call    ch_find
@@:
        dpop    ecx
        dec     ecx
        cmp     ecx,INMAX-1
        ja      err_iq
        inc     ecx
        push    esi
        push    ecx
        mov     eax,ecx                 ; read into the string's block
        call    str_alloc
        pop     ecx
        mov     esi,[g_state.sg.dpv]
        xor     ebx,ebx
@fip_l:
        test    edi,edi
        jz      @fip_k
        call    ch_peek
        jc      @fip_x
        inc     word ptr [edi].TCHAN.cbp
        jmp     @fip_s
@fip_k:
        call    plt_flush
        xor     ah,ah
        int     16h
@fip_s:
        mov     [esi+ebx],al
        inc     ebx
        cmp     ebx,ecx
        jb      @fip_l
        call    str_new
        pop     esi
        dpush   eax
        jmp     ex_lp
@fip_x:
        pop     esi
        mov     al,62                   ; Input past end of file
        jmp     ch_err

; ::::::::::::::
; sc_fattr - FILEATTR ( n a -- i ): a 1 the mode, QBasic's number; 2 the
; DOS handle.
; ::::::::::::::
sc_fattr:
        dpop    ecx
        dpop    eax
        call    ch_find
        movzx   eax,[edi].TCHAN.chnd
        cmp     ecx,2
        je      @sfa_r
        cmp     ecx,1
        jne     err_iq
        mov     cl,[edi].TCHAN.cmode    ; 1 2 4 8 32: INPUT OUTPUT RANDOM
        mov     eax,1                   ; APPEND BINARY
        test    cl,FM_IN
        jnz     @sfa_r
        mov     al,2
        test    cl,FM_OUT
        jnz     @sfa_r
        mov     al,4
        test    cl,FM_RND
        jnz     @sfa_r
        mov     al,8
        test    cl,FM_APP
        jnz     @sfa_r
        mov     al,32
@sfa_r:
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_kill - KILL ( name -- ); sc_mkdir, sc_rmdir, sc_chdir ( name -- ).
; ::::::::::::::
sc_kill:
        mov     ah,41h
        jmp     @sdo_g
sc_mkdir:
        mov     ah,39h
        jmp     @sdo_g
sc_rmdir:
        mov     ah,3Ah
        jmp     @sdo_g
sc_chdir:
        mov     ah,3Bh
@sdo_g:
        mov     [g_dosfn],ah
        dpop    eax
        call    fx_name
        mov     ah,[g_dosfn]
        int     21h
        jc      ch_doserr
        jmp     ex_lp

; ::::::::::::::
; sc_name - NAME old AS new ( old new -- ).
;
; error:  58 File already exists, DOS's
; ::::::::::::::
sc_name:
        dpop    eax
        call    fx_name
        push    esi
        push    edi
        mov     esi,edx
        mov     edi,offset g_fn2
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        pop     edi
        pop     esi
        dpop    eax
        call    fx_name
        push    edi
        mov     edi,offset g_fn2
        mov     ah,56h
        int     21h
        pop     edi
        jnc     ex_lp
        cmp     ax,5
        jne     ch_doserr
        mov     al,58
        jmp     ch_err

; ::::::::::::::
; sc_files - FILES [name] ( [name] flag -- ): the directory, the names
; that fit, and the bytes free, as QBasic prints them.
;
; error:  53 File not found  none fits
; ::::::::::::::
sc_files:
        dpop    eax
        mov     edx,offset u_all
        test    eax,eax
        jz      @F
        dpop    eax
        call    fx_name
@@:
        push    esi
        push    edx
        mov     ah,1Ah                  ; the DTA
        mov     edx,offset g_dta
        int     21h
        mov     al,'\'                  ; the current directory
        call    io_putc
        mov     ah,47h
        xor     dl,dl
        push    esi
        mov     esi,offset g_fn2
        int     21h
        pop     esi
        mov     ebx,offset g_fn2
        call    io_puts
        call    io_nl
        pop     edx
        mov     ah,4Eh
        mov     cx,10h
        int     21h
        mov     al,53
        jc      @sfl_x
@sfl_l:
        mov     ebx,offset g_dta+1Eh    ; NAME.EXT as NAME    .EXT
        xor     ecx,ecx
@sfl_n:
        mov     al,[ebx]
        test    al,al
        jz      @sfl_p
        cmp     al,'.'
        je      @sfl_p
        call    io_putc
        inc     ebx
        inc     ecx
        jmp     @sfl_n
@sfl_p:
        cmp     ecx,8
        jae     @F
        call    io_space
        inc     ecx
        jmp     @sfl_p
@@:
        mov     al,'.'
        cmp     byte ptr [ebx],0
        je      @F
        inc     ebx
@@:
        call    io_putc
        mov     edx,3
@sfl_e:
        mov     al,[ebx]
        test    al,al
        jz      @F
        inc     ebx
        dec     edx
        call    io_putc
        jmp     @sfl_e
@@:
        test    byte ptr [g_dta+15h],10h
        mov     al,' '
        jz      @F
        mov     al,'<'                  ; a directory: <DIR>
        call    io_putc
        mov     ebx,offset u_dir
        call    io_puts
        sub     edx,5
        mov     al,' '
@@:
        add     edx,6                   ; 18 columns a name
@sfl_s:
        call    io_putc
        dec     edx
        jnz     @sfl_s
        cmp     [g_state.io.pcol],62
        jb      @F
        call    io_nl
@@:
        mov     ah,4Fh
        int     21h
        jnc     @sfl_l
        call    io_fresh
        mov     ah,36h                  ; the bytes free
        xor     dl,dl
        int     21h
        movzx   eax,ax
        movzx   ebx,bx
        movzx   ecx,cx
        imul    eax,ecx
        mul     ebx
        call    io_int
        mov     ebx,offset u_free
        call    io_puts
        call    io_nl
        pop     esi
        jmp     ex_lp
@sfl_x:
        pop     esi
        jmp     ch_err

; ::::::::::::::
; sc_gnv - GET's, PUT's no variable ( -- 0 0 ).
; ::::::::::::::
sc_gnv:
        xor     eax,eax
        dpush   eax
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_get - GET #n [, p] [, v] ( n p addr kind -- ); sc_put - PUT.
; RANDOM: record p (0 the next) to or from the record, v from or to it;
; a variable-length string there has its length first. BINARY: v's bytes
; at byte p; a string's own length of them.
;
; error:  54 Bad file mode, 59 Bad record length  v past the record
; ::::::::::::::
sc_get:
        mov     [g_rwdir],3Fh
        jmp     @sgp_g
sc_put:
        mov     [g_rwdir],40h
@sgp_g:
        push    esi
        mov     eax,[esp+4+CELL*3]      ; n
        call    ch_find
        mov     al,54
        test    [edi].TCHAN.cmode,FM_RND+FM_BIN
        jz      @sgp_x
        mov     eax,[esp+4+CELL*2]      ; p
        test    eax,eax
        jnz     @sgp_p
        call    ch_tell                 ; the next: RANDOM's, a whole record on
        test    [edi].TCHAN.cmode,FM_RND
        jz      @sgp_at
        movzx   ecx,[edi].TCHAN.creclen
        add     eax,ecx
        dec     eax
        xor     edx,edx
        div     ecx
        inc     eax
@sgp_p:
        mov     [edi].TCHAN.clast,eax
        call    fx_place
@sgp_at:
        call    ch_seek
        mov     [g_rwbase],eax
        mov     eax,[esp+4]             ; kind
        mov     edx,[esp+4+CELL]        ; addr
        test    [edi].TCHAN.cmode,FM_RND
        jnz     @sgp_r
        cmp     [g_rwdir],3Fh           ; BINARY: v's bytes, there; a string
        jne     @F                      ; read gets a new value
        push    eax
        shr     eax,24
        cmp     al,VA_DESC
        pop     eax
        jne     @F
        call    fx_bgets
        jmp     @sgp_d
@@:
        call    fx_vlen
        call    fx_vrw
        jmp     @sgp_d
@sgp_r:                                 ; RANDOM: through the record
        cmp     [g_rwdir],40h
        jne     @F
        test    eax,eax
        jz      @F
        call    fx_tor
@@:
        call    fx_rec
        movzx   ecx,[edi].TCHAN.creclen
        mov     ah,[g_rwdir]
        call    ch_rw
        cmp     [g_rwdir],3Fh
        jne     @sgp_d
        movzx   ecx,[edi].TCHAN.creclen
        sub     ecx,eax                 ; past the end: the rest 0, EOF
        jz      @F
        mov     [edi].TCHAN.ceof,1
        push    edi
        lea     edi,[edx+eax]
        xor     al,al
        rep     stosb
        pop     edi
@@:
        mov     eax,[esp+4]
        test    eax,eax
        jz      @sgp_d
        mov     edx,[esp+4+CELL]
        call    fx_fromr
@sgp_d:
        pop     esi
        add     esp,CELL*4
        jmp     ex_lp
@sgp_x:
        pop     esi
        jmp     ch_err

; fx_rec - EDX channel EDI's record buffer. Keeps all but EDX.
fx_rec:
        push    eax
        mov     eax,edi
        sub     eax,offset g_ch
        push    ecx
        push    edx
        xor     edx,edx
        mov     ecx,size TCHAN
        div     ecx
        imul    eax,eax,RECMAX
        add     eax,offset g_recb
        pop     edx
        pop     ecx
        mov     edx,eax
        pop     eax
        ret

; fx_vlen - ECX the bytes of the variable at EDX of kind EAX; a Desc's: EDX
; its chars. Keeps all but ECX, EDX.
fx_vlen:
        movzx   ecx,ax
        shr     eax,24
        cmp     al,VA_DESC
        jne     @F
        movzx   ecx,word ptr [edx+D_LEN]
        mov     edx,[edx+D_PTR]
@@:
        shl     eax,24
        or      eax,ecx
        ret

; fx_vrw - BINARY: the variable at EDX (kind EAX, ECX bytes) to or from
; the file; an INT as 2 bytes.
fx_vrw:
        shr     eax,24
        cmp     al,VA_I2
        jne     @fvr_n
        mov     ecx,2
        cmp     [g_rwdir],3Fh
        je      @fvr_i
@fvr_n:
@fvr_w:
        mov     ah,[g_rwdir]
        call    ch_rw
        cmp     [g_rwdir],3Fh
        jne     @F
        cmp     eax,ecx
        jae     @F
        mov     [edi].TCHAN.ceof,1
@@:
        ret
@fvr_i:                                 ; GET an INT: 2 bytes, widened
        push    edx
        mov     edx,offset g_rwtmp
        mov     ah,3Fh
        call    ch_rw
        cmp     eax,2
        jae     @F
        mov     [edi].TCHAN.ceof,1
@@:
        pop     edx
        movsx   eax,word ptr [g_rwtmp]
        mov     [edx],eax
        ret

; fx_bgets - BINARY: the string of the Desc at EDX, its length of bytes
; read, its new value (255 at most).
fx_bgets:
        mov     [g_state.sg.bpo],edx
        movzx   ecx,word ptr [edx+D_LEN]
        cmp     ecx,255
        jbe     @F
        mov     ecx,255
@@:
        mov     edx,offset g_sbuf
        mov     ah,3Fh
        call    ch_rw
        cmp     eax,ecx
        jae     @F
        mov     [edi].TCHAN.ceof,1
@@:
        mov     eax,ecx
        call    str_buftmp
        jmp     str_set

; fx_tor - PUT: the variable at EDX (kind EAX) into the record at its
; start; a variable-length string has its length first.
;
; error:  59 Bad record length  longer than the record
fx_tor:
        push    edi
        push    esi
        movzx   ecx,ax
        mov     ebx,eax
        shr     ebx,24
        mov     esi,edx
        cmp     bl,VA_DESC
        jne     @ftr_r
        movzx   ecx,word ptr [edx+D_LEN]
        mov     esi,[edx+D_PTR]
        test    ax,ax
        jnz     @ftr_r                  ; STRING * n: just its chars
        lea     eax,[ecx+2]
        call    @ftr_chk
        call    fx_rec
        mov     [edx],cx
        add     edx,2
        jmp     @ftr_c
@ftr_r:
        cmp     bl,VA_I2
        jne     @F
        mov     ecx,2
@@:
        mov     eax,ecx
        call    @ftr_chk
        call    fx_rec
@ftr_c:
        mov     edi,edx
        rep     movsb
        pop     esi
        pop     edi
        ret
@ftr_chk:
        cmp     ax,[edi].TCHAN.creclen
        ja      @ftr_e
        ret
@ftr_e:
        mov     al,59
        jmp     ch_err

; fx_fromr - GET: the record into the variable at EDX (kind EAX).
fx_fromr:
        push    edi
        push    esi
        call    fx_rec
        mov     esi,edx                 ; ESI the record
        mov     edx,[esp+8+4+4+CELL]    ; the variable again
        movzx   ecx,ax
        mov     ebx,eax
        shr     ebx,24
        cmp     bl,VA_I2
        jne     @ffr_s
        movsx   eax,word ptr [esi]
        mov     [edx],eax
        jmp     @ffr_r
@ffr_s:
        cmp     bl,VA_DESC
        jne     @ffr_c
        test    cx,cx
        jnz     @ffr_f
        movzx   ecx,word ptr [esi]      ; a length, then the chars: a new value
        add     esi,2
@ffr_f:
        cmp     ecx,255
        jbe     @F
        mov     ecx,255
@@:
        push    edx
        mov     edi,offset g_sbuf
        push    ecx
        rep     movsb
        pop     eax
        pop     edx
        mov     [g_state.sg.bpo],edx
        call    str_buftmp
        call    str_set
        jmp     @ffr_r
@ffr_c:
        mov     edi,edx
        rep     movsb
@ffr_r:
        pop     esi
        pop     edi
        ret

; ::::::::::::::
; sc_fldb - FIELD #n ( n -- ): its fields from the record's start.
; sc_fld - FIELD's w AS v ( w addr kind -- ): v's Desc at the next w bytes
; of the record.
;
; error:  50 FIELD overflow  past the record; 13 Type mismatch  not a
;         string variable
; ::::::::::::::
sc_fldb:
        dpop    eax
        call    ch_find
        mov     [g_fldch],edi
        mov     [edi].TCHAN.cfld,0
        jmp     ex_lp
sc_fld:
        dpop    eax
        shr     eax,24
        cmp     al,VA_DESC
        jne     err_tm
        dpop    ebx                     ; the Desc
        dpop    ecx                     ; w
        mov     edi,[g_fldch]
        movzx   eax,[edi].TCHAN.cfld
        add     eax,ecx
        cmp     ax,[edi].TCHAN.creclen
        mov     dl,50
        ja      @sfd_e
        call    fx_rec
        movzx   eax,[edi].TCHAN.cfld
        add     edx,eax
        mov     [ebx+D_PTR],edx
        mov     [ebx+D_LEN],cx
        add     [edi].TCHAN.cfld,cx
        jmp     ex_lp
@sfd_e:
        mov     al,dl
        jmp     ch_err

; ::::::::::::::
; sc_lset, sc_rset - LSET, RSET v = s ( addr kind s -- ): s in v's
; length, at its left, its right, spaces around it. A FIELD's string is
; changed where it is; another gets a new value.
; ::::::::::::::
sc_lset:
        xor     dl,dl
        jmp     @sls_g
sc_rset:
        mov     dl,1
@sls_g:
        push    esi
        mov     [g_rwdir],dl
        mov     eax,[esp+4]             ; s
        call    str_get
        movzx   ecx,cx
        mov     ebx,[g_state.sg.spt]    ; EBX, ECX the source
        mov     eax,[esp+4+CELL]        ; kind
        mov     edx,[esp+4+CELL*2]      ; addr
        push    eax
        shr     eax,24
        cmp     al,VA_DESC
        pop     eax
        jne     @sls_r
        mov     [g_state.sg.bpo],edx
        movzx   eax,word ptr [edx+D_LEN]        ; its length, its chars
        mov     edi,[edx+D_PTR]
        cmp     edi,offset g_recb
        jb      @sls_n
        cmp     edi,offset g_recb+NCH*RECMAX
        jb      @sls_w                  ; a FIELD's: where it is
@sls_n:
        mov     edi,offset g_sbuf       ; else built in sbuf, then stored
        call    fx_just
        mov     edx,[g_state.sg.bpo]
        movzx   eax,word ptr [edx+D_LEN]
        call    str_buftmp
        call    str_set
        jmp     @sls_x
@sls_r:
        movzx   eax,ax                  ; raw bytes: a record's STRING * n
        mov     edi,edx
@sls_w:
        call    fx_just
@sls_x:
        pop     esi
        add     esp,CELL*3
        jmp     ex_lp

; fx_just - ECX chars at EBX into EAX at EDI, left (rwdir 0) or right,
; spaces around them.
fx_just:
        push    esi
        cmp     ecx,eax
        jbe     @F
        mov     ecx,eax
@@:
        mov     edx,eax
        sub     edx,ecx                 ; EDX the spaces
        cmp     [g_rwdir],0
        je      @F
        xchg    ecx,edx
        mov     al,' '
        rep     stosb
        xchg    ecx,edx
        xor     edx,edx
@@:
        mov     esi,ebx
        rep     movsb
        mov     ecx,edx
        mov     al,' '
        rep     stosb
        pop     esi
        ret

; ::::::::::::::
; sc_bsave - BSAVE name, addr, n ( name addr n -- ): FDh, the address's
; words, n, then the bytes.
; sc_bload - BLOAD name [, addr] ( name [addr] flag -- ).
; ::::::::::::::
sc_bsave:
        mov     eax,[esp+CELL*2]
        call    fx_bname
        push    esi
        mov     ah,3Ch
        xor     ecx,ecx
        int     21h
        jc      @sbs_d
        mov     ebx,eax
        mov     byte ptr [g_rwtmp],0FDh
        mov     eax,[esp+4+CELL]
        rol     eax,16
        mov     dword ptr [g_rwtmp+1],eax
        mov     eax,[esp+4]
        mov     word ptr [g_rwtmp+5],ax
        mov     ah,40h
        mov     ecx,7
        mov     edx,offset g_rwtmp
        int     21h
        mov     ah,40h
        movzx   ecx,word ptr [esp+4]
        mov     edx,[esp+4+CELL]
        int     21h
        mov     ah,3Eh
        int     21h
        pop     esi
        add     esp,CELL*3
        jmp     ex_lp
@sbs_d:
        pop     esi
        jmp     ch_doserr
sc_bload:
        dpop    ecx
        mov     [g_dosfn],cl            ; an address given
        jecxz   @F
        dpop    eax
        mov     [g_rwbase],eax
@@:
        dpop    eax
        call    fx_bname
        push    esi
        mov     ax,3D00h
        int     21h
        jc      @sbs_d
        mov     ebx,eax
        mov     ah,3Fh
        mov     ecx,7
        mov     edx,offset g_rwtmp
        int     21h
        cmp     byte ptr [g_rwtmp],0FDh
        jne     @sbl_m
        mov     edx,[g_rwbase]
        cmp     [g_dosfn],0
        jne     @F
        mov     edx,dword ptr [g_rwtmp+1]
        rol     edx,16
@@:
        mov     ah,3Fh
        movzx   ecx,word ptr [g_rwtmp+5]
        int     21h
        mov     ah,3Eh
        int     21h
        pop     esi
        jmp     ex_lp
@sbl_m:
        mov     ah,3Eh
        int     21h
        pop     esi
        mov     al,54
        jmp     ch_err

; fx_bname - fx_name of EAX, .BAS added when it has no extension.
fx_bname:
        call    fx_name
        push    edi
        mov     edi,edx
        xor     ecx,ecx                 ; ECX: a '.' in the last part
@fbn_l:
        mov     al,[edi]
        test    al,al
        jz      @fbn_e
        inc     edi
        cmp     al,'.'
        jne     @F
        inc     ecx
@@:
        cmp     al,'\'
        je      @fbn_c
        cmp     al,':'
        jne     @fbn_l
@fbn_c:
        xor     ecx,ecx
        jmp     @fbn_l
@fbn_e:
        test    ecx,ecx
        jnz     @F
        mov     dword ptr [edi],'SAB.'
        mov     byte ptr [edi+4],0
@@:
        pop     edi
        ret

; ::::::::::::::
; sc_lock - LOCK, UNLOCK #n ( n -- ): checked; one user, nothing locked.
; sc_drop - ( x -- ).
; ::::::::::::::
sc_lock:
        dpop    eax
        call    ch_find
        jmp     ex_lp
sc_drop:
        add     esp,CELL
        jmp     ex_lp

; ::::::::::::::
; sc_nfa - a feature this build has not: 73 Advanced feature unavailable.
; ::::::::::::::
sc_nfa:
        mov     al,73
        jmp     ch_err

        .data
g_omodes db     'I',FM_IN,'O',FM_OUT,'A',FM_APP,'R',FM_RND,'B',FM_BIN,0
u_all   db      '*.*',0
u_dir   db      'DIR>',0
u_free  db      ' Bytes free',0
        .data?
g_omode db      ?
g_orec  dd      ?
g_rwdir db      ?
g_dosfn db      ?
g_rwbase dd     ?
g_rwtmp db      8 dup(?)
g_fldch dd      ?
g_fn2   db      128 dup(?)
g_dta   db      43 dup(?)
g_recb  db      NCH*RECMAX dup(?)
        .code

        end
