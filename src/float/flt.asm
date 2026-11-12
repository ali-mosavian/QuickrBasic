; ==========================================================================
; flt.asm - the f32 float: compares, conversions, text; the arithmetic is
; the x87's (fpu.asm)
; ==========================================================================
; holds:    flt_cmp, flt_neg, flt_abs, flt_sgn, flt_in, flt_out
; uses:     flt_mul, flt_div, err_raise, err_iq, st_get, io_putc, io_u24,
;           io_space
; data:     FAC EAX, ARG EDX, packed: s:1 e:8 f:23, an IEEE single; e 0 is zero
; calls:
;   flt_in
;     flt_insg, st_get
;     flt_norm, flt_scale
;       flt_step         flt_mul or flt_div
;   flt_out
;     flt_ow, flt_step, io_u24, io_putc
; ==========================================================================

include x86f.inc
        .code

; below the 24-bit significand, as src/core/num/num_f32.s.

; ::::::::::::::
; flt_zero - FAC = 0.
;
; out:    EAX  0
; ::::::::::::::
flt_zero:
        xor     eax,eax
        ret

; ::::::::::::::
; err_ov - ?OVERFLOW.
;
; error:  ?OVERFLOW  always
; ::::::::::::::
err_ov:
        mov     al,E_OV
        jmp     err_raise

; ::::::::::::::
; flt_neg - FAC = -FAC.
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_neg:
        test    eax,7F800000h
        jz      @F
        xor     eax,80000000h
@@:
        ret

; ::::::::::::::
; flt_abs - FAC = |FAC|.
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_abs:
        btr     eax,31
        ret

; ::::::::::::::
; flt_cmp - compare the FAC with the ARG.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    AL  1 FAC above, 0 equal, FFh below
; ::::::::::::::
flt_cmp:
        call    flt_key
        xchg    eax,edx
        mov     ebx,ecx
        call    flt_key
        xchg    eax,edx
        cmp     ebx,ecx
        mov     al,0
        je      @F
        mov     al,1
        jg      @F
        mov     al,0FFh
@@:
        ret

; ::::::::::::::
; flt_key - a signed key of the FAC that keeps its order.
;
; in:     EAX  the FAC, packed
; out:    ECX  the key
; keeps:  EAX, EDX
; ::::::::::::::
flt_key:
        mov     ecx,eax
        btr     ecx,31
        jnc     @F
        neg     ecx
@@:
        ret

; ::::::::::::::
; flt_sgn - the FAC's sign.
;
; in:     EAX  the FAC, packed
; out:    AL  1, 0 or FFh
; ::::::::::::::
flt_sgn:
        test    eax,7F800000h
        jz      @f_s0
        test    eax,eax
        mov     al,1
        jns     @F
        mov     al,0FFh
@@:
        ret
@f_s0:
        xor     al,al
        ret

; ::::::::::::::
; flt_norm - pack a magnitude.
;
; in:     EAX  not 0, under 2^24
;         EBP  the sign, bit 31
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_norm:
        bsr     ecx,eax
        lea     ebx,[ecx+FBIAS]
        neg     ecx
        add     ecx,23
        shl     eax,cl
        btr     eax,23
        or      eax,ebp
        shl     ebx,23
        or      eax,ebx
@f_fu_r:
        ret

; ::::::::::::::
; flt_w16 - is the FAC whole, and under 32768 in size?
;
; in:     EAX  the FAC, packed
; out:    ZF   set when it is
;         AX   its value
; keeps:  BP, SI, DI
; ::::::::::::::
flt_w16:
        mov     ecx,eax
        shr     ecx,23
        and     ecx,0FFh
        jz      @fwh_z
        sub     ecx,FBIAS
        cmp     ecx,15
        jae     @fwh_n
        mov     edx,eax
        and     edx,7FFFFFh
        or      edx,800000h
        neg     cl
        add     cl,23
        mov     ebx,1
        shl     ebx,cl
        dec     ebx
        test    edx,ebx
        jnz     @fwh_r
        shr     edx,cl
        test    eax,eax
        jns     @fwh_p
        neg     dx
@fwh_p:
        mov     ax,dx
        cmp     al,al
        ret
@fwh_z:
        xor     ax,ax
        ret
@fwh_n:
        or      cl,1                    ; ZF clear: CL may be 0 after the sub
@fwh_r:
        ret

; ::::::::::::::
; flt_scale - FAC = FAC * 10^fx, by steps of 10^4 and 10.
;
; in:     EAX  the FAC, packed
;         fx   the power, signed
; out:    EAX  the result, packed
;         fx   0
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_scale:
        mov     cl,[g_state.ft.fx]
        test    cl,cl
        jnz     @F
        ret
@@:
        js      @fs_neg
        cmp     cl,4
        mov     cl,1
        jb      @F
        mov     cl,4
@@:
        jmp     @fs_go
@fs_neg:
        cmp     cl,0FDh
        mov     cl,0FFh
        jae     @fs_go
        mov     cl,0FCh
@fs_go:
        call    flt_step
        jmp     flt_scale

; ::::::::::::::
; flt_step - FAC = FAC * 10^CL; fx less CL.
;
; in:     EAX  the FAC, packed
;         CL   1, 4, -1 or -4
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_step:
        sub     [g_state.ft.fx],cl
        mov     edx,eax
        mov     ch,cl
        test    cl,1
        mov     ax,10
        jnz     @F
        mov     ax,10000
@@:
        push    cx
        call    flt_from16
        pop     cx
        test    ch,ch
        js      flt_div
        jmp     flt_mul

; ::::::::::::::
; flt_insg - step past a sign.
;
; in:     SI  at the sign, or spaces before it
; out:    AL  FFh for a minus, else 0
;         SI  past it
; ::::::::::::::
flt_insg:
        call    st_get
        cmp     al,'-'
        je      @F
        cmp     al,TK_MINUS
        je      @F
        cmp     al,'+'
        je      @fi_p
        cmp     al,TK_PLUS
        jne     @fi_n
@fi_p:
        inc     esi
@fi_n:
        xor     al,al
        ret
@@:
        inc     esi
        mov     al,0FFh
        ret

; ::::::::::::::
; flt_in - read [sign] digits [. digits] [E [sign] digits]; compiling: a scan.
;
; in:     SI   the text
;         csp  not 0 while compiling
; out:    EAX  the number
;         SI   the first byte not taken
; keeps:  DI
; error:  ?OVERFLOW  past the largest FLOAT
; ::::::::::::::
flt_in:
        xor     ebx,ebx                 ; w
        mov     dword ptr [g_state.ft.fx],0        ; fx ff fq fd
        call    flt_insg
        mov     [g_state.ft.fsg],al
@fi_dl:
        call    st_get
        xor     al,'0'
        cmp     al,10
        jae     @fi_nd
        inc     esi
        cmp     ebx,25*65536
        jae     @fi_xd
        imul    ebx,ebx,10
        movzx   eax,al
        add     ebx,eax
        test    [g_state.ft.ff],80h
        jz      @fi_dl
        dec     [g_state.ft.fx]
        jmp     @fi_dl
@fi_xd:
        test    [g_state.ft.ff],80h
        jnz     @fi_dl
        inc     [g_state.ft.fx]
        jmp     @fi_dl
@fi_nd:
        cmp     al,'.' xor '0'
        jne     @fi_ne
        test    [g_state.ft.ff],80h
        jnz     @fi_end
        or      [g_state.ft.ff],80h
        inc     esi
        jmp     @fi_dl
@fi_ne:
        cmp     al,'E' xor '0'
        jne     @fi_end
        or      [g_state.ft.ff],40h
        inc     esi
        call    flt_insg
        mov     [g_state.ft.fd],al
@fi_el:
        call    st_get
        xor     al,'0'
        cmp     al,10
        jae     @fi_end
        inc     esi
        mov     cl,al
        mov     al,127
        cmp     [g_state.ft.fq],12
        jae     @F
        mov     al,[g_state.ft.fq]
        imul    ax,ax,10
        add     al,cl
@@:
        mov     [g_state.ft.fq],al
        jmp     @fi_el
@fi_end:
        mov     al,[g_state.ft.fq]                 ; fx +- exponent, within +-127
        mov     cl,[g_state.ft.fd]
        xor     al,cl
        add     cl,cl
        adc     al,[g_state.ft.fx]
        jno     @F
        mov     al,[g_state.ft.fd]
        xor     al,7Fh
@@:
        mov     [g_state.ft.fx],al
        mov     eax,ebx
        xor     ebp,ebp
        cmp     [g_state.ft.fsg],0
        je      @F
        mov     ebp,80000000h
@@:
        test    eax,eax
        jz      @F
        call    flt_norm
@@:
        call    flt_scale
        ret

; ::::::::::::::
; flt_ow - the FAC's whole part, rounded up past a half.
;
; in:     fout  the FAC
; out:    fw  the whole part
;         fd  not 0 when bits were lost
; ::::::::::::::
flt_ow:
        mov     eax,[g_state.ft.fout]
        and     eax,7FFFFFh
        or      eax,800000h
        mov     ebx,[g_state.ft.fout]              ; no sign here: bits 31..23 are e
        shr     ebx,23
        jnz     @F
        xor     eax,eax
@@:
        mov     cl,FBIAS+23
        sub     cl,bl
        cmp     cl,25
        jbe     @F
        mov     cl,25
@@:
        mov     [g_state.ft.fd],0
        test    cl,cl
        jz      @fo_wd
        mov     edx,eax
        mov     ebx,1
        shl     ebx,cl
        dec     ebx
        and     edx,ebx                 ; the lost bits
        shr     eax,cl
        test    edx,edx
        jz      @fo_wd
        mov     [g_state.ft.fd],1
        dec     cl
        movzx   ecx,cl
        btr     edx,ecx
        jnc     @fo_wd
        test    edx,edx
        jz      @fo_wd
        inc     eax
@fo_wd:
        mov     [g_state.ft.fw],eax
        ret

; ::::::::::::::
; flt_out - print the FAC as PRINT: a space or -, digits, a space.
;
; in:     EAX  the FAC, packed
; keeps:  SI, DI
; ::::::::::::::
flt_out:
        push    esi
        push    edi
        test    eax,eax
        mov     cl,' '
        jns     @F
        mov     cl,'-'
@@:
        mov     [g_state.ft.fout],eax
        mov     al,cl
        call    io_putc
        mov     eax,[g_state.ft.fout]
        btr     eax,31
        mov     [g_state.ft.fx],0
@fo_sc:
        mov     [g_state.ft.fout],eax
        mov     cl,0FCh
        cmp     eax,(FBIAS+24) SHL 23
        jae     @fo_go
        call    flt_ow
        mov     al,[g_state.ft.fx]
        or      al,[g_state.ft.fd]
        jnz     @fo_fr
        mov     eax,[g_state.ft.fw]
        call    io_u24
        jmp     @fo_sp
@fo_fr:
        mov     cl,4
        cmp     dword ptr [g_state.ft.fout],(FBIAS+3*(NDO-4)) SHL 23
        jb      @fo_go
        mov     eax,[g_state.ft.fw]
        mov     edi,NDO
@fo_dd:
        xor     edx,edx
        mov     ebx,10
        div     ebx
        or      dl,'0'
        mov     [g_state.ft.fbuf+edi-1],dl
        test    eax,eax
        jz      @fo_lo
        dec     edi
        jnz     @fo_dd
        mov     cl,0FFh
@fo_go:
        mov     eax,[g_state.ft.fout]
        call    flt_step
        jmp     @fo_sc
@fo_lo:
        mov     cl,1
        dec     edi
        jnz     @fo_go
        mov     edi,NDO+1
@fo_tz:
        dec     edi
        cmp     byte ptr [g_state.ft.fbuf+edi-1],'0'
        je      @fo_tz
        mov     ax,di
        mov     [g_state.ft.fq],al
        mov     al,[g_state.ft.fx]
        add     al,NDO-1
        mov     [g_state.ft.fx],al
        mov     edi,1
        mov     [g_state.ft.ff],0
        add     al,2
        cmp     al,11
        jb      @fo_nE
        mov     [g_state.ft.ff],80h
        jmp     @fo_pt
@fo_nE:
        mov     ah,0
        movzx   edi,ax
        dec     edi
        jz      @fo_ld
        jns     @fo_pt
@fo_ld:
        mov     al,'.'
        call    io_putc
        test    edi,edi
        jz      @fo_pt
        mov     al,'0'
        call    io_putc
        inc     edi
@fo_pt:
        mov     [g_state.ft.fdw],edi
        xor     edi,edi
@fo_dg:
        mov     al,[g_state.ft.fbuf+edi]
@fo_z:
        call    io_putc
        inc     edi
        mov     ax,di
        cmp     al,[g_state.ft.fq]
        jb      @fo_mo
        mov     al,'0'
        cmp     edi,[g_state.ft.fdw]
        jb      @fo_z
        test    [g_state.ft.ff],80h
        jz      @fo_sp
        mov     al,'E'
        call    io_putc
        mov     al,[g_state.ft.fx]
        mov     cl,'+'
        test    al,al
        jns     @F
        neg     al
        mov     cl,'-'
@@:
        mov     ch,al
        mov     al,cl
        call    io_putc
        mov     al,ch
        aam
        add     ax,3030h
        xchg    al,ah
        call    io_putc
        mov     al,ah
        call    io_putc
@fo_sp:
        pop     edi
        pop     esi
        jmp     io_space
@fo_mo:
        cmp     edi,[g_state.ft.fdw]
        jne     @fo_dg
        dec     edi
        mov     [g_state.ft.fdw],edi
        mov     al,'.'
        jmp     @fo_z


        end
