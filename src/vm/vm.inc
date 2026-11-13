; ==========================================================================
; vm/vm.inc - the executor's macros, for its handlers in every module
; ==========================================================================

; A data stack cell is 8 bytes: an INT, a FLOAT, an address in its low
; dword, a DOUBLE in all of it. A register's save stays 4.
CELL    = 8
dpush   macro   x
        sub     esp,4
        push    x
        endm
dpop    macro   x
        pop     x
        add     esp,4
        endm

dispatch macro
        movzx   ebx,byte ptr [esi]
        mov     [g_state.rn.opip],esi              ; the op, for CONT after a BREAK
        inc     esi
        add     ebx,ebx
        jmp     [g_ops+ebx*2]
        endm

; EBX = the slot offset (id * 4) of the variable id at ESI, ESI past it.
vslot   macro
        movzx   ebx,word ptr [esi]
        add     esi,2
        shl     ebx,2
        endm

; ESI = a frame's or CONT's ip, DX its line, from AX: a 16-bit stream
; offset, or an offset in its line's code (cm_flush), under DCODE, which the
; direct line never has.
ipload   macro
        local   @ipa,@ipb
        mov     esi,eax
        cmp     esi,DCODE
        jae     @ipa
        cmp     edx,DLINE
        je      @ipa
        call    ex_resolve
        jmp     @ipb
@ipa:
        add     esi,[g_state.mm.cachebase]
@ipb:
        endm

; Subscript out of range unless the subscript in EAX fits a word. Kills ECX.
subchk  macro
        movsx   ecx,ax
        cmp     ecx,eax
        jne     arr_bs
        endm

; tsp to the procedure's floor: the statement's temporaries go.
tspclr  macro
        push    eax
        mov     ax,[g_state.sg.tspb]
        mov     [g_state.sg.tsp],ax
        pop     eax
        endm

; The STOP key: BREAK, CONT runs this op again.
stopchk macro
        cmp     [g_state.rn.stpflg],0
        jne     ex_brk
        endm

; EDI = element EAX of the 1-D array whose id is at ESI; else to slow with
; EBX the id * 2. sh: 1 a string, 2 an INT or a FLOAT.
a1get   macro   sh,slow
        movzx   ebx,word ptr [esi]
        add     esi,2
        add     ebx,ebx
        mov     edi,fs:[ebx*4].TVAR.vt
        cmp     byte ptr [edi],1
        jne     slow
        movzx   ecx,word ptr [edi+AH_FAST]
        cmp     eax,ecx
        jae     slow
  if sh eq 1
        lea     eax,[eax+eax*2]
        add     eax,eax
  else
        shl     eax,2
  endif
        add     eax,[edi+AH_PTR]
        mov     edi,eax
        endm
