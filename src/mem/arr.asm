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
        mov     cl,[g_sinfo+ebx]
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
; error:  ?BAD SUBSCRIPT     n is not the array's, or si not under ci
;         ?ILLEGAL QUANTITY  si under 0
;         ?OUT OF MEMORY     a new array does not fit
; ::::::::::::::
arr_ref:
        mov     [g_state.mm.aid],bx
        mov     edi,[g_vt+ebx*4]
        test    edi,edi
        jnz     @arf_h
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
; arr_bs - ?ILLEGAL QUANTITY if a subscript is negative, else ?BAD SUBSCRIPT.
;
; in:     narr, dimbuf  the subscripts
; error:  ?ILLEGAL QUANTITY  a subscript under 0
;         ?BAD SUBSCRIPT     none is
; ::::::::::::::
arr_bs:
        call    arr_chk
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

        .data?
g_dimbuf dw      64 dup(?)
g_cntbuf dw      64 dup(?)
g_lbbuf dw      64 dup(?)                   ; the lower bounds
        .code

        end
