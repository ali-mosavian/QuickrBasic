; ==========================================================================
; builtins.asm - the CALL routines of functions: SGN .. MID$
; ==========================================================================
; holds:    sc_fn1 .. sc_mid2, the CALL entries; fn_*, fntab's bodies
; uses:     flt_*, str_*, io_*
; calls:
;   sc_fn1             fntab's fn_*, EAX in and out
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; sc_fn1 - a function of one value ( x -- y ): fntab's routine, EAX in and
; out.
;
; in:     BX  k * 2
; ::::::::::::::
sc_fn1:
        dpop    eax
        call    [g_fntab+ebx*2-4*K_FN]
        dpush   eax
        dispatch

; ::::::::::::::
; sc_left - LEFT$ ( s n -- s ).
; ::::::::::::::
sc_left:
        dpop    eax
        mov     [g_state.sg.s0v],ax
        dpop    eax
        call    fn_left
        dpush   eax
        dispatch
; ::::::::::::::
; sc_right - RIGHT$ ( s n -- s ).
; ::::::::::::::
sc_right:
        dpop    eax
        mov     [g_state.sg.s0v],ax
        dpop    eax
        call    fn_right
        dpush   eax
        dispatch

; ::::::::::::::
; sc_mid - MID$ ( s i n -- s ).
;
; error:  ?ILLEGAL QUANTITY  i 0
; ::::::::::::::
sc_mid:
        dpop    eax
        mov     [g_state.sg.s1v],ax
        jmp     @bmd_g
; ::::::::::::::
; sc_mid2 - MID$ ( s i -- s ).
; ::::::::::::::
sc_mid2:
        mov     [g_state.sg.s1v],SMAX
@bmd_g:
        dpop    eax
        mov     [g_state.sg.s0v],ax
        dpop    eax
        call    fn_mid
        dpush   eax
        dispatch

; ::::::::::::::
; fn_sgn - SGN: an INT.
;
; in:     EAX  the value
; out:    EAX  -1, 0 or 1
; ::::::::::::::
fn_sgn:
        call    flt_sgn
        movsx   eax,al
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
        mov     eax,[g_state.mm.sfr]
        sub     eax,[g_state.mm.aend]
        jmp     flt_from32

; ::::::::::::::
; fn_pos - POS.
;
; out:    EAX  the column
; ::::::::::::::
fn_pos:
        movzx   eax,[g_state.io.pcol]
        ret

; ::::::::::::::
; fn_peek - PEEK, in the POKE memory.
;
; in:     AX  the address
; out:    EAX  the byte
; ::::::::::::::
fn_peek:
        movzx   eax,byte ptr [eax]      ; a flat address
        ret

; ::::::::::::::
; op_size - LEN.
;
; in:     AX  the Desc
; out:    EAX  the length
; ::::::::::::::
fn_len:
        call    str_get
        movzx   eax,cx
        ret

; ::::::::::::::
; fn_asc - ASC.
;
; in:     AX  the Desc
; out:    EAX  the first char
; error:  ?ILLEGAL QUANTITY  empty
; ::::::::::::::
fn_asc:
        call    str_get
        jz      err_iq
        mov     ebx,[g_state.sg.spt]
        movzx   eax,byte ptr [ebx]
        ret

; ::::::::::::::
; fn_chr - CHR$.
;
; in:     AL  the byte
; out:    AX  the temporary
; ::::::::::::::
fn_chr:
        mov     [g_sbuf],al
        mov     ax,1
        jmp     str_buftmp

; ::::::::::::::
; fn_fstr - STR$.
;
; in:     EAX  the FLOAT
; out:    AX   the temporary
; ::::::::::::::
fn_fstr:
        mov     [g_state.io.ocap],80h
        mov     [g_state.io.sbx],0
        call    flt_out
        and     [g_state.io.ocap],7Fh
        mov     eax,[g_state.io.sbx]
        dec     ax
        jmp     str_buftmp

; ::::::::::::::
; sc_dval - VAL ( s -- d ), of a copy with a 0 after it: a DOUBLE, as BC's.
; ::::::::::::::
sc_dval:
        mov     eax,[esp]
        call    str_get
        push    esi
        call    str_near
        call    dbl_in
        pop     esi
        mov     eax,dword ptr [g_dval]
        mov     [esp],eax
        mov     eax,dword ptr [g_dval+4]
        mov     [esp+4],eax
        jmp     ex_lp

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
        sub     dx,[g_state.sg.s0v]
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
        mov     cx,[g_state.sg.s0v]
        mov     [g_state.sg.s1v],cx
        mov     [g_state.sg.s0v],dx
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
        cmp     [g_state.sg.s0v],0
        je      err_iq
        dec     [g_state.sg.s0v]
        jmp     str_sub

; ::::::::::::::
; sc_vseg - VARSEG ( a -- 0 ): flat, no segment.
; ::::::::::::::
sc_vseg:
        and     dword ptr [esp],0
        dispatch

; ::::::::::::::
; sc_err, sc_erl - ERR, ERL ( -- n ).
; ::::::::::::::
sc_err:
        movzx   eax,[g_state.rn.errn]
        dpush   eax
        dispatch
sc_erl:
        movzx   eax,[g_state.rn.erl]
        dpush   eax
        dispatch

; ::::::::::::::
; sc_error - ERROR ( n -- ): raise QBasic's error n.
;
; error:  ?ILLEGAL QUANTITY  n not 1..255
; ::::::::::::::
sc_error:
        dpop    eax
        dec     eax
        cmp     eax,254
        ja      err_iq
        inc     eax
        mov     [g_state.rn.uerr],ax
        mov     al,E_USR
        jmp     err_raise

; ::::::::::::::
; sc_sadd - SADD ( Desc -- a ): its chars' address.
; ::::::::::::::
sc_sadd:
        mov     eax,[esp]
        mov     eax,[eax+D_PTR]
        mov     [esp],eax
        dispatch

; ::::::::::::::
; sc_fix - FIX ( f -- f ): toward 0.
; ::::::::::::::
sc_fix:
        fld     dword ptr [esp]
        fnstcw  [g_fixcw]
        mov     ax,[g_fixcw]
        or      ah,0Ch
        mov     [g_fixcw+2],ax
        fldcw   [g_fixcw+2]
        frndint
        fldcw   [g_fixcw]
        fstp    dword ptr [esp]
        dispatch

; ::::::::::::::
; sc_instr - INSTR ( [n] s t count -- i ): where t is in s from n, 1 the
; first; 0 none.
;
; error:  ?ILLEGAL QUANTITY  n under 1
; ::::::::::::::
sc_instr:
        dpop    ecx
        mov     [g_state.rn.cnarg],cl
        dpop    eax
        call    str_get                 ; t
        mov     edx,[g_state.sg.spt]
        movzx   ebp,[g_state.sg.sln]
        dpop    eax
        call    str_get                 ; s
        mov     ebx,1
        cmp     [g_state.rn.cnarg],3
        jne     @F
        dpop    ebx
@@:
        cmp     ebx,1
        jl      err_iq
        movzx   ecx,[g_state.sg.sln]
        push    esi
        mov     esi,[g_state.sg.spt]
        xor     eax,eax
        cmp     ebx,ecx                 ; past s: 0
        jg      @sin_r
        test    ebp,ebp                 ; t empty: n
        jz      @sin_y
        sub     ecx,ebp                 ; the last start, 0 based
        jl      @sin_r
@sin_l:
        lea     edi,[ebx-1]
        cmp     edi,ecx
        jg      @sin_r
        push    ecx
        push    esi
        add     esi,edi
        mov     edi,edx
        mov     ecx,ebp
        repe    cmpsb
        pop     esi
        pop     ecx
        je      @sin_y
        inc     ebx
        jmp     @sin_l
@sin_y:
        mov     eax,ebx
@sin_r:
        pop     esi
        dpush   eax
        dispatch

; ::::::::::::::
; sc_lbnd, sc_ubnd - LBOUND, UBOUND ( id [d] count -- i ): of dimension d,
; 1 the first.
;
; error:  ?BAD SUBSCRIPT  no dimension d, or ERASEd
; ::::::::::::::
sc_lbnd:
        xor     edx,edx
        jmp     @sbd_g
sc_ubnd:
        mov     edx,1
@sbd_g:
        dpop    ecx
        mov     eax,1
        cmp     ecx,2
        jne     @F
        dpop    eax
@@:
        dpop    ebx
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        jnz     @F
        cmp     [g_adyn+ebx],0          ; ERASEd: none
        jne     arr_bs
        mov     eax,10                  ; none yet: OPTION BASE to 10
        test    edx,edx
        jnz     @sbd_n
        movzx   eax,[g_obase]
@sbd_n:
        dpush   eax
        dispatch
@@:
        movzx   ecx,byte ptr [edi]
        dec     eax
        cmp     eax,ecx
        jae     arr_bs
        lea     ebx,[edi+eax*2+AH_CNT]
        movsx   eax,word ptr [ebx+ecx*2]        ; its lower bound
        test    edx,edx
        jz      @F
        movzx   ecx,word ptr [ebx]
        lea     eax,[eax+ecx-1]
@@:
        dpush   eax
        dispatch

; ::::::::::::::
; sc_space - SPACE$ ( n -- s ).
; ::::::::::::::
sc_space:
        dpop    eax
        mov     dl,' '
        jmp     @sst_n

; ::::::::::::::
; sc_strg - STRING$ ( n x flag -- s ): n of x's first char, flag 1, else
; of char x.
;
; error:  ?ILLEGAL QUANTITY  x empty, not a char, n out of range
; ::::::::::::::
sc_strg:
        dpop    ecx
        dpop    eax
        jecxz   @ssg_n
        call    str_get
        jz      err_iq
        mov     eax,[g_state.sg.spt]
        mov     al,[eax]
        jmp     @ssg_c
@ssg_n:
        cmp     eax,255
        ja      err_iq
@ssg_c:
        mov     dl,al
        dpop    eax
@sst_n:                                 ; n of DL
        cmp     eax,SMAX
        ja      err_iq
        push    edx
        call    str_alloc
        pop     edx
        movzx   ecx,[g_state.sg.sn]
        mov     edi,[g_state.sg.dpv]
        mov     al,dl
        rep     stosb
        call    str_new
        dpush   eax
        dispatch

; ::::::::::::::
; sc_ltrim, sc_rtrim - LTRIM$, RTRIM$ ( s -- s ): without the leading,
; the trailing spaces.
; ::::::::::::::
sc_ltrim:
        mov     eax,[esp]
        movzx   ecx,word ptr [eax+D_LEN]
        mov     edi,[eax+D_PTR]
        xor     edx,edx
@slt_l:
        cmp     edx,ecx
        jae     @slt_d
        cmp     byte ptr [edi+edx],' '
        jne     @slt_d
        inc     edx
        jmp     @slt_l
@slt_d:
        mov     [g_state.sg.s0v],dx
        mov     [g_state.sg.s1v],SMAX
        jmp     @ssu_g
sc_rtrim:
        mov     eax,[esp]
        movzx   ecx,word ptr [eax+D_LEN]
        mov     edi,[eax+D_PTR]
@srt_l:
        jecxz   @srt_d
        cmp     byte ptr [edi+ecx-1],' '
        jne     @srt_d
        dec     ecx
        jmp     @srt_l
@srt_d:
        mov     [g_state.sg.s0v],0
        mov     [g_state.sg.s1v],cx
@ssu_g:
        dpop    eax
        call    str_sub
        dpush   eax
        dispatch

; ::::::::::::::
; sc_ucase, sc_lcase - UCASE$, LCASE$ ( s -- s ).
; ::::::::::::::
sc_ucase:
        mov     dx,'az'
        jmp     @suc_g
sc_lcase:
        mov     dx,'AZ'
@suc_g:
        mov     [g_state.sg.s0v],0
        mov     [g_state.sg.s1v],SMAX
        dpop    eax
        push    edx
        call    str_sub
        pop     edx
        movzx   ecx,word ptr [eax+D_LEN]
        mov     edi,[eax+D_PTR]
        jecxz   @suc_r
@suc_l:
        mov     bl,[edi]
        cmp     bl,dh
        jb      @suc_n
        cmp     bl,dl
        ja      @suc_n
        xor     byte ptr [edi],20h
@suc_n:
        inc     edi
        loop    @suc_l
@suc_r:
        dpush   eax
        dispatch

; ::::::::::::::
; sc_hex, sc_oct - HEX$, OCT$ ( i -- s ): its bits in base 16, 8.
; ::::::::::::::
sc_hex:
        mov     cl,4
        jmp     @shx_g
sc_oct:
        mov     cl,3
@shx_g:
        dpop    eax
        mov     edi,offset g_sbuf+15
        xor     edx,edx
@shx_l:
        mov     ebx,eax
        mov     ch,1
        shl     ch,cl
        dec     ch
        and     bl,ch
        add     bl,'0'
        cmp     bl,'9'
        jbe     @F
        add     bl,'A'-'9'-1
@@:
        mov     [edi],bl
        dec     edi
        inc     edx
        shr     eax,cl
        jnz     @shx_l
        inc     edi
        push    esi
        mov     esi,edi
        mov     edi,offset g_sbuf
        mov     ecx,edx
        rep     movsb
        pop     esi
        mov     eax,edx
        call    str_buftmp
        dpush   eax
        dispatch

; ::::::::::::::
; sc_sfix - ( s n -- s ): s as a STRING * n, cut or padded with spaces.
; ::::::::::::::
sc_sfix:
        dpop    eax
        push    eax
        call    str_alloc
        pop     ecx
        dpop    eax
        push    ecx
        call    str_get
        pop     ecx
        movzx   eax,[g_state.sg.sln]
        cmp     eax,ecx
        jbe     @F
        mov     eax,ecx
@@:
        push    esi
        mov     esi,[g_state.sg.spt]
        mov     edi,[g_state.sg.dpv]
        sub     ecx,eax
        xchg    eax,ecx
        rep     movsb
        mov     ecx,eax
        mov     al,' '
        rep     stosb
        pop     esi
        call    str_new
        dpush   eax
        dispatch

; ::::::::::::::
; sc_mids - the MID$ statement's value ( s i n v -- s ): s with v over its
; chars from i, n at most, its length kept.
;
; error:  ?ILLEGAL QUANTITY  i under 1
; ::::::::::::::
sc_mids:
        dpop    edx                     ; v
        dpop    ecx                     ; n
        dpop    ebx                     ; i
        cmp     ebx,1
        jl      err_iq
        dpop    eax
        push    edx
        push    ecx
        push    ebx
        mov     [g_state.sg.s0v],0
        mov     [g_state.sg.s1v],SMAX
        call    str_sub
        pop     ebx
        pop     ecx
        pop     edx
        dec     ebx
        movzx   edi,word ptr [eax+D_LEN]
        sub     edi,ebx
        jle     @smd_r
        cmp     ecx,edi
        jbe     @F
        mov     ecx,edi
@@:
        movzx   edi,word ptr [edx+D_LEN]
        cmp     ecx,edi
        jbe     @F
        mov     ecx,edi
@@:
        push    esi
        mov     esi,[edx+D_PTR]
        mov     edi,[eax+D_PTR]
        add     edi,ebx
        rep     movsb
        pop     esi
@smd_r:
        dpush   eax
        dispatch

        .data?
g_fixcw dw      2 dup(?)                ; FIX: the control word, then chopping
        .code

        end
