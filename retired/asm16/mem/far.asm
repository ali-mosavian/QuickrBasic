; ==========================================================================
; far.asm - memory past DS: linear addresses, the program's segment
; ==========================================================================
; holds:    mem_fs, mem_lin, mem_movs, mem_cpy, mem_up, mem_zero, mem_line,
;           mem_pmov
; uses:     FS, GS, ES for the moment of a copy
; calls:
;   mem_cpy, mem_up    mem_fs, mem_movs
;   mem_line, mem_pmov mem_movs
; ==========================================================================

; ::::::::::::::
; mem_line - copy line BX of the program to DI.
;
; keeps:  BX, SI
; ::::::::::::::
mem_line:
        push    si
        push    ds
        mov     si,bx
        movzx   cx,byte ptr gs:[bx]
        push    gs
        pop     ds
        call    mem_movs
        pop     ds
        pop     si
        ret

; ::::::::::::::
; mem_pmov - mem_movs inside the program's segment.
;
; keeps:  all but CX, SI, DI
; ::::::::::::::
mem_pmov:
        push    ds
        push    es
        push    gs
        pop     ds
        push    gs
        pop     es
        call    mem_movs
        pop     es
        pop     ds
        ret

; ::::::::::::::
; mem_movs - rep movsb from DS:SI to ES:DI, either direction.
;
; From 24 bytes on: bytes to align DI to 4, dwords, then the 0-3 left.
; Under that, the extra setup and REP startups cost more than they save.
; Down (DF set), SI and DI are the last bytes; a dword then starts 3 below.
; Overlap is safe as for rep movsb: the destination below the source going
; up, above it going down.
;
; in:     CX  the count
; keeps:  all but CX, SI, DI
; ::::::::::::::
mem_movs:
        cmp     cx,24
        jb      @mms_b
        push    bx
        push    dx
        pushf
        pop     bx
        xor     dx,dx
        test    bh,4                    ; DF
        mov     bx,di
        jz      @mms_u
        mov     dl,3                    ; down: a dword starts 3 below
        not     bx                      ; -(~DI) = DI + 1
@mms_u:
        neg     bx                      ; the head: up (-DI) & 3, down (DI + 1) & 3
        and     bx,3
        sub     cx,bx
        xchg    cx,bx
        rep     movsb
        mov     cx,bx
        shr     cx,2
        sub     si,dx
        sub     di,dx
        rep     movsd
        add     si,dx
        add     di,dx
        mov     cx,bx
        and     cx,3
        pop     dx
        pop     bx
@mms_b:
        rep     movsb
        ret

; ::::::::::::::
; mem_fs - FS:BX = the linear address EAX, BX under 16.
;
; in:     EAX  the address
; keeps:  all but BX, FS
; ::::::::::::::
mem_fs:
        push    eax
        shr     eax,4
        mov     fs,ax
        pop     eax
        mov     bx,ax
        and     bx,15
        ret

; ::::::::::::::
; mem_lin - EAX = the linear address of FS:DI.
;
; keeps:  all but EAX
; ::::::::::::::
mem_lin:
        push    edi
        mov     ax,fs
        movzx   eax,ax
        shl     eax,4
        movzx   edi,di
        add     eax,edi
        pop     edi
        ret

; ::::::::::::::
; mem_cpy - copy CX bytes, not overlapping, by mem_movs.
;
; in:     EAX  the source, linear
;         EDX  the destination, linear
; keeps:  EAX, EDX, SI, DI
; ::::::::::::::
mem_cpy:
        push    si
        push    di
        push    es
        push    ds
        call    mem_fs
        mov     si,bx
        mov     ebx,edx
        shr     ebx,4
        mov     es,bx
        mov     di,dx
        and     di,15
        push    fs
        pop     ds
        call    mem_movs
        pop     ds
        pop     es
        pop     di
        pop     si
        ret

; ::::::::::::::
; mem_up - copy CX bytes up from EAX to EDX; they may overlap: mem_movs down.
;
; in:     EAX  the source, linear
;         EDX  the destination, linear, not under EAX
; keeps:  EAX, EDX, SI, DI
; ::::::::::::::
mem_up:
        jcxz    @mup_r
        push    si
        push    di
        push    es
        push    ds
        call    mem_fs
        lea     si,[bx-1]
        add     si,cx
        mov     ebx,edx
        shr     ebx,4
        mov     es,bx
        mov     di,dx
        and     di,15
        dec     di
        add     di,cx
        push    fs
        pop     ds
        std
        call    mem_movs
        cld
        pop     ds
        pop     es
        pop     di
        pop     si
@mup_r:
        ret

; ::::::::::::::
; mem_zero - zero ECX bytes from EAX, past 64K, by dwords.
;
; in:     EAX  the start, linear, on a paragraph: chunks of FFF0h keep it
; keeps:  SI, DI
; ::::::::::::::
mem_zero:
        push    di
        push    es
@mzr_l:
        test    ecx,ecx
        jz      @mzr_r
        call    mem_fs
        push    fs
        pop     es
        mov     di,bx
        mov     edx,ecx
        cmp     edx,0FFF0h
        jbe     @mzr_c
        mov     edx,0FFF0h
@mzr_c:
        sub     ecx,edx
        add     eax,edx
        push    ecx
        mov     cx,dx
        push    eax
        xor     eax,eax
        mov     dx,cx
        shr     cx,2
        rep     stosd
        mov     cx,dx
        and     cx,3
        rep     stosb
        pop     eax
        pop     ecx
        jmp     @mzr_l
@mzr_r:
        pop     es
        pop     di
        ret
