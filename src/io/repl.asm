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
; err_od - ?OUT OF DATA.
;
; error:  ?OUT OF DATA  always
; ::::::::::::::
err_od:
        mov     al,E_OD

; ::::::::::::::
; err_raise - print ?<text> ERROR [IN <line>], or BREAK [IN <line>]; READY.
;
; While compiling the error is cm_err's: an ERR op, raised when the code runs.
; Any error but BREAK clears CONT and drops the frames.
;
; in:     AL   the error, E_*
;         ln   the failing line
;         csp  not 0 while compiling
; ::::::::::::::
err_raise:
        call    ex_fclr
        cmp     [g_state.cm.csp],0
        jne     cm_err
        test    al,al                   ; BREAK: never handled
        jz      @erx_u
        cmp     [g_state.rn.ehnd],0
        je      @erx_u
        cmp     [g_state.rn.inerr],0
        jne     @erx_u
        cmp     [g_state.rn.ln],DLINE
        jne     ex_trap
@erx_u:
        and     [g_state.rn.inerr],0
        cmp     al,E_USR                ; ERROR n: its message, if one
        jne     @erx_k
        push    ecx
        push    edi
        mov     ax,[g_state.rn.uerr]
        mov     edi,offset g_qberr+2
        mov     ecx,E_USR-2
        repne   scasb
        mov     al,E_USR
        jne     @F
        lea     eax,[edi-1]
        sub     eax,offset g_qberr
@@:
        pop     edi
        pop     ecx
@erx_k:
        mov     ah,al
        call    fil_clrch
        call    io_fresh
        mov     al,ah
        mov     esp,[g_state.rn.rsp]
        and     [g_state.sg.tsp],0
        and     [g_state.sg.tspb],0
if IDE
        cmp     [g_ui.lp.ideon],0
        je      @F
        cmp     al,E_USR
        jne     ui_err
        mov     al,E_IQ
        jmp     ui_err
@@:
endif
        test    al,al
        jz      @erx_m
        mov     [g_state.rn.cstk],CSTOP
        and     [g_state.rn.contip],0
        mov     al,'?'
        call    io_putc
        mov     al,ah
        cmp     al,E_USR
        je      @erx_p
        call    io_msg
@erx_e:
        mov     al,MSG_ERR
@erx_m:
        call    io_msg
        movzx   ebx,[g_state.rn.ln]
        cmp     ebx,DLINE
        je      rpl_ready
        cmp     word ptr [g_progb+ebx+1],NONUM
        je      rpl_ready               ; a line with no number: no IN
        mov     al,MSG_IN
        call    io_msg
        mov     ax,word ptr [g_progb+ebx+1]
        call    io_num

        jmp     rpl_ready
@erx_p:                                 ; no message: UNPRINTABLE
        push    esi
        mov     esi,offset g_unpr
@@:
        lodsb
        test    al,al
        jz      @F
        call    io_putc
        jmp     @B
@@:
        pop     esi
        jmp     @erx_e

        .data
; QBasic's error numbers, by E_*: ERR's.
g_qberr db      0,0,2,3,4,5,7,8,9,10,11,13,17,0,6,1,16,15,12,0,0,0,0
        db      67,55,52,53,68,54,54,64,52,0,13,18
g_unpr  db      "UNPRINTABLE",0
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
        cmp     [g_state.rn.stpflg],0
        je      @rpl_x
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
        sub     eax,offset g_cachb
        mov     ebx,[g_state.rn.cstk]
@rpl_f:
        cmp     ebx,CSTOP
        jae     @rpl_g
        cmp     byte ptr [ebx+F_MARK],M_FN
        je      @rpl_n
        cmp     word ptr [ebx+F_LN],DLINE
        jne     @rpl_n
        mov     [ebx+F_IP],ax
@rpl_n:
        movzx   ecx,byte ptr [ebx+F_MARK]
        add     ebx,ecx
        jmp     @rpl_f
@rpl_g:
        mov     esi,[g_state.lnk.ecode]             ; past its statement map
        jmp     ex_lp
@rpl_x:
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
; start is that line's.
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
        mov     [g_state.sg.sln],cx
        mov     [g_state.sg.spt],offset g_fnmb
        mov     [g_state.io.fln],255
        mov     [g_state.io.fdv],8
        call    fil_ropen
        mov     [g_state.io.lmode],1
        mov     [g_state.io.batch],1
        and     [g_state.io.fline],0
@rfl_l:
        call    plt_getline
        jc      @rfl_d
        inc     [g_state.io.fline]
        call    rpl_enter
        test    [g_state.io.iost],40h
        jz      @rfl_l
@rfl_d:
        call    fil_end
        mov     [g_state.io.lmode],0
        and     [g_state.io.fline],0
        call    prg_drop
        call    cm_link
        jmp     ex_runfirst
@rfl_r:
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
; ::::::::::::::
rpl_enter:
        movzx   ebx,ax
        mov     byte ptr [g_inbuf+ebx],0
        mov     [g_state.rn.ln],DLINE
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
@ren_a:
        call    lbl_at
        jne     @ren_b
        call    lbl_nam                 ; the label: its name's line
        or      [g_nflag+eax],NM_LBL
        mov     bx,[g_state.mm.pend]
        mov     [g_nline+eax*2],bx
@ren_b:
        mov     ecx,edi
        sub     ecx,esi
        call    prg_add
        mov     al,2
        ret

        .data?
g_fnmb  db      128 dup(?)               ; the command line's file name
        .code

        end
