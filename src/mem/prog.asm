; ==========================================================================
; prog.asm - the program store, in GS
; ==========================================================================
; holds:    prg_findw, prg_put, prg_add, prg_at, prg_ins, prg_cut, prg_body,
;           prg_fill, prg_drop, prg_init, prg_place
; uses:     var_clr, mem_pmov
; data:     a line: TLINE, then its body, the tokens lx_line made, with
;           their 0, then pre bytes of its text before the body (the
;           editor's: indent, number as typed). A len of 0 ends the lines,
;           at progend.
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
        cmp     gs:[ebx].TLINE.len,0
        je      @pf_o
        cmp     ax,gs:[ebx].TLINE.num
        je      @pf_z
        mov     ecx,gs:[ebx].TLINE.len
        add     ebx,ecx
        jmp     @pf_x
@pf_z:
        pop     ecx
        stc
        ret
@pf_o:
        pop     ecx
        mov     ebx,[g_state.lnk.cursor]
        test    ebx,ebx
        jz      @pf_b
        cmp     ax,gs:[ebx].TLINE.num
        jae     @pf_c
@pf_b:
        mov     ebx,PROGBASE
@pf_c:
        cmp     gs:[ebx].TLINE.len,0
        je      @pf_no
        cmp     ax,gs:[ebx].TLINE.num
        je      @pf_y
        jb      @pf_no
        push    ecx
        mov     ecx,gs:[ebx].TLINE.len
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
        mov     [g_state.mm.bodyptr],esi
        mov     [g_state.mm.bodylen],ecx
        call    prg_findw
        jnc     @F
        call    prg_cut
@@:
        cmp     [g_state.mm.bodylen],1
        je      prg_drop
        call    prg_ins
        jmp     prg_drop

; ::::::::::::::
; prg_cut - the record at EBX removed.
;
; out:    EAX  its size
; keeps:  all but EAX
; ::::::::::::::
prg_cut:
        push    ecx
        push    esi
        push    edi
        mov     eax,gs:[ebx].TLINE.len
        lea     esi,[ebx+eax]
        mov     edi,ebx
        mov     ecx,[g_state.mm.progend]
        sub     ecx,esi
        call    mem_pmov
        mov     [g_state.mm.progend],edi
        mov     gs:[edi].TLINE.len,0
        pop     edi
        pop     esi
        pop     ecx
        ret

; ::::::::::::::
; prg_ins - the line (prg_fill's) stored before the record at EBX, after
; prg_drop: the store whole, should it not fit.
;
; out:    EAX  its size
; keeps:  EBX
; error:  Out of memory
; ::::::::::::::
prg_ins:
        call    prg_size
        mov     edx,[g_state.mm.progend]
        lea     edx,[edx+eax+4]         ; the terminator
        pushad
        call    prg_drop
        popad
        call    prg_room
        push    eax
        mov     ecx,[g_state.mm.progend]
        mov     esi,ecx
        sub     ecx,ebx
        dec     esi
        lea     edi,[esi+eax]
        std
        call    mem_pmov
        cld
        mov     eax,[esp]
        add     [g_state.mm.progend],eax
        mov     ecx,[g_state.mm.progend]
        mov     gs:[ecx].TLINE.len,0
        call    prg_fill
        pop     eax
        ret

; ::::::::::::::
; prg_at - where prg_add stores: addat, else the end.
;
; out:    EDX  it
; keeps:  all but EDX
; ::::::::::::::
prg_at:
        mov     edx,[g_state.mm.addat]
        test    edx,edx
        jnz     @F
        mov     edx,[g_state.mm.progend]
@@:
        ret

; ::::::::::::::
; prg_add - a loaded line: at prg_at, the program not dropped at the end;
; addat past it.
;
; in:     lnum  its number, NONUM none
;         CX    the body length with its 0
;         ESI   the body
; error:  ?OUT OF MEMORY  it does not fit
; ::::::::::::::
prg_add:
        movzx   ecx,cx
        mov     [g_state.mm.bodyptr],esi
        mov     [g_state.mm.bodylen],ecx
        mov     ebx,[g_state.mm.addat]
        test    ebx,ebx
        jz      @F
        call    prg_ins
        add     [g_state.mm.addat],eax
        ret
@@:
        call    prg_size
        mov     ebx,[g_state.mm.progend]
        lea     edx,[ebx+eax+4]
        call    prg_room
        add     [g_state.mm.progend],eax
        call    prg_fill
        mov     ebx,[g_state.mm.progend]
        mov     gs:[ebx].TLINE.len,0
        ret

; ::::::::::::::
; prg_body - the body of the record at EBX, copied to EDI.
;
; out:    EDI  past it
; keeps:  all but EDI
; ::::::::::::::
prg_body:
        push    ecx
        push    esi
        mov     ecx,gs:[ebx].TLINE.len
        sub     ecx,gs:[ebx].TLINE.pre
        sub     ecx,HDR
        lea     esi,[ebx+HDR]
        add     esi,[g_state.mm.progbase]
        rep     movsb
        pop     esi
        pop     ecx
        ret

; ::::::::::::::
; prg_size - the record's size: bodylen, prelen.
;
; out:    EAX  it
; ::::::::::::::
prg_size:
        mov     eax,[g_state.mm.bodylen]
        add     eax,[g_state.mm.prelen]
        add     eax,HDR
        ret

; ::::::::::::::
; prg_fill - write the line at EBX: the record of size EAX, lnum, addfl,
; the body, the text before it (prelen used up).
;
; in:     EAX   the record's size
;         EBX   the record
;         lnum  its number
;         bodyptr, bodylen, preptr, prelen
; ::::::::::::::
prg_fill:
        mov     gs:[ebx].TLINE.len,eax
        mov     ax,[g_state.rn.lnum]
        mov     gs:[ebx].TLINE.num,ax
        mov     gs:[ebx].TLINE.code,0
        mov     al,[g_state.mm.addfl]
        mov     gs:[ebx].TLINE.fl,al
        mov     gs:[ebx].TLINE.sep,0
        xor     ecx,ecx
        xchg    ecx,[g_state.mm.prelen]
        mov     gs:[ebx].TLINE.pre,ecx
        lea     edi,[ebx+HDR]
        add     edi,[g_state.mm.progbase]
        mov     esi,[g_state.mm.bodyptr]
        push    ecx
        mov     ecx,[g_state.mm.bodylen]
        rep     movsb
        pop     ecx
        mov     esi,[g_state.mm.preptr]
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
        mov     [g_state.rn.contln],eax
        mov     [g_state.lnk.cursor],eax
        mov     [g_state.lnk.linked],al
        mov     [g_state.rn.cstk],CSTOP
        mov     ebx,[g_state.mm.progend]
        mov     gs:[ebx].TLINE.len,eax
        jmp     var_clr

; ::::::::::::::
; prg_init - NEW: no program, no variables, then prg_drop.
; ::::::::::::::
; ::::::::::::::
; prg_room - the store EDX bytes at least: grown, the variables dropped
; first (an edit drops them anyway).
;
; keeps:  all
; error:  Out of memory
; ::::::::::::::
prg_room:
        cmp     edx,[g_state.alloc.tab+HPROG*sizeof TALLOC].TALLOC.cap
        jbe     @prm_r
        pushad
        call    var_clr
        mov     eax,HPROG
        mov     ecx,edx
        call    alloc_grow
        popad
@prm_r:
        ret

; ::::::::::::::
; prg_place - the store's block where it now is: pglin, proglin, GS.
; ::::::::::::::
prg_place:
        push    eax
        push    ebx
        mov     eax,HPROG
        mov     bx,[g_state.mm.progsel]
        call    alloc_sel
        mov     [g_state.mm.progsel],bx
        mov     gs,bx
        mov     eax,[g_state.alloc.tab+HPROG*sizeof TALLOC].TALLOC.base
        mov     [g_state.mm.progbase],eax
        add     eax,PROGBASE
        mov     [g_state.mm.progfirst],eax
        pop     ebx
        pop     eax
        ret

prg_init:
        mov     [g_state.mm.progend],PROGBASE
        mov     [g_state.mm.vend],VARBASE
        call    sym_init
        jmp     prg_drop

        end
