; ==========================================================================
; flt.asm - the f32 float: arithmetic, conversions, text
; ==========================================================================
; holds:    flt_add, flt_sub, flt_mul, flt_div, flt_cmp, flt_int, flt_in,
;           flt_out, flt_fix16, flt_from16
; uses:     err_raise, err_iq, st_get, io_putc, io_u24, io_space
; data:     FAC EAX, ARG EDX, packed: s:1 e:8 f:23, an IEEE single; e 0 is zero
; calls:
;   flt_add, flt_mul, flt_div
;     flt_fin            round, pack
;   flt_in
;     flt_insg, st_get
;     flt_norm, flt_scale
;       flt_step         flt_mul or flt_div
;   flt_out
;     flt_ow, flt_step, io_u24, io_putc
; ==========================================================================

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
; flt_fin - round half up and pack.
;
; in:     EAX  the significand at bits 31..8, the rounding bit at 7
;         EBX  the exponent
;         EBP  the sign, bit 31
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_fin:
        add     eax,80h
        jnc     @F
        mov     eax,80000000h
        inc     ebx
@@:
        cmp     ebx,EMAX
        ja      err_ov
        shr     eax,8
        btr     eax,23
        or      eax,ebp
        shl     ebx,23
        or      eax,ebx
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
; flt_sub - FAC = ARG - FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  as flt_add
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_sub:
        test    eax,7F800000h
        jz      flt_add
        xor     eax,80000000h
; ::::::::::::::
; flt_add - FAC = ARG + FAC; 24 guard bits, then sticky.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_add:
        mov     ecx,edx
        shl     ecx,1
        shr     ecx,24
        jnz     @F
        ret
@@:
        mov     ebx,eax
        shl     ebx,1
        shr     ebx,24
        jnz     @F
        mov     eax,edx
        ret
@@:
        cmp     ebx,ecx
        jae     @F
        xchg    eax,edx
        xchg    ebx,ecx
@@:
        sub     ecx,ebx
        neg     ecx                     ; the gap
        cmp     ecx,26
        jb      @F
        ret
@@:
        mov     ebp,eax
        and     ebp,80000000h
        xor     edi,edi
        push    eax
        xor     eax,edx
        shr     eax,31
        mov     ch,al                   ; not 0: the signs differ
        pop     eax
        and     eax,7FFFFFh
        or      eax,800000h
        shl     eax,8
        and     edx,7FFFFFh
        or      edx,800000h
        shl     edx,8
        shrd    edi,edx,cl
        shr     edx,cl
        test    di,di                   ; gap 25: the bit past the guards is sticky
        jz      @F
        or      edi,10000h
        xor     di,di
@@:
        test    ch,ch
        jnz     @f_add_s
        add     eax,edx
        jnc     @F
        rcr     eax,1
        inc     ebx
@@:
        jmp     flt_fin
@f_add_s:
        neg     edi
        sbb     eax,edx
        jnc     @F
        neg     eax
        xor     ebp,80000000h
@@:
        test    eax,eax
        js      flt_fin
        mov     edx,eax
        or      edx,edi
        jz      flt_zero
@@:
        dec     ebx
        jz      flt_zero
        shld    eax,edi,1
        shl     edi,1
        test    eax,eax
        jns     @B
        jmp     flt_fin

; ::::::::::::::
; flt_mul - FAC = ARG * FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_mul:
        mov     ebx,eax
        shl     ebx,1
        shr     ebx,24
        jnz     @F
        ret
@@:
        mov     ecx,edx
        shl     ecx,1
        shr     ecx,24
        jz      flt_zero
        mov     ebp,eax
        xor     ebp,edx
        and     ebp,80000000h
        lea     ebx,[ebx+ecx-FBIAS]
        and     eax,7FFFFFh
        or      eax,800000h
        and     edx,7FFFFFh
        or      edx,800000h
        mul     edx
        shrd    eax,edx,16
        test    ebx,ebx
        js      flt_zero
        test    eax,eax
        js      @F
        test    ebx,ebx
        jz      flt_zero
        add     eax,eax
        jmp     flt_fin
@@:
        inc     ebx
        jmp     flt_fin

; ::::::::::::::
; flt_div - FAC = ARG / FAC.
;
; in:     EAX  the divisor, packed
;         EDX  the dividend, packed
; out:    EAX  the result, packed
; error:  ?DIVISION BY ZERO  FAC 0
;         ?OVERFLOW          past EMAX
; ::::::::::::::
flt_div:
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        jnz     @F
        mov     al,E_DZ
        jmp     err_raise
@@:
        mov     ebx,edx
        shl     ebx,1
        shr     ebx,24
        jz      flt_zero
        mov     ebp,eax
        xor     ebp,edx
        and     ebp,80000000h
        sub     ebx,ecx
        and     eax,7FFFFFh
        or      eax,800000h
        and     edx,7FFFFFh
        or      edx,800000h
        mov     ecx,eax
        mov     eax,FBIAS
        cmp     edx,ecx
        jae     @F
        add     edx,edx
        dec     eax
@@:
        add     ebx,eax
        test    ebx,ebx
        jle     flt_zero
        cmp     ebx,EMAX
        ja      err_ov
        sub     edx,ecx
        mov     eax,edx
        shl     eax,24
        shr     edx,8
        div     ecx
        shl     eax,7
        or      eax,80000000h
        jmp     flt_fin

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
; flt_int - FAC = floor(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_int:
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        jnz     @F
        ret
@@:
        sub     ecx,FBIAS
        jae     @F
        test    eax,eax
        jns     flt_zero
        mov     eax,0BF800000h          ; -1
        ret
@@:
        cmp     ecx,23
        jb      @F
        ret
@@:
        mov     ebx,eax
        and     ebx,7FFFFFh
        or      ebx,800000h
        neg     cl
        add     cl,23
        mov     edx,1
        shl     edx,cl
        dec     edx                     ; the fraction bits
        test    eax,eax
        jns     @F
        add     ebx,edx
@@:
        not     edx
        and     ebx,edx
        mov     ecx,eax                 ; exponent and sign
        and     ecx,0FF800000h
        test    ebx,1000000h
        jz      @F
        shr     ebx,1
        add     ecx,00800000h
@@:
        and     ebx,7FFFFFh
        mov     eax,ebx
        or      eax,ecx
        ret

; ::::::::::::::
; flt_from16 - FAC = the INT in AX.
;
; in:     AX  the INT
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_from16:
        movsx   eax,ax
; ::::::::::::::
; flt_from32 - FAC = the signed EAX, under 2^24.
;
; in:     EAX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_from32:
        xor     ebp,ebp
        test    eax,eax
        jz      @f_fu_r
        jns     flt_norm
        neg     eax
        mov     ebp,80000000h
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
; flt_fromu16 - FAC = AX, unsigned.
;
; in:     AX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_fromu16:
        movzx   eax,ax
        xor     ebp,ebp
        test    eax,eax
        jnz     flt_norm
        ret

; ::::::::::::::
; flt_fixu16 - AX = floor(FAC), 0..65535.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_fixu16:
        xor     cx,cx
        jmp     flt_fix

; ::::::::::::::
; flt_fix16 - AX = floor(FAC), -32768..32767.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_fix16:
        mov     cl,80h
; ::::::::::::::
; flt_fix - flt_fix16 and flt_fixu16.
;
; in:     EAX  the FAC, packed
;         CL   80h signed, 0 unsigned
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_fix:
        mov     ebx,eax
        shl     ebx,1
        shr     ebx,24
        sub     ebx,FBIAS
        jae     @f_fix_w
        test    eax,7F800000h
        jz      flt_zero
        test    eax,eax
        jns     flt_zero
        mov     ch,cl
        mov     edx,1                   ; a negative fraction floors to -1
        jmp     @f_fix_n
@st_flt2:
        jmp     err_iq
@f_fix_w:
        cmp     ebx,16
        jae     @st_flt2
        mov     edx,eax
        and     edx,7FFFFFh
        or      edx,800000h
        mov     ch,cl
        mov     cl,23
        sub     cl,bl
        mov     ebx,1
        shl     ebx,cl
        dec     ebx
        and     ebx,eax                 ; the fraction
        shr     edx,cl
        test    eax,eax
        jns     @f_fix_p
        test    ebx,ebx
        jnz     @f_fix_f
        jmp     @f_fix_n
@f_fix_p:
        test    ch,ch
        jz      @F
        test    dh,80h
        jnz     @st_flt2
@@:
        mov     ax,dx
        ret
@f_fix_f:
        inc     edx
@f_fix_n:
        test    ch,ch
        jz      @st_flt2
        cmp     edx,32768
        ja      @st_flt2
        mov     ax,dx
        neg     ax
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
        mov     cl,[fx]
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
        sub     [fx],cl
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
        inc     si
@fi_n:
        xor     al,al
        ret
@@:
        inc     si
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
        mov     dword ptr [fx],0        ; fx ff fq fd
        call    flt_insg
        mov     [fsg],al
@fi_dl:
        call    st_get
        xor     al,'0'
        cmp     al,10
        jae     @fi_nd
        inc     si
        cmp     ebx,25*65536
        jae     @fi_xd
        imul    ebx,ebx,10
        movzx   eax,al
        add     ebx,eax
        test    [ff],80h
        jz      @fi_dl
        dec     [fx]
        jmp     @fi_dl
@fi_xd:
        test    [ff],80h
        jnz     @fi_dl
        inc     [fx]
        jmp     @fi_dl
@fi_nd:
        cmp     al,'.' xor '0'
        jne     @fi_ne
        test    [ff],80h
        jnz     @fi_end
        or      [ff],80h
        inc     si
        jmp     @fi_dl
@fi_ne:
        cmp     al,'E' xor '0'
        jne     @fi_end
        or      [ff],40h
        inc     si
        call    flt_insg
        mov     [fd],al
@fi_el:
        call    st_get
        xor     al,'0'
        cmp     al,10
        jae     @fi_end
        inc     si
        mov     cl,al
        mov     al,127
        cmp     [fq],12
        jae     @F
        mov     al,[fq]
        imul    ax,ax,10
        add     al,cl
@@:
        mov     [fq],al
        jmp     @fi_el
@fi_end:
        mov     al,[fq]                 ; fx +- exponent, within +-127
        mov     cl,[fd]
        xor     al,cl
        add     cl,cl
        adc     al,[fx]
        jno     @F
        mov     al,[fd]
        xor     al,7Fh
@@:
        mov     [fx],al
        mov     eax,ebx
        xor     ebp,ebp
        cmp     [fsg],0
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
        mov     eax,[fout]
        and     eax,7FFFFFh
        or      eax,800000h
        mov     ebx,[fout]              ; no sign here: bits 31..23 are e
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
        mov     [fd],0
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
        mov     [fd],1
        dec     cl
        movzx   ecx,cl
        btr     edx,ecx
        jnc     @fo_wd
        test    edx,edx
        jz      @fo_wd
        inc     eax
@fo_wd:
        mov     [fw],eax
        ret

; ::::::::::::::
; flt_out - print the FAC as PRINT: a space or -, digits, a space.
;
; in:     EAX  the FAC, packed
; keeps:  SI, DI
; ::::::::::::::
flt_out:
        push    si
        push    di
        test    eax,eax
        mov     cl,' '
        jns     @F
        mov     cl,'-'
@@:
        mov     [fout],eax
        mov     al,cl
        call    io_putc
        mov     eax,[fout]
        btr     eax,31
        mov     [fx],0
@fo_sc:
        mov     [fout],eax
        mov     cl,0FCh
        cmp     eax,(FBIAS+24) SHL 23
        jae     @fo_go
        call    flt_ow
        mov     al,[fx]
        or      al,[fd]
        jnz     @fo_fr
        mov     eax,[fw]
        call    io_u24
        jmp     @fo_sp
@fo_fr:
        mov     cl,4
        cmp     dword ptr [fout],(FBIAS+3*(NDO-4)) SHL 23
        jb      @fo_go
        mov     eax,[fw]
        mov     di,NDO
@fo_dd:
        xor     edx,edx
        mov     ebx,10
        div     ebx
        or      dl,'0'
        mov     [fbuf+di-1],dl
        test    eax,eax
        jz      @fo_lo
        dec     di
        jnz     @fo_dd
        mov     cl,0FFh
@fo_go:
        mov     eax,[fout]
        call    flt_step
        jmp     @fo_sc
@fo_lo:
        mov     cl,1
        dec     di
        jnz     @fo_go
        mov     di,NDO+1
@fo_tz:
        dec     di
        cmp     byte ptr [fbuf+di-1],'0'
        je      @fo_tz
        mov     ax,di
        mov     [fq],al
        mov     al,[fx]
        add     al,NDO-1
        mov     [fx],al
        mov     di,1
        mov     [ff],0
        add     al,2
        cmp     al,11
        jb      @fo_nE
        mov     [ff],80h
        jmp     @fo_pt
@fo_nE:
        mov     ah,0
        mov     di,ax
        dec     di
        jz      @fo_ld
        jns     @fo_pt
@fo_ld:
        mov     al,'.'
        call    io_putc
        test    di,di
        jz      @fo_pt
        mov     al,'0'
        call    io_putc
        inc     di
@fo_pt:
        mov     [fdw],di
        xor     di,di
@fo_dg:
        mov     al,[fbuf+di]
@fo_z:
        call    io_putc
        inc     di
        mov     ax,di
        cmp     al,[fq]
        jb      @fo_mo
        mov     al,'0'
        cmp     di,[fdw]
        jb      @fo_z
        test    [ff],80h
        jz      @fo_sp
        mov     al,'E'
        call    io_putc
        mov     al,[fx]
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
        pop     di
        pop     si
        jmp     io_space
@fo_mo:
        cmp     di,[fdw]
        jne     @fo_dg
        dec     di
        mov     [fdw],di
        mov     al,'.'
        jmp     @fo_z
