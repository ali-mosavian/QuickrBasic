; ==========================================================================
; repl.asm - errors and the READY loop
; ==========================================================================
; holds:    err_*, err_raise, rpl_ready, rpl_enter
; uses:     lx_line, prg_put, cm_line, ex_dcode, ex_lp, io_*
; calls:
;   err_raise          compiling: cm_err; else the message, then rpl_ready
;   rpl_ready          READY., read a line
;     rpl_enter        lx_line; a numbered line: prg_put
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; err_id - ?ILLEGAL DIRECT.
;
; error:  ?ILLEGAL DIRECT  always
; ::::::::::::::
err_id:
        mov     al,E_ID
        jmp     err_raise

; ::::::::::::::
; err_rg - ?RETURN WITHOUT GOSUB.
;
; error:  ?RETURN WITHOUT GOSUB  always
; ::::::::::::::
err_rg:
        mov     al,E_RG
        jmp     err_raise

; ::::::::::::::
; err_us - ?UNDEF'D STATEMENT.
;
; error:  ?UNDEF'D STATEMENT  always
; ::::::::::::::
err_us:
        mov     al,E_US
        jmp     err_raise

; ::::::::::::::
; err_cn - ?CAN'T CONTINUE.
;
; error:  ?CAN'T CONTINUE  always
; ::::::::::::::
err_cn:
        mov     al,E_CN
        jmp     err_raise

; ::::::::::::::
; err_tm - ?TYPE MISMATCH.
;
; error:  ?TYPE MISMATCH  always
; ::::::::::::::
err_tm:
        mov     al,E_TM
        jmp     err_raise

; ::::::::::::::
; err_st - ?FORMULA TOO COMPLEX.
;
; error:  ?FORMULA TOO COMPLEX  always
; ::::::::::::::
err_st:
        mov     al,E_ST
        jmp     err_raise

; ::::::::::::::
; err_sn - ?SYNTAX ERROR.
;
; error:  ?SYNTAX ERROR  always
; ::::::::::::::
err_sn:
        mov     al,E_SN
        jmp     err_raise

; ::::::::::::::
; err_iq - ?ILLEGAL QUANTITY.
;
; error:  ?ILLEGAL QUANTITY  always
; ::::::::::::::
err_iq:
        mov     al,E_IQ
        jmp     err_raise

; ::::::::::::::
; err_om - ?OUT OF MEMORY.
;
; error:  ?OUT OF MEMORY  always
; ::::::::::::::
err_om:
        mov     al,E_OM
        jmp     err_raise

; ::::::::::::::
; err_os - Out of stack space: not trapped.
; ::::::::::::::
err_os:
        mov     al,E_OS
        jmp     err_raise

; ::::::::::::::
; err_od - ?OUT OF DATA.
;
; error:  ?OUT OF DATA  always
; ::::::::::::::
err_od:
        mov     al,E_OD

; err_raise - print the error's message, a file run's where, as BC's: "in
; line n|No line number in module NAME at address s:o"; or BREAK [IN n].
; READY.
;
; While compiling the error is cm_err's: an ERR op, raised when the code runs.
; Any error but BREAK clears CONT and drops the frames.
;
; in:     AL   the error, E_*
;         ln   the failing line, opip the op
;         csp  not 0 while compiling
; ::::::::::::::
err_raise:
        and     [g_lxfill],0            ; a streamed line's lexing ended
        cmp     [g_dbev],0              ; the probe's: back to it
        jne     dbg_ever
        call    ex_fclr
        cmp     [g_state.cm.csp],0
        jne     cm_err
        test    al,al                   ; BREAK: never handled
        jz      @erx_u
        cmp     al,E_OS
        je      @erx_u
        cmp     [g_state.rn.inerr],0
        jne     @erx_u
        cmp     [g_state.rn.line],DLINE
        je      @erx_u
        push    eax
        call    ex_hnd
        pop     eax
        cmp     [g_thnd],0
        jne     ex_trap
@erx_u:
        and     [g_state.rn.inerr],0
        mov     ah,al
        call    ch_clr
        call    io_fresh
        mov     al,ah
        mov     esp,[g_state.rn.rsp]
        and     [g_state.sg.tsp],0
        and     [g_state.sg.tspb],0
if IDE
        cmp     [g_ui.lp.ideon],0
        jne     ui_err
endif
        test    al,al
        jnz     @F
        call    io_msg                  ; BREAK [IN n]
        mov     ebx,[g_state.rn.line]
        cmp     ebx,DLINE
        je      rpl_ready
        cmp     gs:[ebx].TLINE.num,NONUM
        je      rpl_ready
        mov     al,MSG_IN
        call    io_msg
        mov     ax,gs:[ebx].TLINE.num
        call    io_num
        jmp     rpl_ready
@@:
        mov     [g_state.rn.cstk],CSTOP
        and     [g_state.rn.contln],0
        call    er_text
        mov     ebx,esi
        call    io_puts
        mov     ecx,[g_state.rn.line]
        cmp     ecx,DLINE
        je      @erx_d
        cmp     [g_state.io.batch],0
        je      @erx_n
        mov     ebx,offset r_eline
        call    io_puts
        call    ex_erl
        test    edx,edx
        jz      @erx_0
        mov     eax,edx
        call    io_u24
        jmp     @erx_m
@erx_0:
        mov     ebx,offset r_enone
        call    io_puts
@erx_m:
        mov     ebx,offset r_emod
        call    io_puts
        call    rpl_mod
        mov     ebx,offset r_eaddr
        call    io_puts
        mov     eax,[g_state.rn.opip]
        sub     eax,[g_state.mm.cachebase]
        push    eax
        shr     eax,16
        call    io_hex4
        mov     al,':'
        call    io_putc
        pop     eax
        call    io_hex4
        jmp     @erx_d
@erx_n:                                 ; typed lines: in line n, if numbered
        cmp     gs:[ecx].TLINE.num,NONUM
        je      @erx_d
        mov     ebx,offset r_eline
        call    io_puts
        mov     ax,gs:[ecx].TLINE.num
        call    io_num
        jmp     @erx_d
@erx_d:
        call    er_diag
        jmp     rpl_ready

; ::::::::::::::
; er_diag - a compile error's place and words, under BC's message: the line
; as LIST spells it, a mark under the place, what is wrong and what to do.
; Any other error: nothing.
;
; in:     opip  the op that raised it: ERR code dg at:2 st:2, the compiler's
;         ln    its line
; ::::::::::::::
er_diag:
        mov     ebx,[g_state.rn.opip]
        cmp     byte ptr [ebx],O_ERR
        jne     @erd_r
        mov     ax,[ebx+1]
        push    eax
        push    dword ptr [ebx+5]       ; the automaton's state
        movzx   eax,word ptr [ebx+3]
        mov     esi,offset g_cbuf
        mov     ebx,[g_state.rn.line]
        cmp     ebx,DLINE
        je      @F
        lea     esi,[ebx+HDR]
        add     esi,[g_state.mm.progbase]
@@:
        call    ls_text
        pop     edx
        pop     eax
        mov     esi,offset g_sbuf
        call    dx_at
        push    eax
        call    io_nl
        call    @erd_in
        mov     ebx,offset g_sbuf
        call    io_puts
        call    io_nl
        call    @erd_in
        pop     ecx
        jecxz   @erd_m
@@:
        call    io_space
        loop    @B
@erd_m:
        mov     al,'^'
        call    io_putc
        call    io_nl
        jmp     dx_say
@erd_in:
        call    io_space
        jmp     io_space
@erd_r:
        ret

; ::::::::::::::
; rpl_mod - the module's name, as BC's: the file's, without its path and
; type, 8 characters, blanks after.
; ::::::::::::::
rpl_mod:
        mov     esi,offset g_fnmb
        mov     ebx,esi
@rmd_s:                                 ; past the last \ or :
        lodsb
        test    al,al
        jz      @F
        cmp     al,'\'
        je      @rmd_b
        cmp     al,':'
        jne     @rmd_s
@rmd_b:
        mov     ebx,esi
        jmp     @rmd_s
@@:
        mov     esi,ebx
        mov     ecx,8
@rmd_c:
        lodsb
        test    al,al
        jz      @rmd_p
        cmp     al,'.'
        je      @rmd_p
        cmp     al,'a'
        jb      @F
        cmp     al,'z'
        ja      @F
        sub     al,20h
@@:
        call    io_putc
        loop    @rmd_c
        ret
@rmd_p:
        mov     al,' '
        call    io_putc
        loop    @rmd_p
        ret

        .data
r_eline db      " in line ",0
r_enone db      "No line number",0
r_emod  db      " in module ",0
r_eaddr db      " at address ",0
        .code

; ::::::::::::::
; rpl_ready - print READY., then read and enter lines.
;
; in:     cstk  the frames STOP or END left
; ::::::::::::::
rpl_ready:
if IDE
        cmp     [g_ui.lp.ideon],0
        jne     ui_ready
endif
        cmp     [g_state.io.batch],0
        jne     @rpl_x
        call    io_fresh
        mov     al,MSG_READY
        call    io_msg
@rpl_r:
        call    plt_getline
        jnc     @F
        test    [g_state.rn.stpflg],SF_BRK
        jz      @rpl_x
        and     [g_state.rn.stpflg],0              ; the STOP key at READY: nothing
        jmp     @rpl_r
@@:
        call    rpl_enter
        test    al,al
        jz      rpl_run
        dec     al
        jz      @rpl_r
        mov     [g_state.rn.cstk],CSTOP
        jmp     @rpl_r
; ::::::::::::::
; rpl_run - run the direct line rpl_enter left in cbuf.
; ::::::::::::::
rpl_run:
        and     [g_state.rn.stpflg],0              ; a key the editor or READY kept
        call    cm_line
        call    ex_dcode
        mov     eax,[g_state.lnk.eeol]              ; an older direct line's frames: its EOL
        sub     eax,[g_state.mm.cachebase]
        mov     ebx,[g_state.rn.cstk]
@rpl_f:
        cmp     ebx,CSTOP
        jae     @rpl_g
        cmp     [ebx].TFRAME.line,DLINE
        jne     @rpl_n
        mov     [ebx].TFRAME.resume,eax
@rpl_n:
        movzx   ecx,byte ptr [ebx+F_MARK]
        add     ebx,ecx
        jmp     @rpl_f
@rpl_g:
        mov     esi,[g_state.lnk.ecode]             ; past its statement map
        jmp     ex_lp
@rpl_x:
        call    ch_clall
        xor     al,al
        jmp     plt_exit

; ::::::::::::::
; rpl_cont - CONT from the editor: QBasic has no CONT statement to type.
; ::::::::::::::
rpl_cont:
        and     [g_state.rn.stpflg],0
        jmp     sc_cont

; ::::::::::::::
; rpl_file - run the file the command line names, then exit; no name: back.
;
; Its lines are stored in its order, a number or none; a label at a line's
; start is that line's. A '$INCLUDE: 'f' line's file is read from there, to
; its end, on file 254 and down.
; ::::::::::::::
rpl_file:
        assume  fs:nothing
        mov     ax,[g_plat.pspsel]
        push    fs
        mov     fs,ax
        movzx   ecx,byte ptr fs:[80h]
        mov     esi,81h
        mov     edi,offset g_fnmb
@rfl_s:
        jecxz   @rfl_e
        mov     al,fs:[esi]
        inc     esi
        dec     ecx
        cmp     al,' '
        je      @rfl_s
@rfl_c:
        cmp     al,' '
        jbe     @rfl_e
        stosb
        jecxz   @rfl_e
        mov     al,fs:[esi]
        inc     esi
        dec     ecx
        jmp     @rfl_c
@rfl_e:
        pop     fs
        assume  fs:error
        mov     ecx,edi
        sub     ecx,offset g_fnmb
        jz      @rfl_r
        mov     byte ptr [edi],0
        call    rpl_load
        call    prg_drop
        call    cm_link
        jmp     ex_runfirst

; ::::::::::::::
; rpl_load - enter the lines of the file fnmb names, its includes' in place;
; then the console's again.
;
; error:  as ch_open
; ::::::::::::::
rpl_load:
        mov     [g_state.io.lmode],1
        mov     [g_state.io.batch],1
        and     [g_state.io.fline],0
        mov     esi,offset g_fnmb
        mov     eax,255
        call    rpl_read
        mov     [g_state.io.lmode],0
        and     [g_state.io.fline],0
@rfl_r:
        ret

; ::::::::::::::
; rpl_incl - rpl_line, then an include's lines from file 254 (the editor's
; lines).
;
; error:  as rpl_read
; ::::::::::::::
rpl_incl:
        call    rpl_line
        jnc     @F
        mov     esi,offset g_finc
        mov     eax,254
        call    rpl_read
@@:
        ret

; ::::::::::::::
; rpl_line - rpl_enter of inbuf, AX long.
;
; out:    CF    set: a '$INCLUDE: 'f' line, finc f
; error:  as rpl_enter
; ::::::::::::::
rpl_line:
        call    rpl_inc
        pushfd
        call    rpl_enter
        popfd
        ret

; ::::::::::::::
; rpl_read - enter the lines of file ESI, 0-ended, read as file EAX; an
; include's on the file below, in place. Then the console's again.
;
; error:  as ch_open, rpl_enter
; ::::::::::::::
rpl_read:
        push    eax
        mov     dl,FM_IN
        mov     ecx,128
        call    ch_open
        call    ch_in
@rrd_l:
        call    sr_line
        jc      @rrd_e
        inc     [g_state.io.fline]
        mov     [g_lxfill],offset sr_more
        call    rpl_line
        mov     [g_lxfill],0
        jnc     @rrd_l
        mov     eax,[esp]               ; an include: its lines next
        dec     eax
        mov     esi,offset g_finc
        call    rpl_read
        mov     eax,[esp]
        call    ch_find
        call    ch_in
        jmp     @rrd_l
@rrd_e:
        pop     eax
        call    ch_find
        call    ch_close
        jmp     ch_clr

; ::::::::::::::
; rpl_inc - is inbuf, AX long, a '$INCLUDE: 'f' metacommand (or REM's)?
;
; out:    CF    set: it is
;         finc  f, 0-ended
; keeps:  all
; ::::::::::::::
rpl_inc:
        pushad
        mov     esi,offset g_inbuf
        movzx   edx,ax
        add     edx,esi
        call    @rin_b
@@:                                     ; past a line number
        cmp     esi,edx
        jae     @F
        mov     al,[esi]
        sub     al,'0'
        cmp     al,9
        ja      @F
        inc     esi
        jmp     @B
@@:
        call    @rin_b
        cmp     esi,edx
        jae     @rin_n
        cmp     byte ptr [esi],"'"
        jne     @F
        inc     esi
        jmp     @rin_m
@@:
        mov     edi,offset r_rem
        call    @rin_w
        jc      @rin_n
@rin_m:
        call    @rin_b
        mov     edi,offset r_inc
        call    @rin_w
        jc      @rin_n
        call    @rin_b
        cmp     byte ptr [esi],':'
        jne     @rin_n
        inc     esi
        call    @rin_b
        cmp     byte ptr [esi],"'"
        jne     @rin_n
        inc     esi
        mov     edi,offset g_finc
@@:
        cmp     esi,edx
        jae     @rin_n
        lodsb
        cmp     al,"'"
        je      @F
        stosb
        cmp     edi,offset g_finc+127
        jb      @B
        jmp     @rin_n
@@:
        mov     byte ptr [edi],0
        popad
        stc
        ret
@rin_n:
        popad
        clc
        ret
@rin_b:                                 ; ESI past blanks
        cmp     esi,edx
        jae     @F
        cmp     byte ptr [esi],' '
        je      @rin_bn
        cmp     byte ptr [esi],9
        jne     @F
@rin_bn:
        inc     esi
        jmp     @rin_b
@@:
        ret
@rin_w:                                 ; the word at EDI, any case; CF: not it
        mov     al,[edi]
        test    al,al
        jz      @F
        cmp     esi,edx
        jae     @rin_x
        mov     ah,[esi]
        upcase  ah
        cmp     ah,al
        jne     @rin_x
        inc     esi
        inc     edi
        jmp     @rin_w
@@:
        clc
        ret
@rin_x:
        stc
        ret

; ::::::::::::::
; rpl_enter - tokenize inbuf; store a numbered line, an empty text deletes it.
; Loading a file every line is stored, at the end.
;
; in:     AX  the length of inbuf
;         lmode  loading
; out:    AL  0 a direct line in cbuf, 1 empty, 2 stored or deleted
;         ln  DLINE
; error:  ?SYNTAX ERROR   line number 64000 or over
;         ?OUT OF MEMORY  from prg_put, nam_find
;         Duplicate label  as scp_label
; ::::::::::::::
rpl_enter:
        movzx   ebx,ax
        mov     byte ptr [g_inbuf+ebx],0
        mov     [g_state.rn.line],DLINE
        mov     esi,offset g_inbuf
        mov     edi,offset g_cbuf
        call    lx_line
        call    dc_line
        cmp     byte ptr [g_cbuf],0
        mov     al,1
        je      @ren_r
        mov     esi,offset g_cbuf
        call    st_get
        cmp     [g_state.io.lmode],0
        jne     @ren_l
        cmp     al,M_INT
        mov     al,0
        jne     @ren_r
        mov     eax,[esi+1]
        cmp     eax,64000
        jae     err_sn
        mov     [g_state.rn.lnum],ax
        add     esi,5
        call    st_get
        mov     ecx,edi
        sub     ecx,esi
        call    prg_put
        mov     al,2
@ren_r:
        ret
@ren_l:
        mov     [g_state.rn.lnum],NONUM
        cmp     al,M_INT
        jne     @ren_a
        mov     eax,[esi+1]
        cmp     eax,64000
        jae     err_sn
        mov     [g_state.rn.lnum],ax
        add     esi,5
        movzx   ebx,ax                  ; the number: a label
        mov     eax,SC_NUM
        call    @ren_lb
@ren_a:
        call    lbl_at
        jne     @ren_b
        call    lbl_nam
        mov     ebx,eax
        xor     eax,eax
        call    @ren_lb
@ren_b:
        mov     ecx,edi
        sub     ecx,esi
        call    prg_add
        mov     al,2
        ret
@ren_lb:                                ; label EBX here, in the scope
        push    edi
        or      ax,[g_state.lx.dproc]
        mov     edx,[g_state.mm.progend]
        call    scp_label
        pop     edi
        ret

        .data
r_rem   db      'REM',0
r_inc   db      '$INCLUDE',0
        .data?
g_fnmb  db      128 dup(?)               ; the command line's file name
g_finc  db      128 dup(?)               ; an include's
        .code

        end
