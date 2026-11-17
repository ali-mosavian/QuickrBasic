; ==========================================================================
; ftr.asm - LOG, EXP, ^, SIN, COS, TAN, ATN, RND
; ==========================================================================
; holds:    fn_log, fn_exp, fn_pow, fn_sqr, fn_sin, fn_cos, fn_tan, fn_atn,
;           fn_rnd, flt_pi
; uses:     flt.asm, err_iq, err_ov, ft1..ft4, rs
; data:     ktab: the series' terms, highest first, then the constants
; calls:
;   fn_pow             squaring, or fn_log and fn_exp
;   fn_log, fn_sin, fn_atn
;     flt_odd
;       flt_polyf
;   fn_exp
;     flt_poly
; ==========================================================================

fcon    macro   sg,m,e
        dd      ((e+FBIAS) SHL 23) OR (sg SHL 31) OR (m AND 7FFFFFh)
        endm

DSEG    segment
ktab    label   dword
klog    label   dword
        fcon    0,090537Ch,-2
        fcon    0,08F2581h,-2
        fcon    0,0CCEDA9h,-2
        fcon    0,0AAAA6Fh,-1
        fcon    0,0800000h,1
kexp    label   dword
        fcon    0,0E3A3BCh,-13
        fcon    0,0A305ABh,-10
        fcon    0,09E94F4h,-7
        fcon    0,0E34262h,-5
        fcon    0,0F5FED1h,-3
        fcon    0,0B17215h,-1
        fcon    0,0800000h,0
ksin    label   dword
        fcon    0,09ED88Bh,5
        fcon    1,099266Fh,6
        fcon    0,0A33458h,6
        fcon    1,0A55DE1h,5
        fcon    0,0C90FDBh,2
katn    label   dword
        fcon    0,0BD81FFh,-9
        fcon    1,08502B3h,-6
        fcon    0,0AFFD72h,-5
        fcon    1,09A2AF7h,-4
        fcon    0,0DA2743h,-4
        fcon    1,091797Ch,-3
        fcon    0,0CCBA67h,-3
        fcon    1,0AAAA59h,-2
kone    label   dword
        fcon    0,0800000h,0
kln2    label   dword
        fcon    0,0B17218h,-1
kl2e    label   dword
        fcon    0,0B8AA3Bh,0
kr2p    label   dword
        fcon    0,0A2F983h,-3
kp2     label   dword
        fcon    0,0C90FDBh,0
kpi     label   dword
        fcon    0,0C90FDBh,1
DSEG    ends
NLOG    = 5
NEXP    = 7
NSIN    = 5
NATN    = 9

; ::::::::::::::
; flt_poly - FAC = a polynomial of the FAC, Horner.
;
; in:     EAX  the FAC, packed
;         BX   its first term
;         CX   the terms less one
; out:    EAX  the result, packed
; ::::::::::::::
flt_poly:
        mov     [fipt],bx
        mov     [fcnt],cx
; ::::::::::::::
; flt_polyf - flt_poly, the terms in fipt and fcnt.
;
; in:     EAX   the FAC, packed
;         fipt  its first term
;         fcnt  the terms less one
; out:    EAX  the result, packed
; ::::::::::::::
flt_polyf:
        mov     [ft3],eax
        mov     bx,[fipt]
        mov     eax,[bx]
@@:
        mov     edx,[ft3]
        call    flt_mul
        add     [fipt],4
        mov     bx,[fipt]
        mov     edx,[bx]
        call    flt_add
        dec     [fcnt]
        jnz     @B
        ret
@st_ftr1:
        jmp     err_iq

; ::::::::::::::
; flt_odd - FAC = x * P(x^2).
;
; in:     EAX  x, packed
;         BX   P's first term
;         CX   the terms less one
; out:    EAX  the result, packed
; ::::::::::::::
flt_odd:
        mov     [fipt],bx
        mov     [fcnt],cx
        mov     [ft1],eax
        mov     edx,eax
        call    flt_mul
        call    flt_polyf
        mov     edx,[ft1]
        jmp     flt_mul

; ::::::::::::::
; fn_log - FAC = LOG(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; error:  ?ILLEGAL QUANTITY  FAC 0 or below
; ::::::::::::::
fn_log:
        test    eax,eax
        js      @st_ftr1
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        jz      @st_ftr1
        sub     cx,FBIAS
        mov     [flog],cx
        and     eax,807FFFFFh
        or      eax,FBIAS SHL 23
        mov     edx,[kone]
        call    flt_sub                 ; 1 - m
        mov     [ft2],eax
        mov     edx,[kone]
        add     edx,00800000h
        call    flt_sub                 ; 2 - (1 - m)
        mov     edx,[ft2]
        call    flt_div
        mov     bx,offset klog
        mov     cx,NLOG-1
        call    flt_odd
        mov     [ft1],eax
        mov     ax,[flog]
        call    flt_from16
        mov     edx,[kln2]
        call    flt_mul
        mov     edx,[ft1]
        call    flt_sub
        jmp     flt_neg

; ::::::::::::::
; fn_exp - FAC = EXP(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; error:  ?OVERFLOW  past the largest FLOAT
; ::::::::::::::
fn_exp:
        mov     edx,[kl2e]
        call    flt_mul
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        cmp     cl,FBIAS+7
        jae     @fe_big
        mov     edx,eax
        push    edx
        call    flt_int
        call    flt_fix16
        pop     edx
        movsx   cx,al                   ; n
        push    cx
        push    edx
        call    flt_from16
        pop     edx
        call    flt_sub
        mov     bx,offset kexp
        mov     cx,NEXP-1
        call    flt_poly
        pop     cx
        mov     ebx,eax
        shl     ebx,1
        shr     ebx,24
        add     bx,cx
        test    cx,cx
        js      @fe_neg
        cmp     bx,EMAX
        ja      @st_ftr2
@fe_set:
        and     eax,807FFFFFh
        shl     ebx,23
        or      eax,ebx
        ret
@st_ftr2:
        jmp     err_ov
@fe_neg:
        test    bx,bx
        jg      @fe_set
        xor     eax,eax
        ret
@fe_big:
        test    eax,eax
        jns     @st_ftr2
        jmp     flt_zero

; ::::::::::::::
; flt_pi - FAC = pi.
;
; out:    EAX  the result, packed
; ::::::::::::::
flt_pi:
        mov     eax,[kpi]
        ret

; ::::::::::::::
; fn_pow - FAC = ARG ^ FAC; whole powers under 256 by squaring.
;
; in:     EAX  the power, packed
;         EDX  the base, packed
; out:    EAX  the result, packed
; error:  ?ILLEGAL QUANTITY  a negative base, a fraction power
;         ?OVERFLOW          past the largest FLOAT
; ::::::::::::::
fn_pow:
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        jnz     @F
        mov     eax,[kone]
        ret
@@:
        test    edx,7F800000h
        jz      flt_zero
        sub     ecx,FBIAS
        cmp     ecx,8
        jae     @fp_log
        mov     [fipt],cx               ; k
        mov     ebx,eax
        and     ebx,7FFFFFh
        mov     cl,23
        sub     cl,byte ptr [fipt]
        mov     ebp,1
        shl     ebp,cl
        dec     ebp
        test    ebx,ebp                 ; a fraction left?
        jnz     @fp_log
        test    eax,eax                   ; a negative power
        js      @fp_log
        or      ebx,800000h
        shr     ebx,16
        mov     [fcnt],bx               ; the integer bits at bits 7..
        mov     eax,edx
        mov     [ft1],eax
@@:
        shl     byte ptr [fcnt],1
        dec     byte ptr [fipt]
        js      @fp_r
        mov     edx,eax
        call    flt_mul
        test    byte ptr [fcnt],80h
        jz      @B
        mov     edx,[ft1]
        call    flt_mul
        jmp     @B
@fp_r:
        ret
@fp_log:
        mov     [ft4],eax
        push    edx
        mov     eax,edx
        btr     eax,31
        call    fn_log
        mov     edx,[ft4]
        call    flt_mul
        call    fn_exp
        pop     edx
@fp_sg:
        test    edx,edx
        jns     @fp_r
        mov     [ft2],eax
        mov     eax,[ft4]
        call    flt_whl
        jnz     err_iq
        sub     eax,00800000h           ; half
        call    flt_whl
        pushf
        mov     eax,[ft2]
        popf
        jz      @fp_r
        jmp     flt_neg

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
; fn_cos - FAC = COS(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
fn_cos:
        mov     edx,[kp2]
        call    flt_add
; ::::::::::::::
; fn_sin - FAC = SIN(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
fn_sin:
        mov     edx,[kr2p]
        call    flt_mul
        mov     edx,eax
        push    edx
        call    flt_int
        pop     edx
        call    flt_sub
        mov     edx,[kone]
        sub     edx,00800000h
        call    flt_sub
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        cmp     cl,FBIAS-2
        jb      @F
        mov     edx,[kone]
        sub     edx,00800000h
        and     eax,eax                 ; FAC's sign to ARG's
        mov     ecx,eax
        and     ecx,80000000h
        or      edx,ecx
        call    flt_sub
@@:
        mov     bx,offset ksin
        mov     cx,NSIN-1
        jmp     flt_odd

; ::::::::::::::
; fn_tan - FAC = TAN(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; error:  ?DIVISION BY ZERO  COS 0
; ::::::::::::::
fn_tan:
        mov     [ft2],eax
        call    fn_sin
        mov     [ft4],eax
        mov     eax,[ft2]
        call    fn_cos
        mov     edx,[ft4]
        jmp     flt_div

; ::::::::::::::
; fn_atn - FAC = ATN(FAC).
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; ::::::::::::::
fn_atn:
        mov     ecx,eax
        and     ecx,80000000h
        push    ecx
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        cmp     cl,FBIAS
        pushf
        jb      @F
        mov     edx,[kone]
        call    flt_div
@@:
        mov     bx,offset katn
        mov     cx,NATN-1
        call    flt_odd
        popf
        pop     ecx
        jb      @fa_r
        mov     edx,[kp2]
        and     edx,7FFFFFFFh
        or      edx,ecx
        jmp     flt_sub
@fa_r:
        ret

; ::::::::::::::
; fn_rnd - FAC = RND: 24 LFSR steps at once, mask A3000000h.
;
; in:     EAX  negative: seeds rs
; out:    EAX  the result, packed
;         rs   the new state
; ::::::::::::::
fn_rnd:
        test    eax,eax
        jns     @fr_n
        mov     edx,eax
        and     edx,7FFFFFh
        or      edx,800000h             ; the significand: rs is e ho moh mo, low first
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        mov     ebx,edx
        shr     ebx,16
        shl     ebx,8
        or      ecx,ebx
        movzx   ebx,dh
        shl     ebx,16
        or      ecx,ebx
        movzx   ebx,dl
        shl     ebx,24
        or      ecx,ebx
        mov     [rs],ecx
@fr_n:
        mov     eax,[rs]
        mov     edx,eax
        shr     edx,24
        and     eax,0FFFFFFh            ; L
        mov     ecx,eax
        shl     ecx,8
        xor     edx,ecx
        mov     ecx,eax
        shl     ecx,6
        xor     edx,ecx
        mov     ecx,eax
        shl     ecx,2
        xor     edx,ecx
        add     eax,eax
        xor     edx,eax
        mov     [rs],edx
        shr     edx,8
        and     edx,7FFFFFh
        or      edx,FBIAS SHL 23
        mov     eax,[kone]
        jmp     flt_sub

; ::::::::::::::
; fn_sqr - FAC = SQR(FAC): Newton, four steps.
;
; in:     EAX  the FAC, packed
; out:    EAX  the result, packed
; error:  ?ILLEGAL QUANTITY  FAC under 0
; ::::::::::::::
fn_sqr:
        test    eax,7F800000h
        jz      @osq_r
        test    eax,eax
        js      err_iq
        mov     [sqa],eax
        mov     ecx,eax
        shl     ecx,1
        shr     ecx,24
        add     ecx,FBIAS
        shr     ecx,1
        mov     edx,ecx
        adc     edx,0
        shl     edx,23
        and     eax,7FFFFFh
        shl     ecx,23
        or      eax,ecx
        call    flt_add
        sub     eax,00800000h
        mov     [sqn],3
@osq_l:
        mov     [sqg],eax
        mov     edx,[sqa]
        call    flt_div
        mov     edx,[sqg]
        call    flt_add
        sub     eax,00800000h
        dec     [sqn]
        jnz     @osq_l
@osq_r:
        ret
