; ==========================================================================
; arr.asm - arrays
; ==========================================================================
; holds:    arr_ref, arr_dim, arr_new
; uses:     str_gc, err_*
; data:     vt: the header in dsb: n, the data's address, the fast count, the
;           n counts, the n lower bounds (words). The data on the heap, zeroed:
;           VS for a number, 2 for %, DSZ for $.
; calls:
;   arr_ref            a new array: arr_new, every count 11
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; arr_esz - the element size of an array.
;
; in:     EBX  the id
; out:    ECX  VS for a number or %, DSZ for $
; keeps:  EBX
; ::::::::::::::
arr_esz:
        movzx   ecx,[g_vtyp+ebx]
        sub     ecx,ET_REC              ; a record's: its TYPE's size
        jb      @aes_k
        movzx   ecx,word ptr [g_typ+ecx*8+T_SIZE]
        ret
@aes_k:
        mov     cl,[g_sinfo+ebx]
        mov     ch,8
        test    cl,SK_DBL
        jnz     @aes_r
        and     cl,3
        mov     ch,VS
        cmp     cl,2
        jne     @aes_r
        mov     ch,DSZ
@aes_r:
        movzx   ecx,ch
        ret

; ::::::::::::::
; arr_ref - the address of an element; a new array gets every count 11.
;
; in:     EBX     the id
;         narr    n, the subscripts
;         dimbuf  the subscripts
; out:    EDI  the element
; keeps:  ESI
; error:  Subscript out of range  n is not the array's, a subscript is
;                                 out of its bounds, or ERASE freed it
;         Out of memory           a new array does not fit
; ::::::::::::::
arr_ref:
        mov     [g_state.mm.aid],bx
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        jnz     @arf_h
        cmp     [g_adyn+ebx],0          ; ERASE freed it: not made again
        jne     arr_bs
        movzx   ecx,[g_state.rn.narr]
@arf_c:
        mov     word ptr [g_cntbuf+ecx*2-2],11
        and     word ptr [g_lbbuf+ecx*2-2],0
        loop    @arf_c
        call    arr_new
@arf_h:
        mov     cl,[g_state.rn.narr]
        cmp     cl,[edi]
        jne     arr_bs
        push    ebp
        movzx   ebp,cl
        lea     ebp,[edi+ebp*2]         ; the lower bounds, less AH_CNT
        xor     edx,edx
        xor     ebx,ebx
@arf_k:
        mov     ax,[g_dimbuf+ebx]
        sub     ax,[ebp+ebx+AH_CNT]
        cmp     ax,[edi+AH_CNT+ebx]
        jae     @arf_b
        movzx   eax,ax
        add     edx,eax
        dec     cl
        jz      @arf_t
        movzx   eax,word ptr [edi+AH_CNT+2+ebx]
        imul    edx,eax
        add     ebx,2
        jmp     @arf_k
@arf_b:
        pop     ebp
        jmp     arr_bs
@arf_t:
        pop     ebp
        movzx   ebx,[g_state.mm.aid]
        call    arr_esz
        imul    edx,ecx
        add     edx,[edi+AH_PTR]
        mov     edi,edx
        ret

; ::::::::::::::
; arr_bs - Subscript out of range.
; ::::::::::::::
arr_bs:
        mov     al,E_BS
        jmp     err_raise

; ::::::::::::::
; arr_chk - ?ILLEGAL QUANTITY for any negative subscript; the counts, si + 1.
;
; in:     narr, dimbuf  the subscripts
; out:    cntbuf  each si + 1
; error:  ?ILLEGAL QUANTITY  a subscript under 0
; ::::::::::::::
arr_chk:
        movzx   ecx,[g_state.rn.narr]
@ack_l:
        mov     ax,[g_dimbuf+ecx*2-2]
        test    ax,ax
        js      err_iq
        inc     ax
        mov     [g_cntbuf+ecx*2-2],ax
        loop    @ack_l
        ret

; ::::::::::::::
; arr_dim - DIM: create an array; the counts are the subscripts + 1.
;
; in:     EBX     the id
;         narr    n
;         dimbuf  the subscripts
; error:  ?ILLEGAL QUANTITY  a subscript under 0, before all the others
;         ?REDIM'D ARRAY     the array exists
;         ?OUT OF MEMORY     it does not fit
; ::::::::::::::
arr_dim:
        mov     [g_state.mm.aid],bx
        call    arr_chk
        movzx   ebx,[g_state.mm.aid]
        cmp     [g_vt+ebx*4],0
        je      arr_new
        mov     al,E_RD
        jmp     err_raise
@st_arr1:
        jmp     err_om

; ::::::::::::::
; arr_new - create an array: its header at ahd, its data zeroed at aend.
;
; in:     aid     the id
;         narr    n
;         cntbuf  the counts
; out:    EDI  the header
; keeps:  ESI
; error:  ?OUT OF MEMORY  not under the strings
; ::::::::::::::
arr_new:
        movzx   ebx,[g_state.mm.aid]
        call    arr_esz
        mov     edx,ecx
        movzx   ecx,[g_state.rn.narr]
        xor     ebx,ebx
@anw_m:
        movzx   eax,word ptr [g_cntbuf+ebx]
        imul    edx,eax
        jo      @st_arr1
        cmp     edx,100000h
        ja      @st_arr1
        add     ebx,2
        loop    @anw_m
        mov     eax,[g_state.mm.ahd]
        lea     eax,[eax+ebx*2+AH_CNT]
        cmp     eax,HDRLIM
        ja      @st_arr1
        mov     [g_state.mm.asz],edx
        call    @anw_e
        jbe     @anw_f
        call    str_gc
        call    @anw_e
        ja      err_om
@anw_f:
        xchg    eax,[g_state.mm.aend]
        push    eax
        mov     ecx,[g_state.mm.asz]
        call    mem_zero
        pop     eax
        mov     edi,[g_state.mm.ahd]
        mov     [edi+AH_PTR],eax
        xor     ax,ax
        cmp     [g_state.rn.narr],1
        jne     @anw_s
        cmp     word ptr [g_lbbuf],0    ; the fast path: 1-D from 0
        jne     @anw_s
        mov     ax,word ptr [g_cntbuf]
@anw_s:
        mov     [edi+AH_FAST],ax
        movzx   ebx,[g_state.mm.aid]
        mov     [g_vt+ebx*4],edi
        mov     al,[g_dyn]
        mov     [g_adyn+ebx],al
        mov     al,[g_state.rn.narr]
        mov     [edi],al
        movzx   ecx,al
        add     ecx,ecx
        push    esi
        push    edi
        mov     esi,offset g_cntbuf
        add     edi,AH_CNT
        push    ecx
        rep     movsb
        pop     ecx
        mov     esi,offset g_lbbuf
        rep     movsb
        mov     [g_state.mm.ahd],edi
        pop     edi
        pop     esi
        ret
; EAX = the data's end, on 4; flags: above sfr.
@anw_e:
        mov     eax,[g_state.mm.asz]
        add     eax,[g_state.mm.aend]
        add     eax,3
        and     al,0FCh
        cmp     eax,[g_state.mm.sfr]
        ret

; ::::::::::::::
; arr_size - an array's data size.
;
; in:     EBX  the id, EDI  its header
; out:    EAX  the bytes
; keeps:  all but EAX
; ::::::::::::::
arr_size:
        push    ecx
        push    edx
        push    esi
        call    arr_esz
        mov     eax,ecx
        movzx   edx,byte ptr [edi]
        lea     esi,[edi+AH_CNT]
@asz_l:
        movzx   ecx,word ptr [esi]
        imul    eax,ecx
        add     esi,2
        dec     edx
        jnz     @asz_l
        pop     esi
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; arr_free - drop array EBX: its header and data go, what follows them moves
; down. The headers' data pointers, the string blocks' owners in the moved
; data, and the headers kept in vt and on the save stack follow; one kept
; that was this array's becomes afrep. Its strings die with their Descs.
;
; in:     EBX    the id, an array
;         afrep  what this array's other references become
; keeps:  all
; ::::::::::::::
arr_free:
        pushad
        mov     edi,[g_vt+ebx*4]
        and     dword ptr [g_vt+ebx*4],0
        mov     [g_afh],edi
        call    arr_size
        mov     ebp,[edi+AH_PTR]        ; the data: [D, D+S) goes
        add     eax,ebp
        add     eax,3
        and     al,0FCh
        sub     eax,ebp
        mov     [g_afsz],eax
        movzx   edx,byte ptr [edi]
        lea     edx,[edx*4+AH_CNT]
        mov     [g_afhs],edx
        lea     esi,[ebp+eax]           ; the data after it down
        mov     ecx,[g_state.mm.aend]
        sub     ecx,esi
        push    edi
        mov     edi,ebp
        rep     movsb
        pop     edi
        mov     ecx,[g_state.mm.aend]   ; the moved Descs' blocks: their owners
        mov     [g_afend],ecx
        sub     [g_state.mm.aend],eax
        lea     esi,[edi+edx]           ; the headers after it down
        mov     ecx,[g_state.mm.ahd]
        sub     ecx,esi
        rep     movsb
        sub     [g_state.mm.ahd],edx
        mov     edi,HDRBASE             ; their data pointers
@afr_h:
        cmp     edi,[g_state.mm.ahd]
        jae     @afr_s
        mov     eax,[edi+AH_PTR]
        cmp     eax,ebp
        jbe     @F
        sub     eax,[g_afsz]
        mov     [edi+AH_PTR],eax
@@:
        movzx   eax,byte ptr [edi]
        lea     edi,[edi+eax*4+AH_CNT]
        jmp     @afr_h
@afr_s:
        mov     eax,[g_state.mm.htop]   ; the string blocks
@afr_b:
        cmp     eax,[g_state.mm.sfr]
        jbe     @afr_v
        sub     eax,BHDR
        movzx   ecx,word ptr [eax+B_LEN]
        mov     edx,[eax+B_OWN]
        cmp     edx,ebp
        jb      @F
        cmp     edx,[g_afend]
        jae     @F
        sub     edx,[g_afsz]
        mov     [eax+B_OWN],edx
@@:
        sub     eax,ecx
        jmp     @afr_b
@afr_v:
        movzx   ecx,[g_state.lx.nsyms]  ; the headers kept: vt, the ids in use
        dec     ecx
        jecxz   @afr_s2
@afr_t:
        lea     eax,[g_vt+ecx*4]
        call    @afr_r
        loop    @afr_t
@afr_s2:
        mov     edx,offset g_lsav       ; the save stack's arrays
@afr_l:
        cmp     edx,[g_state.rn.lsp]
        jae     @afr_x
        movzx   ecx,word ptr [edx]
        test    [g_sinfo+ecx],80h
        jz      @F
        lea     eax,[edx+4]
        call    @afr_r
@@:
        add     edx,LSSZ
        jmp     @afr_l
@afr_x:
        popad
        ret
; [EAX], a header: past the dropped one moves down, it becomes afrep.
@afr_r:
        mov     ebx,[eax]
        sub     ebx,[g_afh]
        jb      @afr_rr
        jnz     @F
        mov     ebx,[g_afrep]
        mov     [eax],ebx
        ret
@@:
        mov     ebx,[g_state.mm.ahd]
        add     ebx,[g_afhs]
        cmp     [eax],ebx
        jae     @afr_rr
        mov     ebx,[g_afhs]
        sub     [eax],ebx
@afr_rr:
        ret

; ::::::::::::::
; arr_keep - REDIM PRESERVE: each element of the array at pold that the
; array at pnew has too, by its subscripts, copied there; a string's moved.
;
; in:     EBX  the id; pold and pnew, as many dimensions
; keeps:  all
; ::::::::::::::
arr_keep:
        pushad
        mov     [g_kid],ebx
        mov     esi,[g_pold]
        movzx   ecx,byte ptr [esi]
        mov     [g_kn],ecx
        call    arr_esz
        mov     [g_kesz],ecx
        mov     ecx,[g_kn]              ; the old elements: the counts' product
        mov     eax,1
@akp_t:
        movzx   edx,word ptr [esi+AH_CNT+ecx*2-2]
        imul    eax,edx
        loop    @akp_t
        mov     [g_kcnt],eax
        and     [g_ki],0
@akp_e:                                 ; element ki: its subscripts, the last
        mov     eax,[g_ki]              ; the fastest
        cmp     eax,[g_kcnt]
        jae     @akp_r
        mov     esi,[g_pold]
        mov     edi,[g_pnew]
        mov     ecx,[g_kn]
        xor     ebp,ebp                 ; the new index
        mov     [g_kmul],1
@akp_s:
        movzx   ebx,word ptr [esi+AH_CNT+ecx*2-2]
        xor     edx,edx
        div     ebx                     ; EDX the old relative subscript
        mov     ebx,[g_kn]              ; the lower bounds
        lea     ebx,[esi+ebx*2]
        movsx   ebx,word ptr [ebx+AH_CNT+ecx*2-2]
        add     edx,ebx
        mov     ebx,[g_kn]
        lea     ebx,[edi+ebx*2]
        movsx   ebx,word ptr [ebx+AH_CNT+ecx*2-2]
        sub     edx,ebx                 ; the new relative subscript
        movzx   ebx,word ptr [edi+AH_CNT+ecx*2-2]
        cmp     edx,ebx
        jae     @akp_n                  ; not in the new one (or under 0)
        imul    edx,[g_kmul]
        add     ebp,edx
        imul    ebx,[g_kmul]
        mov     [g_kmul],ebx
        loop    @akp_s
        mov     eax,[g_kesz]            ; copy
        mov     ecx,eax
        imul    ebp,eax
        imul    eax,[g_ki]
        add     eax,[esi+AH_PTR]
        add     ebp,[edi+AH_PTR]
        mov     ebx,[g_kid]
        mov     dl,[g_sinfo+ebx]
        and     dl,SK_DBL or 3
        cmp     dl,2
        jne     @akp_b
        cmp     byte ptr [g_vtyp+ebx],ET_REC
        jae     @akp_b
        mov     edi,ebp                 ; a string: its block moves too
        call    ls_mov
        jmp     @akp_n
@akp_b:
        mov     esi,eax
        mov     edi,ebp
        rep     movsb
@akp_n:
        inc     [g_ki]
        jmp     @akp_e
@akp_r:
        popad
        ret

; ::::::::::::::
; arr_rep - the references afrep marked: to array EBX's header.
;
; in:     EBX  the id
; keeps:  all
; ::::::::::::::
arr_rep:
        pushad
        mov     edi,[g_vt+ebx*4]
        movzx   ecx,[g_state.lx.nsyms]  ; the ids in use
        dec     ecx
        jecxz   @arp_s
@arp_t:
        cmp     dword ptr [g_vt+ecx*4],1
        jne     @F
        mov     [g_vt+ecx*4],edi
@@:
        loop    @arp_t
@arp_s:
        mov     edx,offset g_lsav
@arp_l:
        cmp     edx,[g_state.rn.lsp]
        jae     @arp_x
        movzx   ecx,word ptr [edx]
        test    [g_sinfo+ecx],80h
        jz      @F
        cmp     dword ptr [edx+4],1
        jne     @F
        mov     [edx+4],edi
@@:
        add     edx,LSSZ
        jmp     @arp_l
@arp_x:
        popad
        ret

        .data
g_dyn   db      0                       ; arr_new: a dynamic array (REDIM's)
        .data?
g_adyn  db      NSYM dup(?)             ; by id: REDIM made it; ERASE frees it
g_afh   dd      ?                       ; arr_free: the header, its size,
g_afhs  dd      ?
g_afsz  dd      ?                       ; the data's size, the data's old end
g_afend dd      ?
g_afrep dd      ?
g_pold  dd      ?                       ; arr_keep: the old header, the new
g_pnew  dd      ?
g_kn    dd      ?                       ; the dimensions, the element size,
g_kesz  dd      ?                       ; the old elements, this one, the new
g_kcnt  dd      ?                       ; count's product so far
g_ki    dd      ?
g_kmul  dd      ?
g_kid   dd      ?
g_dimbuf dw      64 dup(?)
g_cntbuf dw      64 dup(?)
g_lbbuf dw      64 dup(?)                   ; the lower bounds
        .code

        end
