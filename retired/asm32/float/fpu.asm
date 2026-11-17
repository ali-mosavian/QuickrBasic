; ==========================================================================
; fpu.asm - the float work, on the x87
; ==========================================================================
; holds:    flt_finit, flt_add .. flt_div, flt_from16, flt_from32,
;           flt_fromu16, flt_fix16, flt_fixu16, flt_fix32, flt_int, fn_pow,
;           fn_sqr ..
;           fn_atn, ex_fclr, fx_exp .. fx_atn: the x87 cores DOUBLE shares
; uses:     err_ov, err_iq, err_raise
; data:     a packed FLOAT is an x87 single; past its range infinity, under
;           it a denormal, as BC's.
;           Every routine takes and returns packed singles; the x87 stack is
;           empty between them.
; calls:
;   flt_add .. flt_div
;     flt_fld          both operands, ARG on the x87 stack
;     flt_fst          the result, packed
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; flt_finit - find the x87; with none, say so and leave.
;
; out:    the x87  24-bit precision, exceptions masked
; ::::::::::::::
flt_finit:
        fninit
        mov     [g_state.ft.fpw],5A5Ah
        fnstsw  [g_state.ft.fpw]
        cmp     byte ptr [g_state.ft.fpw],0
        jne     @ffi_n
        fnstcw  [g_state.ft.fpw]
        mov     ax,[g_state.ft.fpw]
        and     ax,103Fh
        cmp     ax,003Fh
        jne     @ffi_n
        fldcw   [g_fcw]
        ret
@ffi_n:
        mov     edx,offset g_nofpu
        mov     ah,9
        int     21h
        mov     ax,4C01h
        int     21h

        .data
g_fcw   dw      007Fh
g_fcwx  dw      037Fh                    ; 64-bit precision: 2^x's steps
g_fcwd  dw      047Fh                    ; round down
g_nofpu db      "A math coprocessor is required.",13,10,"$"
g_dmax  real8   1.7976931348623157E308
        .data?
g_pwa   dt      ?                       ; fx_pow's base, power
g_pwb   dt      ?
g_pwi   dd      ?
        .code

; ::::::::::::::
; flt_fld - both operands to memory; the ARG on the x87 stack.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    fpa  the FAC
;         st0  the ARG
; keeps:  all
; ::::::::::::::
flt_fld:
        mov     [g_state.ft.fpa],eax
        mov     [g_state.ft.fpb],edx
        fld     dword ptr [g_state.ft.fpb]
        ret

; ::::::::::::::
; flt_add - FAC = ARG + FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_add:
        call    flt_fld
        fadd    dword ptr [g_state.ft.fpa]

; ::::::::::::::
; flt_fst - pop the x87's result.
;
; in:     st0  the result
; out:    EAX  the result, packed
; ::::::::::::::
flt_fst:
        fstp    dword ptr [g_state.ft.fpa]
        mov     eax,[g_state.ft.fpa]
        ret

; ::::::::::::::
; flt_sub - FAC = ARG - FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_sub:
        call    flt_fld
        fsub    dword ptr [g_state.ft.fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_mul - FAC = ARG * FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_mul:
        call    flt_fld
        fmul    dword ptr [g_state.ft.fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_div - FAC = ARG / FAC.
;
; in:     EAX  the divisor, packed
;         EDX  the dividend, packed
; out:    EAX  the result, packed
; error:  ?DIVISION BY ZERO  FAC 0
; ::::::::::::::
flt_div:
        test    eax,7FFFFFFFh
        jnz     @ffd_g
        mov     al,E_DZ
        jmp     err_raise
@ffd_g:
        call    flt_fld
        fdiv    dword ptr [g_state.ft.fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_from16 - FAC = the INT in AX.
;
; in:     AX  the INT
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_from16:
        mov     word ptr [g_state.ft.fpi],ax
        fild    word ptr [g_state.ft.fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_from32 - FAC = the signed EAX.
;
; in:     EAX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_from32:
        mov     [g_state.ft.fpi],eax
        fild    dword ptr [g_state.ft.fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_fromu16 - FAC = AX, unsigned.
;
; in:     AX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_fromu16:
        movzx   eax,ax
        mov     [g_state.ft.fpi],eax
        fild    dword ptr [g_state.ft.fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_fixu16 - AX = FAC rounded, 0..65535.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_fixu16:
        xor     cl,cl
        jmp     @fff_l

; ::::::::::::::
; flt_fix16 - AX = FAC rounded, -32768..32767.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_fix16:
        mov     cl,80h
@fff_l:
        mov     [g_state.ft.fpa],eax
        fld     dword ptr [g_state.ft.fpa]

; ::::::::::::::
; flt_xfix - pop st0 as an INT, rounded to the nearest, a half to even, as
; QBasic's.
;
; in:     st0  the value
;         CL   80h signed, 0 unsigned
; out:    AX  the number
; keeps:  EDX, BX, SI, DI
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_xfix:
        fistp   dword ptr [g_state.ft.fpi]
        mov     eax,[g_state.ft.fpi]
        test    cl,cl
        jz      @fxx_u
        movsx   ecx,ax
        cmp     ecx,eax
        jne     @st_fpu1
        ret
@st_fpu1:
        jmp     err_iq
@fxx_u:
        test    eax,0FFFF0000h
        jnz     @st_fpu1
        ret

; ::::::::::::::
; flt_fix32 - EAX = FAC rounded to the nearest, a half to even, as QBasic's.
;
; in:     EAX  the FAC, packed
; out:    EAX  the number
; keeps:  all but EAX
; error:  ?ILLEGAL QUANTITY  past -2^31..2^31-1
; ::::::::::::::
flt_fix32:
        mov     [g_state.ft.fpa],eax
        fld     dword ptr [g_state.ft.fpa]
        fistp   dword ptr [g_state.ft.fpi]
        mov     eax,[g_state.ft.fpi]
        cmp     eax,80000000h           ; out of range, or -2^31 itself
        je      @fx3_e
        ret
@fx3_e:
        cmp     [g_state.ft.fpa],0CF000000h
        jne     err_iq
        ret

; ::::::::::::::
; flt_int - FAC = floor(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_int:
        mov     [g_state.ft.fpa],eax
        fld     dword ptr [g_state.ft.fpa]
        fldcw   [g_fcwd]
        frndint
        fldcw   [g_fcw]
        jmp     flt_fst

; The x87 cores, shared by SINGLE and DOUBLE: st0 in, st0 out, at the
; caller's precision. An error jumps out; err_raise clears the x87.
; ::::::::::::::
; fx_exp - st0 = e^st0. ?OVERFLOW past 2^32767.5, else at most infinity,
; as BC's.
; ::::::::::::::
fx_exp:
        fldl2e
        fmulp   st(1),st
        fcom    [g_dovf]
        fnstsw  ax
        sahf
        jae     err_ov
; ::::::::::::::
; fx_x2x - st0 = 2^st0.
; ::::::::::::::
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
; ::::::::::::::
; fx_pow - st0 = st1 ^ st0, popping one; a whole power by squaring.
;
; error:  ?ILLEGAL QUANTITY  a negative base, a fraction power; 0 ^ a
;                            negative power
;         ?OVERFLOW          past 2^32767.5, as EXP (DOSBox's FXAM sees no
;                            infinity)
; ::::::::::::::
fx_pow:
        fstp    [g_pwb]
        fstp    [g_pwa]
        fld     [g_pwb]
        fld     st(0)
        frndint
        fcomp   st(1)
        fnstsw  ax
        sahf
        jne     @fpw_g                  ; a fraction
        fld     st(0)
        fabs
        fcomp   [g_dbig]
        fnstsw  ax
        sahf
        jae     @fpw_g
        fistp   [g_pwi]
        fld     [g_pwa]
        mov     eax,[g_pwi]
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
        je      err_iq
        fld1
        fdivrp  st(1),st
        jmp     @fpw_c
@fpw_g:                                 ; 2 ^ (b log2 a), a > 0
        fld     [g_pwa]
        ftst
        fnstsw  ax
        sahf
        jb      err_iq
        je      @fpw_z
        fyl2x
        call    fx_x2x
@fpw_c:
        fld     st(0)                   ; infinity: ?OVERFLOW only past
        fabs                            ; 2^32767.5
        fcomp   [g_dmax]
        fnstsw  ax
        sahf
        jbe     @F
        fld     [g_pwb]
        fld     [g_pwa]
        fabs
        fyl2x
        fcomp   [g_dovf]
        fnstsw  ax
        sahf
        jae     err_ov
@@:
        ret
@fpw_z:                                 ; 0 ^ b: 0 when b over 0
        fstp    st(0)
        ftst
        fnstsw  ax
        sahf
        jbe     err_iq
        fstp    st(0)
        fldz
        jmp     @fpw_c
; ::::::::::::::
; fx_log - st0 = ln st0; ?ILLEGAL QUANTITY at 0 or under.
; ::::::::::::::
fx_log:
        ftst
        fnstsw  ax
        sahf
        jbe     err_iq
        fldln2
        fxch
        fyl2x
        ret
; ::::::::::::::
; fx_sin, fx_cos, fx_tan; ?ILLEGAL QUANTITY past 2^31, as BC's.
; ::::::::::::::
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
        fcomp   [g_dbig]
        fnstsw  ax
        sahf
        ja      err_iq
        ret
; ::::::::::::::
; fx_atn - st0 = atan st0.
; ::::::::::::::
fx_atn:
        fld1
        fpatan
        ret

; ::::::::::::::
; ex_fclr - an error leaves nothing on the x87 stack.
; ::::::::::::::
ex_fclr:
        fninit
        fldcw   [g_fcw]
        ret

; ::::::::::::::
; flt_xin - EAX onto the x87 stack.
;
; in:     EAX  packed
; out:    st0  it
; ::::::::::::::
flt_xin:
        mov     [g_state.ft.fpa],eax
        fld     dword ptr [g_state.ft.fpa]
        ret

; ::::::::::::::
; flt_xtst - load EAX and compare it with 0.
;
; out:    st0    EAX
;         flags  as for an unsigned compare of st0 with 0
; ::::::::::::::
flt_xtst:
        call    flt_xin
        ftst
        fnstsw  ax
        sahf
        ret

; ::::::::::::::
; fn_pow - EDX ^ EAX.
; ::::::::::::::
fn_pow:
        mov     [g_state.ft.fpb],edx
        mov     [g_state.ft.fpa],eax
        fldcw   [g_fcwx]
        fld     dword ptr [g_state.ft.fpb]
        fld     dword ptr [g_state.ft.fpa]
        call    fx_pow
        fldcw   [g_fcw]
        jmp     flt_fst

; The x87 builtins: EAX in, EAX out, a single so X=SIN(A):X=SIN(A) holds.
; ::::::::::::::
; fn_sqr - SQR on the x87.
; ::::::::::::::
fn_sqr:
        call    flt_xtst
        jb      err_iq
        fsqrt
        jmp     flt_fst
; ::::::::::::::
; fn_exp - EXP; an infinity stays, as BC's.
; ::::::::::::::
fn_exp:
        call    flt_xin
        fldcw   [g_fcwx]
        call    fx_exp
        fldcw   [g_fcw]
        jmp     flt_fst
; ::::::::::::::
; fn_log .. fn_atn - EAX through its x87 core.
; ::::::::::::::
fn_log:
        mov     edx,offset fx_log
        jmp     flt_xfn
fn_cos:
        mov     edx,offset fx_cos
        jmp     flt_xfn
fn_sin:
        mov     edx,offset fx_sin
        jmp     flt_xfn
fn_tan:
        mov     edx,offset fx_tan
        jmp     flt_xfn
fn_atn:
        mov     edx,offset fx_atn
flt_xfn:
        call    flt_xin
        call    edx
        jmp     flt_fst

        end
