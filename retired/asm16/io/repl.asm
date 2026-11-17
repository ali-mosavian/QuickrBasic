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
        cmp     [csp],0
        jne     cm_err
        mov     ah,al
        call    fil_clrch
        call    io_fresh
        mov     al,ah
        mov     sp,[rsp]
        and     [tsp],0
if IDE
        cmp     [ideon],0
        jne     ui_err
endif
        test    al,al
        jz      @erx_m
        mov     [cstk],CSTOP
        and     [contip],0
        mov     al,'?'
        call    io_putc
        mov     al,ah
        call    io_msg
        mov     al,MSG_ERR
@erx_m:
        call    io_msg
        mov     bx,[ln]
        cmp     bx,DLINE
        je      rpl_ready
        mov     al,MSG_IN
        call    io_msg
        mov     ax,gs:[bx+1]
        call    io_num

; ::::::::::::::
; rpl_ready - print READY., then read and enter lines.
;
; in:     cstk  the frames STOP or END left
; ::::::::::::::
rpl_ready:
if IDE
        cmp     [ideon],0
        jne     ui_ready
endif
        call    io_fresh
        mov     al,MSG_READY
        call    io_msg
@rpl_r:
        call    plt_getline
        jnc     @F
        cmp     [stpflg],0
        je      @rpl_x
        and     [stpflg],0              ; the STOP key at READY: nothing
        jmp     @rpl_r
@@:
        call    rpl_enter
        test    al,al
        jz      rpl_run
        dec     al
        jz      @rpl_r
        mov     [cstk],CSTOP
        jmp     @rpl_r
; ::::::::::::::
; rpl_run - run the direct line rpl_enter left in cbuf.
; ::::::::::::::
rpl_run:
        and     [stpflg],0              ; a key the editor or READY kept
        call    cm_line
        call    ex_dcode
        mov     ax,[eeol]               ; an older direct line's frames: its EOL
        mov     bx,[cstk]
@rpl_f:
        cmp     bx,CSTOP
        jae     @rpl_g
        cmp     byte ptr [bx+F_MARK],M_FN
        je      @rpl_n
        cmp     word ptr [bx+F_LN],DLINE
        jne     @rpl_n
        mov     [bx+F_IP],ax
@rpl_n:
        movzx   cx,byte ptr [bx+F_MARK]
        add     bx,cx
        jmp     @rpl_f
@rpl_g:
        mov     si,offset dcode
        jmp     ex_lp
@rpl_x:
        xor     al,al
        jmp     plt_exit

; ::::::::::::::
; rpl_enter - tokenize inbuf; store a numbered line, an empty text deletes it.
;
; in:     AX  the length of inbuf
; out:    AL  0 a direct line in cbuf, 1 empty, 2 stored or deleted
;         ln  DLINE
; error:  ?SYNTAX ERROR   line number 64000 or over
;         ?OUT OF MEMORY  from prg_put, sym_find
; ::::::::::::::
rpl_enter:
        mov     bx,ax
        mov     byte ptr [inbuf+bx],0
        mov     [ln],DLINE
        mov     [lxd],0
        mov     si,offset inbuf
        mov     di,offset cbuf
        call    lx_line
        cmp     byte ptr [cbuf],0
        mov     al,1
        je      @ren_r
        mov     si,offset cbuf
        call    st_get
        cmp     al,M_INT
        mov     al,0
        jne     @ren_r
        mov     ax,[si+1]
        cmp     ah,0FAh
        jae     err_sn
        mov     [lnum],ax
        add     si,3
        call    st_get
        mov     cx,di
        sub     cx,si
        call    prg_put
        mov     al,2
@ren_r:
        ret
