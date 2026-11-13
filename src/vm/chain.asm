; ==========================================================================
; chain.asm - CHAIN and COMMON
; ==========================================================================
; holds:    sc_cmn, sc_chain, chn_start
; uses:     rpl_load, prg_init, arr_new, arr_free, str_alloc
; data:     cmid: the ids COMMON has named so far in this run, in order.
;           CHAIN writes their values to chb, then loads and runs its file;
;           there each COMMON's variable takes the next value, by position,
;           of the same kind. An entry: the kind (sinfo's 80h, SK_DBL, 3),
;           then a number's 4 or 8 bytes, a string's len:2 and chars; an
;           array's n:1 (0 none), its counts and lower bounds (words), then
;           its elements so. Files stay open.
; ==========================================================================

include x86f.inc
        .code

NCMN    = 512
CHBSZ   = 10000h

; ::::::::::::::
; chn_kind - variable EBX's kind.
;
; out:    AL  sinfo's 80h, SK_DBL and 3
; keeps:  all but EAX
; error:  Type mismatch  a record
; ::::::::::::::
chn_kind:
        cmp     fs:[ebx*8].TVAR.typ,ET_REC
        jae     err_tm
        mov     al,fs:[ebx*8].TVAR.info
        and     al,80h or SK_DBL or 3
        ret

; ::::::::::::::
; chn_cell - scalar EBX of kind AL: its cell.
;
; out:    EDX  the cell; a string's Desc
;         ECX  its bytes; 0 a string
; keeps:  all but ECX, EDX
; ::::::::::::::
chn_cell:
        cellof  edx,ebx
        xor     ecx,ecx
        push    eax
        and     al,SK_DBL or 3
        cmp     al,2
        pop     eax
        je      @cce_r
        mov     ecx,8
        test    al,SK_DBL
        jnz     @cce_r
        mov     ecx,4
@cce_r:
        ret

; ::::::::::::::
; chn_elems - the array at EDX of id EBX: its elements' count, and their
; bytes; 0 for strings.
;
; out:    ECX  the count
;         EAX  the bytes, 0 strings
; keeps:  all but EAX, ECX
; ::::::::::::::
chn_elems:
        push    ebx
        movzx   ebx,byte ptr [edx]
        mov     eax,1
@cel_l:
        movzx   ecx,word ptr [edx+AH_CNT+ebx*2-2]
        imul    eax,ecx
        dec     ebx
        jnz     @cel_l
        pop     ebx
        push    eax
        mov     al,fs:[ebx*8].TVAR.info
        and     al,SK_DBL or 3
        cmp     al,2
        je      @cel_s
        call    arr_esz
        imul    ecx,[esp]
        mov     eax,ecx
        pop     ecx
        ret
@cel_s:
        xor     eax,eax
        pop     ecx
        ret

; ::::::::::::::
; chn_room - ECX more bytes fit in chb at EDI?
;
; keeps:  all
; error:  Out of memory  they do not
; ::::::::::::::
chn_room:
        push    eax
        lea     eax,[edi+ecx]
        cmp     eax,offset g_chb+CHBSZ
        pop     eax
        ja      err_om
        ret

; ::::::::::::::
; chn_sout - the string of the Desc at EAX to EDI: len:2, chars.
;
; keeps:  all but EDI
; ::::::::::::::
chn_sout:
        push    ecx
        push    esi
        movzx   ecx,word ptr [eax+D_LEN]
        add     ecx,2
        call    chn_room
        sub     ecx,2
        mov     [edi],cx
        add     edi,2
        mov     esi,[eax+D_PTR]
        rep     movsb
        pop     esi
        pop     ecx
        ret

; ::::::::::::::
; chn_sin - the string at ESI, len:2 and chars, to the Desc at EDI: a block
; it owns.
;
; out:    ESI  past it
; keeps:  EBX, ECX, EDI
; error:  Out of memory  no room for it
; ::::::::::::::
chn_sin:
        push    ebx
        push    ecx
        push    edi
        lodsw
        movzx   ecx,ax
        jecxz   @csi_z
        push    esi
        push    ecx
        call    str_alloc
        pop     ecx
        pop     esi
        mov     edi,[g_state.sg.dpv]
        mov     [g_state.sg.spt],edi
        mov     [g_state.sg.sln],cx
        rep     movsb
        mov     eax,[esp]
        call    str_own
        mov     edx,eax
        mov     eax,[g_state.sg.spt]
        mov     [edx+D_PTR],eax
        mov     ax,[g_state.sg.sln]
        mov     [edx+D_LEN],ax
        jmp     @csi_r
@csi_z:
        mov     edx,[esp]
        mov     word ptr [edx+D_LEN],0
        mov     dword ptr [edx+D_PTR],0
@csi_r:
        pop     edi
        pop     ecx
        pop     ebx
        ret

; ::::::::::::::
; chn_save - COMMON's variables' values to chb.
;
; error:  Out of memory  more than chb holds
; ::::::::::::::
chn_save:
        pushad
        mov     edi,offset g_chb
        xor     ebp,ebp
@chs_v:
        cmp     ebp,[g_cmn]
        jae     @chs_r
        movzx   ebx,word ptr [g_cmid+ebp*2]
        call    chn_kind
        mov     ecx,1
        call    chn_room
        stosb
        test    al,al
        js      @chs_a
        call    chn_cell
        jecxz   @chs_s
        call    chn_room
        mov     esi,edx
        rep     movsb
        jmp     @chs_n
@chs_s:
        mov     eax,edx
        call    chn_sout
        jmp     @chs_n
@chs_a:
        mov     edx,fs:[ebx*8].TVAR.vt        ; n, the counts, the lower bounds; 0
        xor     ecx,ecx                 ; none
        test    edx,edx
        jz      @F
        mov     cl,[edx]
@@:
        lea     ecx,[ecx*4+1]
        call    chn_room
        test    edx,edx
        jnz     @F
        mov     byte ptr [edi],0
        inc     edi
        jmp     @chs_n
@@:
        mov     al,[edx]
        stosb
        dec     ecx
        lea     esi,[edx+AH_CNT]
        rep     movsb
        call    chn_elems
        mov     esi,[edx+AH_PTR]
        test    eax,eax
        jz      @chs_e
        mov     ecx,eax
        call    chn_room
        rep     movsb
        jmp     @chs_n
@chs_e:                                 ; strings
        mov     eax,esi
        call    chn_sout
        add     esi,DSZ
        loop    @chs_e
@chs_n:
        inc     ebp
        jmp     @chs_v
@chs_r:
        mov     [g_chr],offset g_chb
        mov     [g_che],edi
        popad
        ret

; ::::::::::::::
; sc_cmn - COMMON's variable ( id -- ): named, for a CHAIN; CHAIN's next
; value, if any, stored in it.
;
; error:  Type mismatch  not the value's kind
;         Out of memory  past NCMN variables
; ::::::::::::::
sc_cmn:
        dpop    ebx
        push    esi
        mov     eax,[g_cmn]
        cmp     eax,NCMN
        jae     err_om
        mov     [g_cmid+eax*2],bx
        inc     [g_cmn]
        mov     esi,[g_chr]
        cmp     esi,[g_che]
        jae     @cmn_r
        call    chn_kind
        cmp     al,[esi]
        jne     err_tm
        inc     esi
        test    al,al
        js      @cmn_a
        call    chn_cell
        mov     edi,edx
        jecxz   @cmn_s
        rep     movsb
        jmp     @cmn_e
@cmn_s:
        call    chn_sin
        jmp     @cmn_e
@cmn_a:
        lodsb
        test    al,al
        jz      @cmn_e
        cmp     fs:[ebx*8].TVAR.vt,0
        je      @F
        and     [g_afrep],0
        call    arr_free
@@:
        mov     [g_state.rn.narr],al
        mov     [g_state.mm.aid],bx
        movzx   ecx,al
        push    ecx
        mov     edi,offset g_cntbuf
        rep     movsw
        pop     ecx
        mov     edi,offset g_lbbuf
        rep     movsw
        mov     [g_dyn],1
        call    arr_new
        mov     [g_dyn],0
        mov     edx,edi
        movzx   ebx,[g_state.mm.aid]
        call    chn_elems
        mov     edi,[edx+AH_PTR]
        test    eax,eax
        jz      @cmn_t
        mov     ecx,eax
        rep     movsb
        jmp     @cmn_e
@cmn_t:                                 ; strings
        call    chn_sin
        add     edi,DSZ
        loop    @cmn_t
@cmn_e:
        mov     [g_chr],esi
@cmn_r:
        pop     esi
        jmp     ex_lp

; ::::::::::::::
; sc_chain - CHAIN ( s -- ): COMMON's values kept, the file s (.BAS when it
; has no extension) loaded and run.
;
; error:  Advanced feature unavailable  in the editor
;         as rpl_load
; ::::::::::::::
sc_chain:
if IDE
        cmp     [g_ui.lp.ideon],0
        mov     al,E_AF
        jne     err_raise
endif
        dpop    eax
        call    str_get
        movzx   ecx,cx
        cmp     ecx,120
        ja      err_iq
        mov     esi,[g_state.sg.spt]
        mov     edi,offset g_fnmb
        xor     edx,edx
        jecxz   @chn_x
@@:
        lodsb
        stosb
        cmp     al,'.'
        jne     @chn_c
        mov     dl,1
@chn_c:
        cmp     al,'\'
        jne     @chn_d
        xor     edx,edx
@chn_d:
        loop    @B
@chn_x:
        test    edx,edx
        jnz     @F
        mov     dword ptr [edi],'SAB.'
        add     edi,4
@@:
        mov     byte ptr [edi],0
        call    chn_save
        mov     [g_chf],1
        call    prg_init
        call    rpl_load
        call    prg_drop
        call    cm_link
        mov     ebx,PROGBASE
        cmp     gs:[ebx].TLINE.len,0
        jne     ex_rungo
        jmp     ex_end

; ::::::::::::::
; chn_start - a run's start: no COMMON named; CHAIN's values for this run
; only.
;
; keeps:  all
; ::::::::::::::
chn_start:
        and     [g_cmn],0
        cmp     [g_chf],0
        jne     @F
        push    eax
        mov     eax,[g_che]
        mov     [g_chr],eax
        pop     eax
@@:
        mov     [g_chf],0
        ret

        .data
g_chr   dd      0                       ; chb: the next value, the end
g_che   dd      0
g_chf   db      0                       ; CHAIN's run starting
        .data?
g_cmn   dd      ?
g_cmid  dw      NCMN dup(?)
g_chb   db      CHBSZ dup(?)
        .code

        end
