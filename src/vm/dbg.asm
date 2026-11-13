; ==========================================================================
; dbg.asm - the debugger's VM side: where a run stops, TRON, TROFF
; ==========================================================================
; holds:    op_dbg, dbg_depth, dbg_pos, dbg_bpset, dbg_bpclr, dbg_eval,
;           dbg_evx, dbg_ever, sc_tron, sc_troff
; uses:     ex_smap, sc_stop
; data:     With dbg.emit set (the IDE), each program statement's code
;           starts with DBG. Its stops: a step (mode), a breakpoint at the
;           line's first statement (by line number, g_dbbp), until (ulnum,
;           once). A stop is a STOP: CONT goes on past the DBG. With trace
;           set, each statement first goes to hook, its place in wnum, wb0,
;           wb1. The direct line has no DBG.
;           dbg_eval, the probe: a direct line PRINTs an expression into
;           sbuf while the run is stopped; its end (sc_end, ex_end) and any
;           error (err_raise) come back to it by dbg_evx, and what the line
;           can change of the run is put back from dbsv.
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
        mov     ebx,[g_state.rn.ln]
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
        mov     ebx,[g_state.rn.ln]
@odb_m:
        mov     al,[g_state.dbg.mode]
        cmp     al,DM_STEP
        je      @odb_s
        cmp     al,DM_PSTEP
        jne     @odb_b
        call    dbg_depth
        cmp     eax,[g_state.dbg.pdep]
        jbe     @odb_s
        mov     ebx,[g_state.rn.ln]
@odb_b:                                 ; the line's first statement only
        lea     eax,[esi-1]
        sub     eax,[g_state.mm.cblin]
        cmp     eax,gs:[ebx].TLN.cp
        jne     @odb_d
        movzx   eax,gs:[ebx].TLN.num
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
        mov     dx,gs:[ebx].TLN.num
        mov     [g_state.dbg.wnum],dx
        call    ex_smap
        movzx   eax,word ptr [edi]
        mov     [g_state.dbg.wb0],eax
        mov     eax,BOFF
        cmp     ecx,1
        jbe     @F
        movzx   eax,word ptr [edi+4]
@@:
        mov     [g_state.dbg.wb1],eax
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
; dbg_eval - PRINT expression ESI, in procedure EAX's scope, its output to
; EDI, 0-ended, 79 bytes at most; the stopped run as it was. A FUNCTION it
; calls runs; no breakpoint, step, trace or event stops it.
;
; out:    AL  0, else the error (E_*) + 1: BREAK is 0
; ::::::::::::::
dbg_eval:
        pushad
        mov     [g_dlprc],al
        mov     ebx,offset g_dbsvt      ; what the line can change, kept
        mov     edi,offset g_dbsv
@dbe_s:
        mov     esi,[ebx]
        test    esi,esi
        jz      @F
        movzx   ecx,byte ptr [ebx+4]
        rep     movsb
        add     ebx,5
        jmp     @dbe_s
@@:
        and     dword ptr [g_state.dbg.mode],0  ; mode trace bpon utl
        and     [g_state.rn.stpflg],0
        and     [g_evon],0
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
        mov     esi,[esp+4]             ; "PRINT " and the expression
        mov     edi,offset g_inbuf
        mov     eax,'NIRP'
        stosd
        mov     ax,' T'
        stosw
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        mov     [g_state.rn.ln],DLINE
        mov     [g_dbev],esp
        mov     esi,offset g_inbuf
        mov     edi,offset g_cbuf
        call    lx_line
        call    cm_line
        call    ex_dcode
        mov     [g_state.rn.rsp],esp
        jmp     ex_lp

; ::::::::::::::
; dbg_evx - the probe's line ended (AL 0); dbg_ever - it failed (AL the
; error): back from dbg_eval, its output kept, the run's state put back.
; ::::::::::::::
dbg_ever:
        inc     al
dbg_evx:
        mov     esp,[g_dbev]
        and     [g_dbev],0
        mov     [esp+28],al             ; pushad's EAX
        mov     edi,[esp]               ; pushad's EDI
        mov     ecx,[g_state.io.sbx]
        cmp     ecx,79
        jbe     @F
        mov     ecx,79
@@:
        mov     esi,offset g_sbuf
        rep     movsb
        mov     byte ptr [edi],0
        mov     ebx,offset g_dbsvt
        mov     esi,offset g_dbsv
@dbx_r:
        mov     edi,[ebx]
        test    edi,edi
        jz      @F
        movzx   ecx,byte ptr [ebx+4]
        rep     movsb
        add     ebx,5
        jmp     @dbx_r
@@:
        and     [g_state.cm.csp],0
        mov     [g_dlprc],0
        call    ex_fclr
        popad
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

        .data
g_dbev  dd      0                       ; the probe's ESP, 0 none running
; dbg_eval's: each place the probe's line can change, its bytes; 0 ends
g_dbsvt dd      g_state.rn.rsp
        db      4
        dd      g_state.rn.cstk
        db      4
        dd      g_state.rn.lsp
        db      4
        dd      g_state.rn.ppip
        db      8                       ; ppip, ppend
        dd      g_state.rn.contip
        db      TRUN.fid-TRUN.contip    ; contip, contsi, ln, lnum, vpt
        dd      g_state.rn.opip
        db      4
        dd      g_state.rn.stpflg
        db      1
        dd      g_state.rn.inerr
        db      1
        dd      g_state.sg.tsp
        db      2
        dd      g_state.sg.tspb
        db      2
        dd      g_state.io.ocap
        db      TIO.inlen-TIO.ocap      ; ocap, pcol, sbx
        dd      g_state.dbg.mode
        db      4
        dd      g_evon
        db      1
        dd      g_och
        db      1
        dd      0
        .data?
g_dbsv  db      64 dup(?)
g_dbbp  db      DBBPSZ dup(?)           ; a bit per line number
        .code

        end
