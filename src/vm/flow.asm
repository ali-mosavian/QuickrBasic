; ==========================================================================
; flow.asm - the executor: lines, jumps, FN, FOR and NEXT
; ==========================================================================
; holds:    the ops that move SI or the control stack; ex_line, ex_end
; uses:     prg_findw, cm_line, cm_link, flt_add, err_*
; data:     The control stack holds frames from cstk up to CSTOP, the mark the
;           size: GOSUB mark ln:2 ip:2; FOR mark ln:2 ip:2 id step:4 limit:4
;           cache:4; PCALL mark ln:2 ip:2 proc lsp0:4 lsp1:4 tspb:2.
; calls:
;   ex_jump            ex_line: a stored line, or cm_line and ex_dcode
;   op_nx0, op_nextv
;     for_any          a % inline, else flt_add
; ==========================================================================

include x86f.inc
        .code

; DST, a word = ESI's stream offset. Uses EAX.
ipsave    macro   dst
        mov     eax,esi
        sub     eax,[g_state.mm.cachebase]
        mov     dst,eax
        endm

@st_flow1:
        jmp     err_os

; ::::::::::::::
; op_goto - GOTO, GOSUB num addr: addr is 0 until the jump first runs.
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
op_goto:
        stopchk
        call    ex_target
        jmp     ex_jump
; ::::::::::::::
; op_gosub - GOTO, GOSUB num addr.
; ::::::::::::::
op_gosub:
        stopchk
        call    ex_target
        jmp     ex_gosub

; ::::::::::::::
; ex_target - the TLREF at ESI: its line, found the first time.
;
; out:    EBX  the line
;         ESI  past them
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
ex_target:
        lodsw
        mov     [g_state.rn.lnum],ax
        lodsd
        mov     ebx,eax
        test    ebx,ebx
        jnz     @etg_r
        call    prg_findw
        jnc     err_us
        mov     [esi-4],ebx             ; found once, read after
@etg_r:
        ret

; ::::::::::::::
; ex_gosub - a GOSUB frame, then the line at EBX.
;
; in:     EBX      the line
;         ln, ESI  where RETURN goes on
; error:  Out of stack space  the control stack is full
; ::::::::::::::
ex_gosub:
        mov     al,M_GS
        call    ex_gsf
        jmp     ex_jump

; ::::::::::::::
; ex_gsf - push a GOSUB's frame, mark AL, its size: RETURN goes on at ESI.
;
; out:    EDI  the frame
; error:  Out of stack space  no room
; keeps:  EBX, ESI
; ::::::::::::::
ex_gsf:
        movzx   ecx,al
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSLIM
        jb      err_os
        sub     edi,ecx
        mov     [g_state.rn.cstk],edi
        mov     [edi+F_MARK],al
        mov     ecx,[g_state.rn.line]
        mov     [edi].TFRAME.line,ecx
        ipsave    [edi].TFRAME.resume
        ret

; ::::::::::::::
; ex_jump - run the line at EBX from its start.
;
; in:     EBX  the line
; ::::::::::::::
ex_jump:
        tspclr
        cmp     [g_state.lnk.linked],0
        jne     @exj_g
        push    ebx
        call    cm_link
        pop     ebx
@exj_g:
        mov     [g_state.lnk.cursor],ebx
        mov     [g_state.rn.runln],ebx
        call    ex_line
        jmp     ex_lp

; ::::::::::::::
; op_on - ON kn TLREF*n ( k -- ): kn bit 7 GOSUB; past the list for 0
; or k > n.
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
op_on:
        stopchk
        dpop    edx
        lodsb
        mov     [g_state.rn.onk],al
        and     al,7Fh
        movzx   ecx,al
        imul    ecx,sizeof TLREF
        lea     edi,[esi+ecx]           ; past the list
        dec     dl                      ; k 0: 255, past n < 128
        cmp     dl,al
        jae     @on_past
        movzx   ecx,dl
        imul    ecx,sizeof TLREF
        add     ecx,esi                 ; its TLREF
        mov     ax,[ecx].TLREF.num
        mov     [g_state.rn.lnum],ax
        mov     ebx,[ecx].TLREF.line
        mov     esi,edi
        test    ebx,ebx
        jnz     @on_g
        call    prg_findw
        jnc     err_us
        mov     [ecx].TLREF.line,ebx
@on_g:
        test    [g_state.rn.onk],80h
        jnz     ex_gosub
        jmp     ex_jump
@on_past:
        mov     esi,edi
        dispatch

; ::::::::::::::
; op_ret - RETURN: the FOR frames above the GOSUB frame drop.
;
; error:  ?RETURN WITHOUT GOSUB  no GOSUB frame on top
; ::::::::::::::
op_ret:
        stopchk
        mov     bx,0FFFFh
        call    for_find
        cmp     al,M_GS
        je      @F
        cmp     al,M_EV
        jne     err_rg
        call    ev_ret
@@:
        mov     edx,[ebx].TFRAME.line
        mov     [g_state.rn.line],edx
        movzx   ecx,byte ptr [ebx+F_MARK]
        mov     eax,[ebx].TFRAME.resume
        add     ebx,ecx
        mov     [g_state.rn.cstk],ebx
        tspclr
        ipload
        dispatch

; ::::::::::::::
; for_find - a variable's FOR frame, above the newest GOSUB or FN frame.
;
; in:     BX  the id
; out:    EBX  the frame
;         AL   its mark
;         ZF   set when found
; ::::::::::::::
for_find:
        mov     cx,bx
        mov     ebx,[g_state.rn.cstk]
@ff_lp:
        cmp     ebx,CSTOP
        jae     @ff_no
        mov     al,[ebx+F_MARK]
        cmp     al,M_FR
        jne     @ff_r
        cmp     cx,[ebx+F_ID]
        je      @ff_r
        add     ebx,FRSIZE
        jmp     @ff_lp
@ff_no:
        mov     al,1
        or      al,al
@ff_r:
        ret

; ::::::::::::::
; op_eol - EOL: on to the next line.
;
; in:     ln  the finished line
; ::::::::::::::
op_eol:
        stopchk

; ::::::::::::::
; ex_nextln - a false IF: on to the next line.
; ::::::::::::::
ex_nextln:
        tspclr
        call    ex_eol
        dispatch

; ::::::::::::::
; ex_eol - the next line; at the program's end, END.
;
; in:     ln  the finished line
; out:    ESI  the next line's code
; ::::::::::::::
ex_eol:
        mov     ebx,[g_state.rn.line]
        mov     ecx,gs:[ebx].TLINE.len
        add     ebx,ecx
        cmp     gs:[ebx].TLINE.len,0
        je      @F
        mov     [g_state.rn.runln],ebx
        jmp     ex_line
@@:
        add     esp,4
        mov     esi,[g_state.mm.cachebase]         ; contsi 0: CONT ends again
        jmp     sc_end

; ::::::::::::::
; ex_line - the code of line vpt: a program line's is in the cache, or
; compiled into it; a direct one's is compiled.
;
; in:     vpt  the line
; out:    ln  vpt
;         ESI  the code
; ::::::::::::::
ex_line:
        mov     ebx,[g_state.rn.runln]
        mov     [g_state.rn.line],ebx
        cmp     gs:[ebx].TLINE.len,0   ; the direct line's len
        je      @exl_x
        mov     esi,gs:[ebx].TLINE.code
        test    esi,esi
        jz      cm_cache
        add     esi,[g_state.mm.cachebase]
        ret
@exl_x:
        call    cm_line

; ::::::::::::::
; ex_dcode - the direct line's code, compiled to xbuf, as a stream at DCODE.
;
; out:    ESI  it
; ::::::::::::::
ex_dcode:
        mov     edi,[g_state.mm.cachebase]
        add     edi,DCODE
        cblea   [g_state.lnk.elim],DCODE+1536
        call    cm_xlate
        jc      err_om
        mov     esi,[g_state.lnk.ecode]
        ret

; ::::::::::::::
; op_for - I.FOR F.FOR id ( limit step -- ), I.FOR1 F.FOR1 id ( limit -- ): the
; start is already stored. An older loop of the variable drops; a FOR ending
; its line goes on at the next.
;
; error:  Out of stack space  the control stack is full
; ::::::::::::::
op_for:
        mov     [g_state.rn.fop],bx
        lodsw
        mov     [g_state.rn.fid],ax
        cmp     ebx,O_I_FOR1*2
        jae     @of_1
        dpop    dword ptr [g_state.rn.fstep]
        jmp     @of_l
@of_1:
        mov     eax,1
        cmp     ebx,O_F_FOR1*2
        jne     @of_s
        mov     eax,[g_kone]
@of_s:
        mov     [g_state.rn.fstep],eax
@of_l:
        dpop    dword ptr [g_state.rn.flim]
        mov     bx,[g_state.rn.fid]
        call    for_find
        je      @of_r
        mov     ebx,[g_state.rn.cstk]
        cmp     ebx,CSLIM
        jb      err_os
        sub     ebx,FRSIZE
@of_r:
        mov     [g_state.rn.cstk],ebx
        mov     ax,[g_state.rn.fid]
        mov     [ebx+F_ID],ax
        mov     eax,[g_state.rn.fstep]
        mov     [ebx+F_STEP],eax
        mov     eax,[g_state.rn.flim]
        mov     [ebx+F_LIMIT],eax
        mov     byte ptr [ebx+F_MARK],M_FR
        cmp     [g_state.rn.fop],O_F_FOR*2
        je      @of_k
        cmp     [g_state.rn.fop],O_F_FOR1*2
        jne     @of_m
@of_k:
        call    for_mki
@of_m:
        mov     ebx,[g_state.rn.cstk]
        cmp     byte ptr [esi],O_FSKIP
        jne     @of_p
        call    for_in
        jnc     @of_e
        add     [g_state.rn.cstk],FRSIZE        ; not entered: past its NEXT
        inc     esi
        jmp     ex_jumpto
@of_e:
        add     esi,JSIZE
@of_p:
        ipsave    [ebx].TFRAME.resume
        mov     eax,[g_state.rn.line]
        mov     [ebx].TFRAME.line,eax
        cmp     byte ptr [esi],O_EOL
        jne     @F
        call    ex_eol                  ; no next line: no return, the frame kept
        jmp     @of_m
@@:
        dispatch

; ::::::::::::::
; for_in - is the loop at EBX entered: its start not past its limit, as its
; step's sign has it?
;
; out:    CF  set: not entered
; keeps:  EBX, ESI
; ::::::::::::::
for_in:
        movzx   edi,word ptr [ebx+F_ID]
        mov     cl,fs:[edi*8].TVAR.info
        mov     edi,fs:[edi*8].TVAR.vt        ; its cell
        and     cl,3
        cmp     cl,1
        je      @fin_i
        cmp     word ptr [ebx+F_LIMIT+2],0FFFFh
        je      @fin_c
        fld     dword ptr [edi]
        fcomp   dword ptr [ebx+F_LIMIT]
        fnstsw  ax
        sahf
        mov     cl,byte ptr [ebx+F_STEP+3]
        jb      @fin_lt
        je      @fin_y
        test    cl,cl                   ; past it upward: entered only down
        js      @fin_y
        stc
        ret
@fin_lt:
        test    cl,cl
        jns     @fin_y
        stc
        ret
@fin_c:
        movsx   eax,word ptr [ebx+F_STEP+2]     ; an INT-counted FLOAT loop
        movsx   ecx,word ptr [ebx+F_LIMIT]
        mov     dl,byte ptr [ebx+F_STEP+1]
        jmp     @fin_s
@fin_i:
        mov     eax,[edi]
        mov     ecx,[ebx+F_LIMIT]
        mov     dl,byte ptr [ebx+F_STEP+3]
@fin_s:
        cmp     eax,ecx
        jl      @fin_l2
        je      @fin_y
        test    dl,dl
        js      @fin_y
        stc
        ret
@fin_l2:
        test    dl,dl
        jns     @fin_y
        stc
        ret
@fin_y:
        clc
        ret

; ::::::::::::::
; for_mki - a FLOAT loop whose start, limit and step are whole INTs counts in
; an INT.
;
; in:     EBX  the frame, its values FLOATs
; ::::::::::::::
for_mki:
        mov     ebp,ebx
        mov     eax,[ebp+F_STEP]
        call    flt_w16
        jnz     @fmk_r
        push    eax
        mov     eax,[ebp+F_LIMIT]
        call    flt_w16
        jnz     @fmk_a
        push    eax
        movzx   edi,word ptr [ebp+F_ID]
        mov     edi,fs:[edi*8].TVAR.vt
        mov     eax,[edi]
        mov     [ebp+F_CACHE],eax
        call    flt_w16
        jnz     @fmk_b
        mov     word ptr [ebp+F_STEP+2],ax
        pop     eax
        mov     word ptr [ebp+F_LIMIT],ax
        mov     word ptr [ebp+F_LIMIT+2],0FFFFh
        pop     eax
        mov     word ptr [ebp+F_STEP],ax
        ret
@fmk_b:
        pop     eax
@fmk_a:
        pop     eax
@fmk_r:
        ret

; ::::::::::::::
; op_nextv - NEXT.V id.
;
; error:  ?NEXT WITHOUT FOR  no frame of it
; ::::::::::::::
op_nextv:
        stopchk
        tspclr
        lodsw
        mov     bx,ax
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSTOP
        jae     @onv_f
        cmp     byte ptr [edi+F_MARK],M_FR
        jne     @onv_f
        cmp     ax,[edi+F_ID]
        jne     @onv_f
        mov     ebx,edi
        jmp     for_any
@onv_f:
        call    for_find
        je      for_any

; ::::::::::::::
; err_nf - ?NEXT WITHOUT FOR.
;
; error:  ?NEXT WITHOUT FOR  always
; ::::::::::::::
err_nf:
        mov     al,E_NF
        jmp     err_raise

; ::::::::::::::
; op_nx0 - NEXT, no variable.
;
; error:  ?NEXT WITHOUT FOR  no FOR frame on top
; ::::::::::::::
op_nx0:
        stopchk
        tspclr
        mov     ebx,[g_state.rn.cstk]
        cmp     ebx,CSTOP
        jae     err_nf
        cmp     byte ptr [ebx+F_MARK],M_FR
        jne     err_nf

; ::::::::::::::
; for_any - step the frame at EBX: a % by its INT, else the FLOAT.
;
; in:     EBX  the frame
;         ESI  after the op
; error:  ?ILLEGAL QUANTITY  a % stepped out of range
;         ?OVERFLOW          past the largest FLOAT
; ::::::::::::::
for_any:
        mov     [g_state.rn.cstk],ebx
        movzx   edi,word ptr [ebx+F_ID]
        mov     cl,fs:[edi*8].TVAR.info
        and     cl,3
        mov     ebp,fs:[edi*8].TVAR.vt        ; the variable's cell
        mov     [g_state.rn.fvp],ebp
        cmp     cl,1
        je      @fa_pct
        cmp     word ptr [ebx+F_LIMIT+2],0FFFFh
        jne     @fa_gen
        mov     eax,[ebp]
        cmp     eax,[ebx+F_CACHE]
        jne     @fa_deg
        movsx   eax,word ptr [ebx+F_STEP+2]
        movsx   ecx,word ptr [ebx+F_STEP]
        add     eax,ecx
        mov     word ptr [ebx+F_STEP+2],ax
        mov     [g_state.rn.fsv],eax
        push    esi
        call    flt_from32
        pop     esi
        mov     ebx,[g_state.rn.cstk]
        mov     edi,[g_state.rn.fvp]
        mov     [edi],eax
        mov     [ebx+F_CACHE],eax
        mov     eax,[g_state.rn.fsv]
        movsx   ecx,word ptr [ebx+F_LIMIT]
        test    byte ptr [ebx+F_STEP+1],80h
        jnz     @fa_id
        cmp     eax,ecx
        jle     @fa_lp
        jmp     @fa_done
@fa_id:
        cmp     eax,ecx
        jge     @fa_lp
        jmp     @fa_done
@fa_deg:
        push    esi
        mov     ax,word ptr [ebx+F_LIMIT]        ; the body wrote it: FLOAT from here on
        call    flt_from16
        mov     ebx,[g_state.rn.cstk]
        mov     [ebx+F_LIMIT],eax
        mov     ax,word ptr [ebx+F_STEP]
        call    flt_from16
        pop     esi
        mov     ebx,[g_state.rn.cstk]
        mov     [ebx+F_STEP],eax
        mov     ebp,[g_state.rn.fvp]
        jmp     @fa_gen
@fa_pct:
        mov     eax,[ebp]
        add     eax,[ebx+F_STEP]
        jo      err_iq
        mov     [ebp],eax
        test    byte ptr [ebx+F_STEP+3],80h
        jnz     @fa_dn
        cmp     eax,[ebx+F_LIMIT]
        jle     @fa_lp
@fa_done:
        add     [g_state.rn.cstk],FRSIZE
        dispatch
@fa_dn:
        cmp     eax,[ebx+F_LIMIT]
        jge     @fa_lp
        jmp     @fa_done
@fa_lp:
        mov     edx,[ebx].TFRAME.line
        mov     [g_state.rn.line],edx
        mov     eax,[ebx].TFRAME.resume
        ipload
        dispatch
@fa_gen:
        push    esi
        mov     eax,[ebp]
        mov     edx,[ebx+F_STEP]
        call    flt_add
        mov     ebx,[g_state.rn.fvp]
        mov     [ebx],eax
        mov     ebx,[g_state.rn.cstk]
        mov     edx,[ebx+F_LIMIT]
        call    flt_cmp                 ; 1 past above, FFh below
        pop     esi
        mov     ebx,[g_state.rn.cstk]
        mov     cl,1
        test    byte ptr [ebx+F_STEP+3],80h
        jz      @F
        mov     cl,0FFh
@@:
        cmp     al,cl
        je      @fa_done
        jmp     @fa_lp

; ::::::::::::::
; for_lp - a loop goes on: its frame's line and ip.
;
; in:     EBX  the frame
; ::::::::::::::
for_lp:
        mov     edx,[ebx].TFRAME.line
        mov     [g_state.rn.line],edx
        mov     eax,[ebx].TFRAME.resume
        ipload
        dispatch

; ::::::::::::::
; op_err - ERR code.
;
; error:  that one  always
; ::::::::::::::
op_err:
        mov     al,[esi]
        jmp     err_raise

; ::::::::::::::
; op_onerr - ONERR num addr: ON ERROR GOTO's line; 0 none. GOTO 0 in the
; handler raises the error it handles.
;
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
op_onerr:
        cmp     word ptr [esi],0
        jne     @one_g
        add     esi,sizeof TLREF
        and     [g_state.rn.onerr],0
        cmp     [g_state.rn.inerr],0
        je      @one_d
        mov     ax,[g_state.rn.errn]
        mov     [g_state.rn.uerr],ax
        mov     al,E_USR
        jmp     err_raise
@one_g:
        call    ex_target
        mov     [g_state.rn.onerr],ebx
@one_d:
        dispatch

; ::::::::::::::
; op_onlerr - ONLERR num addr: ON LOCAL ERROR GOTO's line, the running
; procedure's, 0 none; at the module's level ON ERROR's.
;
; error:  Label not defined  no such line
; ::::::::::::::
op_onlerr:
        mov     edi,[g_state.rn.cstk]
@onl_f:
        cmp     edi,CSTOP
        jae     op_onerr
        movzx   eax,byte ptr [edi+F_MARK]
        cmp     al,M_PC
        je      @onl_p
        add     edi,eax
        jmp     @onl_f
@onl_p:
        xor     ebx,ebx
        cmp     word ptr [esi],0
        jne     @F
        add     esi,sizeof TLREF
        jmp     @onl_s
@@:
        call    ex_target
@onl_s:
        mov     [edi].TPROC.onerr,ebx
        dispatch

; ::::::::::::::
; ex_hnd - the handler of an error: the innermost ON LOCAL ERROR's, the
; procedures its procedure called dropped, the error then at its call;
; else ON ERROR's.
;
; out:    thnd     the handler's line, 0 none
;         ln, opip the error's place
; ::::::::::::::
ex_hnd:
        mov     edi,[g_state.rn.cstk]
@ehd_f:
        cmp     edi,CSTOP
        jae     @ehd_m
        movzx   eax,byte ptr [edi+F_MARK]
        cmp     al,M_PC
        jne     @ehd_n
        cmp     [edi].TPROC.onerr,0
        jne     @ehd_y
@ehd_n:
        add     edi,eax
        jmp     @ehd_f
@ehd_m:
        mov     eax,[g_state.rn.onerr]
        mov     [g_thnd],eax
        ret
@ehd_y:
        mov     eax,[edi].TPROC.onerr
        mov     [g_thnd],eax
        mov     [g_hfr],edi
@ehd_u:
        mov     ebx,[g_state.rn.cstk]   ; the innermost procedure's frame
@@:
        movzx   eax,byte ptr [ebx+F_MARK]
        cmp     al,M_PC
        je      @F
        add     ebx,eax
        jmp     @B
@@:
        cmp     ebx,[g_hfr]
        je      @ehd_r
        call    pr_pop
        lea     eax,[ebx+M_PC]
        mov     [g_state.rn.cstk],eax
        mov     edx,[ebx].TFRAME.line           ; the error: at the call
        mov     [g_state.rn.line],edx
        mov     eax,[ebx].TFRAME.resume
        ipload
        dec     esi
        mov     [g_state.rn.opip],esi
        jmp     @ehd_u
@ehd_r:
        ret

        .data?
g_thnd  dd      ?                       ; the error's handler's line
g_hfr   dd      ?                       ; its ON LOCAL ERROR's frame
        .code

; ::::::::::::::
; ex_trap - an error ON ERROR handles: ERR, ERL, where RESUME goes on, then
; the handler on the failing statement's data stack, its frames kept.
;
; in:     AL     the error, E_*
;         ln     the failing line, opip the op
; ::::::::::::::
ex_trap:
        movzx   edx,al
        movzx   eax,[g_qberr+edx]
        cmp     dl,E_USR
        jne     @F
        mov     ax,[g_state.rn.uerr]
@@:
        mov     [g_state.rn.errn],ax
        mov     [g_state.rn.inerr],1
        mov     ecx,[g_state.rn.line]
        mov     [g_state.rn.resln],ecx
        call    ex_erl
        mov     [g_state.rn.erl],dx
        mov     ebx,ecx                 ; the statement, by the map
        mov     eax,[g_state.rn.opip]
        call    ex_stmtof
        mov     eax,[edi].TSTMT.code
        mov     [g_state.rn.resat],eax
        or      eax,-1
        cmp     ecx,1
        jbe     @F
        mov     eax,[edi+sizeof TSTMT].TSTMT.code
@@:
        mov     [g_state.rn.resnext],eax
        call    ch_clr
        call    ex_sbase
        mov     esp,eax
        mov     ebx,[g_thnd]
        jmp     ex_jump

; ::::::::::::::
; ex_erl - ERL: the last line number at or before line ECX, else 0.
;
; out:    EDX  it
; keeps:  ECX, ESI, EDI
; ::::::::::::::
ex_erl:
        push    ebx
        mov     ebx,PROGBASE
        xor     edx,edx
@etr_l:
        cmp     ebx,ecx
        ja      @etr_d
        cmp     gs:[ebx].TLINE.len,0
        je      @etr_d
        movzx   eax,gs:[ebx].TLINE.num
        cmp     ax,NONUM
        je      @F
        mov     edx,eax
@@:
        mov     eax,gs:[ebx].TLINE.len
        add     ebx,eax
        jmp     @etr_l
@etr_d:
        pop     ebx
        ret

; ::::::::::::::
; ex_stmtof - the map entry of the statement holding code address EAX of
; line EBX: the last whose code is at EAX or before, else the first. An
; entry: a TSTMT; the line's EOL's last.
;
; out:    EDI  the entry
;         ECX  the entries from it on
; ::::::::::::::
ex_stmtof:
        mov     ebx,gs:[ebx].TLINE.code
        add     ebx,[g_state.mm.cachebase]
        sub     eax,ebx
        mov     ecx,[ebx-4]             ; the count, after the entries
        lea     edi,[ecx*8+4]
        neg     edi
        add     edi,ebx
@esm_l:
        cmp     ecx,1
        jbe     @esm_r
        mov     edx,[edi+sizeof TSTMT].TSTMT.code
        cmp     edx,eax
        ja      @esm_r
        add     edi,sizeof TSTMT
        dec     ecx
        jmp     @esm_l
@esm_r:
        ret

; ::::::::::::::
; ex_sbase - the data stack at the running procedure's body's start, or
; the module's.
;
; out:    EAX  it
; ::::::::::::::
ex_sbase:
        mov     eax,[g_state.rn.rsp]
        mov     ebx,[g_state.rn.cstk]
@esb_l:
        cmp     ebx,CSTOP
        jae     @esb_r
        movzx   ecx,byte ptr [ebx+F_MARK]
        cmp     cl,M_PC
        je      @esb_f
        add     ebx,ecx
        jmp     @esb_l
@esb_f:
        mov     eax,[ebx+F_SB]
@esb_r:
        ret

; ::::::::::::::
; op_resume - RESUME mode num addr: mode 0 the failing statement again, 1
; the one after it, 2 the line num addr.
;
; error:  RESUME without error (20)  not in a handler
; ::::::::::::::
op_resume:
        stopchk
        cmp     [g_state.rn.inerr],0
        je      @rsm_e
        and     [g_state.rn.inerr],0
        and     [g_state.rn.errn],0
        call    ex_sbase
        mov     esp,eax
        lodsb
        cmp     al,2
        je      @rsm_l
        mov     ecx,[g_state.rn.resat]
        test    al,al
        jz      @F
        mov     ecx,[g_state.rn.resnext]
@@:
        tspclr
        mov     edx,[g_state.rn.resln]
        mov     [g_state.rn.line],edx
        mov     [g_state.rn.runln],edx
        cmp     ecx,-1
        je      @rsm_n
        mov     esi,ecx
        call    ex_resolve
        dispatch
@rsm_n:
        call    ex_eol
        dispatch
@rsm_l:
        call    ex_target
        jmp     ex_jump
@rsm_e:
        mov     [g_state.rn.uerr],20
        mov     al,E_USR
        jmp     err_raise

; ::::::::::::::
; op_size - the size of the op at ESI.
;
; in:     ESI  at it
; out:    ECX  its size
; keeps:  all but ECX
; ::::::::::::::
op_size:
        push    ebx
        movzx   ebx,byte ptr [esi]
        movzx   ecx,[g_opsz+ebx]
        jecxz   @oln_v
        pop     ebx
        ret
@oln_v:
        movzx   ecx,byte ptr [esi+1]
        cmp     bl,O_ON
        je      @oln_on
        add     ecx,2                   ; S.LD.TXT
        pop     ebx
        ret
@oln_on:
        and     cl,7Fh
        imul    ecx,sizeof TLREF
        add     ecx,2
        pop     ebx
        ret

; ::::::::::::::
; ex_resolve - a frame's ip kept as an offset: the address in its line's code.
;
; in:     DX   the line
;         ESI  the offset
; out:    ESI  the address
; error:  ?OUT OF MEMORY  the code does not fit
; ::::::::::::::
ex_resolve:
        movzx   ebx,dx
        mov     eax,esi
        mov     esi,gs:[ebx].TLINE.code
        test    esi,esi
        jnz     @exr_a
        push    eax
        call    cm_cache
        pop     eax
        sub     esi,[g_state.mm.cachebase]
@exr_a:
        lea     esi,[esi+eax]
        add     esi,[g_state.mm.cachebase]
        ret

; ::::::::::::::
; ex_setcont - keep where CONT goes on, not for the direct line.
;
; in:     ln, ESI  the place
; out:    contip, contsi  the line, the code
; keeps:  ESI
; ::::::::::::::
ex_setcont:
        mov     ebx,[g_state.rn.line]
        cmp     ebx,DLINE
        je      @F
        mov     [g_state.rn.contln],ebx
        ipsave    [g_state.rn.contcode]
@@:
        ret

; ::::::::::::::
; ex_end - the run is over: both stacks empty, READY.
; ::::::::::::::
ex_end:
        xor     al,al
        cmp     [g_dbev],0              ; the probe's line ended
        jne     dbg_evx
        call    ch_clall
        mov     esp,[g_state.rn.rsp]
        mov     [g_state.rn.cstk],CSTOP
        jmp     rpl_ready

; ::::::::::::::
; ex_nodir - ?ILLEGAL DIRECT in the direct line.
;
; in:     ln  the running line
; ::::::::::::::
ex_nodir:
        cmp     [g_state.rn.line],DLINE
        je      err_id
        ret

; ::::::::::::::
; op_jmp - JMP target: a block's jump (comp/block.asm).
; ::::::::::::::
op_jmp:
        jmp     ex_jumpto
; ::::::::::::::
; op_ijf - I.JF target ( i -- ): jump when 0.
; ::::::::::::::
op_ijf:
        dpop    eax
        test    eax,eax
        jz      ex_jumpto
@ojf_n:
        add     esi,JSIZE-1
        dispatch
; ::::::::::::::
; op_fjf - F.JF target ( f -- ): jump when 0.
; ::::::::::::::
op_fjf:
        dpop    eax
        test    eax,7FFFFFFFh
        jz      ex_jumpto
        jmp     @ojf_n
; ::::::::::::::
; op_ijt - I.JT target ( i -- ): jump unless 0.
; ::::::::::::::
op_ijt:
        dpop    eax
        test    eax,eax
        jnz     ex_jumpto
        jmp     @ojf_n
; ::::::::::::::
; op_fjt - F.JT target ( f -- ): jump unless 0.
; ::::::::::::::
op_fjt:
        dpop    eax
        test    eax,7FFFFFFFh
        jnz     ex_jumpto
        jmp     @ojf_n

; ::::::::::::::
; op_fpop - FPOP: EXIT FOR's, the innermost FOR's frame dropped.
; ::::::::::::::
op_fpop:
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSTOP
        jae     @ofp_r
        cmp     byte ptr [edi+F_MARK],M_FR
        jne     @ofp_r
        add     edi,M_FR
        mov     [g_state.rn.cstk],edi
@ofp_r:
        dispatch

; ::::::::::::::
; ex_jumpto - go to the target at ESI: its address once found, else the
; target line's code (compiled when it has none), its statement by the map
; before that code, the mode's place in it. The address is kept in the op
; unless the cache was flushed meanwhile, the op with it.
;
; in:     ESI  the target, a TJUMP
; error:  ?SYNTAX ERROR  a target the link gave none (line -1)
; ::::::::::::::
ex_jumpto:
        stopchk
        tspclr
        mov     eax,[esi].TJUMP.dest
        cmp     eax,3
        jb      @ebj_f
        mov     ebx,[esi].TJUMP.line
        mov     [g_state.rn.line],ebx
        mov     esi,[g_state.mm.cachebase]
        add     esi,eax
        dispatch
@ebj_f:
        mov     edx,esi
        mov     ebx,[edx].TJUMP.line
        cmp     ebx,-1
        je      err_sn
        mov     al,[g_state.lnk.nfl]
        push    eax
        push    edx
        mov     [g_state.rn.runln],ebx
        call    ex_line
        pop     edx
        pop     eax
        mov     ecx,[esi-4]             ; the map: n TSMs, then n:4
        lea     edi,[ecx*8+4]
        neg     edi
        add     edi,esi
        mov     ebx,[edx].TJUMP.body
@ebj_l:
        jecxz   @ebj_n
        cmp     [edi].TSTMT.body,ebx
        je      @ebj_y
        add     edi,sizeof TSTMT
        dec     ecx
        jmp     @ebj_l
@ebj_y:
        mov     ebx,[edx].TJUMP.dest      ; the mode
        cmp     bl,1
        mov     ebx,[edi].TSTMT.code
        jb      @ebj_t
        je      @ebj_1
        cmp     ecx,1                   ; mode 2: the next statement's
        jbe     @ebj_n
        mov     ebx,[edi+sizeof TSTMT].TSTMT.code
        jmp     @ebj_t
@ebj_1:
        add     ebx,JSIZE               ; mode 1: past its JMP
@ebj_t:
        add     esi,ebx
        cmp     al,[g_state.lnk.nfl]
        jne     @ebj_d
        mov     eax,esi
        sub     eax,[g_state.mm.cachebase]
        mov     [edx].TJUMP.dest,eax
@ebj_d:
        dispatch
@ebj_n:
        jmp     err_sn

; ::::::::::::::
; op_pcall - PCALL proc target addr ( (value ref)*n -- ): a procedure's
; frame. Its arguments go to save entries (a string copied unless a
; temporary's), its locals are saved there and zeroed, then the entries to
; its parameters; on at its body. A value: an INT, a FLOAT, a string's Desc,
; an array's id. A ref: FFFFh none; an array's id, PRET's store back; else
; a cell: the scalar parameter is it, vt its address, while the procedure
; runs, its value none. An entry: id:2, ref:2 (FFFEh a scalar's), the
; value:8, a scalar's cell before:4.
;
; error:  Out of stack space  a stack is full
; ::::::::::::::
op_pcall:
        cmp     esp,[g_state.rn.stklim]
        jb      @st_flow1
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSLIM
        jb      @st_flow1
        mov     [g_state.rn.ppip],esi
        movzx   ebp,byte ptr [esi]
        shl     ebp,5
        add     ebp,offset g_prc
        movzx   ecx,word ptr [ebp+P_NPAR]
        mov     edi,[g_state.rn.lsp]
        mov     eax,ecx
        shl     eax,4                   ; LSSZ
        add     edi,eax
        cmp     edi,offset g_lsav+LSAVSZ-LSSZ
        jae     @st_flow1
        mov     [g_state.rn.ppend],edi
        test    ecx,ecx
        jz      @pca_s
@pca_a:                                 ; the arguments, the last first
        sub     edi,LSSZ
        movzx   ebx,word ptr [ebp+P_PAR]
        add     ebx,ecx
        movzx   ebx,word ptr [g_ppar+ebx*2-2]
        mov     [edi],bx
        dpop    eax                     ; the ref
        mov     [edi+2],ax
        mov     dl,fs:[ebx*8].TVAR.info
        test    dl,dl
        js      @pca_r
        cmp     eax,0FFFFh
        je      @pca_r
        mov     word ptr [edi+2],0      ; a cell: the parameter's, no value
        mov     [edi+12],eax
        add     esp,8
        jmp     @pca_c
@pca_r:
        pop     eax
        pop     dword ptr [edi+8]       ; a DOUBLE's high half
        test    dl,dl
        js      @pca_y
        and     dl,3
        cmp     dl,2
        je      @pca_t
@pca_v:
        mov     [edi+4],eax
@pca_c:
        dec     ecx
        jnz     @pca_a
        jmp     @pca_s
@pca_y:
        cmp     dl,81h                  ; a record: made if not yet
        jne     @F
        push    ebx
        push    ecx
        push    edi
        push    ebp
        mov     ebx,eax
        call    rec_get
        mov     eax,edi
        pop     ebp
        pop     edi
        pop     ecx
        pop     ebx
        jmp     @pca_v
@@:
        mov     eax,fs:[eax*8].TVAR.vt        ; an array: its header
        jmp     @pca_v
@pca_t:
        push    ecx
        push    edi
        push    ebp
        lea     ebx,[edi+4]
        mov     [g_state.sg.bpo],ebx
        call    str_set
        pop     ebp
        pop     edi
        pop     ecx
        dec     ecx
        jnz     @pca_a
@pca_s:
        mov     edx,[g_state.rn.ppend]   ; the locals, saved and zeroed
        mov     esi,[g_state.rn.ppip]
        movzx   eax,byte ptr [esi]
        movzx   ebx,word ptr [g_plh+eax*2]
@pcl_l:
        test    ebx,ebx
        jz      @pcl_d
        cmp     edx,offset g_lsav+LSAVSZ-LSSZ
        jae     @st_flow1
        mov     [edx],bx
        mov     cl,fs:[ebx*8].TVAR.info
        test    cl,cl
        js      @pcl_y
        test    cl,SK_DBL
        jnz     @pcl_q
        and     cl,3
        cmp     cl,2
        je      @pcl_t
        call    var_cell
        xor     eax,eax
        xchg    eax,[ecx]
        mov     [edx+4],eax
        jmp     @pcl_n
@pcl_q:
        call    var_cell
        xor     eax,eax
        xchg    eax,[ecx]
        mov     [edx+4],eax
        xor     eax,eax
        xchg    eax,[ecx+4]
        mov     [edx+8],eax
        jmp     @pcl_n
@pcl_y:
        xor     eax,eax
        xchg    eax,fs:[ebx*8].TVAR.vt
        mov     [edx+4],eax
        jmp     @pcl_n
@pcl_t:
        call    var_cell
        mov     eax,ecx
        lea     edi,[edx+4]
        call    ls_mov
        and     word ptr [eax+D_LEN],0
@pcl_n:
        add     edx,LSSZ
        vxat    ecx
        movzx   ebx,[ecx+ebx*8].TVX.nloc
        jmp     @pcl_l
@pcl_d:
        mov     edi,[g_state.rn.lsp]    ; the arguments to the parameters
@pcb_l:
        cmp     edi,[g_state.rn.ppend]
        jae     @pcb_d
        movzx   ebx,word ptr [edi]
        test    fs:[ebx*8].TVAR.info,80h
        jnz     @pcb_a
        call    var_cell                ; a scalar: vt its cell, the old one
        cmp     word ptr [edi+2],0      ; kept
        jne     @F
        mov     ecx,[edi+12]
@@:
        xchg    ecx,fs:[ebx*8].TVAR.vt
        mov     [edi+12],ecx
        cmp     word ptr [edi+2],0
        mov     word ptr [edi+2],0FFFEh
        je      @pcb_n                  ; the argument's: no value
@pcb_a:
        mov     eax,[edi+4]
        mov     cl,fs:[ebx*8].TVAR.info
        test    cl,cl
        js      @pcb_y
        test    cl,SK_DBL
        jz      @F
        call    var_cell
        mov     [ecx],eax
        mov     eax,[edi+8]
        mov     [ecx+4],eax
        jmp     @pcb_n
@@:
        and     cl,3
        cmp     cl,2
        je      @pcb_t
        call    var_cell
        mov     [ecx],eax
        jmp     @pcb_n
@pcb_y:
        mov     fs:[ebx*8].TVAR.vt,eax
        movzx   eax,word ptr [edi+2]    ; an array's: dynamic as its ref
        cmp     ax,0FFFFh
        je      @pcb_n
        vxat    ecx
        mov     al,[ecx+eax*8].TVX.dyn
        mov     [ecx+ebx*8].TVX.dyn,al
        jmp     @pcb_n
@pcb_t:
        push    edi
        lea     eax,[edi+4]
        call    var_cell
        mov     edi,ecx
        call    ls_mov
        pop     edi
@pcb_n:
        add     edi,LSSZ
        jmp     @pcb_l
@pcb_d:
        mov     edi,[g_state.rn.cstk]   ; the frame
        sub     edi,M_PC
        mov     [g_state.rn.cstk],edi
        mov     byte ptr [edi+F_MARK],M_PC
        mov     ecx,[g_state.rn.line]
        mov     [edi].TFRAME.line,ecx
        mov     al,[esi]
        mov     [edi+F_PRC],al
        lea     esi,[esi+1+sizeof TJUMP]
        ipsave    [edi].TFRAME.resume
        mov     eax,[g_state.rn.lsp]
        mov     [edi+F_LSP0],eax
        mov     eax,[g_state.rn.ppend]
        mov     [edi+F_LSP1],eax
        mov     [g_state.rn.lsp],edx
        mov     ax,[g_state.sg.tspb]
        mov     [edi+F_TSPB],ax
        mov     ax,[g_state.sg.tsp]
        mov     [g_state.sg.tspb],ax
        mov     [edi+F_SB],esp
        mov     [edi].TPROC.onerr,0
        sub     esi,sizeof TJUMP          ; the target
        jmp     ex_jumpto

; ::::::::::::::
; op_pret - PRET: back from the procedure: its frame and the frames above
; it go; the scalar parameters' cells back, an array's to its ref after
; the locals are restored; a FUNCTION's result is pushed.
;
; error:  ?RETURN WITHOUT GOSUB  no procedure's frame
; ::::::::::::::
op_pret:
        stopchk
        call    pr_pop
        jmp     @prt_x2

; ::::::::::::::
; pr_pop - the innermost procedure's frame, and those above it, dropped as
; its return does: its parameters' cells back, the caller's locals back,
; a FUNCTION's result kept (pres, pres8, lres).
;
; out:    EBX  the frame, cstk at it
;         EBP  its procedure
; error:  RETURN without GOSUB  no frame
; ::::::::::::::
pr_pop:
        mov     ebx,[g_state.rn.cstk]
@prt_f:
        cmp     ebx,CSTOP
        jae     err_rg
        movzx   eax,byte ptr [ebx+F_MARK]
        cmp     al,M_PC
        je      @prt_g
        add     ebx,eax
        jmp     @prt_f
@prt_g:
        mov     [g_state.rn.cstk],ebx
        mov     edx,[ebx+F_LSP0]        ; the parameters' values
@prt_p:
        cmp     edx,[ebx+F_LSP1]
        jae     @prt_r
        cmp     word ptr [edx+2],0FFFFh
        je      @prt_pn
        movzx   ecx,word ptr [edx]
        test    fs:[ecx*8].TVAR.info,80h
        jz      @prt_pn
@prt_pa:                                ; an array: its header, dynamic
        mov     eax,fs:[ecx*8].TVAR.vt
        mov     [edx+4],eax
        vxat    eax
        mov     al,[eax+ecx*8].TVX.dyn
        mov     [edx+8],al
@prt_pn:
        add     edx,LSSZ
        jmp     @prt_p
@prt_r:
        movzx   ebp,byte ptr [ebx+F_PRC]        ; a FUNCTION's result
        shl     ebp,5
        add     ebp,offset g_prc
        cmp     byte ptr [ebp+P_KIND],PK_FUNC
        jne     @prt_l
        movzx   ecx,word ptr [ebp+P_RES]
        mov     al,fs:[ecx*8].TVAR.info
        test    al,SK_DBL
        cellof  edi,ecx
        jz      @F
        mov     eax,[edi]
        mov     dword ptr [g_pres8],eax
        mov     eax,[edi+4]
        mov     dword ptr [g_pres8+4],eax
        jmp     @prt_l
@@:
        and     al,3
        cmp     al,2
        je      @prt_rs
        mov     eax,[edi]
        mov     [g_state.rn.pres],eax
        jmp     @prt_l
@prt_rs:
        mov     eax,edi
        mov     edi,offset g_lres
        call    ls_mov
@prt_l:
        mov     edx,[g_state.rn.lsp]    ; the locals back
@prt_ll:
        cmp     edx,[ebx+F_LSP1]
        jbe     @prt_o
        sub     edx,LSSZ
        movzx   ecx,word ptr [edx]
        mov     al,fs:[ecx*8].TVAR.info
        test    al,al
        js      @prt_ly
        cellof  edi,ecx
        test    al,SK_DBL
        jz      @F
        mov     eax,[edx+4]
        mov     [edi],eax
        mov     eax,[edx+8]
        mov     [edi+4],eax
        jmp     @prt_ll
@@:
        and     al,3
        cmp     al,2
        je      @prt_ls
        mov     eax,[edx+4]
        mov     [edi],eax
        jmp     @prt_ll
@prt_ly:
        cmp     fs:[ecx*8].TVAR.vt,0
        je      @prt_lz
        movzx   eax,word ptr [ebp+P_NPAR]       ; a parameter's is the caller's
        movzx   edi,word ptr [ebp+P_PAR]
        add     eax,edi
@@:
        dec     eax
        cmp     eax,edi
        jl      @F
        cmp     cx,[g_ppar+eax*2]
        je      @prt_lz
        jmp     @B
@@:
        push    ebx
        mov     ebx,ecx
        and     [g_afrep],0
        call    arr_free
        pop     ebx
@prt_lz:
        mov     eax,[edx+4]
        mov     fs:[ecx*8].TVAR.vt,eax
        jmp     @prt_ll
@prt_ls:
        lea     eax,[edx+4]
        call    ls_mov
        jmp     @prt_ll
@prt_o:
        mov     edx,[ebx+F_LSP0]        ; the cells back, arrays to refs
@prt_ol:
        cmp     edx,[ebx+F_LSP1]
        jae     @prt_x
        movzx   ecx,word ptr [edx+2]
        cmp     cx,0FFFFh
        je      @prt_on
        movzx   eax,word ptr [edx]
        cmp     cx,0FFFEh               ; a scalar: its old cell back
        jne     @prt_ob
        mov     ecx,[edx+12]
        mov     fs:[eax*8].TVAR.vt,ecx
        jmp     @prt_on
@prt_ob:                                ; an array: the one it ends as
        mov     eax,[edx+4]
        mov     fs:[ecx*8].TVAR.vt,eax
        vxat    eax
        push    edx
        mov     dl,[edx+8]
        mov     [eax+ecx*8].TVX.dyn,dl
        pop     edx
@prt_on:
        add     edx,LSSZ
        jmp     @prt_ol
@prt_x:
        mov     eax,[ebx+F_LSP0]
        mov     [g_state.rn.lsp],eax
        mov     ax,[g_state.sg.tspb]    ; the callee's temporaries go
        mov     [g_state.sg.tsp],ax
        mov     ax,[ebx+F_TSPB]
        mov     [g_state.sg.tspb],ax
        ret
@prt_x2:
        cmp     byte ptr [ebp+P_KIND],PK_FUNC
        jne     @prt_j
        movzx   ecx,word ptr [ebp+P_RES]
        mov     al,fs:[ecx*8].TVAR.info
        test    al,SK_DBL
        jz      @F
        push    dword ptr [g_pres8+4]
        push    dword ptr [g_pres8]
        jmp     @prt_j
@@:
        and     al,3
        cmp     al,2
        je      @prt_js
        dpush   [g_state.rn.pres]
        jmp     @prt_j
@prt_js:
        movzx   edi,[g_state.sg.tsp]    ; a string: a new temporary
        cmp     edi,DSZ*NTMP
        jae     err_st
        add     [g_state.sg.tsp],DSZ
        add     edi,[g_state.sg.tmplin]
        mov     eax,offset g_lres
        call    ls_mov
        dpush   edi
@prt_j:
        mov     edx,[ebx].TFRAME.line
        mov     [g_state.rn.line],edx
        mov     eax,[ebx].TFRAME.resume
        add     ebx,M_PC
        mov     [g_state.rn.cstk],ebx
        ipload
        dispatch

        .data?
g_pres8 dq      ?                       ; a DOUBLE FUNCTION's result
        .code

; ::::::::::::::
; ls_mov - the Desc at EAX to the Desc at EDI, which then owns its heap
; block.
;
; keeps:  all
; ::::::::::::::
ls_mov:
        push    ebx
        push    ecx
        mov     ebx,[eax+D_PTR]
        mov     [edi+D_PTR],ebx
        movzx   ecx,word ptr [eax+D_LEN]
        mov     [edi+D_LEN],cx
        jecxz   @lsm_r
        cmp     ebx,[g_state.mm.sfr]
        jb      @lsm_r
        cmp     ebx,[g_state.mm.htop]
        jae     @lsm_r
        mov     [ebx+ecx+B_OWN],edi
@lsm_r:
        pop     ecx
        pop     ebx
        ret

        .data?
g_lsav  db      LSAVSZ dup(?)           ; the save stack: LSSZ entries
g_lres  db      8 dup(?)                ; PRET: a FUNCTION's string result
        .code

; --------------------------------------------------------------------------

        end
