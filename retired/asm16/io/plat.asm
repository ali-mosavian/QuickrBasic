; ==========================================================================
; plat.asm - the DOS platform: console, files, clock
; ==========================================================================
; holds:    plt_putc, plt_getline, plt_getc, plt_exit, plt_open, plt_close,
;           plt_chkin, plt_chkout, plt_clrch, plt_rdtim, plt_settim, plt_init
; uses:     INT 21h handles, the BIOS tick at 0040:006Ch, inbuf
; data:     NFILE slots: fdev (FFh free), flfn, ffd (handle), fmd (40h read,
;           80h write), fstt/flk (the look-ahead). cin: the input slot, FFh
;           standard input. obuf: standard output, flushed on NL, before a
;           read and at the exit.
; calls:
;   plt_getline        LF ends a line, CR too in a file
;     plt_getb         a file: one byte ahead, ST 40h with the last
;       plt_raw        AH=3Fh
;   plt_putc           obuf; a file directly
;   plt_open           device 0 stdin, 3 and 4 stdout, others DOS files
; ==========================================================================

NFILE   = 10

; ::::::::::::::
; plt_init - the standard handles, the STOP key's vectors, the clock's origin.
;
; out:    cin, outh, inh, isdev, fdev  ready
; ::::::::::::::
plt_init:
        mov     [cin],0FFh
        mov     [outh],1
        mov     di,offset fdev
        mov     cx,NFILE
        mov     al,0FFh
        rep     stosb
        mov     ax,4400h
        xor     bx,bx
        int     21h
        and     dl,80h
        mov     [isdev],dl
        push    es
        mov     ax,351Bh
        int     21h
        mov     word ptr cs:[old1b],bx
        mov     word ptr cs:[old1b+2],es
        mov     ax,351Ch
        int     21h
        mov     word ptr cs:[old1c],bx
        mov     word ptr cs:[old1c+2],es
        pop     es
        push    ds                      ; DS:DX the handler, in the code
        push    cs
        pop     ds
        mov     ax,251Bh
        mov     dx,offset plt_brk
        int     21h
        mov     ax,2523h
        int     21h
        mov     ax,251Ch
        mov     dx,offset plt_tick
        int     21h
        pop     ds
        ret

; ::::::::::::::
; plt_brk - Ctrl-C and Ctrl-Break: the STOP key.
;
; out:    stpflg  1
; ::::::::::::::
plt_brk:
        push    ds
        mov     ds,cs:[dsv]
        mov     [stpflg],1
        pop     ds
        iret

; ::::::::::::::
; plt_tick - INT 1Ch: a Ctrl+C in the BIOS key buffer is the STOP key. DOS
; sees it only when it reads the console, which a running loop never does.
;
; out:    stpflg  1
; ::::::::::::::
plt_tick:
        push    ds
        push    bx
        push    40h
        pop     ds
        mov     bx,ds:[1Ah]
@ptk_l:
        cmp     bx,ds:[1Ch]
        je      @ptk_x
        cmp     byte ptr ds:[bx],3
        je      @ptk_s
        add     bx,2
        cmp     bx,ds:[82h]
        jb      @ptk_l
        mov     bx,ds:[80h]
        jmp     @ptk_l
@ptk_s:
        mov     ds,cs:[dsv]
        mov     [stpflg],1
@ptk_x:
        pop     bx
        pop     ds
        jmp     dword ptr cs:[old1c]

; ::::::::::::::
; plt_kflush - empty the BIOS key buffer.
;
; keeps:  all
; ::::::::::::::
plt_kflush:
        push    ax
@pkf_l:
        mov     ah,1
        int     16h
        jz      @pkf_r
        xor     ah,ah
        int     16h
        jmp     @pkf_l
@pkf_r:
        pop     ax
        ret

; ::::::::::::::
; plt_flush - write obuf.
;
; keeps:  all
; ::::::::::::::
plt_flush:
        cmp     [oblen],0
        je      @pfl_r
        pusha
        mov     ah,40h
        mov     bx,1
        mov     cx,[oblen]
        mov     dx,offset obuf
        int     21h
        mov     [oblen],0
        popa
@pfl_r:
        ret

; ::::::::::::::
; plt_exit - leave.
;
; in:     AL  the exit code
; ::::::::::::::
plt_exit:
        call    plt_flush
        push    ax
        push    ds
        lds     dx,cs:[old1b]
        mov     ax,251Bh
        int     21h
        lds     dx,cs:[old1c]
        mov     ax,251Ch
        int     21h
        pop     ds
        pop     ax
        mov     ah,4Ch
        int     21h

; ::::::::::::::
; plt_putc - write a character to the output; NL is CR LF on standard output.
;
; in:     AL  the character
; keeps:  all
; ::::::::::::::
plt_putc:
        cmp     [outh],1
        jne     @ppc_f
        push    bx
        cmp     al,NL
        jne     @ppc_b
        call    @ppc_a
        mov     al,10
        call    @ppc_a
        call    plt_flush
        mov     al,NL
        pop     bx
        ret
@ppc_b:
        call    @ppc_a
        pop     bx
        ret
@ppc_a:
        mov     bx,[oblen]
        mov     [obuf+bx],al
        inc     bx
        mov     [oblen],bx
        cmp     bx,OBSZ
        jb      @ppc_r
        call    plt_flush
@ppc_r:
        ret
@ppc_f:
        pusha
        mov     [fb],al
        mov     ah,40h
        mov     bx,[outh]
        mov     cx,1
        mov     dx,offset fb
        int     21h
        popa
        ret

; ::::::::::::::
; plt_getline - read a line into inbuf; only what follows the last full 80.
;
; A console is read key by key, not by DOS's line input: that one restarts
; on Ctrl+C, so INPUT could not break.
;
; in:     cin  the input slot, FFh standard input
; out:    AX  its length
;         CF  set: the end of standard input, nothing read; or the STOP
;             key, stpflg set
; keeps:  SI, DI
; ::::::::::::::
plt_getline:
        call    plt_flush
        push    si
        xor     si,si
        cmp     [cin],0FFh
        jne     @pgl_c
        cmp     [isdev],0
        je      @pgl_c
@pgl_w:
        xor     ah,ah
        int     16h
        test    ax,ax                   ; Ctrl+Break
        jz      @pgl_b
        cmp     al,3
        je      @pgl_b
        cmp     al,13
        je      @pgl_k
        cmp     al,8
        je      @pgl_bs
        cmp     al,' '
        jb      @pgl_w
        cmp     si,80
        jae     @pgl_w
        mov     [inbuf+si],al
        inc     si
        call    @pgl_o
        jmp     @pgl_w
@pgl_bs:
        test    si,si
        jz      @pgl_w
        dec     si
        mov     al,8
        call    @pgl_o
        mov     al,' '
        call    @pgl_o
        mov     al,8
        call    @pgl_o
        jmp     @pgl_w
@pgl_o:
        call    plt_putc
        jmp     plt_flush
@pgl_b:
        mov     [stpflg],1
        call    plt_kflush
        pop     si
        stc
        ret
@pgl_k:
        mov     al,NL
        call    plt_putc
        mov     ax,si
        pop     si
        clc
        ret
@pgl_c:
        call    plt_getb
        jc      @pgl_e
        cmp     al,10
        je      @pgl_d
        cmp     al,13
        jne     @pgl_p
        cmp     [cin],0FFh
        je      @pgl_c
        jmp     @pgl_d
@pgl_p:
        mov     [inbuf+si],al
        inc     si
        cmp     si,80
        jb      @pgl_c
        xor     si,si
        jmp     @pgl_c
@pgl_e:
        test    si,si
        jnz     @pgl_d
        cmp     [cin],0FFh
        jne     @pgl_d
        pop     si
        stc
        ret
@pgl_d:
        cmp     [cin],0FFh
        jne     @pgl_n
        cmp     [isdev],0
        jne     @pgl_n
        mov     al,NL
        call    plt_putc
@pgl_n:
        mov     ax,si
        pop     si
        clc
        ret

; ::::::::::::::
; plt_getc - the next input byte, 0 at the end; none waiting on a console: 0.
;
; out:    AL  the byte
; keeps:  all but AL
; ::::::::::::::
plt_getc:
        cmp     [cin],0FFh
        jne     @pgc_b
        cmp     [isdev],0
        je      @pgc_b
        push    dx
        call    plt_flush
        mov     ah,0Bh
        int     21h
        test    al,al
        jz      @pgc_r
        mov     ah,07h
        int     21h
@pgc_r:
        pop     dx
        ret
@pgc_b:
        call    plt_getb
        jnc     @pgc_e
        xor     al,al
@pgc_e:
        ret

; ::::::::::::::
; plt_getb - the next input byte; a file reads one ahead, as the KERNAL.
;
; in:     cin  the slot, FFh standard input
; out:    AL  the byte
;         CF  set at the end
;         iost  40h once a file's last byte is read
; keeps:  all but AX
; ::::::::::::::
plt_getb:
        cmp     [cin],0FFh
        je      plt_raw
        push    bx
        movzx   bx,[cin]
        cmp     [fstt+bx],0
        jne     @pgb_h
        call    plt_raw
        jc      @pgb_x
        mov     [flk+bx],al
        mov     [fstt+bx],1
@pgb_h:
        cmp     [fstt+bx],1
        jne     @pgb_n
        mov     ah,[flk+bx]
        push    ax
        call    plt_raw
        jc      @pgb_l
        mov     [flk+bx],al
        pop     ax
        mov     al,ah
        pop     bx
        clc
        ret
@pgb_l:
        mov     [fstt+bx],2
        mov     [iost],40h
        pop     ax
        mov     al,ah
        pop     bx
        clc
        ret
@pgb_x:
        mov     [fstt+bx],2
@pgb_n:
        mov     [iost],40h
        pop     bx
        stc
        ret

; ::::::::::::::
; plt_raw - read one byte of the input handle.
;
; in:     inh  the handle
; out:    AL  the byte
;         CF  set at the end
; keeps:  all but AX
; ::::::::::::::
plt_raw:
        push    bx
        push    cx
        push    dx
        mov     ah,3Fh
        mov     bx,[inh]
        mov     cx,1
        mov     dx,offset fb
        int     21h
        pop     dx
        pop     cx
        pop     bx
        jc      @prw_e
        cmp     ax,1
        jne     @prw_e
        mov     al,[fb]
        clc
        ret
@prw_e:
        stc
        ret

; ::::::::::::::
; plt_find - the slot of a file.
;
; in:     AL  the file number
; out:    BX  the slot
;         CF  set when it is not open
; keeps:  EAX, SI
; ::::::::::::::
plt_find:
        mov     bx,NFILE-1
@pfi_l:
        cmp     [fdev+bx],0FFh
        je      @pfi_n
        cmp     al,[flfn+bx]
        je      @pfi_y
@pfi_n:
        dec     bx
        jns     @pfi_l
        stc
        ret
@pfi_y:
        clc
        ret

; ::::::::::::::
; plt_open - open file fln; a DOS file is written for fsa 1 or ",W".
;
; The name ends at its first comma, at most 16 characters.
;
; in:     fln, fdv, fsa, sln, spt  the file
; out:    CF  set: AL is the KERNAL error 1, 2, 4 or 8
;         iost  0
; keeps:  SI
; ::::::::::::::
plt_open:
        push    si
        mov     [iost],0
        mov     al,[fln]
        call    plt_find
        mov     al,2
        jnc     @pop_e
        mov     bx,NFILE-1
@pop_f:
        cmp     [fdev+bx],0FFh
        je      @pop_g
        dec     bx
        jns     @pop_f
        mov     al,1
@pop_e:
        pop     si
        stc
        ret
@pop_g:
        mov     [fstt+bx],0
        mov     al,[fdv]
        test    al,al
        jnz     @pop_n0
        mov     al,40h
        xor     cx,cx
        jmp     @pop_s
@pop_n0:
        cmp     al,3
        je      @pop_o
        cmp     al,4
        jne     @pop_h
@pop_o:
        mov     al,80h
        mov     cx,1
@pop_s:
        mov     [fmd+bx],al
        mov     [ffd+bx],cl
        mov     al,[fln]
        mov     [flfn+bx],al
        mov     al,[fdv]
        mov     [fdev+bx],al
        pop     si
        clc
        ret
@pop_h:
        mov     dl,40h
        cmp     [fsa],1
        jne     @pop_m
        mov     dl,80h
@pop_m:
        push    dx
        call    str_near
        pop     dx
        xor     di,di
        mov     cx,[sln]
@pop_c:
        cmp     di,cx
        je      @pop_z
        lodsb
        cmp     al,','
        je      @pop_w
        mov     [fnam+di],al
        inc     di
        cmp     di,16
        jb      @pop_c
        jmp     @pop_z
@pop_w:
        mov     ah,cl
        push    cx
        mov     cx,si
        sub     cx,offset nbuf
        sub     ah,cl
        pop     cx
        test    ah,ah
        jz      @pop_z
@pop_wl:
        lodsb
        cmp     al,'W'
        jne     @pop_wn
        mov     dl,80h
        jmp     @pop_z
@pop_wn:
        dec     ah
        jnz     @pop_wl
@pop_z:
        mov     al,8
        test    di,di
        jz      @pop_e
        mov     byte ptr [fnam+di],0
        push    bx
        push    dx
        mov     dx,offset fnam
        test    dl,dl
        pop     ax
        push    ax
        test    al,80h
        jnz     @pop_cr
        mov     ax,3D00h
        int     21h
        jmp     @pop_rs
@pop_cr:
        mov     ah,3Ch
        xor     cx,cx
        int     21h
@pop_rs:
        pop     dx
        pop     bx
        jnc     @pop_ok
        mov     al,4
        jmp     @pop_e
@pop_ok:
        mov     cl,al
        mov     al,dl
        jmp     @pop_s

; ::::::::::::::
; plt_close - close a file; not open: nothing.
;
; in:     AL  the file number
; keeps:  SI, DI
; ::::::::::::::
plt_close:
        call    plt_find
        jc      @pcl_r
        mov     [fdev+bx],0FFh
        movzx   bx,[ffd+bx]
        cmp     bx,2
        jb      @pcl_r
        mov     ah,3Eh
        int     21h
@pcl_r:
        ret

; ::::::::::::::
; plt_chkin - input from a file.
;
; in:     AL  the file number
; out:    CF  set: AL is the KERNAL error 3 or 6
; keeps:  SI, DI
; ::::::::::::::
plt_chkin:
        call    plt_find
        mov     al,3
        jc      @pck_r
        test    [fmd+bx],40h
        mov     al,6
        jz      @pck_e
        mov     [cin],bl
        movzx   ax,[ffd+bx]
        mov     [inh],ax
        clc
        ret
@pck_e:
        stc
@pck_r:
        ret

; ::::::::::::::
; plt_chkout - output to a file.
;
; in:     AL  the file number
; out:    CF  set: AL is the KERNAL error 3 or 7
; keeps:  SI, DI
; ::::::::::::::
plt_chkout:
        call    plt_find
        mov     al,3
        jc      @pko_r
        test    [fmd+bx],80h
        mov     al,7
        jz      @pko_e
        call    plt_flush
        movzx   ax,[ffd+bx]
        mov     [outh],ax
        clc
        ret
@pko_e:
        stc
@pko_r:
        ret

; ::::::::::::::
; plt_clrch - back to standard input and output.
;
; out:    cin  FFh
; keeps:  all but AL
; ::::::::::::::
plt_clrch:
        mov     [cin],0FFh
        and     [inh],0
        mov     [outh],1
        ret

; ::::::::::::::
; plt_ticks - the jiffies since midnight, from the BIOS tick.
;
; out:    EAX  ticks * 3295 / 1000
; keeps:  SI, DI
; ::::::::::::::
plt_ticks:
        push    es
        push    40h
        pop     es
        mov     eax,es:[6Ch]
        pop     es
        mov     edx,3295
        mul     edx
        mov     ecx,1000
        div     ecx
        ret

; ::::::::::::::
; plt_rdtim - read the jiffy clock.
;
; out:    EAX  the jiffies, under 5184000
; keeps:  SI, DI
; ::::::::::::::
plt_rdtim:
        call    plt_ticks
        add     eax,[toff]
        xor     edx,edx
        mov     ecx,5184000
        div     ecx
        mov     eax,edx
        ret

; ::::::::::::::
; plt_settim - set the jiffy clock.
;
; in:     EAX  the jiffies
; keeps:  SI, DI
; ::::::::::::::
plt_settim:
        push    eax
        call    plt_ticks
        neg     eax
        pop     edx
        add     eax,edx
        add     eax,5184000
        mov     [toff],eax
        ret
