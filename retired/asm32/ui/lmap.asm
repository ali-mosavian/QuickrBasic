; ==========================================================================
; lmap.asm - the line map: each editor line's record in the program store
; ==========================================================================
; holds:    lm_init, lm_build, lm_off, lm_of
; uses:     alloc_new, alloc_grow, var_clr
; data:     A block (HLMAP) of dwords: line i's record's offset, tx_all of
;           them, rising. An include's records (LF_INC) are no line's.
; ==========================================================================

include x86f.inc
        .code

LMINIT  = 4096*4                ; the map's first size

lmbase  macro   reg
        mov     reg,[g_state.alloc.tab+HLMAP*sizeof TALLOC].TALLOC.base
endm

; ::::::::::::::
; lm_init - the map's block.
; ::::::::::::::
lm_init:
        mov     ecx,LMINIT
        xor     ebx,ebx
        jmp     alloc_new

; ::::::::::::::
; lm_build - the map from the store; tx_all its lines.
;
; keeps:  all
; error:  Out of memory
; ::::::::::::::
lm_build:
        pushad
        xor     ecx,ecx
        mov     ebx,PROGBASE
@lmb_l:
        mov     eax,gs:[ebx].TLINE.len
        test    eax,eax
        jz      @lmb_d
        test    gs:[ebx].TLINE.fl,LF_INC
        jnz     @lmb_n
        lea     edx,[ecx*4+4]
        cmp     edx,[g_state.alloc.tab+HLMAP*sizeof TALLOC].TALLOC.cap
        jbe     @F
        pushad                          ; it grows: the heap emptied first
        call    var_clr
        mov     eax,HLMAP
        mov     ecx,edx
        call    alloc_grow
        popad
        jc      err_om
@@:
        lmbase  edi
        mov     [edi+ecx*4],ebx
        inc     ecx
        mov     eax,gs:[ebx].TLINE.len
@lmb_n:
        add     ebx,eax
        jmp     @lmb_l
@lmb_d:
        mov     [g_ui.tx.tx_all],ecx
        popad
        ret

; ::::::::::::::
; lm_off - line EAX's record; past the last, the store's end.
;
; out:    EBX  its offset
; keeps:  all but EBX
; ::::::::::::::
lm_off:
        mov     ebx,[g_state.mm.progend]
        cmp     eax,[g_ui.tx.tx_all]
        jae     @F
        lmbase  ebx
        mov     ebx,[ebx+eax*4]
@@:
        ret

; ::::::::::::::
; lm_of - the line whose record is at EBX, or the last before it (an
; include's records are its line's).
;
; out:    EAX  the line
;         CF   set: none, EBX before the first
; keeps:  all but EAX
; ::::::::::::::
lm_of:
        push    ecx
        push    edx
        push    esi
        lmbase  esi
        xor     ecx,ecx                 ; [ECX, EDX): the last <= EBX in it
        mov     edx,[g_ui.tx.tx_all]
        test    edx,edx
        jz      @lmo_n
        cmp     ebx,[esi]
        jb      @lmo_n
@lmo_l:
        lea     eax,[ecx+1]
        cmp     eax,edx
        jae     @lmo_f
        lea     eax,[ecx+edx]
        shr     eax,1
        cmp     ebx,[esi+eax*4]
        jb      @F
        mov     ecx,eax
        jmp     @lmo_l
@@:
        mov     edx,eax
        jmp     @lmo_l
@lmo_f:
        mov     eax,ecx
        clc
        jmp     @lmo_r
@lmo_n:
        stc
@lmo_r:
        pop     esi
        pop     edx
        pop     ecx
        ret

        end
