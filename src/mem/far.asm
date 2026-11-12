; ==========================================================================
; far.asm - copies: the heap, the program store
; ==========================================================================
; holds:    mem_fs, mem_lin, mem_movs, mem_cpy, mem_up, mem_zero, mem_line,
;           mem_pmov
; uses:     progb
; data:     Flat: a linear address is the address. The program's lines are
;           16-bit offsets from progb.
; calls:
;   mem_line, mem_pmov mem_movs
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; mem_line - copy line BX of the program to EDI.
;
; in:     EBX  the line, an offset in progb
; keeps:  EBX, ESI
; ::::::::::::::
mem_line:
        push    esi
        lea     esi,[g_progb+ebx]
        movzx   ecx,word ptr [esi]
        call    mem_movs
        pop     esi
        ret

; ::::::::::::::
; mem_pmov - mem_movs inside the program store: ESI, EDI offsets in progb.
;
; keeps:  all but ECX; ESI, EDI past the copy, as offsets
; ::::::::::::::
mem_pmov:
        add     esi,offset g_progb
        add     edi,offset g_progb
        call    mem_movs
        sub     esi,offset g_progb
        sub     edi,offset g_progb
        ret

; ::::::::::::::
; mem_movs - rep movsb from ESI to EDI, either direction.
;
; in:     ECX  the count
; keeps:  all but ECX, ESI, EDI
; ::::::::::::::
mem_movs:
        rep     movsb
        ret

; ::::::::::::::
; mem_fs - EBX = the address EAX (the 16-bit build's FS:BX).
;
; keeps:  all but EBX
; ::::::::::::::
mem_fs:
        mov     ebx,eax
        ret

; ::::::::::::::
; mem_lin - EAX = EDI (the 16-bit build's FS:DI).
;
; keeps:  all but EAX
; ::::::::::::::
mem_lin:
        mov     eax,edi
        ret

; ::::::::::::::
; mem_cpy - copy CX bytes, not overlapping.
;
; in:     EAX  the source
;         EDX  the destination
;         CX   the count
; keeps:  EAX, EDX, ESI, EDI
; ::::::::::::::
mem_cpy:
        push    esi
        push    edi
        mov     esi,eax
        mov     edi,edx
        movzx   ecx,cx
        rep     movsb
        pop     edi
        pop     esi
        ret

; ::::::::::::::
; mem_up - copy CX bytes up from EAX to EDX; they may overlap.
;
; in:     EAX  the source
;         EDX  the destination, not under EAX
;         CX   the count
; keeps:  EAX, EDX, ESI, EDI
; ::::::::::::::
mem_up:
        movzx   ecx,cx
        jecxz   @mup_r
        push    esi
        push    edi
        lea     esi,[eax+ecx-1]
        lea     edi,[edx+ecx-1]
        std
        rep     movsb
        cld
        pop     edi
        pop     esi
@mup_r:
        ret

; ::::::::::::::
; mem_zero - zero ECX bytes from EAX.
;
; keeps:  ESI, EDI
; ::::::::::::::
mem_zero:
        push    edi
        mov     edi,eax
        add     eax,ecx
        push    eax
        xor     eax,eax
        rep     stosb
        pop     eax
        pop     edi
        ret

        end
