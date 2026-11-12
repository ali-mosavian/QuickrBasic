; ==========================================================================
; dbl.asm - DOUBLE: its ops, its text
; ==========================================================================
; holds:    the D.* handlers, dbl_in, dbl_fmt, dbl_p10
; uses:     flt_insg, err_ov, err_iq, err_raise, io_putc, str_buftmp
; data:     A DOUBLE is an IEEE double, a whole data stack cell; a scalar's
;           in its 8-byte slot at SVBASE. The x87 works at 53 bits for a D op
;           (fcwq), back at fcw after it.
; ==========================================================================

include x86f.inc
        .code

; The x87 at 53 bits; back to the FLOATs' 24.
dq53    macro
        fldcw   [g_fcwq]
        endm
dq24    macro
        fldcw   [g_fcw]
        endm

; ::::::::::::::
; dbl_chk - ?OVERFLOW if the DOUBLE at EDI is past the largest.
;
; keeps:  all
; ::::::::::::::
dbl_chk:
        push    eax
        mov     eax,[edi+4]
        and     eax,7FF00000h
        cmp     eax,7FF00000h
        pop     eax
        je      err_ov
        ret

; ::::::::::::::
; dbl_scale - st0 * 10 ^ EAX by the exact powers to 10^22: one rounding
; while EAX is within 22 of 0.
;
; in:     EAX  the power
; keeps:  all but EAX
; ::::::::::::::
dbl_scale:
        cmp     eax,22
        jle     @F
        fmul    [g_p10+22*8]
        sub     eax,22
        jmp     dbl_scale
@@:
        cmp     eax,-22
        jge     @F
        fdiv    [g_p10+22*8]
        add     eax,22
        jmp     @B
@@:
        test    eax,eax
        js      @F
        fmul    [g_p10+eax*8]
        ret
@@:
        neg     eax
        fdiv    [g_p10+eax*8]
        ret

; ::::::::::::::
; dbl_in - read [sign] digits [. digits] [E|D [sign] digits] [# | !].
;
; in:     SI  the text, or spaces before it
; out:    dval  the value
;         AL    1: a DOUBLE by its spelling (D, #, past 7 digits), 2: !
;         SI    past it
; error:  ?OVERFLOW  past the largest DOUBLE
; ::::::::::::::
dbl_in:
        push    ebx
        push    ecx
        push    edx
        push    edi
        fldcw   [g_fcwq]
        call    flt_insg
        mov     [g_dsg],al
        xor     eax,eax
        mov     dword ptr [g_dm],eax
        mov     dword ptr [g_dm+4],eax
        xor     ecx,ecx                 ; the digits taken
        xor     edx,edx                 ; the power of ten
        xor     ebx,ebx                 ; BL a point seen, BH the kind
@din_l:
        mov     al,[esi]
        cmp     al,'.'
        je      @din_p
        sub     al,'0'
        cmp     al,9
        ja      @din_e
        inc     esi
        test    ecx,ecx
        jnz     @din_d
        test    al,al
        jnz     @din_d
        test    bl,bl                   ; a leading zero
        jz      @din_l
        dec     edx
        jmp     @din_l
@din_d:
        cmp     ecx,18                  ; under 2^63
        jae     @din_x
        inc     ecx
        movzx   eax,al                  ; dm = dm * 10 + the digit
        mov     [g_dtmp],eax
        push    edx
        mov     eax,dword ptr [g_dm+4]
        imul    edi,eax,10
        mov     eax,dword ptr [g_dm]
        mov     edx,10
        mul     edx
        add     edx,edi
        add     eax,[g_dtmp]
        adc     edx,0
        mov     dword ptr [g_dm],eax
        mov     dword ptr [g_dm+4],edx
        pop     edx
        test    bl,bl
        jz      @din_l
        dec     edx
        jmp     @din_l
@din_x:                                 ; past 18 digits: their power only
        test    bl,bl
        jnz     @din_l
        inc     edx
        jmp     @din_l
@din_p:
        test    bl,bl
        jnz     @din_e
        inc     bl
        inc     esi
        jmp     @din_l
@din_e:
        mov     al,[esi]
        and     al,0DFh
        cmp     al,'D'
        jne     @F
        mov     bh,1
        jmp     @din_xp
@@:
        cmp     al,'E'
        jne     @din_s
@din_xp:
        inc     esi
        push    edx
        call    flt_insg
        mov     [g_dtmp],eax
        xor     edx,edx
@@:
        movzx   eax,byte ptr [esi]
        sub     al,'0'
        cmp     al,9
        ja      @F
        inc     esi
        cmp     edx,10000
        jae     @B
        imul    edx,edx,10
        add     edx,eax
        jmp     @B
@@:
        cmp     byte ptr [g_dtmp],0
        je      @F
        neg     edx
@@:
        pop     eax
        add     edx,eax
@din_s:
        mov     al,[esi]
        cmp     al,'#'
        jne     @F
        inc     esi
        mov     bh,1
@@:
        cmp     al,'!'
        jne     @F
        inc     esi
        mov     bh,2
@@:
        test    bh,bh
        jnz     @F
        cmp     ecx,7
        jbe     @F
        mov     bh,1
@@:
        fild    [g_dm]
        mov     eax,edx
        call    dbl_scale
        cmp     [g_dsg],0
        je      @F
        fchs
@@:
        fstp    [g_dval]
        dq24
        push    edi
        mov     edi,offset g_dval
        call    dbl_chk
        pop     edi
        mov     al,bh
        pop     edi
        pop     edx
        pop     ecx
        pop     ebx
        ret

; ::::::::::::::
; dbl_digits - st0's digits, 16, rounded once: v = 0.d1d2.. * 10^(dp+1).
;
; in:     st0  v, over 0 and finite; popped
; out:    dig  the digits, ECX of them, the trailing zeros left out
;         dp   the first digit's power of ten
; keeps:  ESI, EDI
; ::::::::::::::
dbl_digits:
        push    ebx
        push    edx
        fldcw   [g_fcwx]                ; 64 bits: v * 10^k rounds once
        fld     st(0)                   ; p: the power of the first digit
        fldlg2
        fxch
        fyl2x
        fldcw   [g_fcwd]
        fistp   [g_dp]
        fldcw   [g_fcwx]
        mov     ebx,4                   ; tries
@ddg_s:
        mov     eax,15                  ; N = v * 10^(15-p), 16 digits
        sub     eax,[g_dp]
        fld     st(0)
        call    dbl_scale
        dec     ebx
        jz      @ddg_fi
        fcom    [g_p10+15*8]            ; under 10^15: p one too high
        fnstsw  ax
        sahf
        jae     @F
        fstp    st(0)
        dec     [g_dp]
        jmp     @ddg_s
@@:
        fistp   [g_dn]
        cmp     dword ptr [g_dn+4],002386F2h    ; 10^16 after rounding
        jb      @ddg_d
        ja      @F
        cmp     dword ptr [g_dn],6FC10000h
        jb      @ddg_d
@@:
        inc     [g_dp]
        jmp     @ddg_s
@ddg_fi:
        fistp   [g_dn]
@ddg_d:
        fstp    st(0)
        fild    [g_dn]
        fbstp   [g_dbcd]
        dq24
        xor     ecx,ecx                 ; 16 digits to dig, the first first
@@:
        mov     eax,15
        sub     eax,ecx
        mov     edx,eax
        shr     edx,1
        mov     dl,byte ptr [g_dbcd+edx]
        test    al,1
        jz      @ddg_lo
        shr     dl,4
@ddg_lo:
        and     dl,0Fh
        add     dl,'0'
        mov     [g_dig+ecx],dl
        inc     ecx
        cmp     ecx,16
        jb      @B
@@:
        cmp     ecx,1                   ; nd: less the trailing zeros
        je      @F
        cmp     byte ptr [g_dig+ecx-1],'0'
        jne     @F
        dec     ecx
        jmp     @B
@@:
        pop     edx
        pop     ebx
        ret

; ::::::::::::::
; dbl_fmt - a DOUBLE as PRINT spells it: a space or -, 16 digits at most,
; no 0 before the point, D+nn past 10^16 or under 10^-4.
;
; in:     EAX  the DOUBLE's address
; out:    dbuf  the text
;         ECX   its length
; keeps:  ESI
; ::::::::::::::
dbl_fmt:
        push    esi
        push    edi
        push    ebx
        push    edx
        mov     esi,eax
        mov     edi,offset g_dbuf
        fldcw   [g_fcwx]                ; 64 bits: v * 10^k rounds once
        fld     qword ptr [eax]
        ftst
        fnstsw  ax
        sahf
        mov     byte ptr [edi],' '
        jne     @F
        fstp    st(0)
        mov     byte ptr [edi+1],'0'
        add     edi,2
        jmp     @dfm_r
@@:
        jae     @F
        mov     byte ptr [edi],'-'
        fabs
@@:
        inc     edi
        mov     eax,[esi+4]             ; infinity is 1.#INF
        and     eax,7FF00000h
        cmp     eax,7FF00000h
        jne     @F
        fstp    st(0)
        mov     dword ptr [edi],'#.1'
        mov     dword ptr [edi+3],'FNI'
        add     edi,6
        jmp     @dfm_r
@@:
        call    dbl_digits
@@:
        mov     edx,[g_dp]
        cmp     edx,-4
        jl      @dfm_x
        cmp     edx,16
        jge     @dfm_x
        xor     ebx,ebx
        test    edx,edx
        js      @dfm_f
@dfm_w:                                 ; the whole part
        mov     al,'0'
        cmp     ebx,ecx
        jae     @F
        mov     al,[g_dig+ebx]
@@:
        stosb
        inc     ebx
        cmp     ebx,edx
        jbe     @dfm_w
        cmp     ebx,ecx
        jae     @dfm_r
        mov     al,'.'
        stosb
@@:
        mov     al,[g_dig+ebx]
        stosb
        inc     ebx
        cmp     ebx,ecx
        jb      @B
        jmp     @dfm_r
@dfm_f:                                 ; under 1: .000ddd
        mov     al,'.'
        stosb
        mov     al,'0'
@@:
        inc     edx
        jz      @F
        stosb
        jmp     @B
@@:
        mov     al,[g_dig+ebx]
        stosb
        inc     ebx
        cmp     ebx,ecx
        jb      @B
        jmp     @dfm_r
@dfm_x:                                 ; d.dddD+nn
        mov     al,[g_dig]
        stosb
        cmp     ecx,1
        je      @dfm_xe
        mov     al,'.'
        stosb
        mov     ebx,1
@@:
        mov     al,[g_dig+ebx]
        stosb
        inc     ebx
        cmp     ebx,ecx
        jb      @B
@dfm_xe:
        mov     al,'D'
        stosb
        mov     al,'+'
        test    edx,edx
        jns     @F
        mov     al,'-'
        neg     edx
@@:
        stosb
        mov     eax,edx
        cmp     eax,100
        jb      @F
        xor     edx,edx
        mov     ebx,100
        div     ebx
        add     al,'0'
        stosb
        mov     eax,edx
@@:
        aam
        add     ax,3030h
        xchg    al,ah
        stosw
@dfm_r:
        mov     ecx,edi
        sub     ecx,offset g_dbuf
        pop     edx
        pop     ebx
        pop     edi
        pop     esi
        ret

; ::::::::::::::
; sc_dpr - D.PRINT ( d -- ): as PRINT, a space after.
; ::::::::::::::
sc_dpr:
        mov     eax,esp
        call    dbl_fmt
        add     esp,CELL
        mov     ebx,offset g_dbuf
@dpr_l:
        mov     al,[ebx]
        call    io_putc
        inc     ebx
        loop    @dpr_l
        call    io_space
        jmp     ex_lp

; ::::::::::::::
; sc_dstr - STR$ ( d -- s ).
; ::::::::::::::
sc_dstr:
        mov     eax,esp
        call    dbl_fmt
        add     esp,CELL
        push    esi
        mov     esi,offset g_dbuf
        mov     edi,offset g_sbuf
        mov     eax,ecx
        rep     movsb
        pop     esi
        call    str_buftmp
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; op_dldv - D.LD.V id ( -- d ).
; ::::::::::::::
op_dldv:
        movzx   ebx,word ptr [esi]
        add     esi,2
        push    dword ptr [SVBASE+ebx*8+4]
        push    dword ptr [SVBASE+ebx*8]
        dispatch
; ::::::::::::::
; op_dstv - D.ST.V id ( d -- ).
; ::::::::::::::
op_dstv:
        movzx   ebx,word ptr [esi]
        add     esi,2
        pop     dword ptr [SVBASE+ebx*8]
        pop     dword ptr [SVBASE+ebx*8+4]
        dispatch
; ::::::::::::::
; op_dldk - D.LD.K q:8 ( -- d ).
; ::::::::::::::
op_dldk:
        push    dword ptr [esi+4]
        push    dword ptr [esi]
        add     esi,8
        dispatch
; ::::::::::::::
; op_dlda1 - D.LD.A1 id ( i -- d ).
; ::::::::::::::
op_dlda1:
        dpop    eax
        movzx   ebx,word ptr [esi]
        add     esi,2
        add     ebx,ebx
        call    arr_r1
@dla_g:
        push    dword ptr [edi+4]
        push    dword ptr [edi]
        dispatch
; ::::::::::::::
; op_dlda - D.LD.A id n ( i1..in -- d ).
; ::::::::::::::
op_dlda:
        call    arr_pn
        jmp     @dla_g
; ::::::::::::::
; op_dsta1 - D.ST.A1 id ( i d -- ).
; ::::::::::::::
op_dsta1:
        pop     dword ptr [g_dstv]
        pop     dword ptr [g_dstv+4]
        dpop    eax
        movzx   ebx,word ptr [esi]
        add     esi,2
        add     ebx,ebx
        call    arr_r1
@dsa_g:
        mov     eax,dword ptr [g_dstv]
        mov     [edi],eax
        mov     eax,dword ptr [g_dstv+4]
        mov     [edi+4],eax
        dispatch
; ::::::::::::::
; op_dstan - D.ST.A id n ( i1..in d -- ).
; ::::::::::::::
op_dstan:
        pop     dword ptr [g_dstv]
        pop     dword ptr [g_dstv+4]
        call    arr_pn
        jmp     @dsa_g

; D.ADD .. D.DIV ( a b -- c ).
dbin    macro   nm,ins
op_&nm:
        dq53
        fld     qword ptr [esp+CELL]
        ins     qword ptr [esp]
        add     esp,CELL
        fstp    qword ptr [esp]
        dq24
        mov     edi,esp
        call    dbl_chk
        dispatch
        endm
        dbin    dadd,fadd
        dbin    dsub,fsub
        dbin    dmul,fmul

; ::::::::::::::
; op_ddiv - D.DIV ( a b -- c ).
;
; error:  ?DIVISION BY ZERO  b 0
; ::::::::::::::
op_ddiv:
        mov     eax,[esp+4]
        add     eax,eax
        or      eax,[esp]
        jnz     @F
        mov     al,E_DZ
        jmp     err_raise
@@:
        dq53
        fld     qword ptr [esp+CELL]
        fdiv    qword ptr [esp]
        add     esp,CELL
        fstp    qword ptr [esp]
        dq24
        mov     edi,esp
        call    dbl_chk
        dispatch

; ::::::::::::::
; op_dneg - D.NEG ( d -- d ): 0 stays 0.
; ::::::::::::::
op_dneg:
        mov     eax,[esp+4]
        add     eax,eax
        or      eax,[esp]
        jz      @F
        xor     byte ptr [esp+7],80h
@@:
        dispatch

; ::::::::::::::
; op_dcmp - D.CMP mask ( a b -- i ): -1 when the relation holds.
; ::::::::::::::
op_dcmp:
        fld     qword ptr [esp+CELL]
        fcomp   qword ptr [esp]
        fnstsw  ax
        add     esp,CELL
        sahf
        mov     al,1
        ja      @F
        mov     al,2
        je      @F
        mov     al,4
@@:
        and     al,[esi]
        inc     esi
        neg     al
        sbb     eax,eax
        mov     [esp],eax
        dispatch

; ::::::::::::::
; op_dnz - D.NZ ( d -- i ): -1 unless 0.
; ::::::::::::::
op_dnz:
        mov     eax,[esp+4]
        add     eax,eax
        or      eax,[esp]
        neg     eax
        sbb     eax,eax
        mov     [esp],eax
        dispatch

; ::::::::::::::
; op_itod, op_ftod - I.TOD F.TOD ( x -- d ).
; ::::::::::::::
op_itod:
        fild    dword ptr [esp]
        fstp    qword ptr [esp]
        dispatch
op_ftod:
        fld     dword ptr [esp]
        fstp    qword ptr [esp]
        dispatch

; ::::::::::::::
; op_dtof - D.TOF ( d -- f ).
;
; error:  ?OVERFLOW  past the largest FLOAT
; ::::::::::::::
op_dtof:
        fld     qword ptr [esp]
        fstp    dword ptr [esp]
        mov     eax,[esp]
        and     eax,7F800000h
        cmp     eax,7F800000h
        je      err_ov
        dispatch

; ::::::::::::::
; op_dtoi - D.TOI ( d -- i ): rounded, a half to even.
;
; error:  ?OVERFLOW  past an INT
; ::::::::::::::
op_dtoi:
        dq53
        fld     qword ptr [esp]
        fistp   qword ptr [esp]
        dq24
        mov     eax,[esp]
        cdq
        cmp     edx,[esp+4]
        jne     err_ov
        dispatch

; ::::::::::::::
; op_dfn - D.FN k ( d -- d ): ABS SGN INT FIX SQR SIN COS TAN ATN LOG EXP;
; SGN's an INT.
;
; error:  ?ILLEGAL QUANTITY  SQR under 0, LOG at 0 or under
;         ?OVERFLOW          past the largest
; ::::::::::::::
op_dfn:
        movzx   ebx,byte ptr [esi]
        inc     esi
        fldcw   [g_fcwx]
        fld     qword ptr [esp]
        jmp     [g_dfn+ebx*4]
@dfn_abs:
        fabs
        jmp     @dfn_st
@dfn_sgn:
        ftst
        fnstsw  ax
        fstp    st(0)
        dq24
        sahf
        mov     eax,1
        ja      @F
        mov     eax,0
        je      @F
        dec     eax
@@:
        mov     [esp],eax
        dispatch
@dfn_int:
        fldcw   [g_fcwd]
        frndint
        jmp     @dfn_st
@dfn_fix:
        fldcw   [g_fcwc]
        frndint
        jmp     @dfn_st
@dfn_sqr:                               ; under 0: NaN, as BC's
        fsqrt
        jmp     @dfn_xs
@dfn_sin:
        call    dfn_trig
        fsin
        jmp     @dfn_st
@dfn_cos:
        call    dfn_trig
        fcos
        jmp     @dfn_st
@dfn_tan:
        call    dfn_trig
        fptan
        fstp    st(0)
        jmp     @dfn_st
@dfn_atn:
        fld1
        fpatan
        jmp     @dfn_st
@dfn_log:
        ftst
        fnstsw  ax
        sahf
        jbe     @dfn_iq
        fldln2
        fxch
        fyl2x
        jmp     @dfn_st
@dfn_exp:                               ; past 2^32767.5 ?OVERFLOW, else at
        fldl2e                          ; most infinity, as BC's
        fmulp   st(1),st
        fcom    [g_dovf]
        fnstsw  ax
        sahf
        jae     @dfn_ov
        fld     st(0)
        frndint
        fsub    st(1),st
        fxch
        f2xm1
        fld1
        faddp   st(1),st
        fscale
        fstp    st(1)
        jmp     @dfn_xs
@dfn_st:
        fstp    qword ptr [esp]
        dq24
        mov     edi,esp
        call    dbl_chk
        dispatch
@dfn_iq:
        fstp    st(0)
        dq24
        jmp     err_iq
@dfn_ov:
        fstp    st(0)
        dq24
        jmp     err_ov
@dfn_xs:
        fstp    qword ptr [esp]
        dq24
        dispatch

; ::::::::::::::
; op_dpow - D.POW ( a b -- a^b ): a whole power by squaring.
;
; error:  ?ILLEGAL QUANTITY  a negative base, a fraction power; 0 ^ a
;                            negative power
;         ?OVERFLOW          past the largest
; ::::::::::::::
op_dpow:
        fldcw   [g_fcwx]
        fld     qword ptr [esp]         ; b
        fld     st(0)
        frndint
        fcomp   st(1)
        fnstsw  ax
        sahf
        jne     @dpw_g                  ; a fraction
        fld     st(0)
        fabs
        fcomp   [g_dbig]
        fnstsw  ax
        sahf
        jae     @dpw_g
        fistp   dword ptr [g_dtmp]
        fld     qword ptr [esp+CELL]    ; a
        mov     eax,[g_dtmp]
        mov     ecx,eax
        test    eax,eax
        jns     @F
        neg     eax
@@:
        fld1                            ; r a
@dpw_l:
        test    eax,eax
        jz      @dpw_e
        shr     eax,1
        jnc     @F
        fmul    st,st(1)
@@:
        fxch
        fmul    st,st(0)
        fxch
        jmp     @dpw_l
@dpw_e:
        fstp    st(1)
        test    ecx,ecx
        jns     @dpw_st
        ftst
        fnstsw  ax
        sahf
        je      @dpw_iq
        fld1
        fdivrp  st(1),st
        jmp     @dpw_st
@dpw_g:                                 ; 2 ^ (b log2 a), a > 0
        fld     qword ptr [esp+CELL]
        ftst
        fnstsw  ax
        sahf
        jb      @dpw_iq2
        je      @dpw_z
        fyl2x
        fld     st(0)
        frndint
        fsub    st(1),st
        fxch
        f2xm1
        fld1
        faddp   st(1),st
        fscale
        fstp    st(1)
@dpw_st:
        fld     st(0)                   ; infinity: ?OVERFLOW only past
        fabs                            ; 2^32767.5, as EXP (DOSBox's FXAM
        fcomp   [g_dmax]                ; sees no infinity)
        fnstsw  ax
        sahf
        jbe     @F
        fld     qword ptr [esp]
        fld     qword ptr [esp+CELL]
        fabs
        fyl2x
        fcomp   [g_dovf]
        fnstsw  ax
        sahf
        jae     @dpw_ov
@@:
        add     esp,CELL
        fstp    qword ptr [esp]
        dq24
        dispatch
@dpw_z:                                 ; 0 ^ b: 0 when b over 0
        fstp    st(0)
        ftst
        fnstsw  ax
        sahf
        jbe     @dpw_iq
        fstp    st(0)
        fldz
        jmp     @dpw_st
@dpw_iq2:
        fstp    st(0)
@dpw_iq:
        fstp    st(0)
        dq24
        jmp     err_iq
@dpw_ov:
        fstp    st(0)
        dq24
        jmp     err_ov

; dfn_trig - ?ILLEGAL QUANTITY for SIN, COS, TAN past 2^31, as BC's.
dfn_trig:
        fld     st(0)
        fabs
        fcomp   [g_dbig]
        fnstsw  ax
        sahf
        ja      @F
        ret
@@:
        add     esp,4
        jmp     @dfn_iq

        .data
g_fcwq  dw      027Fh                   ; 53 bits, to nearest
g_fcwc  dw      0E7Fh                   ; 64 bits, toward 0
g_p10   real8   1.0E0,1.0E1,1.0E2,1.0E3,1.0E4,1.0E5,1.0E6,1.0E7,1.0E8,1.0E9
        real8   1.0E10,1.0E11,1.0E12,1.0E13,1.0E14,1.0E15,1.0E16,1.0E17,1.0E18
        real8   1.0E19,1.0E20,1.0E21,1.0E22
g_dbig  dd      4F000000h               ; 2^31
g_dovf  real4   32767.5                 ; EXP's limit, in powers of 2
g_dmax  real8   1.7976931348623157E308
g_dfn   dd      @dfn_abs,@dfn_sgn,@dfn_int,@dfn_fix,@dfn_sqr,@dfn_sin,@dfn_cos
        dd      @dfn_tan,@dfn_atn,@dfn_log,@dfn_exp
        .data?
g_dval  dq      ?                       ; dbl_in's
g_dstv  dq      ?
g_dn    dq      ?
g_dm    dq      ?                       ; dbl_in's digits
g_dbcd  dt      ?
g_dp    dd      ?
g_dtmp  dd      ?
g_dsg   db      ?
g_dig   db      16 dup(?)
g_dbuf  db      32 dup(?)
        .code

        end
