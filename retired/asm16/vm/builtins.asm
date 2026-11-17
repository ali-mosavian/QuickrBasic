; ==========================================================================
; builtins.asm - the CALL routines of functions: SGN .. MID$
; ==========================================================================
; holds:    sc_fn1 .. sc_mid2, the CALL entries; fn_*, fntab's bodies
; uses:     flt_*, str_*, io_*
; calls:
;   sc_fn1             fntab's fn_*, EAX in and out
; ==========================================================================

; ::::::::::::::
; sc_fn1 - a function of one value ( x -- y ): fntab's routine, EAX in and
; out.
;
; in:     BX  k * 2
; ::::::::::::::
sc_fn1:
        pop     eax
        call    word ptr [fntab+bx-2*K_FN]
        push    eax
        dispatch

; ::::::::::::::
; sc_left - LEFT$ ( s n -- s ).
; ::::::::::::::
sc_left:
        pop     eax
        mov     [s0v],ax
        pop     eax
        call    fn_left
        push    eax
        dispatch
; ::::::::::::::
; sc_right - RIGHT$ ( s n -- s ).
; ::::::::::::::
sc_right:
        pop     eax
        mov     [s0v],ax
        pop     eax
        call    fn_right
        push    eax
        dispatch

; ::::::::::::::
; sc_mid - MID$ ( s i n -- s ).
;
; error:  ?ILLEGAL QUANTITY  i 0
; ::::::::::::::
sc_mid:
        pop     eax
        mov     [s1v],ax
        jmp     @bmd_g
; ::::::::::::::
; sc_mid2 - MID$ ( s i -- s ).
; ::::::::::::::
sc_mid2:
        mov     [s1v],SMAX
@bmd_g:
        pop     eax
        mov     [s0v],ax
        pop     eax
        call    fn_mid
        push    eax
        dispatch

; ::::::::::::::
; fn_sgn - SGN: an INT.
;
; in:     EAX  the value
; out:    AX  -1, 0 or 1
; ::::::::::::::
fn_sgn:
        call    flt_sgn
        cbw
        ret

; ::::::::::::::
; fn_int - INT.
; ::::::::::::::
fn_int:
        jmp     flt_int

; ::::::::::::::
; fn_abs - ABS.
; ::::::::::::::
fn_abs:
        jmp     flt_abs

; ::::::::::::::
; fn_usr - USR: ?ILLEGAL QUANTITY, no routine is set.
; ::::::::::::::
fn_usr:
        jmp     err_iq

; ::::::::::::::
; fn_fre - FRE: the heap's free bytes.
;
; out:    EAX  the FLOAT
; ::::::::::::::
fn_fre:
        call    str_gc
        mov     eax,[sfr]
        sub     eax,[aend]
        jmp     flt_from32

; ::::::::::::::
; fn_pos - POS.
;
; out:    AX  the column
; ::::::::::::::
fn_pos:
        movzx   ax,[pcol]
        ret

; ::::::::::::::
; fn_peek - PEEK, in the POKE segment.
;
; in:     AX  the address
; out:    AX  the byte
; ::::::::::::::
fn_peek:
        mov     fs,[peekseg]
        mov     bx,ax
        movzx   ax,byte ptr fs:[bx]
        ret

; ::::::::::::::
; op_size - LEN.
;
; in:     AX  the Desc
; out:    AX  the length
; ::::::::::::::
fn_len:
        call    str_get
        mov     ax,cx
        ret

; ::::::::::::::
; fn_asc - ASC.
;
; in:     AX  the Desc
; out:    AX  the first char
; error:  ?ILLEGAL QUANTITY  empty
; ::::::::::::::
fn_asc:
        call    str_get
        jz      err_iq
        mov     eax,[spt]
        call    mem_fs
        movzx   ax,byte ptr fs:[bx]
        ret

; ::::::::::::::
; fn_chr - CHR$.
;
; in:     AL  the byte
; out:    AX  the temporary
; ::::::::::::::
fn_chr:
        mov     [sbuf],al
        mov     ax,1
        jmp     str_buftmp

; ::::::::::::::
; fn_fstr - STR$.
;
; in:     EAX  the FLOAT
; out:    AX   the temporary
; ::::::::::::::
fn_fstr:
        mov     [ocap],80h
        mov     [sbx],0
        call    flt_out
        and     [ocap],7Fh
        movzx   ax,[sbx]
        dec     ax
        jmp     str_buftmp

; ::::::::::::::
; fn_val - VAL, of a copy with a 0 after it.
;
; in:     EAX  the Desc
; out:    EAX  the FLOAT
; ::::::::::::::
fn_val:
        call    str_get
        push    si
        call    str_near
        call    flt_in
        pop     si
        ret

; ::::::::::::::
; fn_left - LEFT$.
;
; in:     AX   the Desc
;         s0v  the count
; out:    AX  the temporary
; ::::::::::::::
fn_left:
        xor     dx,dx
        jmp     str_from

; ::::::::::::::
; fn_right - RIGHT$.
;
; in:     AX   the Desc
;         s0v  the count
; out:    AX  the temporary
; ::::::::::::::
fn_right:
        call    str_dlen
        mov     dx,cx
        sub     dx,[s0v]
        jnc     str_from
        jmp     fn_left

; ::::::::::::::
; str_from - str_sub from DX, s0v chars at most.
;
; in:     DX   the start
;         s0v  the count
; out:    AX  the temporary
; ::::::::::::::
str_from:
        mov     cx,[s0v]
        mov     [s1v],cx
        mov     [s0v],dx
        jmp     str_sub

; ::::::::::::::
; fn_mid - MID$.
;
; in:     AX        the Desc
;         s0v, s1v  the start, the count
; out:    AX  the temporary
; error:  ?ILLEGAL QUANTITY  start 0
; ::::::::::::::
fn_mid:
        cmp     [s0v],0
        je      err_iq
        dec     [s0v]
        jmp     str_sub
