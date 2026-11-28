; lib/string.c's block routines for the 386 (-mregparm=3: eax, edx, ecx), the CPU's string instructions where
; they run forward; memcpy and memset are mem.asm's. The direction flag stays clear: an interrupt may find it
; at any instruction.
        .386
        .model  flat
        public  _memmove, _memcmp, _memchr, _strlen
        extern  _memcpy:near

SMALL   equ     16

        .code
; memmove (dst eax, src edx, n ecx): memcpy unless dst is inside src's n bytes; then from the end: under SMALL
; bytes a byte at a time; else bytes down to dst's end's dword boundary, the dwords four a turn, the first bytes.
; Each read is below every write before it.
_memmove:
        sub     eax, edx                ; dst - src
        cmp     eax, ecx
        lea     eax, [eax + edx]        ; dst; lea leaves the flags
        jae     _memcpy                 ; dst below src (wrapped) or past its end
        push    edi
        push    esi
        mov     edi, eax
        mov     esi, edx
        push    eax
        add     esi, ecx
        add     edi, ecx
        cmp     ecx, SMALL
        jb      bk_last
        mov     edx, edi
        and     edx, 3                  ; bytes down to dst's boundary
        sub     ecx, edx
        test    edx, edx
        jz      bk_dwords
bk_align:
        dec     esi
        dec     edi
        mov     al, [esi]
        mov     [edi], al
        dec     edx
        jnz     bk_align
bk_dwords:
        mov     edx, ecx
        shr     ecx, 4                  ; four-dword turns
        test    edx, 4 * 3
        jz      bk_turns
bk_one:
        sub     esi, 4
        sub     edi, 4
        mov     eax, [esi]
        mov     [edi], eax
        sub     edx, 4
        test    edx, 4 * 3
        jnz     bk_one
bk_turns:
        test    ecx, ecx
        jz      bk_rest
bk_turn:
        mov     eax, [esi - 4]
        mov     [edi - 4], eax
        mov     eax, [esi - 8]
        mov     [edi - 8], eax
        mov     eax, [esi - 12]
        mov     [edi - 12], eax
        mov     eax, [esi - 16]
        mov     [edi - 16], eax
        sub     esi, 16
        sub     edi, 16
        dec     ecx
        jnz     bk_turn
bk_rest:
        mov     ecx, edx
        and     ecx, 3
bk_last:
        test    ecx, ecx
        jz      bk_done
bk_byte:
        dec     esi
        dec     edi
        mov     al, [esi]
        mov     [edi], al
        dec     ecx
        jnz     bk_byte
bk_done:
        pop     eax
        pop     esi
        pop     edi
        ret

; memcmp (a eax, b edx, n ecx): the first differing bytes' difference, 0 none
_memcmp:
        push    edi
        push    esi
        mov     esi, eax
        mov     edi, edx
        xor     eax, eax
        test    ecx, ecx
        jz      mc_same
        repe    cmpsb
        je      mc_same
        movzx   eax, byte ptr [esi - 1]
        movzx   edx, byte ptr [edi - 1]
        sub     eax, edx
mc_same:
        pop     esi
        pop     edi
        ret

; memchr (s eax, c edx, n ecx): the first byte c's address, 0 none
_memchr:
        push    edi
        mov     edi, eax
        mov     eax, edx
        test    ecx, ecx
        jz      mh_none
        repne   scasb
        jne     mh_none
        lea     eax, [edi - 1]
        pop     edi
        ret
mh_none:
        xor     eax, eax
        pop     edi
        ret

; strlen (s eax): its bytes before the 0
_strlen:
        push    edi
        mov     edi, eax
        xor     eax, eax
        or      ecx, -1
        repne   scasb
        not     ecx
        lea     eax, [ecx - 1]
        pop     edi
        ret
        end
