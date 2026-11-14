; The STOP key for psys (plat/psys.h).
;
; # How it works
; Two handlers: INT 23h, Ctrl+C as DOS sees it, and the tick (INT 1Ch) scanning the BIOS key
; buffer for a Ctrl+C or Ctrl+Break (0), which DOS sees only when it reads the console. Each ORs
; the bit into the flag; no C runs in them.
        .386
        .model  flat
        public  _pstop_hook, _pstop_unhook
        .data
flag    dd      0                       ; the flag's address
bit     db      0
old23   df      0                       ; the vectors replaced, 0 none
old1c   df      0
dsv     dw      0                       ; DS, for the handlers
        .code

; void pstop_hook(volatile uint8_t *flag, uint8_t bit)
_pstop_hook:
        push    ebx
        mov     eax,[esp+8]
        mov     [flag],eax
        mov     al,[esp+12]
        mov     [bit],al
        cmp     word ptr [old23+4],0
        jne     @F
        mov     [dsv],ds
        mov     ax,0204h                ; DPMI: the protected-mode vectors
        mov     bl,23h
        int     31h
        mov     dword ptr [old23],edx
        mov     word ptr [old23+4],cx
        mov     bl,1Ch
        int     31h
        mov     dword ptr [old1c],edx
        mov     word ptr [old1c+4],cx
        mov     ax,0205h
        mov     cx,cs
        mov     bl,23h
        mov     edx,offset brk
        int     31h
        mov     bl,1Ch
        mov     edx,offset tick
        int     31h
@@:
        pop     ebx
        ret

; void pstop_unhook(void)
_pstop_unhook:
        push    ebx
        cmp     word ptr [old23+4],0
        je      @F
        mov     ax,0205h
        mov     bl,1Ch
        mov     edx,dword ptr [old1c]
        mov     cx,word ptr [old1c+4]
        int     31h
        mov     bl,23h
        mov     edx,dword ptr [old23]
        mov     cx,word ptr [old23+4]
        int     31h
        mov     word ptr [old23+4],0
@@:
        pop     ebx
        ret

; INT 23h: the STOP key.
brk:
        push    ds
        push    eax
        push    ecx
        mov     ds,cs:[dsv]
        mov     eax,[flag]
        mov     cl,[bit]
        or      [eax],cl
        pop     ecx
        pop     eax
        pop     ds
        iretd

; INT 1Ch: a Ctrl+C or Ctrl+Break in the BIOS key buffer is the STOP key.
tick:
        push    ds
        push    eax
        push    ebx
        push    ecx
        mov     ds,cs:[dsv]
        movzx   ebx,word ptr ds:[41Ah]
@tk_l:
        cmp     bx,ds:[41Ch]
        je      @tk_x
        cmp     byte ptr [ebx+400h],3
        je      @tk_s
        cmp     word ptr [ebx+400h],0
        je      @tk_s
        add     ebx,2
        cmp     bx,ds:[482h]
        jb      @tk_l
        movzx   ebx,word ptr ds:[480h]
        jmp     @tk_l
@tk_s:
        mov     eax,[flag]
        mov     cl,[bit]
        or      [eax],cl
@tk_x:
        pop     ecx
        pop     ebx
        pop     eax
        pop     ds
        jmp     fword ptr cs:[old1c]
        end
