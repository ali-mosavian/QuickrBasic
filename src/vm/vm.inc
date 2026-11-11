; ==========================================================================
; vm/vm.inc - the executor's macros, for its handlers in every module
; ==========================================================================

dispatch macro
        movzx   ebx,byte ptr [esi]
        mov     [opip],esi              ; the op, for CONT after a BREAK
        inc     esi
        add     ebx,ebx
        jmp     [ops+ebx*2]
        endm

; EBX = the slot offset (id * 4) of the variable id at ESI, ESI past it.
vslot   macro
        movzx   ebx,byte ptr [esi]
        inc     esi
        shl     ebx,2
        endm

; ESI = a frame's or CONT's ip, DX its line, from AX: a 16-bit stream
; offset, or an offset in its line's code (cm_flush), under DCODE, which the
; direct line never has.
ipfix   macro
        local   @ipa,@ipb
        movzx   esi,ax
        cmp     esi,DCODE
        jae     @ipa
        cmp     dx,DLINE
        je      @ipa
        call    ex_resolve
        jmp     @ipb
@ipa:
        add     esi,offset cachb
@ipb:
        endm

; The STOP key: BREAK, CONT runs this op again.
stopchk macro
        cmp     [stpflg],0
        jne     ex_brk
        endm

; EDI = element AX of the 1-D array whose id is at ESI; else to slow with EBX
; the id * 2. sh: 0 an INT, 1 a string, 2 a FLOAT.
a1get   macro   sh,slow
        movzx   ebx,byte ptr [esi]
        inc     esi
        add     ebx,ebx
        mov     edi,[vt+ebx*2]
        cmp     byte ptr [edi],1
        jne     slow
        cmp     ax,[edi+AH_FAST]
        jae     slow
        movzx   eax,ax
  if sh eq 0
        add     eax,eax
  elseif sh eq 1
        lea     eax,[eax+eax*2]
        add     eax,eax
  else
        shl     eax,2
  endif
        add     eax,[edi+AH_PTR]
        mov     edi,eax
        endm
