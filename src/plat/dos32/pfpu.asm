; The x87's functions (plat/pfpu.h), cdecl.
;
; # How it works
; The cores are retired/asm32/float/fpu.asm's and dbl.asm's, instruction for instruction. An error unwinds to
; the entry's frame (fail) instead of err_raise.
        .386
        .387
        .model  flat
        public  _pfpu_init, _pfpu_reset, _pfpu_fn, _pfpu_dfn, _pfpu_pow, _pfpu_dpow
        public  _pfpu_scale10, _pfpu_digits
        externdef _psys_exit:near

PFPU_IQ = 1
PFPU_OV = 2
PF_EXP = 6

        .data
cw_def  dw      027Fh                   ; between calls: 53 bits, to nearest
cw24    dw      007Fh                   ; the asm's FLOATs' (g_fcw)
cwx     dw      037Fh                   ; 64 bits (g_fcwx)
cwdn    dw      047Fh                   ; round down (g_fcwd)
cwq     dw      027Fh                   ; 53 bits (g_fcwq)
nofpu   db      "A math coprocessor is required.",13,10,"$"
dmax    real8   1.7976931348623157E308
p10     real8   1.0E0,1.0E1,1.0E2,1.0E3,1.0E4,1.0E5,1.0E6,1.0E7,1.0E8,1.0E9
        real8   1.0E10,1.0E11,1.0E12,1.0E13,1.0E14,1.0E15,1.0E16,1.0E17,1.0E18
        real8   1.0E19,1.0E20,1.0E21,1.0E22
dbig    dd      4F000000h               ; 2^31
dovf    dd      46FFFF00h               ; 32767.5: EXP's limit, in powers of 2
        .data?
savesp  dd      ?                       ; the entry's ESP, for fail
fpw     dw      ?
pwa     dt      ?                       ; fx_pow's base, power
pwb     dt      ?
pwi     dd      ?
dpow    dd      ?                       ; dbl_digits's
dnum    dq      ?
        .code

; The entry: EBP the frame, the callee-saved registers pushed, ESP kept for fail.
enter_  macro
        push    ebp
        mov     ebp,esp
        push    ebx
        push    esi
        push    edi
        mov     [savesp],esp
        endm
leave_  macro
        fldcw   [cw_def]
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret
        endm

iq:
        mov     eax,PFPU_IQ
        jmp     fail
ov:
        mov     eax,PFPU_OV
; fail - the x87 cleared, back to the entry's caller with EAX.
fail:
        mov     esp,[savesp]
        fninit
        leave_

; ::::::::::::::
; pfpu_init - flt_finit.
; ::::::::::::::
_pfpu_init:
        fninit
        mov     [fpw],5A5Ah
        fnstsw  [fpw]
        cmp     byte ptr [fpw],0
        jne     @F
        fnstcw  [fpw]
        mov     ax,[fpw]
        and     ax,103Fh
        cmp     ax,003Fh
        jne     @F
        fldcw   [cw_def]
        ret
@@:
        mov     edx,offset nofpu
        mov     ah,9
        int     21h
        push    1
        call    _psys_exit

; ::::::::::::::
; pfpu_reset - ex_fclr.
; ::::::::::::::
_pfpu_reset:
        fninit
        fldcw   [cw_def]
        ret

; ::::::::::::::
; pfpu_fn(f, x, out) - fpu.asm's fn_sqr .. fn_atn.
; ::::::::::::::
_pfpu_fn:
        enter_
        fldcw   [cw24]
        fld     dword ptr [ebp+12]
        mov     eax,[ebp+8]
        cmp     eax,PF_EXP
        jne     @F
        fldcw   [cwx]
@@:
        call    fns[eax*4]
        mov     eax,[ebp+16]
        fstp    dword ptr [eax]
        xor     eax,eax
        leave_

; ::::::::::::::
; pfpu_dfn(f, x, out) - D.FN's SQR .. EXP, at 64 bits.
; ::::::::::::::
_pfpu_dfn:
        enter_
        fldcw   [cwx]
        fld     qword ptr [ebp+12]
        mov     eax,[ebp+8]
        call    dfns[eax*4]
        mov     eax,[ebp+20]
        fstp    qword ptr [eax]
        xor     eax,eax
        leave_

        .data
fns     dd      f_sqr,fx_sin,fx_cos,fx_tan,fx_atn,fx_log,fx_exp
dfns    dd      d_sqr,fx_sin,fx_cos,fx_tan,fx_atn,fx_log,fx_exp
        .code

f_sqr:                                  ; FLOAT: ?ILLEGAL QUANTITY under 0
        ftst
        fnstsw  ax
        sahf
        jb      iq
d_sqr:                                  ; DOUBLE: NaN under 0, as BC's
        fsqrt
        ret

; ::::::::::::::
; pfpu_pow(a, b, out), pfpu_dpow(a, b, out) - fn_pow, op_dpow.
; ::::::::::::::
_pfpu_pow:
        enter_
        fldcw   [cwx]
        fld     dword ptr [ebp+8]
        fld     dword ptr [ebp+12]
        call    fx_pow
        mov     eax,[ebp+16]
        fstp    dword ptr [eax]
        xor     eax,eax
        leave_
_pfpu_dpow:
        enter_
        fldcw   [cwx]
        fld     qword ptr [ebp+8]
        fld     qword ptr [ebp+16]
        call    fx_pow
        mov     eax,[ebp+24]
        fstp    qword ptr [eax]
        xor     eax,eax
        leave_

; ::::::::::::::
; pfpu_scale10(m, p, out) - dbl_in's m * 10^p at 53 bits.
; ::::::::::::::
_pfpu_scale10:
        enter_
        fldcw   [cwq]
        fild    qword ptr [ebp+8]
        mov     eax,[ebp+16]
        call    scale
        mov     eax,[ebp+20]
        fstp    qword ptr [eax]
        leave_

; scale - st0 * 10 ^ EAX by the exact powers to 10^22 (dbl_scale).
scale:
        cmp     eax,22
        jle     @F
        fmul    [p10+22*8]
        sub     eax,22
        jmp     scale
@@:
        cmp     eax,-22
        jge     @F
        fdiv    [p10+22*8]
        add     eax,22
        jmp     @B
@@:
        test    eax,eax
        js      @F
        fmul    [p10+eax*8]
        ret
@@:
        neg     eax
        fdiv    [p10+eax*8]
        ret

; ::::::::::::::
; pfpu_digits(v, ndig, dp) - dbl_digits: EDX:EAX v's ndig digits rounded once, *dp the first's
; power of ten.
; ::::::::::::::
_pfpu_digits:
        enter_
        mov     esi,[ebp+16]            ; ndig
        fld     qword ptr [ebp+8]
        fldcw   [cwx]                   ; 64 bits: v * 10^k rounds once
        fld     st(0)                   ; p: the power of the first digit
        fldlg2
        fxch
        fyl2x
        fldcw   [cwdn]
        fistp   [dpow]
        fldcw   [cwx]
        mov     ebx,4                   ; tries
@ddg_s:
        mov     eax,esi                 ; N = v * 10^(nd-1-p), nd digits
        dec     eax
        sub     eax,[dpow]
        fld     st(0)
        call    scale
        dec     ebx
        jz      @F
        fcom    [p10+esi*8-8]           ; under 10^(nd-1) unrounded: p one too high
        fnstsw  ax
        sahf
        jae     @F
        fstp    st(0)
        dec     [dpow]
        jmp     @ddg_s
@@:
        frndint
        fcom    [p10+esi*8]             ; rounded up to 10^nd: 10^(nd-1), one place up
        fnstsw  ax
        sahf
        jb      @F
        fstp    st(0)
        fld     [p10+esi*8-8]
        inc     [dpow]
@@:
        fistp   [dnum]
        fstp    st(0)
        mov     eax,[dpow]
        mov     edx,[ebp+20]
        mov     [edx],eax
        mov     eax,dword ptr [dnum]
        mov     edx,dword ptr [dnum+4]
        leave_

; The x87 cores, as fpu.asm's: st0 in, st0 out.
; fx_exp - st0 = e^st0; ?OVERFLOW past 2^32767.5.
fx_exp:
        fldl2e
        fmulp   st(1),st
        fcom    dword ptr [dovf]
        fnstsw  ax
        sahf
        jae     ov
; fx_x2x - st0 = 2^st0.
fx_x2x:
        fld     st(0)
        frndint
        fsub    st(1),st
        fxch
        f2xm1
        fld1
        faddp   st(1),st
        fscale
        fstp    st(1)
        ret
; fx_pow - st0 = st1 ^ st0, popping one; a whole power by squaring.
fx_pow:
        fstp    [pwb]
        fstp    [pwa]
        fld     [pwb]
        fld     st(0)
        frndint
        fcomp   st(1)
        fnstsw  ax
        sahf
        jne     @fpw_g                  ; a fraction
        fld     st(0)
        fabs
        fcomp   dword ptr [dbig]
        fnstsw  ax
        sahf
        jae     @fpw_g
        fistp   [pwi]
        fld     [pwa]
        mov     eax,[pwi]
        mov     ecx,eax
        test    eax,eax
        jns     @F
        neg     eax
@@:
        fld1                            ; r a
@fpw_l:
        test    eax,eax
        jz      @fpw_e
        shr     eax,1
        jnc     @F
        fmul    st,st(1)
@@:
        fxch
        fmul    st,st(0)
        fxch
        jmp     @fpw_l
@fpw_e:
        fstp    st(1)
        test    ecx,ecx
        jns     @fpw_c
        ftst
        fnstsw  ax
        sahf
        je      iq
        fld1
        fdivrp  st(1),st
        jmp     @fpw_c
@fpw_g:                                 ; 2 ^ (b log2 a), a > 0
        fld     [pwa]
        ftst
        fnstsw  ax
        sahf
        jb      iq
        je      @fpw_z
        fyl2x
        call    fx_x2x
@fpw_c:
        fld     st(0)                   ; infinity: ?OVERFLOW only past 2^32767.5
        fabs
        fcomp   [dmax]
        fnstsw  ax
        sahf
        jbe     @F
        fld     [pwb]
        fld     [pwa]
        fabs
        fyl2x
        fcomp   dword ptr [dovf]
        fnstsw  ax
        sahf
        jae     ov
@@:
        ret
@fpw_z:                                 ; 0 ^ b: 0 when b over 0
        fstp    st(0)
        ftst
        fnstsw  ax
        sahf
        jbe     iq
        fstp    st(0)
        fldz
        jmp     @fpw_c
; fx_log - st0 = ln st0; ?ILLEGAL QUANTITY at 0 or under.
fx_log:
        ftst
        fnstsw  ax
        sahf
        jbe     iq
        fldln2
        fxch
        fyl2x
        ret
; fx_sin, fx_cos, fx_tan; ?ILLEGAL QUANTITY past 2^31.
fx_sin:
        call    fx_trg
        fsin
        ret
fx_cos:
        call    fx_trg
        fcos
        ret
fx_tan:
        call    fx_trg
        fptan
        fstp    st(0)
        ret
fx_trg:
        fld     st(0)
        fabs
        fcomp   dword ptr [dbig]
        fnstsw  ax
        sahf
        ja      iq
        ret
; fx_atn - st0 = atan st0.
fx_atn:
        fld1
        fpatan
        ret

        end
