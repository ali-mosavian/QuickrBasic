; ==========================================================================
; flt.asm - the f32 float: compares, conversions, text; the arithmetic is
; the x87's (fpu.asm)
; ==========================================================================
; holds:    flt_cmp, flt_neg, flt_abs, flt_sgn, flt_in, flt_out
; uses:     err_raise, err_iq, st_get, num_amp, dbl_scale, flt_fmt, io_putc,
;           io_space
; data:     FAC EAX, ARG EDX, packed: s:1 e:8 f:23, an IEEE single; e 0 is zero
; calls:
;   flt_in
;     num_amp, flt_insg, st_get
;     dbl_scale (dbl.asm)
;   flt_out
;     flt_fmt (dbl.asm), io_putc
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
; num_amp - &H or &O and its digits.
;
; in:     SI  after the '&'
; out:    CF  set: neither, SI kept
;         EDX the value
;         SI  past it
; keeps:  all but EAX, ECX, EDX
; ::::::::::::::
num_amp:
        movzx   eax,byte ptr [esi]
        upcase  al
        mov     cl,4
        cmp     al,'H'
        je      @nam_b
        mov     cl,3
        cmp     al,'O'
        je      @nam_b
        stc
        ret
@nam_b:
        inc     esi
        xor     edx,edx
@nam_l:
        movzx   eax,byte ptr [esi]
        upcase  al
        sub     al,'0'
        cmp     al,10
        jb      @nam_d
        cmp     cl,4
        jne     @nam_e
        sub     al,'A'-'0'
        cmp     al,6
        jae     @nam_e
        add     al,10
@nam_d:
        cmp     cl,3
        jne     @F
        cmp     al,8
        jae     @nam_e
@@:
        shl     edx,cl
        or      edx,eax
        inc     esi
        jmp     @nam_l
@nam_e:
        clc
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
        call    st_get                  ; &H, &O: an INT's digits
        cmp     al,'&'
        jne     @fi_g
        inc     esi
        call    num_amp
        jc      @fi_na
        mov     eax,edx
        jmp     flt_from32
@fi_na:
        dec     esi
@fi_g:
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
        cmp     ebx,100000000           ; 9 digits, the rest their power
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
        movsx   eax,al                  ; w * 10^fx on the x87, rounded once
        push    ebx
        fldcw   [g_fcwx]
        fild    dword ptr [esp]
        call    dbl_scale
        cmp     [g_state.ft.fsg],0
        je      @F
        fchs
@@:
        fstp    dword ptr [esp]
        fldcw   [g_fcw]
        pop     eax
        mov     ecx,eax
        and     ecx,7F800000h
        cmp     ecx,7F800000h
        je      err_ov
        test    ecx,ecx                 ; under the smallest: 0
        jnz     @F
        and     eax,80000000h
@@:
        ret

; ::::::::::::::
; flt_out - print the FAC as PRINT: a space or -, digits (flt_fmt), a space.
;
; in:     EAX  the FAC, packed
; keeps:  SI, DI
; ::::::::::::::
flt_out:
        push    esi
        push    edi
        push    eax
        mov     eax,esp
        call    flt_fmt
        add     esp,4
        mov     ebx,offset g_dbuf
@fo_l:
        mov     al,[ebx]
        call    io_putc
        inc     ebx
        loop    @fo_l
        pop     edi
        pop     esi
        jmp     io_space


        end
