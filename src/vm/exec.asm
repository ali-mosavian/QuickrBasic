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

include x86f.inc
        .code

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
        mov     esi,[g_state.rn.opip]
        jmp     sc_stop

; ::::::::::::::
; op_sldt - S.LD.TXT len ptr:4: a temporary for the text.
;
; error:  ?FORMULA TOO COMPLEX  no temporary left
; ::::::::::::::
op_sldt:
        movzx   ax,byte ptr [esi]
        mov     [g_state.sg.sln],ax
        mov     ebx,[esi+1]
        add     esi,5
        mov     [g_state.sg.spt],ebx
        call    str_push
        push    eax
        dispatch

; ::::::::::::::
; arr_r1 - arr_ref of a 1-D subscript.
;
; in:     EBX  the id * 2
;         EAX  the subscript
; out:    EDI  the element
; error:  as arr_ref, and ?ILLEGAL QUANTITY past a word
; ::::::::::::::
arr_r1:
        subchk
        shr     ebx,1
        mov     [g_dimbuf],ax
        mov     [g_state.rn.narr],1
        jmp     arr_ref

; ::::::::::::::
; arr_pn - id n at ESI: the n subscripts off the stack, under the return.
;
; out:    EDI  the element
; error:  as arr_ref
; ::::::::::::::
arr_pn:
        pop     [g_state.rn.aret]
        movzx   edx,word ptr [esi]
        mov     cl,[esi+2]
        add     esi,3
        mov     [g_state.rn.narr],cl
        movzx   ebx,cl
        add     ebx,ebx
@apn_l:
        pop     eax
        subchk
        mov     [g_dimbuf+ebx-2],ax
        sub     ebx,2
        jnz     @apn_l
        mov     ebx,edx
        call    arr_ref
        jmp     [g_state.rn.aret]

; ::::::::::::::
; op_raddr - R.ADDR id ( -- a ): the record's address; made, zeroed, at its
; first use.
; ::::::::::::::
op_raddr:
        movzx   ebx,word ptr [esi]
        add     esi,2
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        jnz     @rad_g
        mov     [g_state.mm.aid],bx
        mov     [g_state.rn.narr],1
        mov     word ptr [g_cntbuf],1
        and     word ptr [g_lbbuf],0
        call    arr_new
@rad_g:
        push    dword ptr [edi+AH_PTR]
        dispatch

; ::::::::::::::
; op_raddra - RA.ADDR id n ( i1..in -- a ): a record array's element.
; ::::::::::::::
op_raddra:
        call    arr_pn
        push    edi
        dispatch

; ::::::::::::::
; op_flld - FL.LD off et ( a -- x ): a record's number field.
; ::::::::::::::
op_flld:
        pop     edi
        movzx   eax,word ptr [esi]
        add     edi,eax
        cmp     byte ptr [esi+2],ET_SH
        lea     esi,[esi+3]
        je      @fll_h
        push    dword ptr [edi]
        dispatch
@fll_h:
        movsx   eax,word ptr [edi]
        push    eax
        dispatch

; ::::::::::::::
; op_flst - FL.ST off et ( a x -- ).
;
; error:  ?OVERFLOW  a SHORT's past 16 bits
; ::::::::::::::
op_flst:
        pop     eax
        pop     edi
        movzx   ecx,word ptr [esi]
        add     edi,ecx
        cmp     byte ptr [esi+2],ET_SH
        lea     esi,[esi+3]
        je      @fls_h
        mov     [edi],eax
        dispatch
@fls_h:
        movsx   ecx,ax
        cmp     ecx,eax
        jne     err_ov
        mov     [edi],ax
        dispatch

; ::::::::::::::
; op_sfld - SF.LD off n ( a -- s ): a STRING * n field, a temporary.
; ::::::::::::::
op_sfld:
        movzx   eax,word ptr [esi+2]
        call    str_alloc               ; the record does not move
        pop     eax
        movzx   ecx,word ptr [esi]
        add     eax,ecx
        add     esi,4
        push    esi
        mov     esi,eax
        mov     edi,[g_state.sg.dpv]
        movzx   ecx,[g_state.sg.sn]
        rep     movsb
        pop     esi
        call    str_new
        push    eax
        dispatch

; ::::::::::::::
; op_sfst - SF.ST off n ( a s -- ): cut or padded with spaces.
; ::::::::::::::
op_sfst:
        pop     eax
        call    str_get
        pop     edi
        movzx   ecx,word ptr [esi]
        add     edi,ecx
        movzx   edx,word ptr [esi+2]
        add     esi,4
        movzx   ecx,[g_state.sg.sln]
        cmp     ecx,edx
        jbe     @F
        mov     ecx,edx
@@:
        sub     edx,ecx
        push    esi
        mov     esi,[g_state.sg.spt]
        rep     movsb
        mov     ecx,edx
        mov     al,' '
        rep     stosb
        pop     esi
        dispatch

; ::::::::::::::
; op_rcopy - R.COPY n ( a b -- ): record b's n bytes over a's.
; ::::::::::::::
op_rcopy:
        pop     eax
        pop     edi
        movzx   ecx,word ptr [esi]
        add     esi,2
        push    esi
        mov     esi,eax
        rep     movsb
        pop     esi
        dispatch

; ::::::::::::::
; op_ilda1 - X.LD.A1 id ( i -- x ).
; ::::::::::::::
op_ilda1:
        pop     eax
        a1get   2,@ila_s
@ila_g:
        push    dword ptr [edi]
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
        mov     [g_state.sg.bpo],eax
        pop     eax
        call    str_set
        dispatch

; ::::::::::::::
; op_ista1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_ista1:
        pop     dword ptr [g_state.rn.stv]
        pop     eax
        a1get   2,@isa_s
@isa_g:
        mov     eax,[g_state.rn.stv]
        mov     [edi],eax
        dispatch
@isa_s:
        call    arr_r1
        jmp     @isa_g
; ::::::::::::::
; op_fsta1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_fsta1:
        pop     dword ptr [g_state.rn.stv]
        pop     eax
        a1get   2,@fsa_s
@fsa_g:
        mov     eax,[g_state.rn.stv]
        mov     [edi],eax
        dispatch
@fsa_s:
        call    arr_r1
        jmp     @fsa_g
; ::::::::::::::
; op_ssta1 - X.ST.A1 id ( i x -- ).
; ::::::::::::::
op_ssta1:
        pop     dword ptr [g_state.rn.stv]
        pop     eax
        a1get   1,@ssa_s
@ssa_g:
        mov     [g_state.sg.bpo],edi
        mov     eax,[g_state.rn.stv]
        call    str_set
        dispatch
@ssa_s:
        call    arr_r1
        jmp     @ssa_g

; ::::::::::::::
; op_istan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_istan:
        pop     dword ptr [g_state.rn.stv]
        call    arr_pn
        jmp     @isa_g
; ::::::::::::::
; op_fstan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_fstan:
        pop     dword ptr [g_state.rn.stv]
        call    arr_pn
        jmp     @fsa_g
; ::::::::::::::
; op_sstan - X.ST.A id n ( i1..in x -- ).
; ::::::::::::::
op_sstan:
        pop     dword ptr [g_state.rn.stv]
        call    arr_pn
        jmp     @ssa_g

; ::::::::::::::
; int_div - [EBP] / EAX, truncated.
;
; in:     EAX    the divisor
;         [EBP]  the dividend
; out:    EAX  the quotient
; keeps:  EBP, ESI
; error:  ?DIVISION BY ZERO  EAX 0
;         ?OVERFLOW          -2^31 / -1
; ::::::::::::::
int_div:
        test    eax,eax
        jnz     @idv_g
        mov     al,E_DZ
        jmp     err_raise
@idv_g:
        mov     ecx,eax
        mov     eax,[ebp]
        cmp     ecx,-1
        je      @idv_n
        cdq
        idiv    ecx
        ret
@idv_n:
        neg     eax
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
        cmp     [g_state.rn.cop],O_I_CMPIF*2
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
        mov     [g_state.rn.cop],bx
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
        jmp     [g_calls+ebx*2]

        .data
g_ops   label   dword
        oplist  opd
g_calls dd      sc_ipr,sc_fpr,sc_spr,sc_pnl,sc_pzone,sc_ptab,sc_pspc
        dd      sc_fin,sc_sin,sc_fget,sc_sget,sc_inbeg,sc_inpr,sc_infile,sc_inend
        dd      sc_fread,sc_sread,sc_res,sc_resl,sc_dim
        dd      sc_open,sc_close,sc_fiout,sc_fiin,sc_ficlr,sc_load,sc_save,sc_vfy
        dd      sc_run,sc_runl,sc_list,sc_cont,sc_clr,sc_new,sc_nodir,sc_end,sc_stop
        dd      sc_poke,sc_wait,sc_wait2,sc_sys,sc_inln
        dd      sc_ipow,sc_fpow,sc_fti,sc_sti,sc_ist
        dd      20 dup(sc_fn1)
        dd      sc_left,sc_right,sc_mid,sc_mid2
        dd      sc_fix,sc_instr,sc_lbnd,sc_ubnd,sc_space,sc_strg,sc_ltrim
        dd      sc_rtrim,sc_ucase,sc_lcase,sc_hex,sc_oct,sc_sfix,sc_mids
        dd      sc_vseg,sc_sadd

g_bflt  dd      flt_add,flt_sub,flt_mul,flt_div,fn_pow
g_fntab dd      fn_sgn,fn_int,fn_abs,fn_usr,fn_fre,fn_pos,fn_sqr,fn_rnd,fn_log,fn_exp
        dd      fn_cos,fn_sin,fn_tan,fn_atn,fn_peek,fn_len,fn_fstr,fn_val,fn_asc
        dd      fn_chr
        .code

        end
