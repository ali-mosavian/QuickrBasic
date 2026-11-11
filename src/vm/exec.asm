; ==========================================================================
; exec.asm - the executor: the stream's handlers and the data ops
; ==========================================================================
; holds:    ex_lp, the handlers the stream calls, CALL
; uses:     flt_*, str_*, arr_ref, the CALL table
;           The code is a stream of byte ops in cachb, each its op then
;           its operands. A handler enters with ESI at its operands, EBX the
;           op * 2, and ends by dispatch. The data stack is ESP, 4-byte
;           cells: an INT in the low word, a FLOAT packed, a string its Desc's
;           address. Every op takes its operands off it and pushes its
;           result. At a statement's start ESP is rsp. A stream address kept
;           in memory is 16 bits, from cachb.
; calls:
;   ex_lp              a handler; back by dispatch or jmp ex_lp
; ==========================================================================

dispatch macro
        movzx   ebx,byte ptr [esi]
        mov     [opip],esi              ; the op, for CONT after a BREAK
        inc     esi
        add     ebx,ebx
        jmp     [ops+ebx*2]
        endm

; EBX = the slot offset (id * 4) of the variable id at ESI, ESI past it.
vslot   macro
        movzx   ebx,byte ptr [esi]
        inc     esi
        shl     ebx,2
        endm

; ESI = a frame's or CONT's ip, DX its line, from AX: a 16-bit stream
; offset, or an offset in its line's code (cm_flush), under DCODE, which the
; direct line never has.
ipfix   macro
        local   @ipa,@ipb
        movzx   esi,ax
        cmp     esi,DCODE
        jae     @ipa
        cmp     dx,DLINE
        je      @ipa
        call    ex_resolve
        jmp     @ipb
@ipa:
        add     esi,offset cachb
@ipb:
        endm

; The STOP key: BREAK, CONT runs this op again.
stopchk macro
        cmp     [stpflg],0
        jne     ex_brk
        endm

; EDI = element AX of the 1-D array whose id is at ESI; else to slow with EBX
; the id * 2. sh: 0 an INT, 1 a string, 2 a FLOAT.
a1get   macro   sh,slow
        movzx   ebx,byte ptr [esi]
        inc     esi
        add     ebx,ebx
        mov     edi,[vt+ebx*2]
        cmp     byte ptr [edi],1
        jne     slow
        cmp     ax,[edi+AH_FAST]
        jae     slow
        movzx   eax,ax
  if sh eq 0
        add     eax,eax
  elseif sh eq 1
        lea     eax,[eax+eax*2]
        add     eax,eax
  else
        shl     eax,2
  endif
        add     eax,[edi+AH_PTR]
        mov     edi,eax
        endm

; ::::::::::::::
; ex_lp - run ops from ESI.
; ::::::::::::::
ex_lp:
        dispatch

; ::::::::::::::
; ex_brk - BREAK at the op dispatch last ran: CONT runs it again.
; ::::::::::::::
ex_brk:
        call    plt_kflush              ; the Ctrl+C, not a key for the editor
        mov     esi,[opip]
        jmp     sc_stop

; ::::::::::::::
; op_sldt - S.LD.TXT len ptr:4: a temporary for the text.
;
; error:  ?FORMULA TOO COMPLEX  no temporary left
; ::::::::::::::
op_sldt:
        movzx   ax,byte ptr [esi]
        mov     [sln],ax
        mov     ebx,[esi+1]
        add     esi,5
        mov     [spt],ebx
        call    str_push
        push    eax
        dispatch

; ::::::::::::::
; arr_r1 - arr_ref of a 1-D subscript.
;
; in:     EBX  the id * 2
;         AX   the subscript
; out:    EDI  the element
; ::::::::::::::
arr_r1:
        shr     ebx,1
        mov     [dimbuf],ax
        mov     [narr],1
        jmp     arr_ref

; ::::::::::::::
; arr_pn - id n at ESI: the n subscripts off the stack, under the return.
;
; out:    EDI  the element
; error:  as arr_ref
; ::::::::::::::
arr_pn:
        pop     [aret]
        movzx   edx,byte ptr [esi]
        mov     cl,[esi+1]
        add     esi,2
        mov     [narr],cl
        movzx   ebx,cl
        add     ebx,ebx
@apn_l:
        pop     eax
        mov     [dimbuf+ebx-2],ax
        sub     ebx,2
        jnz     @apn_l
        mov     ebx,edx
        call    arr_ref
        jmp     [aret]

; ::::::::::::::
; op_ilda1 - X.LD.A1 id ( i -- x ).
; ::::::::::::::
op_ilda1:
        pop     eax
        a1get   0,@ila_s
@ila_g:
        movzx   eax,word ptr [edi]
        push    eax
        dispatch
@ila_s:
        call    arr_r1
        jmp     @ila_g
; ::::::::::::::
; op_flda1 - X.LD.A1 id ( i -- x ).
; ::::::::::::::
op_flda1:
        pop     eax
        a1get   2,@fla_s
@fla_g:
        push    dword ptr [edi]
        dispatch
@fla_s:
        call    arr_r1
        jmp     @fla_g
; ::::::::::::::
; op_slda1 - X.LD.A1 id ( i -- x ).
; ::::::::::::::
op_slda1:
        pop     eax
        a1get   1,@sla_s
@sla_g:
        push    edi
        dispatch
@sla_s:
        call    arr_r1
        jmp     @sla_g

; ::::::::::::::
; op_lda - X.LD.A id n ( i1..in -- x ).
; ::::::::::::::
op_lda:
        call    arr_pn
        push    dword ptr [edi]
        dispatch
; ::::::::::::::
; op_slda - X.LD.A id n ( i1..in -- x ).
; ::::::::::::::
op_slda:
        call    arr_pn
        push    edi
        dispatch

; ::::::::::::::
; op_sstv - X.ST.V id ( x -- ).
; ::::::::::::::
op_sstv:
        vslot
        lea     eax,[SVBASE+ebx*2]
        mov     [bpo],eax
        pop     eax
        call    str_set
        dispatch

; ::::::::::::::
; op_ista1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_ista1:
        pop     dword ptr [stv]
        pop     eax
        a1get   0,@isa_s
@isa_g:
        mov     ax,word ptr [stv]
        mov     [edi],ax
        dispatch
@isa_s:
        call    arr_r1
        jmp     @isa_g
; ::::::::::::::
; op_fsta1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_fsta1:
        pop     dword ptr [stv]
        pop     eax
        a1get   2,@fsa_s
@fsa_g:
        mov     eax,[stv]
        mov     [edi],eax
        dispatch
@fsa_s:
        call    arr_r1
        jmp     @fsa_g
; ::::::::::::::
; op_ssta1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_ssta1:
        pop     dword ptr [stv]
        pop     eax
        a1get   1,@ssa_s
@ssa_g:
        mov     [bpo],edi
        mov     eax,[stv]
        call    str_set
        dispatch
@ssa_s:
        call    arr_r1
        jmp     @ssa_g

; ::::::::::::::
; op_istan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_istan:
        pop     dword ptr [stv]
        call    arr_pn
        jmp     @isa_g
; ::::::::::::::
; op_fstan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_fstan:
        pop     dword ptr [stv]
        call    arr_pn
        jmp     @fsa_g
; ::::::::::::::
; op_sstan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_sstan:
        pop     dword ptr [stv]
        call    arr_pn
        jmp     @ssa_g

; ::::::::::::::
; int_div - [EBP] / AX, truncated.
;
; in:     AX     the divisor
;         [EBP]  the dividend
; out:    AX  the quotient
; keeps:  EBP, ESI
; error:  ?DIVISION BY ZERO  AX 0
;         ?OVERFLOW          -32768 / -1
; ::::::::::::::
int_div:
        test    ax,ax
        jnz     @idv_g
        mov     al,E_DZ
        jmp     err_raise
@idv_g:
        mov     cx,ax
        mov     ax,[ebp]
        cmp     cx,-1
        je      @idv_n
        cwd
        idiv    cx
        ret
@idv_n:
        neg     ax
        jno     @idv_r
        jmp     err_ov
@idv_r:
        ret

; ::::::::::::::
; ex_cmpt - push the relation, or end the line when a CMPIF's is false.
;
; in:     AL   the relation's bit
;         CL   the mask
;         cop  the op * 2
; ::::::::::::::
ex_cmpt:
        and     al,cl
        cmp     [cop],O_I_CMPIF*2
        jae     @ct_if
        neg     al
        sbb     eax,eax
        push    eax
        dispatch
@ct_if:
        test    al,al
        jz      ex_nextln
        dispatch

; ::::::::::::::
; op_scmp - S.CMP .. S.CMPIF.
; ::::::::::::::
op_scmp:
        mov     [cop],bx
        pop     eax
        pop     edx
        call    str_cmp
        mov     al,cl
        mov     cl,[esi]
        inc     esi
        jmp     ex_cmpt

; ::::::::::::::
; op_sif - X.IF ( x -- ): false ends the line.
; ::::::::::::::
op_sif:
        pop     eax
        call    str_getd
        jz      ex_nextln
        dispatch

; ::::::::::::::
; op_scat - S.CAT ( s s -- s ).
;
; error:  ?STRING TOO LONG  over SMAX chars
; ::::::::::::::
op_scat:
        pop     eax
        pop     edx
        call    str_cat
        push    eax
        dispatch

; ::::::::::::::
; op_call - CALL k: routine k of calls.
; ::::::::::::::
op_call:
        movzx   ebx,byte ptr [esi]
        inc     esi
        add     ebx,ebx
        jmp     [calls+ebx*2]
