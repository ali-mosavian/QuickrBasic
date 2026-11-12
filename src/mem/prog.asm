; ==========================================================================
; prog.asm - the program store, in GS
; ==========================================================================
; holds:    prg_findw, prg_put, prg_fill, prg_drop, prg_init
; uses:     var_clr, mem_pmov
; data:     a line: len:2 num:2 cp:2 body, the tokens lx_line made, with their
;           0. cp is its code in the cache (cm_cache), 0 for none. A len of 0
;           ends the lines, at pend.
; calls:
;   prg_put            rpl_enter: store, replace or delete
;     prg_findw        BX = the first line >= lnum; CF: it is lnum
;     prg_drop         CONT and lc end; (pend) = 0; var_clr
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; prg_findw - find the line numbered lnum: any line so numbered, as a loaded
; program's are in the file's order; else the place by number, from lc when
; lc is not above it.
;
; in:     lnum  the number; NONUM is none
;         lc    a line, or 0
; out:    EBX  the line, else the first numbered more, or the terminator
;         CF  set when BX has that number
; keeps:  SI, DI, CX, DX
; ::::::::::::::
prg_findw:
        mov     ax,[g_state.rn.lnum]
        mov     ebx,PROGBASE
        cmp     ax,NONUM
        je      @pf_no
        push    ecx
@pf_x:
        cmp     word ptr [g_progb+ebx],0
        je      @pf_o
        cmp     ax,word ptr [g_progb+ebx+L_NUM]
        je      @pf_z
        movzx   ecx,word ptr [g_progb+ebx]
        add     ebx,ecx
        jmp     @pf_x
@pf_z:
        pop     ecx
        stc
        ret
@pf_o:
        pop     ecx
        movzx   ebx,[g_state.lnk.lc]
        test    ebx,ebx
        jz      @pf_b
        cmp     ax,word ptr [g_progb+ebx+L_NUM]
        jae     @pf_c
@pf_b:
        mov     ebx,PROGBASE
@pf_c:
        cmp     word ptr [g_progb+ebx],0
        je      @pf_no
        cmp     ax,word ptr [g_progb+ebx+L_NUM]
        je      @pf_y
        jb      @pf_no
        push    ecx
        movzx   ecx,word ptr [g_progb+ebx]
        add     ebx,ecx
        pop     ecx
        jmp     @pf_c
@pf_y:
        stc
        ret
@pf_no:
        clc
        ret

; ::::::::::::::
; prg_put - store, replace or delete a line, then prg_drop.
;
; A replaced line is removed before the room check, as stock.
;
; in:     lnum  the line number
;         CX    the body length with its 0; 1 deletes
;         ESI   the body
; error:  ?OUT OF MEMORY  it does not fit
; ::::::::::::::
prg_put:
        movzx   ecx,cx
        cmp     ecx,1
        je      @pp_d
        mov     [g_state.mm.pbs],esi
        mov     [g_state.mm.pbl],cx
        mov     eax,ecx
        add     eax,HDR
        cmp     eax,LMAX
        ja      err_om
@pp_d:
        push    eax
        push    ecx
        call    prg_findw
        jnc     @pp_i
        mov     [g_state.mm.lyo2],bx
        movzx   eax,word ptr [g_progb+ebx]
        lea     esi,[ebx+eax]
        mov     edi,ebx
        movzx   ecx,[g_state.mm.pend]
        sub     ecx,esi
        call    mem_pmov
        mov     [g_state.mm.pend],di
        movzx   ebx,[g_state.mm.lyo2]
@pp_i:
        pop     ecx
        pop     eax
        cmp     ecx,1
        je      @jt_prog1
        movzx   edx,[g_state.mm.pend]
        add     edx,eax
        add     edx,2                   ; the terminator
        cmp     edx,PROGLIM
        jae     @pp_om
        push    eax
        movzx   ecx,[g_state.mm.pend]
        mov     esi,ecx
        sub     ecx,ebx
        dec     esi
        lea     edi,[esi+eax]
        std
        call    mem_pmov
        cld
        pop     eax
        add     [g_state.mm.pend],ax
        call    prg_fill
@jt_prog1:
        jmp     prg_drop

; ::::::::::::::
; prg_add - a loaded line: at the end, the program not dropped.
;
; in:     lnum  its number, NONUM none
;         CX    the body length with its 0
;         ESI   the body
; error:  ?OUT OF MEMORY  it does not fit
; ::::::::::::::
prg_add:
        movzx   ecx,cx
        mov     [g_state.mm.pbs],esi
        mov     [g_state.mm.pbl],cx
        lea     eax,[ecx+HDR]
        cmp     eax,LMAX
        ja      err_om
        movzx   ebx,[g_state.mm.pend]
        lea     edx,[ebx+eax+2]
        cmp     edx,PROGLIM
        jae     err_om
        add     [g_state.mm.pend],ax
        call    prg_fill
        movzx   ebx,[g_state.mm.pend]
        mov     word ptr [g_progb+ebx],0
        ret

; ::::::::::::::
; prg_fill - write the line at BX: the record of size AX, lnum, the body.
;
; in:     AX    the record's size
;         EBX   the record
;         lnum  its number
;         pbs, pbl  the body, its length
; ::::::::::::::
prg_fill:
        lea     edi,[g_progb+ebx]
        stosw
        mov     ax,[g_state.rn.lnum]
        stosw
        xor     ax,ax
        stosw
        mov     esi,[g_state.mm.pbs]
        movzx   ecx,[g_state.mm.pbl]
        rep     movsb
        ret

; ::::::::::::::
; prg_drop - after a program edit: CONT and lc end, the frames drop, (pend)
; = 0, then var_clr. The next link empties the cache.
;
; in:     pend  the program's end
; out:    contip, lc  0
; ::::::::::::::
prg_drop:
        xor     eax,eax
        mov     [g_state.rn.contip],ax
        mov     [g_state.lnk.lc],ax
        mov     [g_state.lnk.linked],al
        mov     [g_state.rn.cstk],CSTOP
        movzx   ebx,[g_state.mm.pend]
        mov     word ptr [g_progb+ebx],ax
        jmp     var_clr
@pp_om:
        call    prg_drop
        jmp     err_om

; ::::::::::::::
; prg_init - NEW: no program, no variables, then prg_drop.
; ::::::::::::::
prg_init:
        mov     [g_state.mm.pend],PROGBASE
        mov     [g_state.mm.vend],VARBASE
        call    sym_init
        jmp     prg_drop

        end
