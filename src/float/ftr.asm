; ==========================================================================
; ftr.asm - RND, pi
; ==========================================================================
; holds:    fn_rnd, flt_pi
; uses:     rs
; ==========================================================================

include x86f.inc
        .code

fcon    macro   sg,m,e
        dd      ((e+FBIAS) SHL 23) OR (sg SHL 31) OR (m AND 7FFFFFh)
        endm

        .data
g_kone  label   dword
        fcon    0,0800000h,0
g_kpi   label   dword
        fcon    0,0C90FDBh,1
        .code

; ::::::::::::::
; flt_pi - FAC = pi.
;
; out:    EAX  the result, packed
; ::::::::::::::
flt_pi:
        mov     eax,[g_kpi]
        ret

; ::::::::::::::
; fn_rnd - FAC = RND(x), QBasic's: a 24-bit linear congruence (rt/random's
; B$RND1). 0 repeats the last; a negative x seeds from its bits, the top
; byte added into the low three; then the next.
;
; in:     EAX  x, packed
; out:    EAX  the result, packed
;         rs   the seed
; ::::::::::::::
fn_rnd:
        lea     edx,[eax+eax]
        test    edx,edx
        jz      @fr_c
        test    eax,eax
        jns     @fr_n
        mov     edx,eax
        shr     edx,24
        add     eax,edx
        and     eax,0FFFFFFh
        mov     [g_state.ft.rs],eax
@fr_n:
        mov     eax,[g_state.ft.rs]
        imul    eax,eax,16598013
        add     eax,12820163
        and     eax,0FFFFFFh
        mov     [g_state.ft.rs],eax
@fr_c:                                  ; seed / 2^24, exactly: 24 bits
        mov     eax,[g_state.ft.rs]
        test    eax,eax
        jz      @fr_r
        bsr     ecx,eax                 ; the top bit, 0..23
        mov     edx,23
        sub     edx,ecx
        xchg    ecx,edx
        shl     eax,cl
        and     eax,7FFFFFh
        add     edx,FBIAS-24
        shl     edx,23
        or      eax,edx
@fr_r:
        ret

        end
