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
        mov     edi,offset fdev
        mov     ecx,NFILE
        mov     al,0FFh
        rep     stosb
        mov     ax,4400h
        xor     ebx,ebx
        int     21h
        and     dl,80h
        mov     [isdev],dl
        mov     ax,0204h                ; DPMI: the protected-mode vectors
        mov     bl,1Ch
        int     31h
        mov     dword ptr [old1c],edx
        mov     word ptr [old1c+4],cx
        mov     bl,23h
        int     31h
        mov     dword ptr [old23],edx
        mov     word ptr [old23+4],cx
        mov     cx,cs
        mov     ax,0205h
        mov     bl,23h
        mov     edx,offset plt_brk
        int     31h
        mov     bl,1Ch
        mov     edx,offset plt_tick
        int     31h
        ret

; ::::::::::::::
; plt_brk - INT 23h, Ctrl-C seen by DOS: the STOP key.
;
; out:    stpflg  1
; ::::::::::::::
plt_brk:
        push    ds
        mov     ds,cs:[dsv]
        mov     [stpflg],1
        pop     ds
        iretd

; ::::::::::::::
; plt_tick - INT 1Ch: a Ctrl+C or Ctrl+Break (0) in the BIOS key buffer is
; the STOP key. DOS sees one only when it reads the console, which a running
; loop never does.
;
; out:    stpflg  1
; ::::::::::::::
plt_tick:
        push    ds
        push    ebx
        mov     ds,cs:[dsv]
        movzx   ebx,word ptr ds:[41Ah]
@ptk_l:
        cmp     bx,ds:[41Ch]
        je      @ptk_x
        cmp     byte ptr [ebx+400h],3
        je      @ptk_s
        cmp     word ptr [ebx+400h],0
        je      @ptk_s
        add     ebx,2
        cmp     bx,ds:[482h]
        jb      @ptk_l
        movzx   ebx,word ptr ds:[480h]
        jmp     @ptk_l
@ptk_s:
        mov     [stpflg],1
@ptk_x:
        pop     ebx
        pop     ds
        jmp     fword ptr cs:[old1c]

; ::::::::::::::
; plt_kflush - empty the BIOS key buffer.
;
; keeps:  all
; ::::::::::::::
plt_kflush:
        push    eax
@pkf_l:
        mov     ah,1
        int     16h
        jz      @pkf_r
        xor     ah,ah
        int     16h
        jmp     @pkf_l
@pkf_r:
        pop     eax
        ret

; ::::::::::::::
; plt_flush - write obuf.
;
; keeps:  all
; ::::::::::::::
plt_flush:
        cmp     [oblen],0
        je      @pfl_r
        pushad
        mov     ah,40h
        mov     ebx,1
        movzx   ecx,[oblen]
        mov     edx,offset obuf
        int     21h
        mov     [oblen],0
        popad
@pfl_r:
        ret

; ::::::::::::::
; plt_exit - leave.
;
; in:     AL  the exit code
; ::::::::::::::
plt_exit:
        call    plt_flush
        push    eax
        mov     ax,0205h
        mov     bl,1Ch
        mov     edx,dword ptr [old1c]
        mov     cx,word ptr [old1c+4]
        int     31h
        mov     bl,23h
        mov     edx,dword ptr [old23]
        mov     cx,word ptr [old23+4]
        int     31h
        pop     eax
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
        push    ebx
        cmp     al,NL
        jne     @ppc_b
        call    @ppc_a
        mov     al,10
        call    @ppc_a
        call    plt_flush
        mov     al,NL
        pop     ebx
        ret
@ppc_b:
        call    @ppc_a
        pop     ebx
        ret
@ppc_a:
        movzx   ebx,[oblen]
        mov     [obuf+ebx],al
        inc     ebx
        mov     [oblen],bx
        cmp     ebx,OBSZ
        jb      @ppc_r
        call    plt_flush
@ppc_r:
        ret
@ppc_f:
        pushad
        mov     [fb],al
        mov     ah,40h
        movzx   ebx,[outh]
        mov     ecx,1
        mov     edx,offset fb
        int     21h
        popad
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
        push    esi
        xor     esi,esi
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
        cmp     esi,80
        jae     @pgl_w
        mov     [inbuf+esi],al
        inc     esi
        call    @pgl_o
        jmp     @pgl_w
@pgl_bs:
        test    esi,esi
        jz      @pgl_w
        dec     esi
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
        pop     esi
        stc
        ret
@pgl_k:
        mov     al,NL
        call    plt_putc
        mov     eax,esi
        pop     esi
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
        mov     [inbuf+esi],al
        inc     esi
        cmp     esi,80
        jb      @pgl_c
        xor     esi,esi
        jmp     @pgl_c
@pgl_e:
        test    esi,esi
        jnz     @pgl_d
        cmp     [cin],0FFh
        jne     @pgl_d
        pop     esi
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
        mov     eax,esi
        pop     esi
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
        push    edx
        call    plt_flush
        mov     ah,0Bh
        int     21h
        test    al,al
        jz      @pgc_r
        mov     ah,07h
        int     21h
@pgc_r:
        pop     edx
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
        push    ebx
        movzx   ebx,[cin]
        cmp     [fstt+ebx],0
        jne     @pgb_h
        call    plt_raw
        jc      @pgb_x
        mov     [flk+ebx],al
        mov     [fstt+ebx],1
@pgb_h:
        cmp     [fstt+ebx],1
        jne     @pgb_n
        mov     ah,[flk+ebx]
        push    eax
        call    plt_raw
        jc      @pgb_l
        mov     [flk+ebx],al
        pop     eax
        mov     al,ah
        pop     ebx
        clc
        ret
@pgb_l:
        mov     [fstt+ebx],2
        mov     [iost],40h
        pop     eax
        mov     al,ah
        pop     ebx
        clc
        ret
@pgb_x:
        mov     [fstt+ebx],2
@pgb_n:
        mov     [iost],40h
        pop     ebx
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
        push    ebx
        push    ecx
        push    edx
        mov     ah,3Fh
        movzx   ebx,[inh]
        mov     ecx,1
        mov     edx,offset fb
        int     21h
        pop     edx
        pop     ecx
        pop     ebx
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
        mov     ebx,NFILE-1
@pfi_l:
        cmp     [fdev+ebx],0FFh
        je      @pfi_n
        cmp     al,[flfn+ebx]
        je      @pfi_y
@pfi_n:
        dec     ebx
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
        push    esi
        mov     [iost],0
        mov     al,[fln]
        call    plt_find
        mov     al,2
        jnc     @pop_e
        mov     ebx,NFILE-1
@pop_f:
        cmp     [fdev+ebx],0FFh
        je      @pop_g
        dec     ebx
        jns     @pop_f
        mov     al,1
@pop_e:
        pop     esi
        stc
        ret
@pop_g:
        mov     [fstt+ebx],0
        mov     al,[fdv]
        test    al,al
        jnz     @pop_n0
        mov     al,40h
        xor     ecx,ecx
        jmp     @pop_s
@pop_n0:
        cmp     al,3
        je      @pop_o
        cmp     al,4
        jne     @pop_h
@pop_o:
        mov     al,80h
        mov     ecx,1
@pop_s:
        mov     [fmd+ebx],al
        mov     [ffd+ebx],cl
        mov     al,[fln]
        mov     [flfn+ebx],al
        mov     al,[fdv]
        mov     [fdev+ebx],al
        pop     esi
        clc
        ret
@pop_h:
        mov     dl,40h
        cmp     [fsa],1
        jne     @pop_m
        mov     dl,80h
@pop_m:
        push    edx
        call    str_near
        pop     edx
        xor     edi,edi
        movzx   ecx,[sln]
@pop_c:
        cmp     edi,ecx
        je      @pop_z
        lodsb
        cmp     al,','
        je      @pop_w
        mov     [fnam+edi],al
        inc     edi
        cmp     edi,16
        jb      @pop_c
        jmp     @pop_z
@pop_w:
        mov     ah,cl
        push    ecx
        mov     ecx,esi
        sub     ecx,offset nbuf
        sub     ah,cl
        pop     ecx
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
        test    edi,edi
        jz      @pop_e
        mov     byte ptr [fnam+edi],0
        push    ebx
        push    edx
        mov     edx,offset fnam
        test    dl,dl
        pop     eax
        push    eax
        test    al,80h
        jnz     @pop_cr
        mov     ax,3D00h
        int     21h
        jmp     @pop_rs
@pop_cr:
        mov     ah,3Ch
        xor     ecx,ecx
        int     21h
@pop_rs:
        pop     edx
        pop     ebx
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
        mov     [fdev+ebx],0FFh
        movzx   ebx,[ffd+ebx]
        cmp     ebx,2
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
        test    [fmd+ebx],40h
        mov     al,6
        jz      @pck_e
        mov     [cin],bl
        movzx   ax,[ffd+ebx]
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
        test    [fmd+ebx],80h
        mov     al,7
        jz      @pko_e
        call    plt_flush
        movzx   ax,[ffd+ebx]
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
        mov     eax,ds:[46Ch]
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
