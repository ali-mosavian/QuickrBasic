; ==========================================================================
; flow.asm - the executor: lines, jumps, FN, FOR and NEXT
; ==========================================================================
; holds:    the ops that move SI or the control stack; ex_line, ex_end
; uses:     prg_findw, cm_line, cm_link, flt_add, err_*
; data:     The control stack holds frames from cstk up to CSTOP, the mark the
;           size: GOSUB mark ln:2 ip:2; FOR mark ln:2 ip:2 id step:4 limit:4
;           cache:4; FN mark ip:2 id old:4; PCALL mark ln:2 ip:2 proc
;           lsp0:4 lsp1:4 tspb:2.
; calls:
;   ex_jump            ex_line: a stored line, or cm_line and ex_dcode
;   op_nx0, op_nextv
;     for_any          a % inline, else flt_add
; ==========================================================================

include x86f.inc
        .code

; DST, a word = ESI's stream offset. Uses EAX.
ipst    macro   dst
        mov     eax,esi
        sub     eax,offset g_cachb
        mov     dst,ax
        endm

; ::::::::::::::
; op_fndef - FN.DEF id len:2: the slot holds the DEF's line and the body's
; offset in its code; the body is skipped.
; ::::::::::::::
op_fndef:
        vslot
        movzx   eax,word ptr [esi]
        add     esi,2
        mov     cx,[g_state.rn.ln]
        mov     [VARBASE+ebx],cx
        movzx   edi,cx
        movzx   edx,word ptr [g_progb+edi+L_CP]
        add     edx,offset g_cachb
        neg     edx
        add     edx,esi
        mov     [VARBASE+ebx+2],dx
        add     esi,eax
        jmp     ex_lp
@st_flow1:
        jmp     err_om

; ::::::::::::::
; op_fncall - FN.CALL id ( f -- f ): an FN frame, then the body.
;
; error:  ?UNDEF'D FUNCTION  no DEF ran
;         ?OUT OF MEMORY     a stack is full
; ::::::::::::::
op_fncall:
        cmp     esp,[g_state.rn.stklim]
        jb      @st_flow1
        vslot
        mov     dx,[VARBASE+ebx]
        test    dx,dx
        jnz     @fnc_d
        mov     al,E_UF
        jmp     err_raise
@fnc_d:
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSLIM
        jb      @st_flow1
        sub     edi,M_FN
        mov     [g_state.rn.cstk],edi
        mov     byte ptr [edi],M_FN
        ipst    [edi+F_IP]
        mov     cx,[g_state.rn.ln]
        mov     [edi+F_LN],cx
        movzx   esi,word ptr [VARBASE+ebx+2]
        call    ex_resolve
        dispatch

; ::::::::::::::
; op_fnent - FN.ENTER id ( f -- ): the parameter's value kept, the argument in.
; ::::::::::::::
op_fnent:
        vslot
        mov     edi,[g_state.rn.cstk]
        mov     eax,[VARBASE+ebx]
        mov     [edi+F_OLD],eax
        dpop    eax
        mov     ecx,ebx
        shr     ecx,2
        test    [g_sinfo+ecx],1
        jz      @fne_f
        push    ebx
        push    esi
        call    flt_fix32
        pop     esi
        pop     ebx
@fne_f:
        mov     [VARBASE+ebx],eax
        dispatch

; ::::::::::::::
; op_fnret - FN.RET id ( f -- f ): the parameter back, on after FN.CALL.
; ::::::::::::::
op_fnret:
        vslot
        mov     edi,[g_state.rn.cstk]
        mov     eax,[edi+F_OLD]
        mov     [VARBASE+ebx],eax
        mov     dx,[edi+F_LN]
        add     edi,M_FN
        mov     [g_state.rn.cstk],edi
        mov     ax,[edi-M_FN+F_IP]
        ipfix
        dispatch

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
; ex_target - num addr at ESI: the line.
;
; out:    EBX  the line
;         ESI  past them
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
ex_target:
        lodsw
        mov     [g_state.rn.lnum],ax
        lodsw
        movzx   ebx,ax
        test    ebx,ebx
        jnz     @etg_r
        call    prg_findw
        jnc     err_us
        mov     [esi-2],bx           ; found once, read after
@etg_r:
        ret

; ::::::::::::::
; ex_gosub - a GOSUB frame, then the line at EBX.
;
; in:     EBX      the line
;         ln, ESI  where RETURN goes on
; error:  ?OUT OF MEMORY  the control stack is full
; ::::::::::::::
ex_gosub:
        mov     edi,[g_state.rn.cstk]
        cmp     edi,CSLIM
        jb      err_om
        sub     edi,M_GS
        mov     [g_state.rn.cstk],edi
        mov     byte ptr [edi+F_MARK],M_GS
        mov     cx,[g_state.rn.ln]
        mov     [edi+F_LN],cx
        ipst    [edi+F_IP]

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
        mov     [g_state.lnk.lc],bx
        mov     [g_state.rn.vpt],bx
        call    ex_line
        jmp     ex_lp

; ::::::::::::::
; op_on - ON kn (num addr)*n ( k -- ): kn bit 7 GOSUB; past the list for 0
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
        lea     edi,[esi+ecx*4]         ; past the list
        dec     dl                      ; k 0: 255, past n < 128
        cmp     dl,al
        jae     @on_past
        movzx   ebx,dl
        lea     ecx,[esi+ebx*4+2]
        mov     ax,[esi+ebx*4]
        mov     [g_state.rn.lnum],ax
        movzx   ebx,word ptr [ecx]
        mov     esi,edi
        test    ebx,ebx
        jnz     @on_g
        call    prg_findw
        jnc     err_us
        mov     [ecx],bx
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
        jne     err_rg
        mov     dx,[ebx+F_LN]
        mov     [g_state.rn.ln],dx
        mov     ax,[ebx+F_IP]
        add     ebx,M_GS
        mov     [g_state.rn.cstk],ebx
        tspclr
        ipfix
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
        movzx   ebx,[g_state.rn.ln]
        movzx   ecx,byte ptr [g_progb+ebx]
        add     ebx,ecx
        cmp     byte ptr [g_progb+ebx],0
        je      @F
        mov     [g_state.rn.vpt],bx
        jmp     ex_line
@@:
        add     esp,4
        mov     esi,offset g_cachb         ; contsi 0: CONT ends again
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
        movzx   ebx,[g_state.rn.vpt]
        mov     [g_state.rn.ln],bx
        cmp     byte ptr [g_progb+ebx],0   ; the direct line's len
        je      @exl_x
        movzx   esi,word ptr [g_progb+ebx+L_CP]
        test    esi,esi
        jz      cm_cache
        add     esi,offset g_cachb
        ret
@exl_x:
        call    cm_line

; ::::::::::::::
; ex_dcode - the direct line's code, compiled to xbuf, as a stream at DCODE.
;
; out:    ESI  it
; ::::::::::::::
ex_dcode:
        mov     edi,offset g_cachb+DCODE
        mov     [g_state.lnk.elim],offset g_cachb+DCODE+1536
        call    cm_xlate
        jc      err_om
        mov     esi,[g_state.lnk.ecode]
        ret

; ::::::::::::::
; op_for - I.FOR F.FOR id ( limit step -- ), I.FOR1 F.FOR1 id ( limit -- ): the
; start is already stored. An older loop of the variable drops; a FOR ending
; its line goes on at the next.
;
; error:  ?OUT OF MEMORY  the control stack is full
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
        jb      err_om
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
        jmp     ex_bjump
@of_e:
        add     esi,JSIZE
@of_p:
        ipst    [ebx+F_IP]
        mov     ax,[g_state.rn.ln]
        mov     [ebx+F_LN],ax
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
        mov     cl,[g_sinfo+edi]
        and     cl,3
        cmp     cl,1
        je      @fin_i
        cmp     word ptr [ebx+F_LIMIT+2],0FFFFh
        je      @fin_c
        fld     dword ptr [VARBASE+edi*4]
        fcomp   dword ptr [ebx+F_LIMIT]
        fnstsw  ax
        sahf
        mov     cl,[ebx+F_STEP+3]
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
        mov     dl,[ebx+F_STEP+1]
        jmp     @fin_s
@fin_i:
        mov     eax,[VARBASE+edi*4]
        mov     ecx,[ebx+F_LIMIT]
        mov     dl,[ebx+F_STEP+3]
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
        mov     eax,[VARBASE+edi*4]
        mov     [ebp+F_CACHE],eax
        call    flt_w16
        jnz     @fmk_b
        mov     [ebp+F_STEP+2],ax
        pop     eax
        mov     [ebp+F_LIMIT],ax
        mov     word ptr [ebp+F_LIMIT+2],0FFFFh
        pop     eax
        mov     [ebp+F_STEP],ax
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
        mov     cl,[g_sinfo+edi]
        and     cl,3
        lea     ebp,[VARBASE+edi*4]     ; the variable
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
        mov     [ebx+F_STEP+2],ax
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
        mov     ax,[ebx+F_LIMIT]        ; the body wrote it: FLOAT from here on
        call    flt_from16
        mov     ebx,[g_state.rn.cstk]
        mov     [ebx+F_LIMIT],eax
        mov     ax,[ebx+F_STEP]
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
        mov     dx,[ebx+F_LN]
        mov     [g_state.rn.ln],dx
        mov     ax,[ebx+F_IP]
        ipfix
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
        mov     dx,[ebx+F_LN]
        mov     [g_state.rn.ln],dx
        mov     ax,[ebx+F_IP]
        ipfix
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
        add     esi,4
        and     [g_state.rn.ehnd],0
        cmp     [g_state.rn.inerr],0
        je      @one_d
        mov     ax,[g_state.rn.errn]
        mov     [g_state.rn.uerr],ax
        mov     al,E_USR
        jmp     err_raise
@one_g:
        call    ex_target
        mov     [g_state.rn.ehnd],bx
@one_d:
        dispatch

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
        movzx   ecx,[g_state.rn.ln]     ; ERL: the last number at or before it
        mov     [g_state.rn.rln],cx
        mov     ebx,PROGBASE
        xor     edx,edx
@etr_l:
        cmp     ebx,ecx
        ja      @etr_d
        cmp     byte ptr [g_progb+ebx],0
        je      @etr_d
        mov     ax,word ptr [g_progb+ebx+1]
        cmp     ax,NONUM
        je      @F
        mov     dx,ax
@@:
        movzx   eax,byte ptr [g_progb+ebx]
        add     ebx,eax
        jmp     @etr_l
@etr_d:
        mov     [g_state.rn.erl],dx
        mov     ebx,ecx                 ; the statement, by the map
        mov     eax,[g_state.rn.opip]
        call    ex_smap
        mov     ax,[edi+1]
        mov     [g_state.rn.rc0],ax
        mov     ax,0FFFFh
        cmp     ecx,1
        jbe     @F
        mov     ax,[edi+4]
@@:
        mov     [g_state.rn.rc1],ax
        call    ch_clr
        call    ex_sbase
        mov     esp,eax
        movzx   ebx,[g_state.rn.ehnd]
        jmp     ex_jump

; ::::::::::::::
; ex_smap - the map entry of the statement holding code address EAX of
; line EBX: the last whose code is at EAX or before, else the first. An
; entry: body offset, code offset:2; the line's EOL's last.
;
; out:    EDI  the entry
;         ECX  the entries from it on
; ::::::::::::::
ex_smap:
        movzx   ebx,word ptr [g_progb+ebx+L_CP]
        add     ebx,offset g_cachb
        sub     eax,ebx
        movzx   ecx,byte ptr [ebx-1]   ; the count, after the entries
        lea     edi,[ecx+ecx*2+1]
        neg     edi
        add     edi,ebx
@esm_l:
        cmp     ecx,1
        jbe     @esm_r
        movzx   edx,word ptr [edi+4]
        cmp     edx,eax
        ja      @esm_r
        add     edi,3
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
        mov     cx,[g_state.rn.rc0]
        test    al,al
        jz      @F
        mov     cx,[g_state.rn.rc1]
@@:
        tspclr
        mov     dx,[g_state.rn.rln]
        mov     [g_state.rn.ln],dx
        mov     [g_state.rn.vpt],dx
        cmp     cx,0FFFFh
        je      @rsm_n
        movzx   esi,cx
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
        shl     ecx,2
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
        movzx   esi,word ptr [g_progb+ebx+L_CP]
        test    esi,esi
        jnz     @exr_a
        push    eax
        call    cm_cache
        pop     eax
        sub     esi,offset g_cachb
@exr_a:
        lea     esi,[esi+eax+offset g_cachb]
        ret

; ::::::::::::::
; ex_setcont - keep where CONT goes on, not for the direct line.
;
; in:     ln, ESI  the place
; out:    contip, contsi  the line, the code
; keeps:  ESI
; ::::::::::::::
ex_setcont:
        mov     bx,[g_state.rn.ln]
        cmp     bx,DLINE
        je      @F
        mov     [g_state.rn.contip],bx
        ipst    [g_state.rn.contsi]
@@:
        ret

; ::::::::::::::
; ex_end - the run is over: both stacks empty, READY.
; ::::::::::::::
ex_end:
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
        cmp     [g_state.rn.ln],DLINE
        je      err_id
        ret

; ::::::::::::::
; op_jmp - JMP target: a block's jump (comp/block.asm).
; ::::::::::::::
op_jmp:
        jmp     ex_bjump
; ::::::::::::::
; op_ijf - I.JF target ( i -- ): jump when 0.
; ::::::::::::::
op_ijf:
        dpop    eax
        test    eax,eax
        jz      ex_bjump
@ojf_n:
        add     esi,JSIZE-1
        dispatch
; ::::::::::::::
; op_fjf - F.JF target ( f -- ): jump when 0.
; ::::::::::::::
op_fjf:
        dpop    eax
        test    eax,7F800000h
        jz      ex_bjump
        jmp     @ojf_n
; ::::::::::::::
; op_ijt - I.JT target ( i -- ): jump unless 0.
; ::::::::::::::
op_ijt:
        dpop    eax
        test    eax,eax
        jnz     ex_bjump
        jmp     @ojf_n
; ::::::::::::::
; op_fjt - F.JT target ( f -- ): jump unless 0.
; ::::::::::::::
op_fjt:
        dpop    eax
        test    eax,7F800000h
        jnz     ex_bjump
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
; ex_bjump - go to the target at ESI: its address once found, else the
; target line's code (compiled when it has none), its statement by the map
; before that code, the mode's place in it. The address is kept in the op
; unless the cache was flushed meanwhile, the op with it.
;
; in:     ESI  the target: line:2 boff:1 mode:1, then the address:2, 0 none
; error:  ?SYNTAX ERROR  a target the link gave none (FFFFh)
; ::::::::::::::
ex_bjump:
        stopchk
        tspclr
        movzx   eax,word ptr [esi+4]
        test    eax,eax
        jz      @ebj_f
        mov     bx,[esi]
        mov     [g_state.rn.ln],bx
        lea     esi,[g_cachb+eax]
        dispatch
@ebj_f:
        mov     edx,esi
        mov     bx,[edx]
        cmp     bx,0FFFFh
        je      err_sn
        mov     al,[g_state.lnk.nfl]
        push    eax
        push    edx
        mov     [g_state.rn.vpt],bx
        call    ex_line
        pop     edx
        pop     eax
        movzx   ecx,byte ptr [esi-1]    ; the map: n of boff:1 coff:2, then n
        lea     edi,[ecx+ecx*2+1]
        neg     edi
        add     edi,esi
        mov     bl,[edx+2]
@ebj_l:
        jecxz   @ebj_n
        cmp     [edi],bl
        je      @ebj_y
        add     edi,3
        dec     ecx
        jmp     @ebj_l
@ebj_y:
        movzx   ebx,word ptr [edi+1]
        cmp     byte ptr [edx+3],1
        jb      @ebj_t
        je      @ebj_1
        cmp     ecx,1                   ; mode 2: the next statement's
        jbe     @ebj_n
        movzx   ebx,word ptr [edi+4]
        jmp     @ebj_t
@ebj_1:
        add     ebx,JSIZE               ; mode 1: past its JMP
@ebj_t:
        add     esi,ebx
        cmp     al,[g_state.lnk.nfl]
        jne     @ebj_d
        mov     eax,esi
        sub     eax,offset g_cachb
        mov     [edx+4],ax
@ebj_d:
        dispatch
@ebj_n:
        jmp     err_sn

; ::::::::::::::
; op_pcall - PCALL proc target addr ( (value ref)*n -- ): a procedure's
; frame. Its arguments go to save entries (a string copied unless a
; temporary's), its locals are saved there and zeroed, then the entries to
; its parameters; on at its body. A value: an INT, a FLOAT, a string's Desc,
; an array's id; a ref: the variable PRET stores the parameter back to, or
; FFFFh.
;
; error:  ?OUT OF MEMORY  a stack is full
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
        lea     eax,[ecx+ecx*2]
        lea     edi,[edi+eax*4]
        cmp     edi,offset g_lsav+LSAVSZ-LSSZ
        jae     @st_flow1
        mov     [g_state.rn.ppend],edi
        jecxz   @pca_s
@pca_a:                                 ; the arguments, the last first
        sub     edi,LSSZ
        dpop    eax
        mov     [edi+2],ax
        movzx   ebx,word ptr [ebp+P_PAR]
        add     ebx,ecx
        movzx   ebx,word ptr [g_ppar+ebx*2-2]
        mov     [edi],bx
        pop     eax
        pop     dword ptr [edi+8]       ; a DOUBLE's high half
        mov     dl,[g_sinfo+ebx]
        test    dl,dl
        js      @pca_y
        and     dl,3
        cmp     dl,2
        je      @pca_t
@pca_v:
        mov     [edi+4],eax
        loop    @pca_a
        jmp     @pca_s
@pca_y:
        mov     eax,[g_vt+eax*4]        ; an array: its header
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
        loop    @pca_a
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
        mov     cl,[g_sinfo+ebx]
        test    cl,cl
        js      @pcl_y
        test    cl,SK_DBL
        jnz     @pcl_q
        and     cl,3
        cmp     cl,2
        je      @pcl_t
        xor     eax,eax
        xchg    eax,[VARBASE+ebx*4]
        mov     [edx+4],eax
        jmp     @pcl_n
@pcl_q:
        xor     eax,eax
        xchg    eax,[SVBASE+ebx*8]
        mov     [edx+4],eax
        xor     eax,eax
        xchg    eax,[SVBASE+ebx*8+4]
        mov     [edx+8],eax
        jmp     @pcl_n
@pcl_y:
        xor     eax,eax
        xchg    eax,[g_vt+ebx*4]
        mov     [edx+4],eax
        jmp     @pcl_n
@pcl_t:
        lea     eax,[SVBASE+ebx*8]
        lea     edi,[edx+4]
        call    ls_mov
        and     word ptr [eax+D_LEN],0
@pcl_n:
        add     edx,LSSZ
        movzx   ebx,word ptr [g_vnloc+ebx*2]
        jmp     @pcl_l
@pcl_d:
        mov     edi,[g_state.rn.lsp]    ; the arguments to the parameters
@pcb_l:
        cmp     edi,[g_state.rn.ppend]
        jae     @pcb_d
        movzx   ebx,word ptr [edi]
        mov     eax,[edi+4]
        mov     cl,[g_sinfo+ebx]
        test    cl,cl
        js      @pcb_y
        test    cl,SK_DBL
        jz      @F
        mov     [SVBASE+ebx*8],eax
        mov     eax,[edi+8]
        mov     [SVBASE+ebx*8+4],eax
        jmp     @pcb_n
@@:
        and     cl,3
        cmp     cl,2
        je      @pcb_t
        mov     [VARBASE+ebx*4],eax
        jmp     @pcb_n
@pcb_y:
        mov     [g_vt+ebx*4],eax
        jmp     @pcb_n
@pcb_t:
        push    edi
        lea     eax,[edi+4]
        lea     edi,[SVBASE+ebx*8]
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
        mov     cx,[g_state.rn.ln]
        mov     [edi+F_LN],cx
        mov     al,[esi]
        mov     [edi+F_PRC],al
        lea     esi,[esi+7]
        ipst    [edi+F_IP]
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
        sub     esi,6                   ; the target
        jmp     ex_bjump

; ::::::::::::::
; op_pret - PRET: back from the procedure: its frame and the frames above
; it go; the parameters with a ref are stored back to it after the locals
; are restored; a FUNCTION's result is pushed.
;
; error:  ?RETURN WITHOUT GOSUB  no procedure's frame
; ::::::::::::::
op_pret:
        stopchk
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
        mov     al,[g_sinfo+ecx]
        test    al,SK_DBL
        jz      @F
        mov     eax,[SVBASE+ecx*8]
        mov     [edx+4],eax
        mov     eax,[SVBASE+ecx*8+4]
        mov     [edx+8],eax
        jmp     @prt_pn
@@:
        and     al,3
        cmp     al,2
        je      @prt_ps
        mov     eax,[VARBASE+ecx*4]
        mov     [edx+4],eax
        jmp     @prt_pn
@prt_ps:
        lea     eax,[SVBASE+ecx*8]
        lea     edi,[edx+4]
        call    ls_mov
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
        mov     al,[g_sinfo+ecx]
        test    al,SK_DBL
        jz      @F
        mov     eax,[SVBASE+ecx*8]
        mov     dword ptr [g_pres8],eax
        mov     eax,[SVBASE+ecx*8+4]
        mov     dword ptr [g_pres8+4],eax
        jmp     @prt_l
@@:
        and     al,3
        cmp     al,2
        je      @prt_rs
        mov     eax,[VARBASE+ecx*4]
        mov     [g_state.rn.pres],eax
        jmp     @prt_l
@prt_rs:
        lea     eax,[SVBASE+ecx*8]
        mov     edi,offset g_lres
        call    ls_mov
@prt_l:
        mov     edx,[g_state.rn.lsp]    ; the locals back
@prt_ll:
        cmp     edx,[ebx+F_LSP1]
        jbe     @prt_o
        sub     edx,LSSZ
        movzx   ecx,word ptr [edx]
        mov     al,[g_sinfo+ecx]
        test    al,al
        js      @prt_ly
        test    al,SK_DBL
        jz      @F
        mov     eax,[edx+4]
        mov     [SVBASE+ecx*8],eax
        mov     eax,[edx+8]
        mov     [SVBASE+ecx*8+4],eax
        jmp     @prt_ll
@@:
        and     al,3
        cmp     al,2
        je      @prt_ls
        mov     eax,[edx+4]
        mov     [VARBASE+ecx*4],eax
        jmp     @prt_ll
@prt_ly:
        cmp     dword ptr [g_vt+ecx*4],0
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
        mov     [g_vt+ecx*4],eax
        jmp     @prt_ll
@prt_ls:
        lea     eax,[edx+4]
        lea     edi,[SVBASE+ecx*8]
        call    ls_mov
        jmp     @prt_ll
@prt_o:
        mov     edx,[ebx+F_LSP0]        ; the parameters to their refs
@prt_ol:
        cmp     edx,[ebx+F_LSP1]
        jae     @prt_x
        movzx   ecx,word ptr [edx+2]
        cmp     cx,0FFFFh
        je      @prt_on
        movzx   eax,word ptr [edx]
        mov     al,[g_sinfo+eax]
        test    al,SK_DBL
        jz      @F
        mov     eax,[edx+4]
        mov     [SVBASE+ecx*8],eax
        mov     eax,[edx+8]
        mov     [SVBASE+ecx*8+4],eax
        jmp     @prt_on
@@:
        and     al,3
        cmp     al,2
        je      @prt_os
        mov     eax,[edx+4]
        mov     [VARBASE+ecx*4],eax
        jmp     @prt_on
@prt_os:
        lea     eax,[edx+4]
        lea     edi,[SVBASE+ecx*8]
        call    ls_mov
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
        cmp     byte ptr [ebp+P_KIND],PK_FUNC
        jne     @prt_j
        movzx   ecx,word ptr [ebp+P_RES]
        mov     al,[g_sinfo+ecx]
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
        mov     dx,[ebx+F_LN]
        mov     [g_state.rn.ln],dx
        mov     ax,[ebx+F_IP]
        add     ebx,M_PC
        mov     [g_state.rn.cstk],ebx
        ipfix
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
