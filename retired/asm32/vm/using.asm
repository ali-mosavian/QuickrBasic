; ==========================================================================
; using.asm - PRINT USING (as llrm's runtime/qb/using.c)
; ==========================================================================
; holds:    sc_usb, sc_usi, sc_usf, sc_usd, sc_uss, sc_use, sc_usen
; uses:     io_putc, str_get, dbl_digits
; data:     The format is literal text and fields: ! the first character,
;           & all of it, \  \ as wide as that, or a number: # a digit, a
;           point, commas, a leading + or $$ or ** or **$, a trailing + or
;           -, ^^^^ an exponent; _ makes the next character literal. Each
;           item goes to the next field; the format starts again past its
;           last. A number is digits: v = 0.text * 10^ni.
; ==========================================================================

include x86f.inc

FMTMAX  = 255
NDIG    = 80                    ; a number's digits at most

TUSF    struc                   ; a numeric field
    uflen   dd      ?               ; its characters in the format
    ufwid   dd      ?               ; its columns, the signs not counted
    uflp    dd      ?               ; a leading +
    uftr    dd      ?               ; a trailing + or -, else 0
    ufst    dd      ?               ; **
    ufdol   dd      ?               ; a $
    ufcom   dd      ?               ; commas
    ufbp    dd      ?               ; places before the point
    ufdec   dd      ?               ; after it
    ufpt    dd      ?               ; a point
    ufcar   dd      ?               ; 4 or 5 for an exponent
TUSF    ends

        .code

; ::::::::::::::
; sc_usb - US.BEGIN ( fmt -- ).
;
; error:  ?ILLEGAL QUANTITY  over 255 characters
; ::::::::::::::
sc_usb:
        dpop    eax
        push    esi
        call    str_get
        movzx   ecx,cx
        cmp     ecx,FMTMAX
        ja      err_iq
        mov     [g_ufl],ecx
        mov     esi,[g_state.sg.spt]
        mov     edi,offset g_ufmt
        rep     movsb
        mov     [g_uat],0
        pop     esi
        jmp     ex_lp

; ::::::::::::::
; sc_use - US.END: the text up to the next field, a new line; sc_usen -
; the same, no new line (; or , last).
; ::::::::::::::
sc_use:
        push    esi
        call    us_lit
        pop     esi
        call    io_nl
        jmp     ex_lp
sc_usen:
        push    esi
        call    us_lit
        pop     esi
        jmp     ex_lp

; ::::::::::::::
; sc_uss - US.STR ( s -- ): to a string field.
;
; error:  ?TYPE MISMATCH  a numeric field
; ::::::::::::::
sc_uss:
        push    esi
        call    us_next
        cmp     al,4
        je      err_tm
        add     [g_uat],ecx
        push    eax
        push    ecx
        mov     eax,[esp+12]            ; the string, under ESI
        call    str_get
        movzx   edx,cx                  ; EDX its length
        mov     esi,[g_state.sg.spt]
        pop     ecx
        pop     eax
        cmp     al,1
        jne     @F
        mov     al,' '                  ; ! the first, a space for none
        test    edx,edx
        jz      @uss_c
        mov     al,[esi]
@uss_c:
        call    io_putc
        jmp     @uss_r
@@:
        cmp     al,2                    ; & all of it
        jne     @F
        mov     ecx,edx
        call    us_out
        jmp     @uss_r
@@:
        push    ecx                     ; \  \ its width, filled with spaces
        cmp     edx,ecx
        jae     @F
        mov     ecx,edx
@@:
        sub     [esp],ecx
        call    us_out
        pop     ecx
        jecxz   @uss_r
@@:
        mov     al,' '
        call    io_putc
        loop    @B
@uss_r:
        pop     esi
        add     esp,CELL
        jmp     ex_lp

; us_out - ECX chars at ESI to the output. Keeps all but EAX, ECX, ESI.
us_out:
        jecxz   @uso_r
@@:
        lodsb
        call    io_putc
        loop    @B
@uso_r:
        ret

; ::::::::::::::
; sc_usi - US.INT ( i -- ); sc_usf - US.FLT ( f -- ); sc_usd - US.DBL
; ( d -- ): to a numeric field.
;
; error:  ?TYPE MISMATCH  a string field
; ::::::::::::::
sc_usi:
        push    esi
        mov     eax,[esp+4]
        call    us_ldi
        jmp     @usn_n
sc_usf:
        push    esi
        fld     dword ptr [esp+4]
        mov     ecx,7
        jmp     @usn_l
sc_usd:
        push    esi
        fld     qword ptr [esp+4]
        mov     ecx,16
@usn_l:
        call    us_ldr
@usn_n:
        call    us_next
        cmp     al,4
        jne     err_tm
        add     [g_uat],ecx
        cmp     [g_usf.ufcar],0
        je      @F
        mov     eax,[g_usf.ufcar]
        call    us_fexp
        call    us_trail
        call    us_body
        jmp     @usn_w
@@:
        call    us_plain
@usn_w:
        pop     esi
        add     esp,CELL
        jmp     ex_lp

; ::::::::::::::
; us_ldi - the number EAX, a whole one.
; ::::::::::::::
us_ldi:
        xor     edx,edx
        mov     [g_nsci],dl
        test    eax,eax
        setl    [g_nneg]
        jns     @F
        neg     eax
@@:
        mov     ebx,10                  ; the digits backwards, then turned
        xor     ecx,ecx
@@:
        xor     edx,edx
        div     ebx
        add     dl,'0'
        push    edx
        inc     ecx
        test    eax,eax
        jnz     @B
        mov     [g_nc],ecx
        mov     [g_ni],ecx
        xor     ebx,ebx
@@:
        pop     edx
        mov     [g_nt+ebx],dl
        inc     ebx
        loop    @B
        ret

; ::::::::::::::
; us_ldr - the number st0 (popped), a SINGLE when ECX is 7, a DOUBLE's 16:
; its digits to 16 places; nsci, PRINT would give it an exponent.
; ::::::::::::::
us_ldr:
        mov     [g_utd],ecx
        ftst
        fnstsw  ax
        sahf
        setb    [g_nneg]
        jne     @F
        fstp    st(0)
        mov     [g_nt],'0'
        mov     [g_nc],1
        mov     [g_ni],1
        mov     [g_nsci],0
        ret
@@:
        fabs
        mov     [g_ndig],16
        call    dbl_digits
        push    esi
        push    edi
        mov     esi,offset g_dig
        mov     edi,offset g_nt
        mov     [g_nc],ecx
        mov     [g_utc],ecx
        rep     movsb
        pop     edi
        pop     esi
        mov     eax,[g_dp]
        inc     eax
        mov     [g_ni],eax
        mov     ebx,[g_utd]             ; rounded to the type's digits
        cmp     [g_nc],ebx
        jbe     @usl_s
        mov     [g_nc],ebx
        cmp     [g_nt+ebx],'5'
        jb      @usl_z
@@:
        dec     ebx                     ; carried up past the 9s
        js      @usl_c
        cmp     [g_nt+ebx],'9'
        jne     @F
        mov     [g_nc],ebx
        jmp     @B
@@:
        inc     [g_nt+ebx]
        jmp     @usl_z
@usl_c:                                 ; all 9s: 1, a place more
        mov     [g_nt],'1'
        mov     [g_nc],1
        inc     [g_ni]
@usl_z:
        mov     ecx,[g_nc]
@@:
        cmp     ecx,1
        jbe     @F
        cmp     [g_nt+ecx-1],'0'
        jne     @F
        dec     ecx
        jmp     @B
@@:
        mov     [g_nc],ecx
@usl_s:                                 ; past the type's places: ni or
        mov     ebx,[g_utd]             ; |ni - nc| over its digits
        mov     eax,[g_ni]
        cmp     eax,ebx
        jg      @usl_y
        sub     eax,[g_nc]
        cdq
        xor     eax,edx
        sub     eax,edx
        cmp     eax,ebx
        setg    [g_nsci]
        jmp     @F
@usl_y:
        mov     [g_nsci],1
@@:
        push    esi                     ; the digits: all 16 of them
        push    edi
        mov     esi,offset g_dig
        mov     edi,offset g_nt
        mov     ecx,[g_utc]
        mov     [g_nc],ecx
        rep     movsb
        pop     edi
        pop     esi
        mov     eax,[g_dp]
        inc     eax
        mov     [g_ni],eax
        ret

; ::::::::::::::
; us_next - to the next field for an item, the text before it written.
;
; out:    AL   its kind: 1 !, 2 &, 3 \ \, 4 a number
;         ECX  its length
; error:  ?ILLEGAL QUANTITY  a format with no field
; ::::::::::::::
us_next:
        push    [g_uat]
        call    us_lit
        pop     edx
        test    al,al
        jnz     @F
        test    edx,edx
        jz      err_iq
        mov     [g_uat],0
        call    us_lit
        test    al,al
        jz      err_iq
@@:
        mov     ebx,[g_uat]
        jmp     us_fld

; ::::::::::::::
; us_lit - the literal text from uat to the next field written.
;
; out:    AL  0: the format's end, else a field at uat
; ::::::::::::::
us_lit:
        mov     ebx,[g_uat]
        cmp     ebx,[g_ufl]
        jae     @usi_e
        mov     al,[g_ufmt+ebx]
        cmp     al,'_'
        jne     @F
        lea     eax,[ebx+1]
        cmp     eax,[g_ufl]
        jae     @F
        mov     al,[g_ufmt+ebx+1]
        call    io_putc
        add     [g_uat],2
        jmp     us_lit
@@:
        call    us_fld
        test    al,al
        jnz     @usi_r
        mov     ebx,[g_uat]
        mov     al,[g_ufmt+ebx]
        call    io_putc
        inc     [g_uat]
        jmp     us_lit
@usi_e:
        xor     al,al
@usi_r:
        ret

; ::::::::::::::
; us_fld - the field at EBX: AL its kind (0 none), ECX its length; a
; number's in usf.
; ::::::::::::::
us_fld:
        mov     al,[g_ufmt+ebx]
        mov     ecx,1
        cmp     al,'!'
        je      @usf_1
        cmp     al,'&'
        je      @usf_2
        cmp     al,'\'
        je      @usf_3
        mov     edi,offset u_numc
        mov     ecx,5
        repne   scasb
        jne     @usf_0
        call    us_numf
        jc      @usf_0
        mov     ecx,[g_usf.uflen]
        mov     al,4
        ret
@usf_1:
        mov     al,1
        ret
@usf_2:
        mov     al,2
        ret
@usf_3:
        lea     edx,[ebx+1]
@@:
        cmp     edx,[g_ufl]
        jae     @usf_0
        cmp     byte ptr [g_ufmt+edx],'\'
        je      @F
        cmp     byte ptr [g_ufmt+edx],' '
        jne     @usf_0
        inc     edx
        jmp     @B
@@:
        lea     ecx,[edx+1]
        sub     ecx,ebx
        mov     al,3
        ret
@usf_0:
        xor     al,al
        ret

; ::::::::::::::
; us_numf - the numeric field at EBX, in usf.
;
; out:    CF  set: none
; keeps:  EBX
; ::::::::::::::
us_numf:
        mov     edi,offset g_usf
        mov     ecx,size TUSF / 4
        xor     eax,eax
        rep     stosd
        mov     edx,ebx                 ; EDX the place, EDI digits seen
        xor     edi,edi
        call    @unf_c
        cmp     al,'+'
        jne     @F
        lea     eax,[edx+1]
        cmp     eax,[g_ufl]
        jae     @F
        mov     [g_usf.uflp],1
        inc     edx
@@:
        lea     eax,[edx+1]
        cmp     eax,[g_ufl]
        jae     @unf_d
        mov     ax,word ptr [g_ufmt+edx]
        cmp     ax,'**'
        jne     @unf_s
        mov     [g_usf.ufst],1
        add     [g_usf.ufwid],2
        add     [g_usf.ufbp],2
        add     edx,2
        call    @unf_c
        cmp     al,'$'
        jne     @unf_d
        mov     [g_usf.ufdol],1
        inc     [g_usf.ufwid]
        inc     edx
        jmp     @unf_d
@unf_s:
        cmp     ax,'$$'
        jne     @unf_d
        mov     [g_usf.ufdol],1
        add     [g_usf.ufwid],2
        inc     [g_usf.ufbp]
        add     edx,2
@unf_d:                                 ; # and , before the point
        call    @unf_c
        cmp     al,'#'
        jne     @F
        inc     [g_usf.ufbp]
        jmp     @unf_n
@@:
        cmp     al,','
        jne     @unf_p
        mov     [g_usf.ufcom],1
@unf_n:
        inc     [g_usf.ufwid]
        mov     edi,1
        inc     edx
        jmp     @unf_d
@unf_p:
        cmp     al,'.'
        jne     @unf_a
        lea     eax,[edx+1]
        cmp     eax,[g_ufl]
        jae     @unf_q
        cmp     byte ptr [g_ufmt+edx+1],'#'
        jne     @unf_q
        mov     [g_usf.ufpt],1
        inc     [g_usf.ufwid]
        inc     edx
@@:
        call    @unf_c
        cmp     al,'#'
        jne     @unf_a
        inc     [g_usf.ufdec]
        inc     [g_usf.ufwid]
        mov     edi,1
        inc     edx
        jmp     @B
@unf_q:
        test    edi,edi                 ; ##. : a point, no places after it
        jz      @unf_a
        mov     [g_usf.ufpt],1
        inc     [g_usf.ufwid]
        inc     edx
@unf_a:
        mov     eax,edi
        or      eax,[g_usf.ufst]
        or      eax,[g_usf.ufdol]
        jz      @unf_x
        lea     eax,[edx+3]             ; ^^^^, ^^^^^
        cmp     eax,[g_ufl]
        jae     @unf_t
        cmp     dword ptr [g_ufmt+edx],'^^^^'
        jne     @unf_t
        mov     [g_usf.ufcar],4
        add     edx,4
        call    @unf_c
        cmp     al,'^'
        jne     @unf_t
        mov     [g_usf.ufcar],5
        inc     edx
@unf_t:
        cmp     [g_usf.uflp],0
        jne     @F
        call    @unf_c
        cmp     al,'+'
        je      @unf_g
        cmp     al,'-'
        jne     @F
@unf_g:
        movzx   eax,al
        mov     [g_usf.uftr],eax
        inc     edx
@@:
        sub     edx,ebx
        mov     [g_usf.uflen],edx
        clc
        ret
@unf_x:
        stc
        ret
@unf_c:                                 ; AL the format's char at EDX, 0 past it
        xor     al,al
        cmp     edx,[g_ufl]
        jae     @F
        mov     al,[g_ufmt+edx]
@@:
        ret

; ::::::::::::::
; us_norm - zeros before the digits so that every place before the point
; has one: ni not under 0.
; ::::::::::::::
us_norm:
        mov     ecx,[g_ni]
        neg     ecx
        jle     @unm_r
        cmp     ecx,NDIG
        jbe     @F
        mov     ecx,NDIG
@@:
        push    esi
        push    edi
        std                             ; the digits ECX places on
        mov     esi,[g_nc]
        lea     edi,[esi+ecx-1+offset g_nt]
        add     esi,offset g_nt-1
        push    ecx
        mov     ecx,[g_nc]
        rep     movsb
        cld
        pop     ecx
        add     [g_nc],ecx
        mov     edi,offset g_nt
        mov     al,'0'
        rep     stosb
        pop     edi
        pop     esi
        mov     [g_ni],0
        cmp     [g_nc],NDIG
        jbe     @unm_r
        mov     [g_nc],NDIG
@unm_r:
        ret

; ::::::::::::::
; us_round - the digits rounded, half up, to EAX places after the point,
; zeros added to them.
; ::::::::::::::
us_round:
        push    eax
        call    us_norm
        pop     eax
        add     eax,[g_ni]              ; EAX the digits kept
        cmp     eax,NDIG
        jbe     @F
        mov     eax,NDIG
@@:
        mov     ecx,[g_nc]
        cmp     eax,ecx
        jb      @ugr_c
@@:
        cmp     ecx,eax                 ; fewer: zeros after them
        jae     @F
        mov     [g_nt+ecx],'0'
        inc     ecx
        jmp     @B
@@:
        mov     [g_nc],ecx
        ret
@ugr_c:
        mov     [g_nc],eax
        cmp     [g_nt+eax],'5'
        jb      @ugr_r
@ugr_9:
        dec     eax
        js      @ugr_o
        cmp     [g_nt+eax],'9'
        je      @F
        inc     [g_nt+eax]
        ret
@@:
        mov     [g_nt+eax],'0'
        jmp     @ugr_9
@ugr_o:                                 ; all 9s: a 1 before them
        mov     ecx,[g_nc]
        jecxz   @ugr_1
@@:
        mov     al,[g_nt+ecx-1]
        mov     [g_nt+ecx],al
        loop    @B
@ugr_1:
        mov     [g_nt],'1'
        inc     [g_nc]
        inc     [g_ni]
@ugr_r:
        ret

; ::::::::::::::
; us_plain - a number without an exponent, written.
; ::::::::::::::
us_plain:
        mov     eax,[g_usf.ufdec]
        call    us_round
        mov     edi,offset g_ubody      ; the digits before the point, commas
        xor     ebx,ebx
@@:
        cmp     ebx,[g_ni]
        jae     @F
        cmp     [g_nt+ebx],'0'
        jne     @F
        inc     ebx
        jmp     @B
@@:
        mov     ecx,[g_ni]
        sub     ecx,ebx                 ; ECX the digits
        jnz     @upl_i
        cmp     [g_usf.ufbp],0
        je      @upl_f
        mov     al,'0'
        stosb
        jmp     @upl_f
@upl_i:
        xor     edx,edx                 ; EDX the digit's index
@upl_l:
        cmp     [g_usf.ufcom],0
        je      @F
        test    edx,edx
        jz      @F
        push    edx
        mov     eax,ecx
        sub     eax,edx
        xor     edx,edx
        push    ecx
        mov     ecx,3
        div     ecx
        pop     ecx
        test    edx,edx
        pop     edx
        jnz     @F
        mov     al,','
        stosb
@@:
        mov     al,[g_nt+ebx+edx]
        stosb
        inc     edx
        cmp     edx,ecx
        jb      @upl_l
@upl_f:
        cmp     [g_usf.ufpt],0          ; the point, the places after it
        je      @upl_w
        mov     al,'.'
        stosb
        mov     ecx,[g_usf.ufdec]
        mov     ebx,[g_ni]
        jecxz   @upl_w
@@:
        mov     al,[g_nt+ebx]
        stosb
        inc     ebx
        loop    @B
@upl_w:
        sub     edi,offset g_ubody
        mov     ecx,edi                 ; ECX the body's length
        cmp     [g_nsci],0
        je      @F
        call    us_fits
        jbe     @F
        mov     al,'%'                  ; past the field: an exponent
        call    io_putc
        mov     eax,4
        call    us_fexp
        jmp     us_body
@@:
        mov     edx,[g_usf.ufwid]
        add     edx,[g_usf.uflp]

; ::::::::::::::
; us_wnum - the body's ECX chars in EDX columns: right-aligned, filled with
; spaces or *s, the signs and $ around them; a % first when past them.
; ::::::::::::::
us_wnum:
        mov     edi,offset g_uline
        push    edx
        call    us_lead
        mov     bl,al                   ; BL the sign before, BH a minus
        call    us_tsign
        mov     ah,al
        xor     bh,bh
        cmp     [g_nneg],0
        je      @F
        test    bl,bl
        jnz     @F
        test    ah,ah
        jnz     @F
        mov     bh,'-'
@@:
        mov     edx,ecx                 ; EDX the columns needed
        test    bx,bx
        jz      @F
        inc     edx
@@:
        add     edx,[g_usf.ufdol]
        pop     esi                     ; ESI the columns
        push    eax
        cmp     edx,esi
        jbe     @F
        mov     al,'%'
        stosb
        xor     esi,esi
        jmp     @uwn_s
@@:
        sub     esi,edx
        mov     al,' '
        cmp     [g_usf.ufst],0
        je      @F
        mov     al,'*'
@@:
        push    ecx
        mov     ecx,esi
        rep     stosb
        pop     ecx
@uwn_s:
        pop     eax
        test    bl,bl
        jz      @F
        mov     al,bl
        stosb
@@:
        test    bh,bh
        jz      @F
        mov     al,bh
        stosb
@@:
        cmp     [g_usf.ufdol],0
        je      @F
        mov     al,'$'
        stosb
@@:
        mov     esi,offset g_ubody
        rep     movsb
        test    ah,ah
        jz      @F
        mov     al,ah
        stosb
@@:
        mov     ecx,edi
        mov     esi,offset g_uline
        sub     ecx,esi
        jmp     us_out

; us_fits - the body's ECX chars, its signs and $, against the field's
; columns: above: past them. Keeps ECX.
us_fits:
        mov     eax,ecx
        cmp     [g_usf.uflp],0
        jne     @F
        cmp     [g_nneg],0
        je      @ufi_d
        cmp     [g_usf.uftr],0
        jne     @ufi_d
@@:
        inc     eax
@ufi_d:
        add     eax,[g_usf.ufdol]
        mov     edx,[g_usf.ufwid]
        add     edx,[g_usf.uflp]
        cmp     eax,edx
        ret

; us_lead - AL the sign before the number, 0 none.
us_lead:
        xor     al,al
        cmp     [g_usf.uflp],0
        je      @F
        mov     al,'+'
        cmp     [g_nneg],0
        je      @F
        mov     al,'-'
@@:
        ret

; us_tsign - AL the sign after it, 0 none.
us_tsign:
        mov     eax,[g_usf.uftr]
        test    al,al
        jz      @F
        cmp     [g_nneg],0
        je      @uts_p
        mov     al,'-'
        ret
@uts_p:
        cmp     al,'+'
        je      @F
        mov     al,' '
@@:
        ret

; us_trail - the sign after the number after the body's ECX chars.
us_trail:
        call    us_tsign
        test    al,al
        jz      @F
        mov     [g_ubody+ecx],al
        inc     ecx
@@:
        ret

; us_body - the body's ECX chars written.
us_body:
        mov     esi,offset g_ubody
        jmp     us_out

; ::::::::::::::
; us_fexp - the number as digits and an exponent of EAX carets (4: two
; digits, 5: three) in the field's places, to the body.
;
; out:    ECX  the body's length
; ::::::::::::::
us_fexp:
        mov     [g_ucar],eax
        mov     eax,[g_usf.ufbp]        ; ints: the places before the point
        dec     eax                     ; less the sign's
        jns     @F
        xor     eax,eax
@@:
        mov     [g_uint],eax
        add     eax,[g_usf.ufdec]
        mov     [g_upl],eax
        call    us_norm
        xor     ebx,ebx                 ; EBX the first digit not 0
@@:
        cmp     ebx,[g_nc]
        jae     @F
        cmp     [g_nt+ebx],'0'
        jne     @F
        inc     ebx
        jmp     @B
@@:
        mov     [g_ufst],ebx
        mov     [g_ushf],0
        cmp     ebx,[g_nc]
        jae     @ugx_z
        mov     eax,[g_ni]
        sub     eax,ebx
        mov     [g_uexp],eax
        push    esi
        push    edi
        lea     esi,[g_nt+ebx]          ; from the first digit not 0
        mov     edi,offset g_nt
        mov     ecx,[g_nc]
        sub     ecx,ebx
        mov     [g_nc],ecx
        rep     movsb
        pop     edi
        pop     esi
        mov     [g_ni],0
        mov     eax,[g_upl]
        call    us_round
        cmp     [g_ni],0
        je      @F
        inc     [g_uexp]
        mov     [g_ni],0
@@:
        mov     eax,[g_uexp]
        sub     eax,[g_uint]
        mov     [g_ushf],eax
        jmp     @ugx_b
@ugx_z:                                 ; 0: zeros in every place
        mov     ecx,[g_upl]
        mov     edi,offset g_nt
        mov     al,'0'
        rep     stosb
@ugx_b:
        mov     edi,offset g_ubody
        mov     al,'-'
        cmp     [g_nneg],0
        jne     @F
        mov     al,' '
        cmp     [g_uint],0
        jne     @F
        cmp     [g_usf.ufpt],0
        je      @F
        mov     al,'0'
@@:
        stosb
        xor     ebx,ebx                 ; each place
@ugx_l:
        cmp     ebx,[g_upl]
        jae     @ugx_e
        cmp     ebx,[g_uint]
        jne     @F
        cmp     [g_usf.ufpt],0
        je      @F
        mov     al,'.'
        stosb
@@:
        mov     al,[g_nt+ebx]
        mov     edx,[g_uint]            ; a lead 0 of 0 a space
        dec     edx
        cmp     ebx,edx
        jge     @F
        cmp     al,'0'
        jne     @F
        mov     edx,[g_ufst]
        cmp     edx,[g_nc]
        jne     @F
        mov     al,' '
@@:
        stosb
        inc     ebx
        jmp     @ugx_l
@ugx_e:
        mov     eax,[g_uint]
        cmp     eax,[g_upl]
        jne     @F
        cmp     [g_usf.ufpt],0
        je      @F
        mov     al,'.'
        stosb
@@:
        mov     al,'E'
        stosb
        mov     eax,[g_ushf]
        mov     dl,'+'
        test    eax,eax
        jns     @F
        neg     eax
        mov     dl,'-'
@@:
        mov     [edi],dl
        inc     edi
        cmp     [g_ucar],5
        jne     @F
        push    eax
        xor     edx,edx
        mov     ecx,100
        div     ecx
        xor     edx,edx
        mov     ecx,10
        div     ecx
        add     dl,'0'
        mov     [edi],dl
        inc     edi
        pop     eax
@@:
        xor     edx,edx
        mov     ecx,100
        div     ecx
        mov     eax,edx
        aam
        add     ax,'00'
        xchg    al,ah
        stosw
        mov     ecx,edi
        sub     ecx,offset g_ubody
        ret

        .data
u_numc  db      '#.+$*'
        .data?
g_ufmt  db      FMTMAX dup(?)
g_ufl   dd      ?                       ; the format's length, the place
g_uat   dd      ?
g_usf   TUSF    <>
g_nt    db      NDIG+20 dup(?)          ; the number: its digits, count,
g_nc    dd      ?                       ; places before the point, sign,
g_ni    dd      ?                       ; PRINT's exponent
g_nneg  db      ?
g_nsci  db      ?
g_ubody db      FMTMAX+NDIG dup(?)
g_uline db      FMTMAX+NDIG+8 dup(?)
g_ucar  dd      ?                       ; us_fexp's carets, places before
g_uint  dd      ?                       ; the point, all places, first
g_upl   dd      ?                       ; digit, exponent, shift
g_ufst  dd      ?
g_uexp  dd      ?
g_ushf  dd      ?
g_utd   dd      ?                       ; the type's digits, 7 or 16;
g_utc   dd      ?                       ; dbl_digits's
        .code

        end
