; ==========================================================================
; flow.asm - the executor: lines, jumps, FN, FOR and NEXT
; ==========================================================================
; holds:    the ops that move SI or the control stack; ex_line, ex_end
; uses:     prg_findw, cm_line, cm_link, flt_add, err_*
; data:     The control stack holds frames from cstk up to CSTOP, the mark the
;           size: GOSUB mark ln:2 ip:2; FOR mark ln:2 ip:2 id step:4 limit:4
;           cache:4; FN mark ip:2 id old:4.
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
        pop     eax
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
        and     [g_state.sg.tsp],0
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
        pop     edx
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
        and     [g_state.sg.tsp],0
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
        and     [g_state.sg.tsp],0
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
        mov     esi,offset g_cachb+DCODE
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
        pop     dword ptr [g_state.rn.fstep]
        jmp     @of_l
@of_1:
        mov     eax,1
        cmp     ebx,O_F_FOR1*2
        jne     @of_s
        mov     eax,[g_kone]
@of_s:
        mov     [g_state.rn.fstep],eax
@of_l:
        pop     dword ptr [g_state.rn.flim]
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
        and     [g_state.sg.tsp],0
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
        and     [g_state.sg.tsp],0
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

; --------------------------------------------------------------------------

        end
