; ==========================================================================
; fpu.asm - the float work, on the x87
; ==========================================================================
; holds:    flt_finit, flt_add .. flt_div, flt_from16, flt_from32,
;           flt_fromu16, flt_fix16, flt_fixu16, flt_fix32, flt_int, fn_pow, fn_sqr ..
;           fn_atn, ex_fclr
; uses:     flt_zero, err_ov, err_iq, err_raise
; data:     a packed FLOAT is an x87 single: e 0 is 0, e 255 is ?OVERFLOW.
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
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_add:
        call    flt_fld
        fadd    dword ptr [g_state.ft.fpa]

; ::::::::::::::
; flt_fst - pop the x87's result; a denormal is 0, an infinity ?OVERFLOW.
;
; in:     st0  the result
; out:    EAX  the result, packed
; error:  ?OVERFLOW  an infinity
; ::::::::::::::
flt_fst:
        fstp    dword ptr [g_state.ft.fpa]
        mov     eax,[g_state.ft.fpa]
        mov     ecx,eax
        and     ecx,7F800000h
        jz      flt_zero
        cmp     ecx,7F800000h
        je      err_ov
        ret

; ::::::::::::::
; flt_sub - FAC = ARG - FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
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
; error:  ?OVERFLOW  past EMAX
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
;         ?OVERFLOW          past EMAX
; ::::::::::::::
flt_div:
        test    eax,0FF000000h
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

; ::::::::::::::
; flt_x2x - st0 = 2^st0.
;
; in:     st0  the power
;         fcwx  loaded
; out:    st0  the result
;         fcw   loaded
; ::::::::::::::
flt_x2x:
        fld     st(0)
        frndint
        fsub    st(1),st
        fxch
        f2xm1
        fld1
        faddp   st(1),st
        fscale
        fstp    st(1)
        fldcw   [g_fcw]
        ret

; ::::::::::::::
; flt_xpw - st0 ^ EAX; whole powers under 256 by squaring.
;
; in:     st0  the base
;         EAX  the power, packed
; out:    st0  the result
; keeps:  SI
; error:  ?ILLEGAL QUANTITY  a negative base, a fraction power
;         ?OVERFLOW          past the largest FLOAT
; ::::::::::::::
flt_xpw:
        mov     [g_state.ft.ft4],eax
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        jnz     @fxw_b
        fstp    st(0)
        fld1
        ret
@fxw_b:
        mov     edx,eax
        ftst
        fnstsw  ax
        sahf
        je      @fxw_r
        setb    bl
        mov     eax,edx
        sub     ecx,FBIAS
        cmp     ecx,8
        jae     @fxw_g
        mov     ch,cl                   ; k
        mov     cl,23
        sub     cl,ch
        mov     edx,1
        shl     edx,cl
        dec     edx
        test    eax,edx                 ; a fraction left?
        jnz     @fxw_g
        test    eax,eax                 ; a negative power
        js      @fxw_g
        and     eax,7FFFFFh
        or      eax,800000h
        shr     eax,16                  ; the power's bits, the top at bit 7
        cmp     al,80h
        je      @fxw_2
        fld     st(0)
@fxw_l:
        shl     al,1
        dec     ch
        js      @fxw_e
        fmul    st,st(0)
        test    al,80h
        jz      @fxw_l
        fmul    st,st(1)
        jmp     @fxw_l
@fxw_e:
        fstp    st(1)
@fxw_r:
        ret
@fxw_2:
        dec     ch
        js      @fxw_r
        fmul    st,st(0)
        jmp     @fxw_2
@fxw_g:
        fldcw   [g_fcwx]
        fabs
        fld     dword ptr [g_state.ft.ft4]
        fxch
        fyl2x
        call    flt_x2x
        call    flt_fst
        movzx   edx,bl
        shl     edx,31
        call    flt_psg
        mov     [g_state.ft.fpa],eax
        fld     dword ptr [g_state.ft.fpa]
        ret

; ::::::::::::::
; flt_psg - a negative base's power: its sign by the power, odd or even.
;
; in:     EAX  |base| ^ power, packed
;         EDX  bit 31: the base was negative
;         ft4  the power, packed
; out:    EAX  the result
; error:  ?ILLEGAL QUANTITY  a negative base, a fraction power
; ::::::::::::::
flt_psg:
        test    edx,edx
        jns     @fps_r
        mov     [g_state.ft.ft2],eax
        mov     eax,[g_state.ft.ft4]
        call    flt_whl
        jnz     err_iq
        sub     eax,00800000h           ; half
        call    flt_whl
        pushf
        mov     eax,[g_state.ft.ft2]
        popf
        jz      @fps_r
        jmp     flt_neg
@fps_r:
        ret

; ::::::::::::::
; flt_whl - is the FAC whole?
;
; in:     EAX  the FAC, packed
; out:    ZF   set when whole
;         EAX  the FAC
; ::::::::::::::
flt_whl:
        mov     edx,eax
        call    flt_int
        push    edx
        call    flt_cmp
        pop     edx
        test    al,al
        mov     eax,edx
        ret

; ::::::::::::::
; flt_xchk - a result: a denormal is 0, an infinity ?OVERFLOW.
;
; in:     EAX  the result
; out:    EAX  the result, packed
; keeps:  BX, SI, DI
; error:  ?OVERFLOW  an infinity
; ::::::::::::::
flt_xchk:
        mov     ecx,eax
        and     ecx,7F800000h
        jz      flt_zero
        cmp     ecx,7F800000h
        je      err_ov
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
@st_fpu2:
        jmp     err_iq

; ::::::::::::::
; fn_pow - EDX ^ EAX on the x87.
;
; in:     EAX  the power, packed
;         EDX  the base, packed
; out:    EAX  the result, packed
; ::::::::::::::
fn_pow:
        mov     [g_state.ft.fpb],edx
        fld     dword ptr [g_state.ft.fpb]
        call    flt_xpw
        jmp     flt_fst

; The x87 builtins: EAX in, EAX out, a single so X=SIN(A):X=SIN(A) holds.
; ::::::::::::::
; fn_sqr - SQR on the x87.
; ::::::::::::::
fn_sqr:
        call    flt_xtst
        jb      @st_fpu2
        fsqrt
        jmp     flt_fst
; ::::::::::::::
; fn_log - LOG on the x87.
; ::::::::::::::
fn_log:
        call    flt_xtst
        jbe     @st_fpu2
        fldln2
        fxch
        fyl2x
        jmp     flt_fst
; ::::::::::::::
; fn_exp - EXP on the x87.
; ::::::::::::::
fn_exp:
        call    flt_xin
        fldcw   [g_fcwx]
        fldl2e
        fmulp   st(1),st
        call    flt_x2x
        jmp     flt_fst
; ::::::::::::::
; fn_cos - COS on the x87.
; ::::::::::::::
fn_cos:
        call    flt_xin
        fcos
        jmp     @fxn_c2
; ::::::::::::::
; fn_sin - SIN on the x87.
; ::::::::::::::
fn_sin:
        call    flt_xin
        fsin
@fxn_c2:
        fnstsw  ax
        test    ah,4                    ; C2: past 2^63, not reduced
        jz      flt_fst
@fxn_c0:
        fstp    st(0)
        xor     eax,eax
        ret
; ::::::::::::::
; fn_tan - TAN on the x87.
; ::::::::::::::
fn_tan:
        call    flt_xin
        fptan
        fnstsw  ax
        test    ah,4
        jnz     @fxn_c0
        fstp    st(0)
        jmp     flt_fst
; ::::::::::::::
; fn_atn - ATN on the x87.
; ::::::::::::::
fn_atn:
        call    flt_xin
        fld1
        fpatan
        jmp     flt_fst

        end
