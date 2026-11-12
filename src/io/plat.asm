; ==========================================================================
; plat.asm - the DOS platform: console, files, clock
; ==========================================================================
; holds:    plt_putc, plt_getline, plt_getc, plt_getb, plt_exit, plt_rdtim,
;           plt_settim, plt_init, plt_heap
; uses:     INT 21h handles, the BIOS tick at 0040:006Ch, inbuf, ch_getb
; data:     cin: FFh standard input, else the input channel (io/chan.asm).
;           obuf: standard output, flushed on NL, before a read and at the
;           exit.
; calls:
;   plt_getline        LF ends a line, CR too in a file
;     plt_getb         standard input's, else ch_getb
;       plt_raw        AH=3Fh
;   plt_putc           obuf
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; plt_heap - the heap: DPMI's largest free block, HROOM left for the host.
;
; out:    hbot, htop  its ends, DS offsets
; error:  a message, the exit: under HMIN
; ::::::::::::::
plt_heap:
        mov     ax,0500h                ; the free memory: its largest block
        mov     edi,offset g_inbuf
        int     31h
        mov     eax,dword ptr [g_inbuf]
        sub     eax,HROOM
        jb      @phe_m
        and     eax,not 0FFFh
        cmp     eax,HMIN
        jb      @phe_m
        push    eax
        mov     cx,ax
        shr     eax,16
        mov     bx,ax
        mov     ax,0501h
        int     31h
        pop     edx
        jc      @phe_m
        shl     ebx,16                  ; BX:CX the block, linear
        mov     bx,cx
        push    ebx
        push    edx
        mov     ax,0006h                ; DS's base: offset = linear - base
        mov     bx,ds
        int     31h
        shl     ecx,16
        mov     cx,dx
        pop     edx
        pop     eax
        sub     eax,ecx
        mov     [g_state.mm.hbot],eax
        add     eax,edx
        mov     [g_state.mm.htop],eax
        ret
@phe_m:
        mov     ah,40h
        mov     ebx,2
        mov     ecx,p_nomemz-p_nomem
        mov     edx,offset p_nomem
        int     21h
        mov     ax,4C01h
        int     21h
        .data
p_nomem db      "Out of memory",13,10
p_nomemz label  byte
        .code

; ::::::::::::::
; plt_init - the standard handles, the STOP key's vectors, the clock's origin.
;
; out:    cin, inh, isdev  ready
; ::::::::::::::
plt_init:
        mov     [g_state.io.cin],0FFh
        mov     ax,4400h
        xor     ebx,ebx
        int     21h
        and     dl,80h
        mov     [g_state.io.isdev],dl
        mov     ax,0204h                ; DPMI: the protected-mode vectors
        mov     bl,1Ch
        int     31h
        mov     dword ptr [g_plat.old1c],edx
        mov     word ptr [g_plat.old1c+4],cx
        mov     bl,23h
        int     31h
        mov     dword ptr [g_plat.old23],edx
        mov     word ptr [g_plat.old23+4],cx
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
        mov     ds,cs:[g_plat.dsv]
        mov     [g_state.rn.stpflg],1
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
        mov     ds,cs:[g_plat.dsv]
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
        mov     [g_state.rn.stpflg],1
@ptk_x:
        pop     ebx
        pop     ds
        jmp     fword ptr cs:[g_plat.old1c]

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
        cmp     [g_state.io.oblen],0
        je      @pfl_r
        pushad
        mov     ah,40h
        mov     ebx,1
        movzx   ecx,[g_state.io.oblen]
        mov     edx,offset g_obuf
        int     21h
        mov     [g_state.io.oblen],0
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
        mov     edx,dword ptr [g_plat.old1c]
        mov     cx,word ptr [g_plat.old1c+4]
        int     31h
        mov     bl,23h
        mov     edx,dword ptr [g_plat.old23]
        mov     cx,word ptr [g_plat.old23+4]
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
        movzx   ebx,[g_state.io.oblen]
        mov     [g_obuf+ebx],al
        inc     ebx
        mov     [g_state.io.oblen],bx
        cmp     ebx,OBSZ
        jb      @ppc_r
        call    plt_flush
@ppc_r:
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
        cmp     [g_state.io.cin],0FFh
        jne     @pgl_c
        cmp     [g_state.io.isdev],0
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
        mov     [g_inbuf+esi],al
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
        mov     [g_state.rn.stpflg],1
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
        cmp     [g_state.io.cin],0FFh
        je      @pgl_c
        call    ch_skiplf               ; a file's CR LF: one line's end
        jmp     @pgl_d
@pgl_p:
        cmp     esi,INMAX               ; past it: left out
        jae     @pgl_c
        mov     [g_inbuf+esi],al
        inc     esi
        jmp     @pgl_c
@pgl_e:
        test    esi,esi
        jnz     @pgl_d
        pop     esi
        stc
        ret
@pgl_d:
        cmp     [g_state.io.cin],0FFh
        jne     @pgl_n
        cmp     [g_state.io.isdev],0
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
        cmp     [g_state.io.cin],0FFh
        jne     @pgc_b
        cmp     [g_state.io.isdev],0
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
; plt_getb - the next input byte: standard input's, or the input channel's.
;
; in:     cin  FFh standard input, else a channel
; out:    AL  the byte
;         CF  set at the end
; keeps:  all but AX
; ::::::::::::::
plt_getb:
        cmp     [g_state.io.cin],0FFh
        je      plt_raw
        jmp     ch_getb

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
        movzx   ebx,[g_state.io.inh]
        mov     ecx,1
        mov     edx,offset g_state.io.fb
        int     21h
        pop     edx
        pop     ecx
        pop     ebx
        jc      @prw_e
        cmp     ax,1
        jne     @prw_e
        mov     al,[g_state.io.fb]
        clc
        ret
@prw_e:
        stc
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
        add     eax,[g_state.io.toff]
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
        mov     [g_state.io.toff],eax
        ret

        .data?
g_obuf  db      OBSZ dup(?)
        .code

        .data
g_plat  TPLAT   <>
        .code

        end
