; pdos_int: a software interrupt with the registers of a PdosRegs (plat/dos32/dos.h), cdecl.
;
; # How it works
; The vector is patched into the INT instruction's operand, as the code is writable under DOS/32A.
; Also holds pdos_ds, pdos_cmdline and pdos_environ.
        .386
        .model  flat
        public  _pdos_int, _pdos_ds, _pdos_cmdline, _pdos_environ
        extern  LL$PSP:word
        extern  LL$ENVIRONMENT_READ:near
        .code
; int pdos_int(int vector, PdosRegs *r): the registers in, out; the flags returned.
_pdos_int:
        push    ebx
        push    esi
        push    edi
        push    ebp
        mov     eax,[esp+20]
        mov     byte ptr [@vec],al      ; the vector: the code is writable under DOS/32A
        jmp     short @F                ; the prefetch queue flushed
@@:
        mov     ebp,[esp+24]
        mov     eax,[ebp]
        mov     ebx,[ebp+4]
        mov     ecx,[ebp+8]
        mov     edx,[ebp+12]
        mov     esi,[ebp+16]
        mov     edi,[ebp+20]
        push    ebp
        db      0CDh                    ; int n
@vec    db      0
        pop     ebp
        mov     [ebp],eax
        mov     [ebp+4],ebx
        mov     [ebp+8],ecx
        mov     [ebp+12],edx
        mov     [ebp+16],esi
        mov     [ebp+20],edi
        pushfd
        pop     eax
        mov     [ebp+24],eax
        pop     ebp
        pop     edi
        pop     esi
        pop     ebx
        ret
; uint32_t pdos_environ(uint32_t index, uint8_t *buf, uint32_t cap): llrm's, its arguments as given.
_pdos_environ:
        jmp     LL$ENVIRONMENT_READ
; uint16_t pdos_ds(void): DS's selector.
_pdos_ds:
        xor     eax,eax
        mov     ax,ds
        ret
; uint32_t pdos_cmdline(char *buf, uint32_t max): the PSP's command tail without its leading
; blanks, at most max bytes; its length. (llrm's LL$COMMAND_LINE reads its arguments a slot high.)
_pdos_cmdline:
        push    esi
        push    edi
        push    es
        mov     es,[LL$PSP]
        movzx   ecx,byte ptr es:[80h]
        mov     esi,81h
@@:
        jecxz   @F
        cmp     byte ptr es:[esi],' '
        jne     @F
        inc     esi
        dec     ecx
        jmp     @B
@@:
        cmp     ecx,[esp+20]
        jbe     @F
        mov     ecx,[esp+20]
@@:
        mov     edi,[esp+16]
        mov     eax,ecx
        jecxz   @cmd_r
@@:
        mov     dl,es:[esi]
        mov     [edi],dl
        inc     esi
        inc     edi
        loop    @B
@cmd_r:
        pop     es
        pop     edi
        pop     esi
        ret
        end
