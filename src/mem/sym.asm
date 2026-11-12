; ==========================================================================
; sym.asm - the name table and the variables
; ==========================================================================
; holds:    var_clr, sym_init, nam_find, var_find
; uses:     hbot, htop, rd_first
; data:     A name, 1 .. NNAM-1, as QBasic's name table: nname its text, its
;           length then its letters in upper case, a suffix left out; nflag
;           NM_*; nline a label's line. The lexer makes names; what a name
;           is the compiler finds here.
;           A variable, 1 .. NSYM-1, as QBasic's variable manager: of a name,
;           a type (ET_*) and whether an array. vnam, vtyp the name and type;
;           sinfo bit 7 array, bits 1-0 the kind (3 DEF FN's); vt its slot, an array's header.
;           Each table chains its hashes from a head, 0 ends a chain.
; calls:
;   nam_find           the name of a text, else a new one
;   var_find           the variable of a name, type and kind, else a new one
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; var_clr - CLR: zero variables, drop arrays, reset READ, empty the heap.
;
; out:    aend, sfr, tsp, dlp, dyo  reset
; keeps:  SI
; ::::::::::::::
var_clr:
        mov     edi,VARBASE
        mov     ecx,NSYM*12              ; varb, svarb
        xor     al,al
        rep     stosb
        xor     ebx,ebx
@vc_a:
        test    [g_sinfo+ebx],80h
        jz      @vc_n
        mov     [g_vt+ebx*4],0
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
        jmp     rd_first

; ::::::::::::::
; sym_init - no names, no variables.
;
; keeps:  ESI
; ::::::::::::::
sym_init:
        mov     [g_state.lx.nsyms],1
        mov     [g_state.lx.nnams],1
        mov     [g_state.lx.nsfree],offset g_snmb
        mov     edi,offset g_nhead
        mov     ecx,HSIZE/2
        xor     eax,eax
        rep     stosd
        mov     edi,offset g_vhead
        mov     ecx,HSIZE/2
        rep     stosd
        mov     edi,offset g_deft
        mov     ecx,26
        mov     al,ET_R4
        rep     stosb
        ret

; ::::::::::::::
; nam_find - the name of a text; a new one is made. A new name starting FN
; is DEF FN's: NM_FN.
;
; in:     nptr  the text
;         nlen  its length, a suffix left out
; out:    EAX  the name
; keeps:  ESI, EDI
; error:  ?OUT OF MEMORY  NNAM names, or the names' room is full
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
        mov     edi,[g_nname+ebx*4]
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
        movzx   ebx,word ptr [g_nnext+ebx*2]
        jmp     @nf_lp
@nf_om:
        jmp     err_om
@nf_new:
        movzx   ebx,[g_state.lx.nnams]
        cmp     ebx,NNAM
        jae     @nf_om
        mov     edi,[g_state.lx.nsfree]
        lea     eax,[edi+ecx+1]
        cmp     eax,offset g_snmb+NMSZ
        ja      @nf_om
        mov     [g_nname+ebx*4],edi
        mov     al,cl
        stosb
@nf_cp:
        movzx   eax,byte ptr [esi]
        upcase  eax
        stosb
        inc     esi
        loop    @nf_cp
        mov     [g_state.lx.nsfree],edi
        pop     eax
        mov     cx,[g_nhead+eax*2]
        mov     [g_nnext+ebx*2],cx
        mov     [g_nhead+eax*2],bx
        mov     [g_nflag+ebx],0
        mov     [g_nline+ebx*2],0
        mov     [g_ntyp+ebx],0
        mov     [g_natyp+ebx],0
        mov     edi,[g_nname+ebx*4]
        cmp     byte ptr [edi],2
        jbe     @nf_n
        cmp     word ptr [edi+1],'NF'
        jne     @nf_n
        mov     [g_nflag+ebx],NM_FN
@nf_n:
        inc     [g_state.lx.nnams]
@nf_ok:
        mov     eax,ebx
        pop     edi
        pop     esi
        ret

; ::::::::::::::
; var_find - the variable of a name, a type and a kind; a new one is made, a
; new scalar zeroed.
;
; in:     AX  the name
;         CL  the type, ET_*
;         DL  the kind: bit 7 an array, bits 1-0 as sinfo's
; out:    EAX  the variable
; keeps:  ESI, EDI
; error:  ?OUT OF MEMORY  NSYM variables
; ::::::::::::::
var_find:
        push    edi
        movzx   eax,ax
        movzx   ebx,cl
        lea     edi,[eax*4+eax]
        lea     edi,[edi+ebx*2]
        movzx   ebx,dl
        shr     ebx,7
        add     edi,ebx
        and     edi,HSIZE-1
        movzx   ebx,word ptr [g_vhead+edi*2]
@vf_lp:
        test    ebx,ebx
        jz      @vf_new
        cmp     ax,[g_vnam+ebx*2]
        jne     @vf_s
        cmp     cl,[g_vtyp+ebx]
        jne     @vf_s
        mov     ch,[g_sinfo+ebx]
        and     ch,83h
        cmp     ch,dl
        je      @vf_ok
@vf_s:
        movzx   ebx,word ptr [g_vnext+ebx*2]
        jmp     @vf_lp
@vf_om:
        jmp     err_om
@vf_new:
        movzx   ebx,[g_state.lx.nsyms]
        cmp     ebx,NSYM
        jae     @vf_om
        mov     [g_vnam+ebx*2],ax
        mov     [g_vtyp+ebx],cl
        mov     ax,[g_vhead+edi*2]
        mov     [g_vnext+ebx*2],ax
        mov     [g_vhead+edi*2],bx
        mov     [g_sinfo+ebx],dl
        and     [g_vt+ebx*4],0
        test    dl,dl
        js      @vf_a
        lea     ecx,[VARBASE+ebx*4]
        mov     [g_vt+ebx*4],ecx           ; a slot's variable is in its address
        and     word ptr [SVBASE+ebx*8+D_LEN],0
        and     dword ptr [ecx],0
@vf_a:
        inc     [g_state.lx.nsyms]
@vf_ok:
        mov     eax,ebx
        pop     edi
        ret

        .data?
g_vt    dd      NSYM dup(?)
g_vnam  dw      NSYM dup(?)
g_vnext dw      NSYM dup(?)
g_vhead dw      HSIZE dup(?)
g_vtyp  db      NSYM dup(?)
g_sinfo db      NSYM dup(?)
g_nname dd      NNAM dup(?)
g_nnext dw      NNAM dup(?)
g_nline dw      NNAM dup(?)
g_nhead dw      HSIZE dup(?)
g_nflag db      NNAM dup(?)
g_ntyp  db      NNAM dup(?)                 ; a name's declared type, ET_*
g_natyp db      NNAM dup(?)                 ; its array's
g_deft  db      26 dup(?)                   ; DEFtype's, by first letter
        .code

        end
