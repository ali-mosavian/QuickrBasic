; ==========================================================================
; arr.asm - arrays
; ==========================================================================
; holds:    arr_ref, arr_dim, arr_new
; uses:     str_gc, err_*
; data:     vt: the header in DS: n, the data's segment, the fast count, the
;           n counts (words). The data on the heap, zeroed, past 64K as needed:
;           VS for a number, 2 for %, DSZ for $.
; calls:
;   arr_ref            a new array: arr_new, every count 11
; ==========================================================================

; ::::::::::::::
; arr_esz - the element size of an array.
;
; in:     BX  the id
; out:    CX  VS for a number, 2 for %, DSZ for $
; keeps:  BX
; ::::::::::::::
arr_esz:
        mov     cl,[sinfo+bx]
        and     cl,3
        mov     ch,VS
        cmp     cl,1
        jne     @aes_s
        mov     ch,2
@aes_s:
        cmp     cl,2
        jne     @aes_r
        mov     ch,DSZ
@aes_r:
        movzx   cx,ch
        ret

; ::::::::::::::
; arr_ref - the address of an element; a new array gets every count 11.
;
; in:     BX      the id
;         narr    n, the subscripts
;         dimbuf  the subscripts
; out:    FS:DI  the element, DI under 16
; keeps:  SI
; error:  ?BAD SUBSCRIPT     n is not the array's, or si not under ci
;         ?ILLEGAL QUANTITY  si under 0
;         ?OUT OF MEMORY     a new array does not fit
; ::::::::::::::
arr_ref:
        mov     [aid],bx
        add     bx,bx
        mov     di,[vt+bx]
        test    di,di
        jnz     @arf_h
        movzx   cx,[narr]
@arf_c:
        mov     bx,cx
        add     bx,bx
        mov     word ptr [cntbuf+bx-2],11
        loop    @arf_c
        call    arr_new
@arf_h:
        mov     cl,[narr]
        cmp     cl,[di]
        jne     arr_bs
        xor     edx,edx
        xor     bx,bx
@arf_k:
        mov     ax,[dimbuf+bx]
        cmp     ax,[di+AH_CNT+bx]
        jae     arr_bs
        movzx   eax,ax
        add     edx,eax
        dec     cl
        jz      @arf_t
        movzx   eax,word ptr [di+AH_CNT+2+bx]
        imul    edx,eax
        add     bx,2
        jmp     @arf_k
@arf_t:
        mov     bx,[aid]
        call    arr_esz
        movzx   ecx,cx
        imul    edx,ecx
        movzx   eax,word ptr [di+AH_SEG]
        shl     eax,4
        add     eax,edx
        call    mem_fs
        mov     di,bx
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
        movzx   cx,[narr]
@ack_l:
        mov     bx,cx
        add     bx,bx
        mov     ax,[dimbuf+bx-2]
        test    ax,ax
        js      err_iq
        inc     ax
        mov     [cntbuf+bx-2],ax
        loop    @ack_l
        ret

; ::::::::::::::
; arr_dim - DIM: create an array; the counts are the subscripts + 1.
;
; in:     BX      the id
;         narr    n
;         dimbuf  the subscripts
; error:  ?ILLEGAL QUANTITY  a subscript under 0, before all the others
;         ?REDIM'D ARRAY     the array exists
;         ?OUT OF MEMORY     it does not fit
; ::::::::::::::
arr_dim:
        mov     [aid],bx
        call    arr_chk
        mov     bx,[aid]
        add     bx,bx
        cmp     word ptr [vt+bx],0
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
; out:    DI  the header
; keeps:  SI
; error:  ?OUT OF MEMORY  not under the strings
; ::::::::::::::
arr_new:
        mov     bx,[aid]
        call    arr_esz
        movzx   edx,cx
        movzx   cx,[narr]
        xor     bx,bx
@anw_m:
        movzx   eax,word ptr [cntbuf+bx]
        imul    edx,eax
        jo      @st_arr1
        cmp     edx,100000h
        ja      @st_arr1
        add     bx,2
        loop    @anw_m
        mov     ax,[ahd]
        add     ax,bx
        add     ax,AH_CNT
        cmp     ax,HDRLIM
        ja      @st_arr1
        mov     [asz],edx
        call    @anw_e
        jbe     @anw_f
        call    str_gc
        call    @anw_e
        ja      err_om
@anw_f:
        xchg    eax,[aend]
        push    eax
        mov     ecx,[asz]
        call    mem_zero
        pop     eax
        shr     eax,4
        mov     di,[ahd]
        mov     [di+AH_SEG],ax
        xor     ax,ax
        cmp     [asz],10000h
        ja      @anw_s
        cmp     [narr],1
        jne     @anw_s
        mov     ax,word ptr [cntbuf]
@anw_s:
        mov     [di+AH_FAST],ax
        mov     bx,[aid]
        add     bx,bx
        mov     [vt+bx],di
        mov     al,[narr]
        mov     [di],al
        movzx   cx,al
        add     cx,cx
        push    si
        push    di
        mov     si,offset cntbuf
        add     di,AH_CNT
        rep     movsb
        mov     [ahd],di
        pop     di
        pop     si
        ret
; EAX = the data's end, paragraphs whole; flags: above sfr.
@anw_e:
        mov     eax,[asz]
        add     eax,[aend]
        add     eax,15
        and     al,0F0h
        cmp     eax,[sfr]
        ret
