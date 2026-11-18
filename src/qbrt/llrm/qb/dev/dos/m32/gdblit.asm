;; name: gdblit.asm
;; desc: the 256-colour mode's GET and PUT: a box of rows, by dwords
;;
;; obs.: the registers are C's; gdfill.inc has the structure

                .386
                .model  flat

                public  B$LBLIT
                public  B$SOLID

                include gdfill.inc

;; ROWS op, t: a row of `count` dwords and t tail bytes combined into dst by
;; `op`; count mod 4 of the dwords by a jump into four, the rest four a turn
ROWS            macro   op, t
                local   l_row, l_e3, l_e2, l_e1, l_e0, l_quad, l_next, l_tab
                mov     eax, [ebx].LinearBlit.count
                and     eax, 3
                mov     edx, l_tab[eax*4]
                shr     [ebx].LinearBlit.count, 2       ;; fours
l_row:
                mov     ecx, [ebx].LinearBlit.count
                jmp     edx
l_e3:
                mov     eax, [esi]
                op      [edi], eax
                add     esi, 4
                add     edi, 4
l_e2:
                mov     eax, [esi]
                op      [edi], eax
                add     esi, 4
                add     edi, 4
l_e1:
                mov     eax, [esi]
                op      [edi], eax
                add     esi, 4
                add     edi, 4
l_e0:
                test    ecx, ecx
                jz      l_next
l_quad:
                mov     eax, [esi]
                op      [edi], eax
                mov     eax, [esi + 4]
                op      [edi + 4], eax
                mov     eax, [esi + 8]
                op      [edi + 8], eax
                mov     eax, [esi + 12]
                op      [edi + 12], eax
                add     esi, 16
                add     edi, 16
                dec     ecx
                jnz     l_quad
l_next:
  if t eq 1
                mov     al, [esi]
                op      [edi], al
  elseif t eq 2
                mov     ax, [esi]
                op      [edi], ax
  elseif t eq 3
                mov     ax, [esi]
                op      [edi], ax
                mov     al, [esi + 2]
                op      [edi + 2], al
  endif
                add     esi, [ebx].LinearBlit.sstep
                add     edi, [ebx].LinearBlit.dstep
                dec     ebp
                jnz     l_row
                ret
l_tab           dd      l_e0, l_e1, l_e2, l_e3
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
                mov     edx, eax
                and     edx, 3
                shr     eax, 2
                mov     [ebx].LinearBlit.count, eax     ;; dwords
                mov     [ebx].LinearBlit.tail, edx
                mov     ecx, [ebx].LinearBlit.op
                dec     ecx                             ;; AND 0, OR 1, XOR 2
                cmp     ecx, 2
                ja      @@whole
                shl     eax, 2                          ;; the tail's bytes not stepped over
                sub     [ebx].LinearBlit.sstep, eax
                sub     [ebx].LinearBlit.dstep, eax
                lea     ecx, [ecx*4 + edx]
                jmp     @@combine[ecx*4]
@@whole:
                lea     ecx, [eax*4 + edx]              ;; from a row's end
                sub     [ebx].LinearBlit.sstep, ecx
                sub     [ebx].LinearBlit.dstep, ecx
                cmp     [ebx].LinearBlit.op, 4
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

@@combine       dd      @@and0, @@and1, @@and2, @@and3, @@or0, @@or1, @@or2, @@or3
                dd      @@xor0, @@xor1, @@xor2, @@xor3
@@and0:
                ROWS    and, 0
@@and1:
                ROWS    and, 1
@@and2:
                ROWS    and, 2
@@and3:
                ROWS    and, 3
@@or0:
                ROWS    or, 0
@@or1:
                ROWS    or, 1
@@or2:
                ROWS    or, 2
@@or3:
                ROWS    or, 3
@@xor0:
                ROWS    xor, 0
@@xor1:
                ROWS    xor, 1
@@xor2:
                ROWS    xor, 2
@@xor3:
                ROWS    xor, 3
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
