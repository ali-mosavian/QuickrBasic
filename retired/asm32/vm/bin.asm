; ==========================================================================
; bin.asm - numbers as their bytes: MKI$ MKL$ MKS$ MKD$, CVI CVL CVS CVD,
; and the Microsoft Binary Format's MKSMBF$ MKDMBF$ CVSMBF CVDMBF
; ==========================================================================
; holds:    sc_mki, sc_mkl, sc_mks, sc_mkd, sc_mksm, sc_mkdm, sc_cvi,
;           sc_cvl, sc_cvs, sc_cvd, sc_cvsm, sc_cvdm
; uses:     str_get, sx_sret
; data:     A cell holds an INT or a SINGLE in its low dword, a DOUBLE in
;           all of it: its bytes are the value's, low first. MBF: the
;           significand low first, its sign in the top bit of the byte under
;           the exponent; the exponent last, 0 for zero, biased 129 with the
;           point before the hidden 1 (IEEE's +2 single, -894 double).
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; sc_mki - MKI$ ( i -- s ): its low 2 bytes.
;
; error:  ?OVERFLOW  outside -32768 to 32767
; ::::::::::::::
sc_mki:
        mov     eax,[esp]
        movsx   ecx,ax
        cmp     ecx,eax
        jne     err_ov
        mov     ecx,2
        jmp     bn_mk
; sc_mkl, sc_mks - MKL$, MKS$ ( x -- s ): 4 bytes; sc_mkd - MKD$: 8.
sc_mkl:
sc_mks:
        mov     ecx,4
        jmp     bn_mk
sc_mkd:
        mov     ecx,8

; bn_mk - the cell's first ECX bytes, a string.
bn_mk:
        push    esi
        lea     esi,[esp+4]
        mov     edi,offset g_sbuf
        push    ecx
        rep     movsb
        pop     ecx
        pop     esi
        add     esp,CELL
        jmp     sx_sret

; ::::::::::::::
; sc_mksm - MKSMBF$ ( f -- s ).
; ::::::::::::::
sc_mksm:
        mov     eax,[esp]
        mov     edx,eax
        shr     edx,23
        and     edx,0FFh                ; the exponent
        jz      @bms_z                  ; 0 and denormals: 0
        add     edx,2
        cmp     edx,0FFh
        ja      err_ov
        mov     ecx,eax
        and     eax,7FFFFFh             ; significand, sign at bit 23
        shr     ecx,31
        shl     ecx,23
        or      eax,ecx
        shl     edx,24
        or      eax,edx
        jmp     @bms_s
@bms_z:
        xor     eax,eax
@bms_s:
        mov     [esp],eax
        mov     ecx,4
        jmp     bn_mk

; ::::::::::::::
; sc_mkdm - MKDMBF$ ( d -- s ): the significand's 52 bits as 55.
; ::::::::::::::
sc_mkdm:
        mov     eax,[esp]               ; EDX:EAX the bits
        mov     edx,[esp+4]
        mov     ebx,edx
        shr     ebx,20
        and     ebx,7FFh
        jz      @bmd_z
        sub     ebx,1023-129
        jle     @bmd_z                  ; too small: 0
        cmp     ebx,0FFh
        ja      err_ov
        mov     ecx,edx
        and     edx,0FFFFFh
        shld    edx,eax,3
        shl     eax,3
        and     edx,7FFFFFh
        shr     ecx,31
        shl     ecx,23
        or      edx,ecx
        shl     ebx,24
        or      edx,ebx
        jmp     @bmd_s
@bmd_z:
        xor     eax,eax
        xor     edx,edx
@bmd_s:
        mov     [esp],eax
        mov     [esp+4],edx
        mov     ecx,8
        jmp     bn_mk

; ::::::::::::::
; bn_cv - the string at the stack's top, its first ECX bytes in the cell
; instead.
;
; error:  ?ILLEGAL QUANTITY  shorter
; ::::::::::::::
bn_cv:
        push    ecx
        mov     eax,[esp+8]
        call    str_get
        movzx   edx,cx
        pop     ecx
        cmp     edx,ecx
        jb      err_iq
        mov     eax,[g_state.sg.spt]
        mov     edx,[eax+4]
        mov     eax,[eax]
        mov     [esp+4],eax
        mov     [esp+8],edx
        ret

; ::::::::::::::
; sc_cvi - CVI ( s -- i ); sc_cvl - CVL; sc_cvs - CVS ( s -- f ); sc_cvd -
; CVD ( s -- d ).
; ::::::::::::::
sc_cvi:
        mov     ecx,2
        call    bn_cv
        movsx   eax,word ptr [esp]
        mov     [esp],eax
        jmp     ex_lp
sc_cvl:
sc_cvs:
        mov     ecx,4
        call    bn_cv
        jmp     ex_lp
sc_cvd:
        mov     ecx,8
        call    bn_cv
        jmp     ex_lp

; ::::::::::::::
; sc_cvsm - CVSMBF ( s -- f ).
; ::::::::::::::
sc_cvsm:
        mov     ecx,4
        call    bn_cv
        mov     eax,[esp]
        mov     edx,eax
        shr     edx,24                  ; the exponent
        cmp     edx,2
        jbe     @bcs_z                  ; 0, and under the single's range
        sub     edx,2
        mov     ecx,eax
        shr     ecx,23
        and     ecx,1
        shl     ecx,31
        and     eax,7FFFFFh
        shl     edx,23
        or      eax,edx
        or      eax,ecx
        jmp     @bcs_s
@bcs_z:
        xor     eax,eax
@bcs_s:
        mov     [esp],eax
        jmp     ex_lp

; ::::::::::::::
; sc_cvdm - CVDMBF ( s -- d ): 55 bits rounded to 52.
; ::::::::::::::
sc_cvdm:
        mov     ecx,8
        call    bn_cv
        mov     eax,[esp]
        mov     edx,[esp+4]
        mov     ebx,edx
        shr     ebx,24
        jz      @bcd_z
        add     ebx,1023-129
        mov     ecx,edx
        shr     ecx,23
        and     ecx,1                   ; the sign
        and     edx,7FFFFFh
        add     eax,4                   ; round the 3 bits away
        adc     edx,0
        shrd    eax,edx,3
        shr     edx,3
        test    edx,100000h             ; rounded up past the top
        jz      @F
        and     edx,0FFFFFh
        inc     ebx
@@:
        shl     ebx,20
        or      edx,ebx
        shl     ecx,31
        or      edx,ecx
        jmp     @bcd_s
@bcd_z:
        xor     eax,eax
        xor     edx,edx
@bcd_s:
        mov     [esp],eax
        mov     [esp+4],edx
        jmp     ex_lp

        end
