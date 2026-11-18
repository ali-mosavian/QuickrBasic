;; name: gdblit.asm
;; desc: the 256-colour mode's GET and PUT: a box of rows, by dwords
;;
;; obs.: the registers are C's; gdfill.inc has the structure

                .386
                .model  flat

                public  B$LBLIT
                public  B$SOLID

                include gdfill.inc

;; TAIL op, t: a row's last t bytes at esi, combined into edi's; then the
;; next row's, ebp rows in all
TAIL            macro   op, t, l_row
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
                endm

;; BYTES op, t: rows of t bytes and no dwords
BYTES           macro   op, t
                local   l_row
l_row:
                TAIL    op, t, l_row
                endm

;; ROWS op, t: rows of dwords and t tail bytes combined into dst by `op`, eight
;; dwords a turn. esi and edi are at the end of a row's dwords; ecx counts up
;; to 0 from the tail field (the dwords negated, less the pairs skipped), the
;; first turn entered at edx, ENTRY's pair from `name`.
PAIRS           equ     8
PAIR0           equ     6                       ;; mov eax, [esi + ecx*4]; op [edi + ecx*4], eax
PAIRN           equ     8                       ;; the same with a disp8

ROWS            macro   op, t, name
                local   l_row, l_loop
l_row:
                mov     ecx, [ebx].LinearBlit.tail
                jmp     edx
name:
l_loop:
                mov     eax, [esi + ecx*4]
                op      [edi + ecx*4], eax
  j = 1
  repeat PAIRS - 1
                mov     eax, [esi + ecx*4 + j*4]
                op      [edi + ecx*4 + j*4], eax
    j = j + 1
  endm
                .erre   ($ - l_loop) eq PAIR0 + (PAIRS - 1) * PAIRN
                add     ecx, PAIRS
                jnz     l_loop
                TAIL    op, t, l_row
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
                lea     esi, [esi + eax*4]
                lea     edi, [edi + eax*4]
                lea     ecx, [ecx*4 + edx]              ;; op * 4 + tail
                neg     eax
                jz      @@flat
                mov     edx, eax
                and     edx, PAIRS - 1                  ;; the first turn's pairs skipped
                sub     eax, edx
                mov     [ebx].LinearBlit.tail, eax
                mov     ecx, @@loops[ecx*4]
                add     ecx, @@entry[edx*4]
                mov     edx, ecx
                mov     ecx, eax
                jmp     edx
@@flat:
                jmp     @@flats[ecx*4]
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

@@loops         dd      @@and0, @@and1, @@and2, @@and3, @@or0, @@or1
                dd      @@or2, @@or3, @@xor0, @@xor1, @@xor2, @@xor3
@@flats         dd      @@andf0, @@andf1, @@andf2, @@andf3, @@orf0, @@orf1
                dd      @@orf2, @@orf3, @@xorf0, @@xorf1, @@xorf2, @@xorf3
;; a turn's entry with j pairs skipped
@@entry         dd      0
  j = 1
  repeat PAIRS - 1
                dd      PAIR0 + (j - 1) * PAIRN
    j = j + 1
  endm
                ROWS    and, 0, @@and0
@@andf0:
                BYTES   and, 0
                ROWS    and, 1, @@and1
@@andf1:
                BYTES   and, 1
                ROWS    and, 2, @@and2
@@andf2:
                BYTES   and, 2
                ROWS    and, 3, @@and3
@@andf3:
                BYTES   and, 3
                ROWS    or, 0, @@or0
@@orf0:
                BYTES   or, 0
                ROWS    or, 1, @@or1
@@orf1:
                BYTES   or, 1
                ROWS    or, 2, @@or2
@@orf2:
                BYTES   or, 2
                ROWS    or, 3, @@or3
@@orf3:
                BYTES   or, 3
                ROWS    xor, 0, @@xor0
@@xorf0:
                BYTES   xor, 0
                ROWS    xor, 1, @@xor1
@@xorf1:
                BYTES   xor, 1
                ROWS    xor, 2, @@xor2
@@xorf2:
                BYTES   xor, 2
                ROWS    xor, 3, @@xor3
@@xorf3:
                BYTES   xor, 3
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
