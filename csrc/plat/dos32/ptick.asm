; The tick (INT 1Ch) for pclk and pkey (plat/pclk.h, plat/pkey.h), and the I/O ports and SYS for
; psys (plat/psys.h). The handler runs no C: it ORs pclk's bit into its flag and, with traps set,
; takes a trapped key at the BIOS buffer's head, as the asm's ev_tick.
        .386
        .model  flat
        public  _ptick_flag, _ptick_bit, _ptick_kflag, _ptick_kbit, _ptick_traps
        public  _ptick_hook, _ptick_unhook, _pkey_taken
        public  _psys_inp, _psys_outp, _psys_call
        .data
_ptick_flag  dd 0                       ; pclk_init's flag, 0 none
_ptick_kflag dd 0                       ; pkey_init's
_ptick_bit   db 0
_ptick_kbit  db 0
_ptick_traps db 64 dup(0)               ; PkeyTrap by slot: scan, shift (FFh any)
taken   dd      0                       ; the slots taken, bit n slot n
oldvec  df      0                       ; the vector replaced, 0 none
dsv     dw      0                       ; DS, for the handler
        .code

; ptick_hook: the handler in. ptick_unhook: the old one back.
_ptick_hook:
        push    ebx
        cmp     word ptr [oldvec+4],0
        jne     @F
        mov     [dsv],ds
        mov     ax,0204h
        mov     bl,1Ch
        int     31h
        mov     dword ptr [oldvec],edx
        mov     word ptr [oldvec+4],cx
        mov     ax,0205h
        mov     bl,1Ch
        mov     cx,cs
        mov     edx,offset tick
        int     31h
@@:
        pop     ebx
        ret
_ptick_unhook:
        push    ebx
        cmp     word ptr [oldvec+4],0
        je      @F
        mov     ax,0205h
        mov     bl,1Ch
        mov     edx,dword ptr [oldvec]
        mov     cx,word ptr [oldvec+4]
        int     31h
        mov     word ptr [oldvec+4],0
@@:
        pop     ebx
        ret

tick:
        push    ds
        pushad
        mov     ds,cs:[dsv]
        mov     eax,[_ptick_flag]
        test    eax,eax
        jz      @F
        mov     cl,[_ptick_bit]
        or      [eax],cl
@@:
        cmp     [_ptick_kflag],0
        je      @tk_x
        movzx   esi,word ptr ds:[41Ah]  ; the buffer's head
        cmp     si,ds:[41Ch]
        je      @tk_x
        mov     dl,ds:[esi+401h]        ; its scan code
        mov     dh,ds:[417h]            ; the shift flags
        and     dh,0Fh
        mov     ecx,1
@tk_l:
        mov     al,[_ptick_traps+ecx*2]
        test    al,al
        jz      @tk_n
        cmp     al,dl
        jne     @tk_n
        mov     al,[_ptick_traps+ecx*2+1]
        cmp     al,0FFh
        je      @tk_y
        and     al,0Fh
        cmp     al,dh
        je      @tk_y
@tk_n:
        inc     ecx
        cmp     ecx,31
        jbe     @tk_l
        jmp     @tk_x
@tk_y:
        add     esi,2                   ; taken from the buffer
        cmp     si,ds:[482h]
        jb      @F
        mov     si,ds:[480h]
@@:
        mov     ds:[41Ah],si
        bts     [taken],ecx
        mov     eax,[_ptick_kflag]
        mov     cl,[_ptick_kbit]
        or      [eax],cl
@tk_x:
        popad
        pop     ds
        jmp     fword ptr cs:[oldvec]

; uint32_t pkey_taken(void): the slots taken, cleared.
_pkey_taken:
        xor     eax,eax
        xchg    eax,[taken]
        ret

; uint8_t psys_inp(uint16_t port); void psys_outp(uint16_t port, uint8_t v)
_psys_inp:
        mov     edx,[esp+4]
        xor     eax,eax
        in      al,dx
        ret
_psys_outp:
        mov     edx,[esp+4]
        mov     eax,[esp+8]
        out     dx,al
        ret

; void psys_call(Addr a): SYS, every register kept.
_psys_call:
        mov     eax,[esp+4]
        pushad
        call    eax
        popad
        ret
        end
