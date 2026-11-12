; ==========================================================================
; ftr.asm - RND, pi
; ==========================================================================
; holds:    fn_rnd, flt_pi
; uses:     flt_sub, rs
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
        mov     [g_state.ft.rs],ecx
@fr_n:
        mov     eax,[g_state.ft.rs]
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
        mov     [g_state.ft.rs],edx
        shr     edx,8
        and     edx,7FFFFFh
        or      edx,FBIAS SHL 23
        mov     eax,[g_kone]
        jmp     flt_sub

        end
