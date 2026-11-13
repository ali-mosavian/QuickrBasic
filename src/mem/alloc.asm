; ==========================================================================
; alloc.asm - the allocator: blocks in the heap's arena
; ==========================================================================
; holds:    alloc_new, alloc_sel
; uses:     plt_heap's hbot, htop
; data:     Blocks are packed from the arena's start, in g_state.alloc.tab by
;           handle; the BASIC heap, hbot to htop, is what lies above the
;           last. A block is addressed by its offset from its base, kept
;           only in bk.tab, never by a stored pointer.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; alloc_new - a block of ECX bytes after the last; the heap shrinks by it.
; Only while the heap is empty.
;
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
        mov     [g_state.alloc.tab+eax*sizeof TALLOC].TALLOC.base,edx
        mov     [g_state.alloc.tab+eax*sizeof TALLOC].TALLOC.cap,ecx
        inc     [g_state.alloc.nb]
        push    edx
        add     edx,ecx
        mov     [g_state.mm.hbot],edx
        pop     edx
        ret

; ::::::::::::::
; alloc_sel - a selector for block EAX: its base and size; a new one when BX
; is 0.
;
; out:    BX   the selector
; error:  Out of memory: no descriptor
; ::::::::::::::
alloc_sel:
        pushad
        test    bx,bx
        jnz     @bks_b
        mov     bx,ds                   ; DS's alias: a data descriptor
        mov     ax,000Ah
        push    eax
        int     31h
        mov     bx,ax
        pop     eax
        jc      err_om
        mov     eax,[esp+28]
@bks_b:
        mov     [esp+16],ebx
        imul    esi,eax,sizeof TALLOC
        mov     edx,[g_state.alloc.tab+esi].TALLOC.base
        add     edx,[g_plat.dsbase]
        mov     ecx,edx
        shr     ecx,16
        mov     ax,0007h
        int     31h
        jc      err_om
        mov     edx,[g_state.alloc.tab+esi].TALLOC.cap
        dec     edx
        mov     ecx,edx
        shr     ecx,16
        mov     ax,0008h
        int     31h
        jc      err_om
        popad
        ret

        end
