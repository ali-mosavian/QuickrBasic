; ==========================================================================
; dbg.asm - the debugger's VM side: where a run stops, TRON, TROFF
; ==========================================================================
; holds:    op_dbg, dbg_depth, dbg_pos, dbg_bpset, dbg_bpclr, sc_tron,
;           sc_troff
; uses:     ex_smap, sc_stop
; data:     With dbg.emit set (the IDE), each program statement's code
;           starts with DBG. Its stops: a step (mode), a breakpoint at the
;           line's first statement (by line number, g_dbbp), until (ulnum,
;           once). A stop is a STOP: CONT goes on past the DBG. With trace
;           set, each statement first goes to hook, its place in wnum, wb0,
;           wb1. The direct line has no DBG.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; op_dbg - DBG: a statement starts.
; ::::::::::::::
op_dbg:
        cmp     dword ptr [g_state.dbg.mode],0  ; mode trace bpon utl
        jne     @F
        dispatch
@@:
        movzx   ebx,[g_state.rn.ln]
        cmp     ebx,DLINE
        je      @odb_d
        cmp     [g_state.dbg.trace],0
        je      @odb_m
        mov     eax,[g_state.dbg.hook]
        test    eax,eax
        jz      @odb_m
        push    esi
        lea     eax,[esi-1]
        call    dbg_pos
        call    [g_state.dbg.hook]
        pop     esi
        movzx   ebx,[g_state.rn.ln]
@odb_m:
        mov     al,[g_state.dbg.mode]
        cmp     al,DM_STEP
        je      @odb_s
        cmp     al,DM_PSTEP
        jne     @odb_b
        call    dbg_depth
        cmp     eax,[g_state.dbg.pdep]
        jbe     @odb_s
        movzx   ebx,[g_state.rn.ln]
@odb_b:                                 ; the line's first statement only
        lea     eax,[esi-1]
        sub     eax,offset g_cachb
        cmp     ax,word ptr [g_progb+ebx+L_CP]
        jne     @odb_d
        movzx   eax,word ptr [g_progb+ebx+1]
        cmp     [g_state.dbg.utl],0
        je      @F
        cmp     ax,[g_state.dbg.ulnum]
        je      @odb_s
@@:
        cmp     [g_state.dbg.bpon],0
        je      @odb_d
        bt      dword ptr [g_dbbp],eax
        jc      @odb_s
@odb_d:
        dispatch
@odb_s:
        and     [g_state.dbg.mode],0
        and     [g_state.dbg.utl],0
        jmp     sc_stop

; ::::::::::::::
; dbg_depth - the procedures, FNs and GOSUBs being run.
;
; out:    EAX  their count
; ::::::::::::::
dbg_depth:
        xor     eax,eax
        mov     ebx,[g_state.rn.cstk]
@ddp_l:
        cmp     ebx,CSTOP
        jae     @ddp_r
        movzx   ecx,byte ptr [ebx+F_MARK]
        cmp     cl,M_PC
        je      @ddp_c
        cmp     cl,M_GS
        je      @ddp_c
        cmp     cl,M_FN
        jne     @ddp_n
@ddp_c:
        inc     eax
@ddp_n:
        add     ebx,ecx
        jmp     @ddp_l
@ddp_r:
        ret

; ::::::::::::::
; dbg_pos - the place of code address EAX in line EBX to wnum, wb0, wb1.
; ::::::::::::::
dbg_pos:
        mov     dx,word ptr [g_progb+ebx+1]
        mov     [g_state.dbg.wnum],dx
        call    ex_smap
        mov     al,[edi]
        mov     [g_state.dbg.wb0],al
        mov     al,0FFh
        cmp     ecx,1
        jbe     @F
        mov     al,[edi+3]
@@:
        mov     [g_state.dbg.wb1],al
        ret

; ::::::::::::::
; dbg_bpset - a breakpoint at line number AX.
; ::::::::::::::
dbg_bpset:
        movzx   eax,ax
        bts     dword ptr [g_dbbp],eax
        mov     [g_state.dbg.bpon],1
        ret

; ::::::::::::::
; dbg_bpclr - no breakpoints.
; ::::::::::::::
dbg_bpclr:
        push    edi
        push    ecx
        mov     edi,offset g_dbbp
        mov     ecx,DBBPSZ/4
        xor     eax,eax
        rep     stosd
        mov     [g_state.dbg.bpon],al
        pop     ecx
        pop     edi
        ret

; ::::::::::::::
; sc_tron - TRON; sc_troff - TROFF: trace on, off.
; ::::::::::::::
sc_tron:
        mov     [g_state.dbg.trace],1
        jmp     ex_lp
sc_troff:
        mov     [g_state.dbg.trace],0
        jmp     ex_lp

        .data?
g_dbbp  db      DBBPSZ dup(?)           ; a bit per line number
        .code

        end
