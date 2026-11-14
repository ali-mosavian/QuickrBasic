; ==========================================================================
; sym.asm - the name table and the variables
; ==========================================================================
; holds:    sym_new, var_size, var_clr, var_cell, sym_init, var_init, nam_find, var_find
; uses:     tab_*, sel_set, hbot, htop, rd_first
; data:     A name, 1 .. nnams-1, as QBasic's name table: a TNAM in nams,
;           its text in ntext, its length then its letters in upper case, a
;           suffix left out. The lexer makes names; what a name is the
;           compiler finds here.
;           A variable, 1 .. nsyms-1, as QBasic's variable manager: of a
;           name, a type (ET_*) and whether an array. vars holds its 8-byte
;           cell, a TVAR (at FS:id*8) and a TVX. vt is its cell, an array's
;           header. Compiled code holds cells' addresses: the cells move
;           only when an edit drops the code. Each table chains its hashes
;           from a head, 0 ends a chain.
; calls:
;   nam_find           the name of a text, else a new one
;   var_find           the variable of a name, type and kind, else a new one
; ==========================================================================

include x86f.inc
        .code

VINIT   = 1024                  ; the first variables' room
NINIT   = 1024                  ;   names'
IDMAX   = 0FFFFh                ; ids are words
VSLACK  = 256                   ; variables past the program's: direct lines'

; ::::::::::::::
; sym_new - the tables' blocks, after the program's.
; ::::::::::::::
sym_new:
        mov     esi,offset g_state.sym.vars
        mov     [esi].TTAB.narr,4
        mov     [esi].TTAB.sz,8         ; the cells, TVAR, TVX, kval
        mov     dword ptr [esi+4].TTAB.sz,8
        mov     dword ptr [esi+8].TTAB.sz,8
        mov     dword ptr [esi+12].TTAB.sz,8
        mov     [esi].TTAB.max,IDMAX
        mov     ecx,VINIT
        mov     ebx,offset var_place
        call    tab_new
        call    var_place
        mov     esi,offset g_state.sym.nams
        mov     [esi].TTAB.narr,2
        mov     [esi].TTAB.sz,8         ; TNAM, the chains
        mov     dword ptr [esi+4].TTAB.sz,2
        mov     [esi].TTAB.max,IDMAX
        mov     ecx,NINIT
        mov     ebx,offset nam_place
        call    tab_new
        mov     esi,offset g_state.sym.ntext
        mov     [esi].TTAB.narr,1
        mov     [esi].TTAB.sz,1
        mov     [esi].TTAB.max,IDMAX*256
        mov     ecx,NINIT*8
        mov     ebx,offset txt_place
        jmp     tab_new

; ::::::::::::::
; var_place - the variables' block moved or grew: FS, and scalars' vt by
; the cells' move.
; ::::::::::::::
var_place:
        mov     esi,offset g_state.sym.vars
        call    tab_place
        mov     eax,[esi].TTAB.base     ; EAX the cells' move
        sub     eax,edx
        mov     edx,[esi+4].TTAB.at
        mov     ecx,[esi].TTAB.cap
        shl     ecx,3
        mov     bx,[g_state.sym.vsel]
        call    sel_set
        mov     [g_state.sym.vsel],bx
        mov     fs,bx
        test    eax,eax
        jz      @vp_r
        mov     ebx,1
@vp_l:
        cmp     bx,[g_state.lx.nsyms]
        jae     @vp_r
        test    fs:[ebx*8].TVAR.info,80h
        jnz     @F
        cmp     fs:[ebx*8].TVAR.vt,0
        je      @F
        add     fs:[ebx*8].TVAR.vt,eax
@@:
        inc     ebx
        jmp     @vp_l
@vp_r:
        ret

nam_place:
        mov     esi,offset g_state.sym.nams
        jmp     tab_place

txt_place:
        mov     esi,offset g_state.sym.ntext
        jmp     tab_place

; ::::::::::::::
; var_size - room for the program's variables before it runs: compiled
; lazily, they are made while the heap holds strings and arrays, when the
; block cannot grow. Each M_VAR byte in the store counts as one; it may
; also be another token's byte, never less.
;
; keeps:  ESI
; ::::::::::::::
var_size:
        movzx   ecx,[g_state.lx.nsyms]
        add     ecx,VSLACK
        mov     ebx,PROGBASE
@vs_l:
        mov     edx,gs:[ebx].TLINE.len
        test    edx,edx
        jz      @vs_d
        lea     edi,[ebx+HDR]
        add     ebx,edx
@vs_b:
        cmp     edi,ebx
        jae     @vs_l
        cmp     byte ptr gs:[edi],M_VAR
        jne     @F
        inc     ecx
@@:
        inc     edi
        jmp     @vs_b
@vs_d:
        cmp     ecx,IDMAX
        jbe     @F
        mov     ecx,IDMAX
@@:
        push    esi
        mov     esi,offset g_state.sym.vars
        call    tab_room                ; no room: the variable past it errs
        pop     esi
        ret

; ::::::::::::::
; var_clr - CLR: zero variables, drop arrays, reset READ, empty the heap.
;
; out:    aend, sfr, tsp, dlp, dyo  reset
; keeps:  EDX, ESI
; ::::::::::::::
var_clr:
        mov     edi,[g_state.sym.vars.at]
        mov     ecx,[g_state.sym.vars.cap]
        shl     ecx,1
        xor     eax,eax
        rep     stosd
        xor     ebx,ebx
        vxat    eax
@vc_a:
        test    fs:[ebx*8].TVAR.info,80h
        jz      @vc_c
        and     fs:[ebx*8].TVAR.vt,0
        mov     [eax+ebx*8].TVX.dyn,0
        jmp     @vc_n
@vc_c:
        call    var_cell                ; a parameter's bound elsewhere: back
        mov     fs:[ebx*8].TVAR.vt,ecx
@vc_n:
        inc     ebx
        cmp     bx,[g_state.lx.nsyms]
        jb      @vc_a
        mov     eax,[g_state.mm.hbot]
        mov     [g_state.mm.aend],eax
        mov     eax,[g_state.mm.htop]
        mov     [g_state.mm.sfr],eax
        mov     [g_state.mm.ahd],HDRBASE
        and     [g_state.sg.tsp],0
        and     [g_state.sg.tspb],0
        mov     [g_state.rn.lsp],offset g_lsav
        jmp     rd_first

; ::::::::::::::
; var_cell - scalar EBX's own cell: an INT's or a FLOAT's first 4 bytes, a
; DOUBLE's 8, a string's Desc. vt holds a scalar's cell: its own, a
; parameter's the argument's while its procedure runs.
;
; out:    ECX  it
; keeps:  all but ECX
; ::::::::::::::
var_cell:
        mov     ecx,[g_state.sym.vars.at]
        lea     ecx,[ecx+ebx*8]
        ret

; ::::::::::::::
; sym_init - no names, no variables, no declarations.
;
; keeps:  ESI
; ::::::::::::::
sym_init:
        mov     [g_state.lx.nnams],1
        and     [g_state.sym.tfree],0
        mov     edi,offset g_nhead
        mov     ecx,HSIZE/2
        xor     eax,eax
        rep     stosd
        call    var_init
        jmp     dc_reset

; ::::::::::::::
; var_init - no variables; the names kept.
;
; keeps:  ESI
; ::::::::::::::
var_init:
        mov     [g_state.lx.nsyms],1
        mov     edi,offset g_vhead
        mov     ecx,HSIZE/2
        xor     eax,eax
        rep     stosd
        mov     edi,offset g_plh
        mov     ecx,NPROC/2
        rep     stosd
        ret

; ::::::::::::::
; nam_find - the name of a text; a new one is made. A new name starting FN
; is DEF FN's: NM_FN.
;
; in:     nptr  the text
;         nlen  its length, a suffix left out
; out:    EAX  the name
; keeps:  ESI, EDI
; error:  ?OUT OF MEMORY  past IDMAX names, or the heap is not empty
; ::::::::::::::
nam_find:
        push    esi
        push    edi
        mov     esi,[g_state.lx.nptr]
        movzx   ecx,[g_state.lx.nlen]
        push    ecx
        push    esi
        xor     eax,eax
@nf_hl:
        movzx   edi,byte ptr [esi]
        upcase  edi
        imul    eax,eax,31
        add     eax,edi
        inc     esi
        loop    @nf_hl
        and     eax,HSIZE-1
        pop     esi
        pop     ecx
        push    eax
        movzx   ebx,word ptr [g_nhead+eax*2]
@nf_lp:
        test    ebx,ebx
        jz      @nf_new
        nmat    edi
        mov     edi,[edi+ebx*8].TNAM.text
        add     edi,[g_state.sym.ntext.at]
        cmp     cl,[edi]
        jne     @nf_s
        push    ecx
        push    esi
@nf_c:
        inc     edi
        movzx   eax,byte ptr [esi]
        upcase  eax
        cmp     al,[edi]
        jne     @nf_m
        inc     esi
        loop    @nf_c
        pop     esi
        pop     ecx
        pop     eax
        jmp     @nf_ok
@nf_m:
        pop     esi
        pop     ecx
@nf_s:
        mov     edi,[g_state.sym.nams.at+4]
        movzx   ebx,word ptr [edi+ebx*2]
        jmp     @nf_lp
@nf_new:
        movzx   ebx,[g_state.lx.nnams]
        push    esi
        push    ecx
        lea     ecx,[ebx+1]
        mov     esi,offset g_state.sym.nams
        call    tab_room
        jc      @F
        mov     ecx,[esp]
        add     ecx,[g_state.sym.tfree]
        inc     ecx
        mov     esi,offset g_state.sym.ntext
        call    tab_room
@@:
        pop     ecx
        pop     esi
        jc      @nf_om
        nmat    edx
        mov     edi,[g_state.sym.tfree]
        mov     [edx+ebx*8].TNAM.text,edi
        add     edi,[g_state.sym.ntext.at]
        mov     al,cl
        stosb
@nf_cp:
        movzx   eax,byte ptr [esi]
        upcase  eax
        stosb
        inc     esi
        loop    @nf_cp
        sub     edi,[g_state.sym.ntext.at]
        mov     [g_state.sym.tfree],edi
        pop     eax
        mov     cx,[g_nhead+eax*2]
        mov     edi,[g_state.sym.nams.at+4]
        mov     [edi+ebx*2],cx
        mov     [g_nhead+eax*2],bx
        and     dword ptr [edx+ebx*8].TNAM.flag,0     ; flag, typ, prc
        mov     edi,[edx+ebx*8].TNAM.text
        add     edi,[g_state.sym.ntext.at]
        cmp     byte ptr [edi],2
        jbe     @nf_n
        cmp     word ptr [edi+1],'NF'
        jne     @nf_n
        mov     [edx+ebx*8].TNAM.flag,NM_FN
@nf_n:
        inc     [g_state.lx.nnams]
@nf_ok:
        mov     eax,ebx
        pop     edi
        pop     esi
        ret
@nf_om:                                 ; the caller's ESI, EDI: cm_err's
        pop     eax
        pop     edi
        pop     esi
        jmp     err_om

; ::::::::::::::
; var_find - the variable of a name, a type and a kind in scope cscope; a new
; one is made, a new scalar zeroed. A new one of a procedure joins its
; locals (plh, vnloc), unless cstat.
;
; in:     AX  the name
;         CL  the type, ET_*
;         DL  the kind: bit 7 an array, bits 1-0 as sinfo's
;         cscope  0 the module's, else the procedure's
;         cstat   1: a STATIC one, not a local
; out:    EAX  the variable
; keeps:  ESI, EDI
; error:  ?OUT OF MEMORY  past IDMAX variables, or the heap is not empty
; ::::::::::::::
var_find:
        push    edi
        push    esi
        movzx   eax,ax
        movzx   ebx,cl
        lea     edi,[eax*4+eax]
        lea     edi,[edi+ebx*2]
        movzx   ebx,dl
        shr     ebx,7
        add     edi,ebx
        movzx   ebx,[g_state.cm.cscope]
        lea     edi,[edi+ebx*8]
        and     edi,HSIZE-1
        movzx   ebx,word ptr [g_vhead+edi*2]
        vxat    esi
@vf_lp:
        test    ebx,ebx
        jz      @vf_new
        cmp     ax,fs:[ebx*8].TVAR.nam
        jne     @vf_s
        cmp     cl,fs:[ebx*8].TVAR.typ
        jne     @vf_s
        mov     ch,fs:[ebx*8].TVAR.info
        and     ch,87h
        cmp     ch,dl
        jne     @vf_s
        mov     ch,[g_state.cm.cscope]
        cmp     ch,[esi+ebx*8].TVX.scp
        je      @vf_ok
@vf_s:
        movzx   ebx,[esi+ebx*8].TVX.next
        jmp     @vf_lp
@vf_new:
        movzx   ebx,[g_state.lx.nsyms]
        cmp     ebx,[g_state.sym.vars.cap]
        jb      @F
        push    ecx
        lea     ecx,[ebx+1]
        mov     esi,offset g_state.sym.vars
        call    tab_room
        pop     ecx
        jc      @vf_om
        vxat    esi
@@:
        mov     fs:[ebx*8].TVAR.nam,ax
        mov     fs:[ebx*8].TVAR.typ,cl
        mov     fs:[ebx*8].TVAR.info,dl
        mov     ax,[g_vhead+edi*2]
        mov     [esi+ebx*8].TVX.next,ax
        mov     [g_vhead+edi*2],bx
        mov     al,[g_state.cm.cscope]
        mov     [esi+ebx*8].TVX.scp,al
        and     [esi+ebx*8].TVX.nloc,0
        mov     [esi+ebx*8].TVX.dyn,0
        mov     [esi+ebx*8].TVX.kop,0
        test    al,al
        jz      @vf_m
        cmp     [g_state.cm.cstat],0
        jne     @vf_m
        movzx   eax,al                  ; a local: on its procedure's list
        mov     cx,[g_plh+eax*2]
        mov     [esi+ebx*8].TVX.nloc,cx
        mov     [g_plh+eax*2],bx
@vf_m:
        and     fs:[ebx*8].TVAR.vt,0
        test    dl,dl
        js      @vf_a
        call    var_cell
        mov     fs:[ebx*8].TVAR.vt,ecx
        and     dword ptr [ecx],0
        and     dword ptr [ecx+4],0
@vf_a:
        inc     [g_state.lx.nsyms]
@vf_ok:
        mov     eax,ebx
        pop     esi
        pop     edi
        ret
@vf_om:                                 ; the caller's ESI, EDI: cm_err's
        pop     esi
        pop     edi
        jmp     err_om

        .data?
g_vhead dw      HSIZE dup(?)
g_plh   dw      NPROC dup(?)                ; a procedure's first local
g_nhead dw      HSIZE dup(?)
g_deft  db      26 dup(?)                   ; DEFtype's, by first letter
        .code

        end
