; ==========================================================================
; fpu.asm - the float work on the x87, patched over the soft routines' entries
; ==========================================================================
; holds:    flt_finit, the x87 stubs under the soft entries
; uses:     flt_add, flt_sub, flt_mul, flt_div, flt_zero, err_ov, err_iq,
;           err_raise, fn_pow, fn_sqr .. fn_atn
; data:     a packed FLOAT is an x87 single: e 0 is 0, e 255 is ?OVERFLOW.
;           Every routine takes and returns packed singles; the x87 stack is
;           empty between them.
; calls:
;   flt_finit          no FPU: nothing; else the jmps and the handlers
;   flt_fadd .. flt_fdiv
;     flt_fld          both operands, ARG on the x87 stack
;     flt_fst          the result, packed
; ==========================================================================

; ::::::::::::::
; flt_finit - find the FPU; with one, send the soft entries to the stubs.
;
; out:    the x87  24-bit precision, exceptions masked
; ::::::::::::::
flt_finit:
        fninit
        mov     [fpw],5A5Ah
        fnstsw  [fpw]
        cmp     byte ptr [fpw],0
        jne     @ffi_r
        fnstcw  [fpw]
        mov     ax,[fpw]
        and     ax,103Fh
        cmp     ax,003Fh
        jne     @ffi_r
        fldcw   [fcw]
        mov     bx,offset fptab
@ffi_l:
        mov     cx,[bx]
        jcxz    @ffi_o
        mov     ax,[bx+2]
        sub     ax,cx
        sub     ax,3
        xchg    bx,cx
        mov     byte ptr [bx],0E9h
        mov     [bx+1],ax
        xchg    bx,cx
        add     bx,4
        jmp     @ffi_l
@ffi_o:
        mov     [hasfpu],1
@ffi_r:
        ret

DSEG    segment
fcw     dw      007Fh
fcwx    dw      037Fh                   ; 64-bit precision: 2^x's steps
fcwd    dw      047Fh                   ; round down
fptab   dw      flt_add,flt_fadd,flt_sub,flt_fsub,flt_mul,flt_fmul,flt_div,flt_fdiv
        dw      flt_from16,flt_ffrom,flt_from32,flt_ffrom32,flt_fromu16,flt_ffromu
        dw      flt_fix16,flt_ffix
        dw      flt_fixu16,flt_ffixu,flt_int,flt_ffloor,fn_pow,flt_fpow
        dw      fn_sqr,flt_fsqr,fn_log,flt_flog,fn_exp,flt_fexp,fn_cos,flt_fcos
        dw      fn_sin,flt_fsin,fn_tan,flt_ftan,fn_atn,flt_fatn,0
DSEG    ends

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
        mov     [fpa],eax
        mov     [fpb],edx
        fld     dword ptr [fpb]
        ret

; ::::::::::::::
; flt_fadd - FAC = ARG + FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_fadd:
        call    flt_fld
        fadd    dword ptr [fpa]

; ::::::::::::::
; flt_fst - pop the x87's result; a denormal is 0, an infinity ?OVERFLOW.
;
; in:     st0  the result
; out:    EAX  the result, packed
; error:  ?OVERFLOW  an infinity
; ::::::::::::::
flt_fst:
        fstp    dword ptr [fpa]
        mov     eax,[fpa]
        mov     ecx,eax
        and     ecx,7F800000h
        jz      flt_zero
        cmp     ecx,7F800000h
        je      err_ov
        ret

; ::::::::::::::
; flt_fsub - FAC = ARG - FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_fsub:
        call    flt_fld
        fsub    dword ptr [fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_fmul - FAC = ARG * FAC.
;
; in:     EAX  the FAC, packed
;         EDX  the ARG, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past EMAX
; ::::::::::::::
flt_fmul:
        call    flt_fld
        fmul    dword ptr [fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_fdiv - FAC = ARG / FAC.
;
; in:     EAX  the divisor, packed
;         EDX  the dividend, packed
; out:    EAX  the result, packed
; error:  ?DIVISION BY ZERO  FAC 0
;         ?OVERFLOW          past EMAX
; ::::::::::::::
flt_fdiv:
        test    eax,0FF000000h
        jnz     @ffd_g
        mov     al,E_DZ
        jmp     err_raise
@ffd_g:
        call    flt_fld
        fdiv    dword ptr [fpa]
        jmp     flt_fst

; ::::::::::::::
; flt_ffrom - FAC = the INT in AX.
;
; in:     AX  the INT
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_ffrom:
        mov     word ptr [fpi],ax
        fild    word ptr [fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_ffrom32 - FAC = the signed EAX.
;
; in:     EAX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_ffrom32:
        mov     [fpi],eax
        fild    dword ptr [fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_ffromu - FAC = AX, unsigned.
;
; in:     AX  the number
; out:    EAX  the result, packed
; keeps:  EDX
; ::::::::::::::
flt_ffromu:
        movzx   eax,ax
        mov     [fpi],eax
        fild    dword ptr [fpi]
        jmp     flt_fst

; ::::::::::::::
; flt_ffixu - AX = floor(FAC), 0..65535.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_ffixu:
        xor     cl,cl
        jmp     @fff_l

; ::::::::::::::
; flt_ffix - AX = floor(FAC), -32768..32767.
;
; in:     EAX  the FAC, packed
; out:    AX  the number
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_ffix:
        mov     cl,80h
@fff_l:
        mov     [fpa],eax
        fld     dword ptr [fpa]

; ::::::::::::::
; flt_xfix - pop st0 as an INT, floored.
;
; in:     st0  the value
;         CL   80h signed, 0 unsigned
; out:    AX  the number
; keeps:  EDX, BX, SI, DI
; error:  ?ILLEGAL QUANTITY  out of range
; ::::::::::::::
flt_xfix:
        fldcw   [fcwd]
        fistp   dword ptr [fpi]
        fldcw   [fcw]
        mov     eax,[fpi]
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
; flt_ffloor - FAC = floor(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_ffloor:
        mov     [fpa],eax
        fld     dword ptr [fpa]
        fldcw   [fcwd]
        frndint
        fldcw   [fcw]
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
        fldcw   [fcw]
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
        mov     [ft4],eax
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
        fldcw   [fcwx]
        fabs
        fld     dword ptr [ft4]
        fxch
        fyl2x
        call    flt_x2x
        call    flt_fst
        movzx   edx,bl
        shl     edx,31
        call    @fp_sg
        mov     [fpa],eax
        fld     dword ptr [fpa]
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
        cmp     [hasfpu],0
        je      @fcl_r
        fninit
        fldcw   [fcw]
@fcl_r:
        ret

; ::::::::::::::
; flt_xin - EAX onto the x87 stack.
;
; in:     EAX  packed
; out:    st0  it
; ::::::::::::::
flt_xin:
        mov     [fpa],eax
        fld     dword ptr [fpa]
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
; flt_fpow - EDX ^ EAX on the x87.
;
; in:     EAX  the power, packed
;         EDX  the base, packed
; out:    EAX  the result, packed
; ::::::::::::::
flt_fpow:
        mov     [fpb],edx
        fld     dword ptr [fpb]
        call    flt_xpw
        jmp     flt_fst

; The x87 builtins: EAX in, EAX out, a single so X=SIN(A):X=SIN(A) holds.
; ::::::::::::::
; flt_fsqr - SQR on the x87.
; ::::::::::::::
flt_fsqr:
        call    flt_xtst
        jb      @st_fpu2
        fsqrt
        jmp     flt_fst
; ::::::::::::::
; flt_flog - LOG on the x87.
; ::::::::::::::
flt_flog:
        call    flt_xtst
        jbe     @st_fpu2
        fldln2
        fxch
        fyl2x
        jmp     flt_fst
; ::::::::::::::
; flt_fexp - EXP on the x87.
; ::::::::::::::
flt_fexp:
        call    flt_xin
        fldcw   [fcwx]
        fldl2e
        fmulp   st(1),st
        call    flt_x2x
        jmp     flt_fst
; ::::::::::::::
; flt_fcos - COS on the x87.
; ::::::::::::::
flt_fcos:
        call    flt_xin
        fcos
        jmp     @fxn_c2
; ::::::::::::::
; flt_fsin - SIN on the x87.
; ::::::::::::::
flt_fsin:
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
; flt_ftan - TAN on the x87.
; ::::::::::::::
flt_ftan:
        call    flt_xin
        fptan
        fnstsw  ax
        test    ah,4
        jnz     @fxn_c0
        fstp    st(0)
        jmp     flt_fst
; ::::::::::::::
; flt_fatn - ATN on the x87.
; ::::::::::::::
flt_fatn:
        call    flt_xin
        fld1
        fpatan
        jmp     flt_fst
