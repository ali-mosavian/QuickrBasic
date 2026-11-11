; ==========================================================================
; file.asm - files: OPEN's defaults, LOAD, SAVE, VERIFY
; ==========================================================================
; holds:    fil_*
; uses:     plt_open, plt_chkin, plt_chkout, plt_clrch, ls_all, rpl_enter
; calls:
;   fil_load           NEW, then rpl_enter per line
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; fil_open - plt_open, then fil_chk.
;
; in:     fln, fdv, fsa, sln, spt  the file
; ::::::::::::::
fil_open:
        call    plt_open

; ::::::::::::::
; fil_chk - raise a KERNAL error.
;
; in:     CF  set for an error
;         AL  its code
; error:  ?TOO MANY FILES .. ?ILLEGAL DEVICE NUMBER  CF set
; ::::::::::::::
fil_chk:
        jnc     @fck_r
        add     al,E_TMF-1
        jmp     err_raise
@fck_r:
        ret

; ::::::::::::::
; fil_def - the defaults of OPEN, LOAD, SAVE, VERIFY.
;
; out:    fln, fdv, fsa, sln  255, 1, 0, 0
; keeps:  EAX
; ::::::::::::::
fil_def:
        mov     [g_state.io.fln],255
        mov     [g_state.io.fdv],1
        mov     [g_state.io.fsa],0
        and     [g_state.sg.sln],0
        ret

; ::::::::::::::
; fil_clrch - back to the screen and keyboard.
;
; out:    infl  bit 7 clear
; keeps:  all but AL
; ::::::::::::::
fil_clrch:
        and     [g_state.io.infl],7Fh
        jmp     plt_clrch

; ::::::::::::::
; fil_ropen - file 255 for input, secondary 0.
;
; in:     fdv, sln, spt  the device, the name
; ::::::::::::::
fil_ropen:
        mov     [g_state.io.fsa],0
        call    fil_open
        mov     al,255
        call    plt_chkin
        jmp     fil_chk

; ::::::::::::::
; fil_end - fil_clrch, then close file 255.
; ::::::::::::::
fil_end:
        call    fil_clrch
        mov     al,255
        jmp     plt_close

; ::::::::::::::
; fil_load - LOAD: NEW, then the file's lines; READY direct, else RUN.
;
; in:     fdv, sln, spt  the device, the name
; ::::::::::::::
fil_load:
        call    fil_ropen
        mov     ax,[g_state.rn.ln]
        mov     [g_state.io.lsv],ax
        call    prg_init
@ldl_l:
        call    plt_getline
        jc      @ldl_e
        call    rpl_enter
        test    [g_state.io.iost],40h
        jz      @ldl_l
@ldl_e:
        call    fil_end
        cmp     [g_state.io.lsv],DLINE
        je      ex_end
        call    cm_link
        jmp     ex_runfirst

; ::::::::::::::
; fil_save - SAVE: the listing to file 255.
;
; in:     fdv, sln, spt  the device, the name
; ::::::::::::::
fil_save:
        mov     [g_state.io.fsa],1
        call    fil_open
        mov     al,255
        call    plt_chkout
        call    fil_chk
        call    ls_all
        call    fil_end
        jmp     ex_lp

; ::::::::::::::
; fil_verify - VERIFY: the listing against the file.
;
; in:     fdv, sln, spt  the device, the name
; error:  ?VERIFY ERROR  a byte differs, or the file is longer
; ::::::::::::::
fil_verify:
        call    fil_ropen
        mov     [g_state.io.ocap],40h
        call    ls_all
        call    plt_getc
        test    al,al
        jz      @vfy_e
        or      [g_state.io.ocap],1
@vfy_e:
        call    fil_end
        test    [g_state.io.ocap],1
        mov     [g_state.io.ocap],0
        jz      @vfy_o
        mov     al,E_VF
        jmp     err_raise
@vfy_o:
        cmp     [g_state.rn.ln],DLINE
        jne     @jt_file1
        mov     al,MSG_OK
        call    io_msg
@jt_file1:
        jmp     ex_lp

        end
