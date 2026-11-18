;; name: gdblit.asm
;; desc: the 256-colour mode's GET and PUT: a box of rows, by dwords
;;
;; obs.: the registers are C's; gdfill.inc has the structure

                .386
                .model  flat

                public  B$LBLIT
                public  B$SOLID

                include gdfill.inc

;; ROWS op: a row of dwords and its tail bytes combined into dst by `op`
ROWS            macro   op, label
                local   l_row, l_dword, l_tail, l_byte, l_next
l_row:
                mov     ecx, [ebx].LinearBlit.count
                test    ecx, ecx
                jz      l_tail
l_dword:
                mov     eax, [esi]
                op      [edi], eax
                add     esi, 4
                add     edi, 4
                dec     ecx
                jnz     l_dword
l_tail:
                mov     ecx, [ebx].LinearBlit.tail
                test    ecx, ecx
                jz      l_next
l_byte:
                mov     al, [esi]
                op      [edi], al
                inc     esi
                inc     edi
                dec     ecx
                jnz     l_byte
l_next:
                add     esi, [ebx].LinearBlit.sstep
                add     edi, [ebx].LinearBlit.dstep
                dec     ebp
                jnz     l_row
                ret
                endm


.code
;; B$LBLIT - `rows` rows of `width` bytes from `src` to `dst`, combined by `op`
;; (0 set, 1 and, 2 or, 3 xor, 4 set inverted); a row `sstep` and `dstep`
;; bytes after the one before.
;;
;; Args:
;;     blit (eax): ptr to a LinearBlit.
;;
;; Saves:    ebx ecx edx esi edi ebp
;; Clobbers: eax, flags
B$LBLIT         proc    uses ebx ecx edx esi edi ebp
                cld
                mov     ebx, eax
                mov     ebp, [ebx].LinearBlit.rows
                test    ebp, ebp
                jz      @@done
                mov     esi, [ebx].LinearBlit.src
                mov     edi, [ebx].LinearBlit.dst
                mov     eax, [ebx].LinearBlit.count
                sub     [ebx].LinearBlit.sstep, eax     ;; from a row's end
                sub     [ebx].LinearBlit.dstep, eax
                mov     edx, eax
                and     edx, 3
                shr     eax, 2
                mov     [ebx].LinearBlit.count, eax     ;; dwords
                mov     [ebx].LinearBlit.tail, edx
                mov     ecx, [ebx].LinearBlit.op
                cmp     ecx, 1
                je      @@and
                cmp     ecx, 2
                je      @@or
                cmp     ecx, 3
                je      @@xor
                cmp     ecx, 4
                je      @@not
@@set:
                mov     ecx, eax
                rep     movsd
                mov     ecx, edx
                rep     movsb
                add     esi, [ebx].LinearBlit.sstep
                add     edi, [ebx].LinearBlit.dstep
                dec     ebp
                jnz     @@set
@@done:
                ret

@@and:
                ROWS    and
@@or:
                ROWS    or
@@xor:
                ROWS    xor
;; PRESET: each byte inverted
@@not:
                mov     ecx, [ebx].LinearBlit.count
                test    ecx, ecx
                jz      @@ntail
@@ndword:
                mov     eax, [esi]
                not     eax
                mov     [edi], eax
                add     esi, 4
                add     edi, 4
                dec     ecx
                jnz     @@ndword
@@ntail:
                mov     ecx, [ebx].LinearBlit.tail
                test    ecx, ecx
                jz      @@nnext
@@nbyte:
                mov     al, [esi]
                not     al
                mov     [edi], al
                inc     esi
                inc     edi
                dec     ecx
                jnz     @@nbyte
@@nnext:
                add     esi, [ebx].LinearBlit.sstep
                add     edi, [ebx].LinearBlit.dstep
                dec     ebp
                jnz     @@not
                ret
B$LBLIT         endp

;; B$SOLID - `rows` rows of `count` bytes from `dst` set to `color`, a row
;; `dstep` bytes after the one before: a loop for each width.
;;
;; Args:
;;     box (eax): ptr to a SolidBox.
;;
;; Saves:    ebx ecx edx esi edi ebp
;; Clobbers: eax, flags
B$SOLID         proc    uses ebx ecx edx esi edi ebp
                cld
                mov     ebx, eax
                mov     ebp, [ebx].SolidBox.rows
                test    ebp, ebp
                jz      @@done
                mov     edi, [ebx].SolidBox.dst
                mov     edx, [ebx].SolidBox.dstep
                mov     ecx, [ebx].SolidBox.count
                movzx   eax, byte ptr [ebx].SolidBox.color
                imul    eax, 01010101h
                cmp     ecx, 1
                je      @@col1
                cmp     ecx, 2
                je      @@col2
                sub     edx, ecx
                mov     esi, ecx
                shr     esi, 2
                and     ecx, 3
                mov     ebx, ecx
@@wide:
                mov     ecx, esi
                rep     stosd
                mov     ecx, ebx
                rep     stosb
                add     edi, edx
                dec     ebp
                jnz     @@wide
                ret
@@col1:
                mov     [edi], al
                add     edi, edx
                dec     ebp
                jnz     @@col1
                ret
@@col2:
                mov     [edi], ax
                add     edi, edx
                dec     ebp
                jnz     @@col2
@@done:
                ret
B$SOLID         endp
                end
