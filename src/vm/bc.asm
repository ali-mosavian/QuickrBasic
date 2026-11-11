; ==========================================================================
; bc.asm - handlers for the ops the 16-bit build writes out as machine code
; ==========================================================================
; holds:    bc_* the loads, stores, INT and FLOAT operators, compares, IF,
;           conversions, DROP
; uses:     flt_*, int_div, ex_nextln, err_ov, err_iq
; data:     As comp/native.asm's templates: an INT op writes the top cell's
;           low word; a compare's mask, its last operand: 1 greater, 2
;           equal, 4 less.
; calls:
;   bc_*               dispatch to the next op
; ==========================================================================

include x86f.inc
        .code

; EBX = the slot of the variable id at ESI, ESI past it.
bvar    macro
        movzx   ebx,byte ptr [esi]
        inc     esi
        lea     ebx,[VARBASE+ebx*4]
        endm

; flt routine fn with ESI kept.
fcall   macro   fn
        push    esi
        call    fn
        pop     esi
        endm

; EAX = an INT right operand. src: 0 popped, 1 a variable, 2 the U8.
isrc    macro   src
  if src eq 0
        pop     eax
  elseif src eq 1
        bvar
        mov     eax,[ebx]
  else
        movzx   eax,byte ptr [esi]
        inc     esi
  endif
        endm

; EAX = a FLOAT right operand. src: 0 popped, 1 a variable, 2 an INT
; variable, 3 the U8, 4 the F32.
fsrc    macro   src
  if src eq 0
        pop     eax
  elseif src eq 1
        bvar
        mov     eax,[ebx]
  elseif src eq 2
        bvar
        mov     eax,[ebx]
        fcall   flt_from32
  elseif src eq 3
        movzx   eax,byte ptr [esi]
        inc     esi
        fcall   flt_from32
  else
        mov     eax,[esi]
        add     esi,4
  endif
        endm

; ::::::::::::::
; bc_I_LD_V - X.LD.V id: the variable's cell.
; ::::::::::::::
bc_I_LD_V:
; ::::::::::::::
; bc_F_LD_V - X.LD.V id: the variable's cell.
; ::::::::::::::
bc_F_LD_V:
        bvar
        push    dword ptr [ebx]
        dispatch
; ::::::::::::::
; bc_I_LD_I16 - I.LD.I16 w: the word.
; ::::::::::::::
bc_I_LD_I16:
        movzx   eax,word ptr [esi]
        add     esi,2
        push    eax
        dispatch
; ::::::::::::::
; bc_I_LD_U8 - I.LD.U8 b: the byte.
; ::::::::::::::
bc_I_LD_U8:
        movzx   eax,byte ptr [esi]
        inc     esi
        push    eax
        dispatch
; ::::::::::::::
; bc_F_LD_F32 - F.LD.F32 f: the FLOAT.
; ::::::::::::::
bc_F_LD_F32:
        push    dword ptr [esi]
        add     esi,4
        dispatch
; ::::::::::::::
; bc_F_LD_U8 - F.LD.U8 b: the byte as a FLOAT.
; ::::::::::::::
bc_F_LD_U8:
        fsrc    3
        push    eax
        dispatch
; ::::::::::::::
; bc_F_LD_VI - F.LD.VI id: the INT variable as a FLOAT.
; ::::::::::::::
bc_F_LD_VI:
        fsrc    2
        push    eax
        dispatch
; ::::::::::::::
; bc_F_LD_PI - F.LD.PI: pi.
; ::::::::::::::
bc_F_LD_PI:
        fcall   flt_pi
        push    eax
        dispatch
; ::::::::::::::
; bc_S_LD_V - S.LD.V id: the string variable's Desc.
; ::::::::::::::
bc_S_LD_V:
        movzx   eax,byte ptr [esi]
        inc     esi
        lea     eax,[SVBASE+eax*8]
        push    eax
        dispatch
; ::::::::::::::
; bc_I_ST_V - I.ST.V id ( i -- ): the INT variable.
; ::::::::::::::
bc_I_ST_V:
        bvar
        pop     dword ptr [ebx]
        dispatch
; ::::::::::::::
; bc_F_ST_V - F.ST.V id ( f -- ): the FLOAT variable.
; ::::::::::::::
bc_F_ST_V:
        bvar
        pop     dword ptr [ebx]
        dispatch

; INT operators on the top cell: k 0 + 1 - 2 * 3 / 4 AND 5 OR.
ibin    macro   nm,src,k
bc_&nm:
        isrc    src
  if k eq 0
        add     [esp],eax
        jo      err_ov
  elseif k eq 1
        sub     [esp],eax
        jo      err_ov
  elseif k eq 2
        imul    eax,[esp]
        jo      err_ov
        mov     [esp],eax
  elseif k eq 3
        mov     ebp,esp
        push    esi
        call    int_div
        pop     esi
        mov     [esp],eax
  elseif k eq 4
        and     [esp],eax
  else
        or      [esp],eax
  endif
        dispatch
        endm

        ibin    I_ADD,0,0
        ibin    I_SUB,0,1
        ibin    I_MUL,0,2
        ibin    I_DIV,0,3
        ibin    I_AND,0,4
        ibin    I_OR,0,5
        ibin    I_ADD_V,1,0
        ibin    I_SUB_V,1,1
        ibin    I_MUL_V,1,2
        ibin    I_DIV_V,1,3
        ibin    I_AND_V,1,4
        ibin    I_OR_V,1,5
        ibin    I_ADD_U8,2,0
        ibin    I_SUB_U8,2,1
        ibin    I_MUL_U8,2,2
        ibin    I_DIV_U8,2,3
        ibin    I_AND_U8,2,4
        ibin    I_OR_U8,2,5

; ::::::::::::::
; bc_I_NEG - I.NEG ( i -- i ).
; ::::::::::::::
bc_I_NEG:
        neg     dword ptr [esp]
        jo      err_ov
        dispatch
; ::::::::::::::
; bc_I_NOT - I.NOT ( i -- i ).
; ::::::::::::::
bc_I_NOT:
        not     dword ptr [esp]
        dispatch

; FLOAT operators: the right in EAX, the left popped to EDX, bflt's k.
fbin    macro   nm,src,k
bc_&nm:
        fsrc    src
        pop     edx
        push    esi
        call    [g_bflt+k*4]
        pop     esi
        push    eax
        dispatch
        endm

        fbin    F_ADD,0,0
        fbin    F_SUB,0,1
        fbin    F_MUL,0,2
        fbin    F_DIV,0,3
        fbin    F_ADD_V,1,0
        fbin    F_SUB_V,1,1
        fbin    F_MUL_V,1,2
        fbin    F_DIV_V,1,3
        fbin    F_ADD_VI,2,0
        fbin    F_SUB_VI,2,1
        fbin    F_MUL_VI,2,2
        fbin    F_DIV_VI,2,3
        fbin    F_ADD_U8,3,0
        fbin    F_SUB_U8,3,1
        fbin    F_MUL_U8,3,2
        fbin    F_DIV_U8,3,3
        fbin    F_ADD_F32,4,0
        fbin    F_SUB_F32,4,1
        fbin    F_MUL_F32,4,2
        fbin    F_DIV_F32,4,3

; ::::::::::::::
; bc_F_NEG - F.NEG ( f -- f ): 0 stays 0.
; ::::::::::::::
bc_F_NEG:
        test    dword ptr [esp],7F800000h
        jz      @F
        xor     byte ptr [esp+3],80h
@@:
        dispatch

; The relation's bit in AL, the mask at ESI: a compare pushes -1 or 0, a
; CMPIF's false ends the line.
ctail   macro   cif
        inc     esi
        and     al,[esi-1]              ; ZF after the inc: jz reads it
  if cif
        jz      ex_nextln
  else
        neg     al
        sbb     eax,eax
        push    eax
  endif
        dispatch
        endm

; INT compare: left popped to EDX, the right in EAX.
icmp    macro   nm,src,cif
        local   @icl
bc_&nm:
        isrc    src
        pop     edx
        cmp     edx,eax
        mov     al,1
        jg      @icl
        mov     al,2
        je      @icl
        mov     al,4
@icl:
        ctail   cif
        endm

        icmp    I_CMP,0,0
        icmp    I_CMP_V,1,0
        icmp    I_CMP_U8,2,0
        icmp    I_CMPIF,0,1
        icmp    I_CMPIF_V,1,1
        icmp    I_CMPIF_U8,2,1

; FLOAT compare: flt_cmp's FFh, 0, 1 the mask's bit 0, 1, 2.
fcmp    macro   nm,src,cif
bc_&nm:
        fsrc    src
        pop     edx
        fcall   flt_cmp
        mov     cl,al
        inc     cl
        mov     al,1
        shl     al,cl
        ctail   cif
        endm

        fcmp    F_CMP,0,0
        fcmp    F_CMP_V,1,0
        fcmp    F_CMP_VI,2,0
        fcmp    F_CMP_U8,3,0
        fcmp    F_CMP_F32,4,0
        fcmp    F_CMPIF,0,1
        fcmp    F_CMPIF_V,1,1
        fcmp    F_CMPIF_VI,2,1
        fcmp    F_CMPIF_U8,3,1
        fcmp    F_CMPIF_F32,4,1

; ::::::::::::::
; bc_I_IF - I.IF ( i -- ): 0 ends the line.
; ::::::::::::::
bc_I_IF:
        pop     eax
        test    eax,eax
        jz      ex_nextln
        dispatch
; ::::::::::::::
; bc_F_IF - ?
; ::::::::::::::
bc_F_IF:
        pop     eax
        test    eax,7F800000h
        jz      ex_nextln
        dispatch

; ::::::::::::::
; bc_I_TOF - I.TOF ( i -- f ).
; ::::::::::::::
bc_I_TOF:
        pop     eax
        fcall   flt_from32
        push    eax
        dispatch
; ::::::::::::::
; bc_F_TOI - F.TOI ( f -- i ).
; ::::::::::::::
bc_F_TOI:
        pop     eax
        fcall   flt_fix32
        push    eax
        dispatch
; ::::::::::::::
; bc_F_TOB - F.TOB ( f -- i ): 0..255.
; ::::::::::::::
bc_F_TOB:
        pop     eax
        fcall   flt_fix16
        test    ah,ah
        jnz     err_iq
        movzx   eax,al
        push    eax
        dispatch
; ::::::::::::::
; bc_F_TOA - F.TOA ( f -- i ): an address, 0..65535.
; ::::::::::::::
bc_F_TOA:
        pop     eax
        fcall   flt_fixu16
        movzx   eax,ax
        push    eax
        dispatch
; ::::::::::::::
; bc_I_TOB - I.TOB ( i -- i ): 0..255.
; ::::::::::::::
bc_I_TOB:
        cmp     dword ptr [esp],255
        ja      err_iq
        dispatch
; ::::::::::::::
; bc_I_TOA - I.TOA ( i -- i ): 0..65535.
; ::::::::::::::
bc_I_TOA:
        cmp     dword ptr [esp],65535
        ja      err_iq
        dispatch
; ::::::::::::::
; bc_DROP - DROP ( x -- ).
; ::::::::::::::
bc_DROP:
        add     esp,4
        dispatch

        end
