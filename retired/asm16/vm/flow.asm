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

; ::::::::::::::
; op_fndef - FN.DEF id len:2: the slot holds the DEF's line and the body's
; offset in its code; the body is skipped.
; ::::::::::::::
op_fndef:
        vslot
        mov     ax,[si]
        add     si,2
        mov     cx,[ln]
        mov     [VARBASE+bx],cx
        mov     dx,si
        mov     di,cx
        sub     dx,gs:[di+L_CP]
        mov     [VARBASE+bx+2],dx
        add     si,ax
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
        cmp     sp,[stklim]
        jb      @st_flow1
        vslot
        mov     ax,[VARBASE+bx]
        test    ax,ax
        jnz     @fnc_d
        mov     al,E_UF
        jmp     err_raise
@fnc_d:
        mov     di,[cstk]
        cmp     di,CSLIM
        jb      @st_flow1
        sub     di,M_FN
        mov     [cstk],di
        mov     byte ptr [di],M_FN
        mov     [di+F_IP],si
        mov     cx,[ln]
        mov     [di+F_LN],cx
        mov     dx,[VARBASE+bx+2]
        mov     si,dx
        mov     dx,ax
        call    ex_resolve
        dispatch

; ::::::::::::::
; op_fnent - FN.ENTER id ( f -- ): the parameter's value kept, the argument in.
; ::::::::::::::
op_fnent:
        vslot
        mov     di,[cstk]
        mov     eax,[VARBASE+bx]
        mov     [di+F_OLD],eax
        pop     eax
        mov     cx,bx
        shr     cx,2
        mov     di,cx
        test    [sinfo+di],1
        jz      @fne_f
        push    bx
        call    flt_fix16
        pop     bx
@fne_f:
        mov     [VARBASE+bx],eax
        dispatch

; ::::::::::::::
; op_fnret - FN.RET id ( f -- f ): the parameter back, on after FN.CALL.
; ::::::::::::::
op_fnret:
        vslot
        mov     di,[cstk]
        mov     eax,[di+F_OLD]
        mov     [VARBASE+bx],eax
        mov     si,[di+F_IP]
        mov     dx,[di+F_LN]
        add     di,M_FN
        mov     [cstk],di
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
; op_gotol - GOTO with the line found when the stream was made (cm_xlate).
; ::::::::::::::
op_gotol:
        stopchk
        mov     bx,[si+2]
        jmp     ex_jump
; ::::::::::::::
; op_gosubl - GOSUB, likewise.
; ::::::::::::::
op_gosubl:
        stopchk
        mov     bx,[si+2]
        add     si,4
        jmp     ex_gosub

; ::::::::::::::
; ex_target - num addr at SI: the line.
;
; out:    BX  the line
;         SI  past them
; error:  ?UNDEF'D STATEMENT  no such line
; ::::::::::::::
ex_target:
        lods    word ptr [si]
        mov     [lnum],ax
        lods    word ptr [si]
        mov     bx,ax
        test    bx,bx
        jnz     @etg_r
        call    prg_findw
        jnc     err_us
        mov     [si-2],bx            ; found once, read after
@etg_r:
        ret

; ::::::::::::::
; ex_gosub - a GOSUB frame, then the line at BX.
;
; in:     BX      the line
;         ln, SI  where RETURN goes on
; error:  ?OUT OF MEMORY  the control stack is full
; ::::::::::::::
ex_gosub:
        mov     di,[cstk]
        cmp     di,CSLIM
        jb      err_om
        sub     di,M_GS
        mov     [cstk],di
        mov     byte ptr [di+F_MARK],M_GS
        mov     cx,[ln]
        mov     [di+F_LN],cx
        mov     [di+F_IP],si

; ::::::::::::::
; ex_jump - run the line at BX from its start.
;
; in:     BX  the line
; ::::::::::::::
ex_jump:
        and     [tsp],0
        cmp     [linked],0
        jne     @exj_g
        push    bx
        call    cm_link
        pop     bx
@exj_g:
        mov     [lc],bx
        mov     [vpt],bx
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
        lods    byte ptr [si]
        mov     [onk],al
        and     al,7Fh
        movzx   cx,al
        mov     di,si
        shl     cx,2
        add     di,cx                   ; past the list
        dec     dl                      ; k 0: 255, past n < 128
        cmp     dl,al
        jae     @on_past
        movzx   bx,dl
        shl     bx,2
        mov     ax,[si+bx]
        mov     [lnum],ax
        lea     cx,[si+bx+2]
        mov     bx,[si+bx+2]
        mov     si,di
        test    bx,bx
        jnz     @on_g
        call    prg_findw
        jnc     err_us
        mov     di,cx
        mov     gs:[di],bx
@on_g:
        test    [onk],80h
        jnz     ex_gosub
        jmp     ex_jump
@on_past:
        mov     si,di
        dispatch

; ::::::::::::::
; op_ret - RETURN: the FOR frames above the GOSUB frame drop.
;
; error:  ?RETURN WITHOUT GOSUB  no GOSUB frame on top
; ::::::::::::::
op_ret:
        stopchk
        mov     bl,0FFh
        call    for_find
        cmp     al,M_GS
        jne     err_rg
        mov     dx,[bx+F_LN]
        mov     [ln],dx
        mov     si,[bx+F_IP]
        add     bx,M_GS
        mov     [cstk],bx
        and     [tsp],0
        ipfix
        dispatch

; ::::::::::::::
; for_find - a variable's FOR frame, above the newest GOSUB or FN frame.
;
; in:     BL  the id
; out:    BX  the frame
;         AL  its mark
;         ZF  set when found
; ::::::::::::::
for_find:
        mov     cl,bl
        mov     bx,[cstk]
@ff_lp:
        cmp     bx,CSTOP
        jae     @ff_no
        mov     al,[bx+F_MARK]
        cmp     al,M_FR
        jne     @ff_r
        cmp     cl,[bx+F_ID]
        je      @ff_r
        add     bx,FRSIZE
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
        and     [tsp],0
        call    ex_eol
        dispatch

; ::::::::::::::
; ex_eol - the next line; at the program's end, END.
;
; in:     ln  the finished line
; out:    SI  the next line's code
; ::::::::::::::
ex_eol:
        mov     bx,[ln]
        movzx   cx,byte ptr gs:[bx]
        add     bx,cx
        cmp     byte ptr gs:[bx],0
        je      @F
        mov     [vpt],bx
        jmp     ex_line
@@:
        add     sp,2
        xor     si,si                   ; contsi 0: CONT ends again
        jmp     sc_end

; ::::::::::::::
; ex_line - the code of line vpt: a program line's is in the cache, or
; compiled into it; a direct one's is compiled.
;
; in:     vpt  the line
; out:    ln  vpt
;         SI  the code
; ::::::::::::::
ex_line:
        mov     bx,[vpt]
        mov     [ln],bx
        cmp     byte ptr gs:[bx],0      ; the direct line's len
        je      @exl_x
        mov     si,gs:[bx+L_CP]
        test    si,si
        jz      cm_cache
        ret
@exl_x:
        call    cm_line

; ::::::::::::::
; ex_dcode - the direct line's code, compiled to xbuf, as a stream in dcode.
;
; out:    SI  it
; ::::::::::::::
ex_dcode:
        mov     di,offset dcode
        mov     [elim],offset dcode+1536
        call    cm_xlate
        jc      err_om
        mov     si,offset dcode
        ret

; ::::::::::::::
; op_for - I.FOR F.FOR id ( limit step -- ), I.FOR1 F.FOR1 id ( limit -- ): the
; start is already stored. An older loop of the variable drops; a FOR ending
; its line goes on at the next.
;
; error:  ?OUT OF MEMORY  the control stack is full
; ::::::::::::::
op_for:
        mov     [fop],bx
        lods    byte ptr [si]
        mov     [fid],al
        cmp     bx,O_I_FOR1*2
        jae     @of_1
        pop     dword ptr [fstep]
        jmp     @of_l
@of_1:
        mov     eax,1
        cmp     bx,O_F_FOR1*2
        jne     @of_s
        mov     eax,[kone]
@of_s:
        mov     [fstep],eax
@of_l:
        pop     dword ptr [flim]
        mov     bl,[fid]
        call    for_find
        je      @of_r
        mov     bx,[cstk]
        cmp     bx,CSLIM
        jb      err_om
        sub     bx,FRSIZE
@of_r:
        mov     [cstk],bx
        mov     al,[fid]
        mov     [bx+F_ID],al
        mov     eax,[fstep]
        mov     [bx+F_STEP],eax
        mov     eax,[flim]
        mov     [bx+F_LIMIT],eax
        mov     byte ptr [bx+F_MARK],M_FR
        cmp     [fop],O_F_FOR*2
        je      @of_k
        cmp     [fop],O_F_FOR1*2
        jne     @of_m
@of_k:
        call    for_mki
@of_m:
        mov     bx,[cstk]
        mov     [bx+F_IP],si
        mov     ax,[ln]
        mov     [bx+F_LN],ax
        cmp     byte ptr [si],0BEh
        jne     @F
        mov     ax,si
        add     ax,6+CSOFF              ; the jmp's target, in CS
        add     ax,[si+4]
        cmp     ax,offset op_eol
        jne     @F
        call    ex_eol                  ; no next line: no return, the frame kept
        jmp     @of_m
@@:
        dispatch

; ::::::::::::::
; for_mki - a FLOAT loop whose start, limit and step are whole INTs counts in
; an INT.
;
; in:     BX  the frame, its values FLOATs
; ::::::::::::::
for_mki:
        mov     bp,bx
        mov     eax,[bp+F_STEP]
        call    flt_w16
        jnz     @fmk_r
        push    ax
        mov     eax,[bp+F_LIMIT]
        call    flt_w16
        jnz     @fmk_a
        push    ax
        movzx   di,byte ptr [bp+F_ID]
        shl     di,2
        mov     eax,[VARBASE+di]
        mov     [bp+F_CACHE],eax
        call    flt_w16
        jnz     @fmk_b
        mov     [bp+F_STEP+2],ax
        pop     ax
        mov     [bp+F_LIMIT],ax
        mov     word ptr [bp+F_LIMIT+2],0FFFFh
        pop     ax
        mov     [bp+F_STEP],ax
        ret
@fmk_b:
        pop     ax
@fmk_a:
        pop     ax
@fmk_r:
        ret

; ::::::::::::::
; op_nextv - NEXT.V id.
;
; error:  ?NEXT WITHOUT FOR  no frame of it
; ::::::::::::::
op_nextv:
        stopchk
        and     [tsp],0
        lods    byte ptr [si]
        mov     bl,al
        mov     di,[cstk]
        cmp     di,CSTOP
        jae     @onv_f
        cmp     byte ptr [di+F_MARK],M_FR
        jne     @onv_f
        cmp     al,[di+F_ID]
        jne     @onv_f
        mov     bx,di
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
        and     [tsp],0
        mov     bx,[cstk]
        cmp     bx,CSTOP
        jae     err_nf
        cmp     byte ptr [bx+F_MARK],M_FR
        jne     err_nf

; ::::::::::::::
; for_any - step the frame at BX: a % by its INT, else the FLOAT.
;
; in:     BX  the frame
;         SI  after the op
; error:  ?ILLEGAL QUANTITY  a % stepped out of range
;         ?OVERFLOW          past the largest FLOAT
; ::::::::::::::
for_any:
        mov     [cstk],bx
        movzx   di,byte ptr [bx+F_ID]
        mov     cl,[sinfo+di]
        and     cl,3
        shl     di,2
        lea     bp,[VARBASE+di]         ; the variable
        mov     [fvp],bp
        cmp     cl,1
        je      @fa_pct
        cmp     word ptr [bx+F_LIMIT+2],0FFFFh
        jne     @fa_gen
        mov     eax,[bp]
        cmp     eax,[bx+F_CACHE]
        jne     @fa_deg
        movsx   eax,word ptr [bx+F_STEP+2]
        movsx   ecx,word ptr [bx+F_STEP]
        add     eax,ecx
        mov     [bx+F_STEP+2],ax
        mov     [fsv],eax
        call    flt_from32
        mov     bx,[cstk]
        mov     di,[fvp]
        mov     [di],eax
        mov     [bx+F_CACHE],eax
        mov     eax,[fsv]
        movsx   ecx,word ptr [bx+F_LIMIT]
        test    byte ptr [bx+F_STEP+1],80h
        jnz     @fa_id
        cmp     eax,ecx
        jle     @fa_lp
        jmp     @fa_done
@fa_id:
        cmp     eax,ecx
        jge     @fa_lp
        jmp     @fa_done
@fa_deg:
        mov     ax,[bx+F_LIMIT]         ; the body wrote it: FLOAT from here on
        call    flt_from16
        mov     bx,[cstk]
        mov     [bx+F_LIMIT],eax
        mov     ax,[bx+F_STEP]
        call    flt_from16
        mov     bx,[cstk]
        mov     [bx+F_STEP],eax
        mov     bp,[fvp]
        jmp     @fa_gen
@fa_pct:
        mov     ax,[bp]
        add     ax,[bx+F_STEP]
        jo      err_iq
        mov     [bp],ax
        mov     dx,[bx+F_STEP]
        test    dx,dx
        js      @fa_dn
        cmp     ax,[bx+F_LIMIT]
        jle     @fa_lp
@fa_done:
        add     [cstk],FRSIZE
        dispatch
@fa_dn:
        cmp     ax,[bx+F_LIMIT]
        jge     @fa_lp
        jmp     @fa_done
@fa_lp:
        mov     dx,[bx+F_LN]
        mov     [ln],dx
        mov     si,[bx+F_IP]
        ipfix
        dispatch
@fa_gen:
        mov     eax,[bp]
        mov     edx,[bx+F_STEP]
        call    flt_add
        mov     bx,[fvp]
        mov     [bx],eax
        mov     bx,[cstk]
        mov     edx,[bx+F_LIMIT]
        call    flt_cmp                 ; 1 past above, FFh below
        mov     bx,[cstk]
        mov     cl,1
        test    byte ptr [bx+F_STEP+3],80h
        jz      @F
        mov     cl,0FFh
@@:
        cmp     al,cl
        je      @fa_done
        jmp     @fa_lp

; ::::::::::::::
; for_lp - a loop goes on: its frame's line and ip.
;
; in:     BX  the frame
; ::::::::::::::
for_lp:
        mov     dx,[bx+F_LN]
        mov     [ln],dx
        mov     si,[bx+F_IP]
        ipfix
        dispatch

; ::::::::::::::
; op_err - ERR code.
;
; error:  that one  always
; ::::::::::::::
op_err:
        mov     al,[si]
        jmp     err_raise

; ::::::::::::::
; op_size - the size of the op at SI.
;
; in:     SI  at it
; out:    CX  its size
; keeps:  all but CX
; ::::::::::::::
op_size:
        push    bx
        movzx   bx,byte ptr [si]
        movzx   cx,[opsz+bx]
        jcxz    @oln_v
        pop     bx
        ret
@oln_v:
        movzx   cx,byte ptr [si+1]
        cmp     bl,O_ON
        je      @oln_on
        add     cx,2                    ; S.LD.TXT
        pop     bx
        ret
@oln_on:
        and     cl,7Fh
        shl     cx,2
        add     cx,2
        pop     bx
        ret

; ::::::::::::::
; ex_resolve - a frame's ip kept as an offset: the address in its line's code.
;
; in:     DX  the line
;         SI  the offset
; out:    SI  the address
; error:  ?OUT OF MEMORY  the code does not fit
; ::::::::::::::
ex_resolve:
        mov     bx,dx
        mov     ax,si
        mov     si,gs:[bx+L_CP]
        test    si,si
        jnz     @exr_a
        push    ax
        call    cm_cache
        pop     ax
@exr_a:
        add     si,ax
        ret

; ::::::::::::::
; ex_setcont - keep where CONT goes on, not for the direct line.
;
; in:     ln, SI  the place
; out:    contip, contsi  the line, the code
; keeps:  SI
; ::::::::::::::
ex_setcont:
        mov     bx,[ln]
        cmp     bx,DLINE
        je      @F
        mov     [contip],bx
        mov     [contsi],si
@@:
        ret

; ::::::::::::::
; ex_end - the run is over: both stacks empty, READY.
; ::::::::::::::
ex_end:
        mov     sp,[rsp]
        mov     [cstk],CSTOP
        jmp     rpl_ready

; ::::::::::::::
; ex_nodir - ?ILLEGAL DIRECT in the direct line.
;
; in:     ln  the running line
; ::::::::::::::
ex_nodir:
        cmp     [ln],DLINE
        je      err_id
        ret

; --------------------------------------------------------------------------
; The CALL routines. Each enters by jmp with BX = k * 2.
; --------------------------------------------------------------------------
