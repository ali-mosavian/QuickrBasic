; ==========================================================================
; tab.asm - tables: parallel arrays, one entry each per id, in one block
; ==========================================================================
; holds:    tab_new, tab_room, tab_place
; uses:     alloc_new, alloc_grow
; data:     A TTAB's arrays lie in its block one after another, each cap
;           entries of its size. It grows as its block does: the arrays
;           after the first move up to their new places (tab_place, from
;           the block's moved routine). The first array never moves while
;           the block does not.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; tab_new - table ESI's block, ECX entries; sz, narr, max set.
;
; in:     EBX  the block's moved routine
; keeps:  ESI
; error:  Out of memory
; ::::::::::::::
tab_new:
        call    @tb_row
        imul    ecx,eax
        call    alloc_new
        mov     [esi].TTAB.hnd,eax
        mov     [esi].TTAB.base,edx
        and     [esi].TTAB.cap,0
        jmp     tab_place

; ::::::::::::::
; tab_room - table ESI ECX entries at least.
;
; out:    CF   set: no room, past max or the heap not empty
; keeps:  all
; ::::::::::::::
tab_room:
        cmp     ecx,[esi].TTAB.cap
        jbe     @tbr_r
        cmp     [esi].TTAB.max,ecx
        jb      @tbr_f
        pushad
        call    @tb_row
        imul    ecx,eax
        mov     eax,[esi].TTAB.hnd
        call    alloc_grow
        popad
        ret
@tbr_r:
        clc
        ret
@tbr_f:
        stc
        ret

; ::::::::::::::
; tab_place - table ESI where its block now is: its arrays moved up to a
; larger cap, at.
;
; out:    EDX  the block's base before
; keeps:  ESI
; ::::::::::::::
tab_place:
        call    @tb_row
        mov     ebx,eax                 ; EBX the row
        mov     eax,[esi].TTAB.hnd
        imul    eax,sizeof TALLOC
        mov     edi,[g_state.alloc.tab+eax].TALLOC.base
        mov     eax,[g_state.alloc.tab+eax].TALLOC.cap
        xor     edx,edx
        div     ebx
        cmp     eax,[esi].TTAB.max
        jbe     @F
        mov     eax,[esi].TTAB.max
@@:
        mov     ebp,eax                 ; EBP the new cap
        mov     ecx,[esi].TTAB.narr     ; the last array first
@tbp_m:
        dec     ecx
        jz      @tbp_a
        push    ecx
        xor     eax,eax                 ; EAX the sizes before it
        xor     edx,edx
@@:
        add     eax,[esi+edx*4].TTAB.sz
        inc     edx
        cmp     edx,ecx
        jb      @B
        push    esi
        push    edi
        mov     edx,[esi+ecx*4].TTAB.sz
        imul    edx,[esi].TTAB.cap      ; EDX its bytes
        push    edx
        mov     edx,eax
        imul    eax,[esi].TTAB.cap
        imul    edx,ebp
        pop     ecx
        lea     esi,[edi+eax]
        add     edi,edx
        lea     esi,[esi+ecx-1]
        lea     edi,[edi+ecx-1]
        std
        rep     movsb
        cld
        pop     edi
        pop     esi
        pop     ecx
        jmp     @tbp_m
@tbp_a:
        mov     [esi].TTAB.cap,ebp
        mov     edx,[esi].TTAB.base
        mov     [esi].TTAB.base,edi
        xor     ecx,ecx                 ; at: the arrays' places
@@:
        mov     [esi+ecx*4].TTAB.at,edi
        mov     eax,[esi+ecx*4].TTAB.sz
        imul    eax,ebp
        add     edi,eax
        inc     ecx
        cmp     ecx,[esi].TTAB.narr
        jb      @B
        ret

@tb_row:                                ; EAX table ESI's bytes per entry
        push    ecx
        xor     eax,eax
        mov     ecx,[esi].TTAB.narr
@@:
        add     eax,[esi+ecx*4-4].TTAB.sz
        loop    @B
        pop     ecx
        ret

        end
