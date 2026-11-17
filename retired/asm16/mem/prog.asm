; ==========================================================================
; prog.asm - the program store, in GS
; ==========================================================================
; holds:    prg_findw, prg_put, prg_fill, prg_drop, prg_init
; uses:     cm_infclr, var_clr, mem_pmov
; data:     a line: len:1 num:2 cp:2 body, the tokens lx_line made, with their
;           0. cp is its code in the cache (cm_cache), 0 for none. A len of 0
;           ends the lines, at pend.
; calls:
;   prg_put            rpl_enter: store, replace or delete
;     prg_findw        BX = the first line >= lnum; CF: it is lnum
;     prg_drop         CONT and lc end; (pend) = 0; var_clr
; ==========================================================================

; ::::::::::::::
; prg_findw - find the line numbered lnum, from lc when lc is not above it.
;
; in:     lnum  the number
;         lc    a line, or 0
; out:    BX  the first line numbered that or more, or the terminator
;         CF  set when BX has that number
; keeps:  SI, DI, CX, DX
; ::::::::::::::
prg_findw:
        mov     ax,[lnum]
        mov     bx,[lc]
        test    bx,bx
        jz      @pf_b
        cmp     ax,gs:[bx+1]
        jae     @pf_c
@pf_b:
        mov     bx,PROGBASE
@pf_c:
        cmp     byte ptr gs:[bx],0
        je      @pf_no
        cmp     ax,gs:[bx+1]
        je      @pf_y
        jb      @pf_no
        push    cx
        movzx   cx,byte ptr gs:[bx]
        add     bx,cx
        pop     cx
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
;         SI    the body
; error:  ?OUT OF MEMORY  it does not fit
; ::::::::::::::
prg_put:
        cmp     cx,1
        je      @pp_d
        mov     [pbs],si
        mov     [pbl],cx
        mov     ax,cx
        add     ax,HDR
        cmp     ax,255
        ja      err_om
@pp_d:
        push    ax
        push    cx
        call    prg_findw
        jnc     @pp_i
        mov     [lyo2],bx
        movzx   ax,byte ptr gs:[bx]
        mov     si,bx
        add     si,ax
        mov     di,bx
        mov     cx,[pend]
        sub     cx,si
        call    mem_pmov
        mov     [pend],di
        mov     bx,[lyo2]
@pp_i:
        pop     cx
        pop     ax
        cmp     cx,1
        je      @jt_prog1
        mov     dx,[pend]
        add     dx,ax
        inc     dx
        cmp     dx,PROGLIM
        jae     @pp_om
        push    ax
        mov     cx,[pend]
        mov     si,cx
        sub     cx,bx
        dec     si
        mov     di,si
        add     di,ax
        std
        call    mem_pmov
        cld
        pop     ax
        add     [pend],ax
        call    prg_fill
@jt_prog1:
        jmp     prg_drop

; ::::::::::::::
; prg_fill - write the line at BX: the record of size AL, lnum, the body.
;
; in:     AL    the record's size
;         BX    the record
;         lnum  its number
;         pbs, pbl  the body, its length
; ::::::::::::::
prg_fill:
        push    es
        push    gs
        pop     es
        mov     di,bx
        stosb
        mov     ax,[lnum]
        stosw
        xor     ax,ax
        stosw
        mov     si,[pbs]
        mov     cx,[pbl]
        rep     movsb
        pop     es
        ret

; ::::::::::::::
; prg_drop - after a program edit: CONT and lc end, the frames drop, (pend)
; = 0, then var_clr. The next link empties the cache.
;
; in:     pend  the program's end
; out:    contip, lc  0
; ::::::::::::::
prg_drop:
        xor     ax,ax
        mov     [contip],ax
        mov     [lc],ax
        mov     [linked],al
        mov     [cstk],CSTOP
        mov     bx,[pend]
        mov     gs:[bx],al
        call    cm_infclr
        jmp     var_clr
@pp_om:
        call    prg_drop
        jmp     err_om

; ::::::::::::::
; prg_init - NEW: no program, no variables, then prg_drop.
; ::::::::::::::
prg_init:
        mov     [pend],PROGBASE
        mov     [vend],VARBASE
        mov     [nsyms],0
        jmp     prg_drop
