; ==========================================================================
; alloc.asm - the allocator: blocks in the heap's arena
; ==========================================================================
; holds:    alloc_new, alloc_grow, alloc_sel, sel_set
; uses:     plt_heap's hbot, htop
; data:     Blocks are packed from the arena's start, in g_state.alloc.tab by
;           handle; the BASIC heap, hbot to htop, is what lies above the
;           last. A block is addressed by its offset from its base, kept
;           only in alloc.tab, never by a stored pointer. A block grows
;           while the heap is empty: the blocks after it move up.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; alloc_new - a block of ECX bytes after the last; the heap shrinks by it.
; Only while the heap is empty.
;
; in:     EBX  its moved routine, 0 none
; out:    EAX  its handle
;         EDX  its base
; error:  Out of memory
; ::::::::::::::
alloc_new:
        mov     eax,[g_state.alloc.nb]
        cmp     eax,NBLKH
        jae     err_om
        mov     edx,[g_state.mm.hbot]
        add     edx,15
        and     edx,not 15
        push    edx
        add     edx,ecx
        jc      err_om
        add     edx,HMIN
        jc      err_om
        cmp     edx,[g_state.mm.htop]
        pop     edx
        ja      err_om
        push    esi
        imul    esi,eax,sizeof TALLOC
        add     esi,offset g_state.alloc.tab
        mov     [esi].TALLOC.base,edx
        mov     [esi].TALLOC.cap,ecx
        mov     [esi].TALLOC.moved,ebx
        pop     esi
        inc     [g_state.alloc.nb]
        push    edx
        add     edx,ecx
        mov     [g_state.mm.hbot],edx
        pop     edx
        ret

; ::::::::::::::
; alloc_grow - block EAX to ECX bytes at least, half as many again: the
; blocks after it move up, each block from it on told (moved).
;
; out:    CF   set: not grown, the heap not empty or without room
; keeps:  all
; ::::::::::::::
alloc_grow:
        pushad
        mov     edi,eax                 ; EDI the handle
        imul    esi,eax,sizeof TALLOC
        add     esi,offset g_state.alloc.tab
        mov     edx,[esi].TALLOC.cap
        cmp     ecx,edx
        jbe     @alg_r
        shr     edx,1                   ; EDX the new size
        add     edx,[esi].TALLOC.cap
        cmp     edx,ecx
        jae     @F
        mov     edx,ecx
@@:
        add     edx,15
        and     edx,not 15
        mov     ebx,[g_state.mm.hbot]   ; the heap empty: nothing in it moves
        cmp     [g_state.mm.aend],ebx
        jne     @alg_f
        mov     ebx,[g_state.mm.htop]
        cmp     [g_state.mm.sfr],ebx
        jne     @alg_f
        mov     ebp,edx                 ; EBP the growth
        sub     ebp,[esi].TALLOC.cap
        mov     ebx,[g_state.mm.hbot]
        add     ebx,ebp
        jc      @alg_f
        add     ebx,HMIN
        jc      @alg_f
        cmp     ebx,[g_state.mm.htop]
        ja      @alg_f
        mov     [esi].TALLOC.cap,edx
        push    esi
        push    edi
        mov     ecx,[g_state.mm.hbot]   ; the bytes after it, up by EBP
        mov     eax,[esi].TALLOC.base
        add     eax,[esi].TALLOC.cap
        sub     eax,ebp                 ; its old end
        sub     ecx,eax
        lea     esi,[eax+ecx-1]
        lea     edi,[esi+ebp]
        std
        rep     movsb
        cld
        pop     edi
        pop     esi
        add     [g_state.mm.hbot],ebp
        mov     eax,[g_state.mm.hbot]
        mov     [g_state.mm.aend],eax
        call    @alg_t                  ; it, then those after it moved
@alg_m:
        add     esi,sizeof TALLOC
        inc     edi
        cmp     edi,[g_state.alloc.nb]
        jae     @alg_r
        add     [esi].TALLOC.base,ebp
        call    @alg_t
        jmp     @alg_m
@alg_r:
        popad
        clc
        ret
@alg_f:
        popad
        stc
        ret
@alg_t:                                 ; block ESI told
        mov     eax,[esi].TALLOC.moved
        test    eax,eax
        jz      @F
        pushad
        call    eax
        popad
@@:
        ret

; ::::::::::::::
; alloc_sel - a selector for block EAX: its base and size; a new one when BX
; is 0.
;
; out:    BX   the selector
; error:  Out of memory: no descriptor
; ::::::::::::::
alloc_sel:
        push    ecx
        push    edx
        imul    edx,eax,sizeof TALLOC
        mov     ecx,[g_state.alloc.tab+edx].TALLOC.cap
        mov     edx,[g_state.alloc.tab+edx].TALLOC.base
        call    sel_set
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; sel_set - a selector for the ECX bytes at EDX; a new one when BX is 0.
;
; out:    BX   the selector
; error:  Out of memory: no descriptor
; ::::::::::::::
sel_set:
        pushad
        test    bx,bx
        jnz     @sls_b
        mov     bx,ds                   ; DS's alias: a data descriptor
        mov     ax,000Ah
        int     31h
        jc      err_om
        mov     bx,ax
@sls_b:
        mov     [esp+16],ebx
        mov     esi,ecx
        add     edx,[g_plat.dsbase]
        mov     ecx,edx
        shr     ecx,16
        mov     ax,0007h
        int     31h
        jc      err_om
        lea     edx,[esi-1]
        cmp     edx,100000h             ; past 1 MB: whole pages
        jb      @F
        or      edx,0FFFh
@@:
        mov     ecx,edx
        shr     ecx,16
        mov     ax,0008h
        int     31h
        jc      err_om
        popad
        ret

        end
