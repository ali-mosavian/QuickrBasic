; ==========================================================================
; event.asm - event trapping: ON TIMER, KEY, COM, PEN, STRIG GOSUB
; ==========================================================================
; holds:    op_onev, sc_evst, sc_keydef, ev_tick, ev_run, ev_ret, ev_clr
; uses:     the BIOS key buffer and flags at 0040:0017, 001Ah..; stpflg
; data:     evtab: a TEVSLOT by slot: line (0 none), state (0 OFF, 1 ON, 2
;           STOP), flags (1 pending, 2 its handler running). Slots: 0 TIMER,
;           1..31 KEY(n), 32..33 COM(n), 34 PEN, 35..38 STRIG(0, 2, 4, 6).
;           ev_tick, from the tick interrupt, counts the timer down and takes
;           a trapped key from the buffer's head; an event an ON slot can run
;           sets stpflg's bit 1, so the next STOP check runs ev_run: a GOSUB
;           to its line that comes back to the op that checked.
; ==========================================================================

include x86f.inc
        .code

NEV     = 39
        .data?
g_evtab db      NEV*EVSZ dup(?)         ; before its uses: [g_evtab+i].TEVSLOT
        .code

; ::::::::::::::
; ev_slot - the slot of kind ECX (EK_*), n EAX.
;
; out:    EBX  the slot
; keeps:  all but EBX
; error:  Illegal function call  no such n
; ::::::::::::::
ev_slot:
        xor     ebx,ebx
        cmp     ecx,EK_KEY
        jb      @evs_r
        ja      @F
        lea     ebx,[eax-1]             ; KEY 1..31
        cmp     ebx,31
        jae     err_iq
        inc     ebx
        ret
@@:
        cmp     ecx,EK_PEN
        jb      @evs_c
        ja      @evs_s
        mov     ebx,34
        ret
@evs_c:                                 ; COM 1..2
        lea     ebx,[eax-1]
        cmp     ebx,2
        jae     err_iq
        add     ebx,32
        ret
@evs_s:                                 ; STRIG 0..7, by twos
        cmp     eax,8
        jae     err_iq
        mov     ebx,eax
        shr     ebx,1
        add     ebx,35
@evs_r:
        ret

; ::::::::::::::
; op_onev - ONEV kind num addr ( n -- ): ON event(n) GOSUB's line, 0 none;
; ON TIMER's n the seconds, 1..86400.
;
; error:  Illegal function call  no such n
;         Label not defined       no such line
; ::::::::::::::
op_onev:
        dpop    eax
        movzx   ecx,byte ptr [esi]
        inc     esi
        call    ev_slot
        test    ebx,ebx
        jnz     @onv_l
        dec     eax                     ; TIMER: 1..86400 s, in ticks
        cmp     eax,86400
        jae     err_iq
        inc     eax
        imul    eax,eax,182065
        add     eax,5000
        xor     edx,edx
        mov     ecx,10000
        div     ecx
        mov     [g_evti],eax
        mov     [g_evtc],eax
@onv_l:
        xor     edx,edx
        cmp     word ptr [esi],0
        jne     @F
        add     esi,sizeof TLREF
        jmp     @onv_s
@@:
        push    ebx
        call    ex_target
        mov     edx,ebx
        pop     ebx
@onv_s:
        mov     [g_evtab+ebx*EVSZ].TEVSLOT.line,edx
        dispatch

; ::::::::::::::
; sc_evst - event(n) ON, OFF, STOP ( n ks -- ): ks the kind * 4 + the state.
; OFF drops one pending; ON runs one.
;
; error:  Illegal function call  no such n
; ::::::::::::::
sc_evst:
        dpop    ecx
        dpop    eax
        mov     edx,ecx
        shr     ecx,2
        and     edx,3
        call    ev_slot
        lea     ebx,[g_evtab+ebx*EVSZ]
        mov     al,[ebx].TEVSLOT.state
        mov     [ebx].TEVSLOT.state,dl
        test    dl,dl
        jnz     @F
        and     [ebx].TEVSLOT.flags,not 1
@@:
        cmp     ebx,offset g_evtab      ; the TIMER started: a whole interval
        jne     @F
        test    al,al
        jnz     @F
        mov     eax,[g_evti]
        mov     [g_evtc],eax
@@:
        call    ev_sig
        jmp     ex_lp

; ::::::::::::::
; ev_sig - the slot at EBX: an event to run now, stpflg bit 1.
;
; keeps:  all
; ::::::::::::::
ev_sig:
        cmp     [ebx].TEVSLOT.line,0
        je      @F
        cmp     word ptr [ebx].TEVSLOT.state,1+1*256        ; ON, pending, not running
        jne     @F
        or      [g_state.rn.stpflg],SF_EV
@@:
        ret

; ::::::::::::::
; sc_keydef - KEY n, s ( n s -- ): KEY 15..25's key, s its shift flags and
; scan code; another n's text is the screen's, not kept.
;
; error:  Illegal function call  n not 1..25, 30, 31
; ::::::::::::::
sc_keydef:
        dpop    eax
        call    str_get
        mov     edx,[g_state.sg.spt]
        dpop    ebx
        dec     ebx
        cmp     ebx,25
        jb      @F
        cmp     ebx,29
        jb      err_iq
        cmp     ebx,31
        jae     err_iq
@@:
        sub     ebx,14
        jb      ex_lp
        cmp     ebx,11
        jae     ex_lp
        cmp     cx,2
        jb      err_iq
        mov     ax,[edx]
        mov     [g_evuk+ebx*2],ax
        jmp     ex_lp

; ::::::::::::::
; ev_tick - the tick interrupt's: the TIMER counted; a trapped key at the
; buffer's head taken.
;
; keeps:  all
; ::::::::::::::
ev_tick:
        cmp     [g_evon],0
        je      @evt_r
        pushad
        cmp     [g_evtab].TEVSLOT.state,0
        je      @evt_k
        dec     [g_evtc]
        jnz     @evt_k
        mov     eax,[g_evti]
        mov     [g_evtc],eax
        mov     ebx,offset g_evtab
        call    @evt_p
@evt_k:
        movzx   esi,word ptr ds:[41Ah]
        cmp     si,ds:[41Ch]
        je      @evt_x
        mov     dl,ds:[esi+401h]        ; the scan code
        mov     dh,ds:[417h]            ; the shift flags
        and     dh,0Fh
        mov     ecx,1
@evt_l:
        cmp     [g_evtab+ecx*EVSZ].TEVSLOT.state,0
        je      @evt_n
        movzx   eax,[g_evsc+ecx]
        cmp     ecx,15
        jb      @evt_m
        cmp     ecx,25
        ja      @evt_m
        mov     ax,[g_evuk+ecx*2-30]
        and     al,0Fh
        cmp     al,dh
        jne     @evt_n
        mov     al,ah
@evt_m:
        test    al,al
        jz      @evt_n
        cmp     al,dl
        je      @evt_y
@evt_n:
        inc     ecx
        cmp     ecx,31
        jbe     @evt_l
        jmp     @evt_x
@evt_y:
        add     esi,2                   ; taken from the buffer
        cmp     si,ds:[482h]
        jb      @F
        mov     si,ds:[480h]
@@:
        mov     ds:[41Ah],si
        lea     ebx,[g_evtab+ecx*EVSZ]
        call    @evt_p
@evt_x:
        popad
@evt_r:
        ret
@evt_p:                                 ; the slot at EBX: pending
        or      [ebx].TEVSLOT.flags,1
        jmp     ev_sig

; ::::::::::::::
; ev_run - stpflg's bit 1: an ON slot's pending event runs, a GOSUB to its
; line back to the op at opip; none: that op.
; ::::::::::::::
ev_run:
        and     [g_state.rn.stpflg],not SF_EV
        mov     esi,[g_state.rn.opip]
        mov     ebx,offset g_evtab
@evr_l:
        cmp     [ebx].TEVSLOT.line,0
        je      @F
        cmp     word ptr [ebx].TEVSLOT.state,1+1*256
        je      @evr_g
@@:
        add     ebx,sizeof TEVSLOT
        cmp     ebx,offset g_evtab+NEV*EVSZ
        jb      @evr_l
        jmp     ex_lp
@evr_g:
        mov     [ebx].TEVSLOT.flags,2      ; running, no longer pending
        mov     al,M_EV
        call    ex_gsf
        mov     eax,ebx
        sub     eax,offset g_evtab
        shr     eax,3
        mov     [edi+F_EVN],al
        mov     ebx,[ebx].TEVSLOT.line
        jmp     ex_jump

; ::::::::::::::
; ev_ret - RETURN from the handler of the event frame at EBX: its slot's
; next event can run.
;
; keeps:  all
; ::::::::::::::
ev_ret:
        push    ebx
        movzx   ebx,byte ptr [ebx+F_EVN]
        lea     ebx,[g_evtab+ebx*EVSZ]
        and     [ebx].TEVSLOT.flags,not 2
        call    ev_sig
        pop     ebx
        ret

; ::::::::::::::
; ev_clr - no events: a run's start, READY.
;
; keeps:  all
; ::::::::::::::
ev_clr:
        push    eax
        push    ecx
        push    edi
        mov     edi,offset g_evtab
        mov     ecx,NEV*EVSZ/4
        xor     eax,eax
        rep     stosd
        mov     [g_evon],1
        pop     edi
        pop     ecx
        pop     eax
        ret

        .data
; KEY n's scan code: F1..F10, up, left, right, down, 15..25 the user's,
; F11, F12
g_evsc  db      0,3Bh,3Ch,3Dh,3Eh,3Fh,40h,41h,42h,43h,44h,48h,4Bh,4Dh,50h
        db      11 dup(0),0,0,0,0,85h,86h
.erre ($ - g_evsc) EQ 32
g_evon  db      0                       ; ev_tick: evtab is set
        .data?
g_evti  dd      ?                       ; ON TIMER's ticks, the count down
g_evtc  dd      ?
g_evuk  dw      11 dup(?)               ; KEY 15..25: shift flags, scan code
        .code

        end
