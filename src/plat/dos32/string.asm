; lib/string.c's block routines for the 386 (-mregparm=3: eax, edx, ecx), the CPU's string instructions where
; they run forward. The direction flag stays clear: an interrupt may find it at any instruction.
        .386
        .model  flat
        public  _memcpy, _memmove, _memset, _memcmp, _strlen

        .code
; memcpy (dst eax, src edx, n ecx): dst. Under SMALL bytes a byte at a time; else bytes to dst's dword
; boundary, its dwords, the last bytes.
SMALL   equ     16

_memcpy:
        push    edi
        push    esi
        mov     edi, eax
        mov     esi, edx
forward:
        cmp     ecx, SMALL
        jb      fw_bytes
        mov     edx, edi
        neg     edx
        and     edx, 3                  ; bytes to dst's boundary
        sub     ecx, edx
        xchg    ecx, edx
        rep     movsb
        mov     ecx, edx
        shr     ecx, 2
        rep     movsd
        mov     ecx, edx
        and     ecx, 3
fw_bytes:
        rep     movsb
        pop     esi
        pop     edi
        ret

; memmove (dst eax, src edx, n ecx): forward unless dst is inside src's n bytes; then from the end: under SMALL
; bytes a byte at a time; else bytes down to dst's end's dword boundary, the dwords four a turn, the first bytes.
; Each read is below every write before it.
_memmove:
        push    edi
        push    esi
        mov     edi, eax
        mov     esi, edx
        neg     edx
        add     edx, eax                ; dst - src
        cmp     edx, ecx
        jae     forward                 ; dst below src (wrapped) or past its end
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

; memset (dst eax, c edx, n ecx): dst; aligned as memcpy
_memset:
        push    edi
        push    eax
        mov     edi, eax
        movzx   eax, dl
        imul    eax, 01010101h
        cmp     ecx, SMALL
        jb      st_bytes
        mov     edx, edi
        neg     edx
        and     edx, 3
        sub     ecx, edx
        xchg    ecx, edx
        rep     stosb
        mov     ecx, edx
        shr     ecx, 2
        rep     stosd
        mov     ecx, edx
        and     ecx, 3
st_bytes:
        rep     stosb
        pop     eax
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
