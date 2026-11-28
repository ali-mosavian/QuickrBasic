; memcpy and memset for the 386: under SMALL bytes a byte at a time; else bytes to dst's dword boundary,
; its dwords, the last bytes, each a rep string instruction. The direction flag stays clear: an interrupt
; may find it at any instruction.
;
; The runtime passes the arguments in registers (-mregparm=3: eax, edx, ecx); gfx's slice and the
; drivers, assembled with -DCDECL, on the stack.
        .386
        .model  flat
        public  _memcpy, _memset

SMALL   equ     16

; cdecl's arguments into regparm's registers, before anything is pushed
ifdef CDECL
from_stack macro
        mov     eax, [esp + 4]
        mov     edx, [esp + 8]
        mov     ecx, [esp + 12]
        endm
else
from_stack macro
        endm
endif

; n ecx bytes at edi by op (movs, stos); edx spent
spans   macro   op
        local   few
        cmp     ecx, SMALL
        jb      few
        mov     edx, ecx
        mov     ecx, edi
        neg     ecx
        and     ecx, 3                  ; bytes to dst's boundary
        sub     edx, ecx
        rep     op&b
        mov     ecx, edx
        shr     ecx, 2
        rep     op&d
        mov     ecx, edx
        and     ecx, 3
few:
        rep     op&b
        endm

        .code
; memcpy (dst eax, src edx, n ecx): dst
_memcpy:
        from_stack
        push    edi
        push    esi
        mov     edi, eax
        mov     esi, edx
        spans   movs
        pop     esi
        pop     edi
        ret

; memset (dst eax, c edx, n ecx): dst
_memset:
        from_stack
        push    edi
        push    eax
        mov     edi, eax
        movzx   eax, dl
        imul    eax, 01010101h
        spans   stos
        pop     eax
        pop     edi
        ret
        end
