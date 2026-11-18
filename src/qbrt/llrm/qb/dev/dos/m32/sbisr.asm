;; name: B$SBISR, B$SBHOOK, B$SBUNHOOK, B$IRQSAVE, B$IRQRESTORE, B$DSSEL
;; desc: the Sound Blaster's interrupt, flat, and the interrupt flag kept
;;
;; chng: nov/26 written [ali]
;; obs.: the card interrupts when it has played half its DMA buffer. The
;;       handler keeps every register, puts the flat data selector (its own
;;       stack's) in ds and es, and calls the C routine B$SBFILL (sb.c), which
;;       fills that half, acknowledges the card and ends the interrupt at the
;;       PICs. The vector is the protected-mode one (DPMI 0204h, 0205h). The
;;       data selector is the one B$SBHOOK saw: the interrupt can come while
;;       the host runs on a stack of its own.

                .386
                .model  flat

                extrn   B$SBFILL:near

                public  B$SBISR
                public  B$SBHOOK
                public  B$SBUNHOOK
                public  B$IRQSAVE
                public  B$IRQRESTORE
                public  B$DSSEL

DPMI_GET_VECTOR equ     0204h
DPMI_SET_VECTOR equ     0205h

.data
vector          dd      0                       ;; the vector taken
selector        dw      0                       ;; the program's data selector
previous        dd      0                       ;; the handler it had: offset
                dw      0                       ;; and selector

.code
;; B$SBISR - the card's handler: fill the half played.
;;
;; Args:
;;     None.
;;
;; Saves:    all registers (pushad, ds, es)
;; Clobbers: none
B$SBISR         proc

                pushad
                push    ds
                push    es
                mov     ax, cs:selector         ;; flat: cs and ds share a base
                mov     ds, ax
                mov     es, ax
                cld
                call    B$SBFILL
                pop     es
                pop     ds
                popad
                iretd
B$SBISR         endp

;; B$SBHOOK - point a protected-mode interrupt vector at B$SBISR.
;;
;; Keeps the handler it had for B$SBUNHOOK.
;;
;; Args:
;;     number (eax): the interrupt's number.
;;
;; Saves:    ebx ecx edx
;; Clobbers: eax, flags
;; Patches:  vector, previous, selector.
B$SBHOOK        proc uses ebx ecx edx

                mov     vector, eax
                mov     selector, ds
                mov     ebx, eax
                mov     eax, DPMI_GET_VECTOR
                int     31h
                mov     previous, edx
                mov     word ptr previous+4, cx
                mov     eax, DPMI_SET_VECTOR
                mov     ebx, vector
                mov     ecx, cs
                mov     edx, offset B$SBISR
                int     31h
                ret
B$SBHOOK        endp

;; B$SBUNHOOK - give the vector B$SBHOOK took its handler back.
;;
;; Args:
;;     None.
;;
;; Saves:    ebx ecx edx
;; Clobbers: eax, flags
B$SBUNHOOK      proc uses ebx ecx edx

                mov     eax, DPMI_SET_VECTOR
                mov     ebx, vector
                movzx   ecx, word ptr previous+4
                mov     edx, previous
                int     31h
                ret
B$SBUNHOOK      endp

;; B$IRQSAVE - the flags, then interrupts off.
;;
;; Returns:
;;     eax: the flags as they were, for B$IRQRESTORE.
;;
;; Saves:    all but eax
;; Clobbers: eax
B$IRQSAVE       proc

                pushfd
                pop     eax
                cli
                ret
B$IRQSAVE       endp

;; B$IRQRESTORE - the flags (the interrupt flag) as B$IRQSAVE found them.
;;
;; Args:
;;     flags (eax): what B$IRQSAVE returned.
;;
;; Saves:    all
;; Clobbers: flags
B$IRQRESTORE    proc

                push    eax
                popfd
                ret
B$IRQRESTORE    endp

;; B$DSSEL - the data selector.
;;
;; Returns:
;;     eax: ds.
;;
;; Saves:    all but eax
;; Clobbers: eax
B$DSSEL         proc

                xor     eax, eax
                mov     ax, ds
                ret
B$DSSEL         endp
                end
