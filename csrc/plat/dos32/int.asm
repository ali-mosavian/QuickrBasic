; pdos_int: a software interrupt with the registers of a PdosRegs (plat/dos32/dos.h), cdecl.
        .386
        .model  flat
        public  _pdos_int, _pdos_ds
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
; uint16_t pdos_ds(void): DS's selector.
_pdos_ds:
        xor     eax,eax
        mov     ax,ds
        ret
        end
