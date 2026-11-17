; ==========================================================================
; exec.asm - the executor: the stream's handlers and the data ops
; ==========================================================================
; holds:    ex_lp, the handlers the stream calls, CALL
; uses:     flt_*, str_*, arr_ref, the CALL table
;           The code is a stream in the cache: simple ops are machine code
;           (comp/native.asm), the rest mov si,operands; jmp handler, and the
;           handler ends by jmp si. Its addresses are DS's; CS's are CSOFF
;           more. A handler enters with SI at its operands,
;           BX the op * 2 where the op is FB. The data stack is SP, 4-byte
;           cells: an INT in the low word, a FLOAT packed, a string its Desc's
;           linear address. Every op takes its operands off it and pushes its
;           result. At a statement's start SP is rsp.
; calls:
;   ex_lp              a handler; back by dispatch or jmp ex_lp
; ==========================================================================

dispatch macro
        add     si,CSOFF                ; the stream's DS address, run in CS
        jmp     si
        endm

; BX = the slot of the variable id at SI, SI past it.
vslot   macro
        movzx   bx,byte ptr [si]
        inc     si
        shl     bx,2
        endm

; SI = a frame's or CONT's ip, DX its line: an address in the code, or an
; offset from its line's (cm_flush), which the direct line never has.
ipfix   macro
        cmp     si,offset bss_e
        jae     @F
        cmp     dx,DLINE
        je      @F
        call    ex_resolve
@@:
        endm

; The STOP key: BREAK, CONT runs this op again.
stopchk macro
        cmp     [stpflg],0
        jne     ex_brk
        endm

; FS:DI = element AX of the 1-D array whose id is at SI; else to slow with BX
; the id * 2. AH_FAST is 0 unless the data is in one segment. sh: 0 an INT,
; 1 a string, 2 a FLOAT.
a1get   macro   sh,slow
        movzx   bx,byte ptr [si]
        inc     si
        add     bx,bx
        mov     di,[vt+bx]
        cmp     byte ptr [di],1
        jne     slow
        cmp     ax,[di+AH_FAST]
        jae     slow
        mov     fs,[di+AH_SEG]
  if sh eq 0
        add     ax,ax
  elseif sh eq 1
        mov     di,ax
        add     ax,ax
        add     ax,di
        add     ax,ax
  else
        shl     ax,2
  endif
        mov     di,ax
        endm

; ::::::::::::::
; ex_lp - run ops from SI.
;
; in:     SI  the code
; ::::::::::::::
ex_lp:
        dispatch

; ::::::::::::::
; op_nat - the ops written out as machine code (comp/native.asm) have no
; handler; one that is reached is a fault.
; ::::::::::::::
op_nat:
        mov     al,E_SN
        jmp     err_raise

; ::::::::::::::
; ex_brk - BREAK at the op whose operands are at SI: its element, 6 or 9
; bytes back (mov si,SI).
; ::::::::::::::
ex_brk:
        call    plt_kflush              ; the Ctrl+C, not a key for the editor
        mov     bx,si
        sub     bx,6
        cmp     byte ptr [bx],0BEh
        jne     @brk_b
        cmp     [bx+1],si
        je      @brk_f
@brk_b:
        sub     bx,3
@brk_f:
        mov     si,bx
        jmp     sc_stop

; ::::::::::::::
; op_sldt - S.LD.TXT len ptr:4: a temporary for the text.
;
; error:  ?FORMULA TOO COMPLEX  no temporary left
; ::::::::::::::
op_sldt:
        movzx   ax,byte ptr [si]
        mov     [sln],ax
        mov     ebx,[si+1]
        add     si,5
        mov     [spt],ebx
        call    str_push
        push    eax
        dispatch

; ::::::::::::::
; arr_r1 - arr_ref of a 1-D subscript.
;
; in:     BX  the id * 2
;         AX  the subscript
; out:    DI  the element
; ::::::::::::::
arr_r1:
        shr     bx,1
        mov     [dimbuf],ax
        mov     [narr],1
        jmp     arr_ref

; ::::::::::::::
; arr_pn - id n at SI: the n subscripts off the stack, under the return.
;
; out:    DI  the element
; error:  as arr_ref
; ::::::::::::::
arr_pn:
        pop     [aret]
        movzx   dx,byte ptr [si]
        mov     cl,[si+1]
        add     si,2
        mov     [narr],cl
        movzx   bx,cl
        add     bx,bx
@apn_l:
        pop     eax
        mov     [dimbuf+bx-2],ax
        sub     bx,2
        jnz     @apn_l
        mov     bx,dx
        call    arr_ref
        jmp     [aret]

; ::::::::::::::
; op_ilda1 - X.LD.A1 id ( i -- x ).
; ::::::::::::::
op_ilda1:
        pop     eax
        a1get   0,@ila_s
@ila_g:
        push    dword ptr fs:[di]
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
        push    dword ptr fs:[di]
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
        call    mem_lin
        push    eax
        dispatch
@sla_s:
        call    arr_r1
        jmp     @sla_g

; ::::::::::::::
; op_lda - X.LD.A id n ( i1..in -- x ).
; ::::::::::::::
op_lda:
        call    arr_pn
        push    dword ptr fs:[di]
        dispatch
; ::::::::::::::
; op_slda - X.LD.A id n ( i1..in -- x ).
; ::::::::::::::
op_slda:
        call    arr_pn
        call    mem_lin
        push    eax
        dispatch

; ::::::::::::::
; op_sstv - X.ST.V id ( x -- ).
; ::::::::::::::
op_sstv:
        vslot
        add     bx,bx
        movzx   eax,bx
        add     eax,SVBASE
        add     eax,[dslin]
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
        mov     fs:[di],ax
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
        mov     fs:[di],eax
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
        call    mem_lin
        mov     [bpo],eax
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
; int_div - [BP] / AX, truncated.
;
; in:     AX    the divisor
;         [BP]  the dividend
; out:    AX  the quotient
; keeps:  BP, SI
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
        mov     ax,[bp]
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
        mov     cl,[si]
        inc     si
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
        movzx   bx,byte ptr [si]
        inc     si
        add     bx,bx
        jmp     word ptr [calls+bx]
