; ==========================================================================
; str.asm - strings and their heap
; ==========================================================================
; holds:    str_*
; uses:     mem_fs, mem_cpy, mem_up, io_putc, err_*
; data:     A string value: EAX = the linear address of its Desc (len:2,
;           ptr:4 linear): a variable's slot, an array element or a temporary
;           in tmpd; tsp = DSZ * temporaries. At most SMAX chars.
;           The heap runs from hbot to htop: arrays up to aend, strings down
;           to sfr. A string block: the chars, len:2, owner:4, the owner the
;           Desc's linear address; live while that Desc points at the chars.
; calls:
;   str_set            pointed at, taken over, or copied by str_alloc
;   str_alloc          full: str_gc
; ==========================================================================

; ::::::::::::::
; str_get - str_getd of the string value.
; ::::::::::::::
str_get:

; ::::::::::::::
; str_getd - the length and chars of the Desc at EAX; a top temporary pops.
;
; in:     EAX  the Desc
; out:    sln, CX  the length
;         spt      the chars
;         ZF       set when empty
; keeps:  EAX, EDX, SI, DI
; ::::::::::::::
str_getd:
        movzx   ebx,[tsp]
        add     ebx,[tmplin]
        sub     ebx,DSZ
        cmp     ebx,eax
        jne     @sgd_n
        sub     [tsp],DSZ
@sgd_n:
        call    mem_fs
        mov     cx,fs:[bx+D_LEN]
        mov     ebx,dword ptr fs:[bx+D_PTR]
        mov     [spt],ebx
        mov     [sln],cx
        test    cx,cx
        ret

; ::::::::::::::
; str_new - str_push of the block str_alloc made; the temporary owns it.
;
; in:     dpv, sn  from str_alloc
; out:    as str_push
; ::::::::::::::
str_new:
        mov     eax,[dpv]
        mov     [spt],eax
        mov     ax,[sn]
        mov     [sln],ax
        movzx   eax,[tsp]
        add     eax,[tmplin]
        call    str_own

; ::::::::::::::
; str_push - the string value is a new temporary for sln/spt.
;
; in:     sln, spt  the length, the chars
; out:    EAX  the temporary
; error:  ?FORMULA TOO COMPLEX  NTMP in use
; ::::::::::::::
str_push:
        mov     bx,[tsp]
        cmp     bx,DSZ*NTMP
        jae     err_st
        mov     eax,[spt]
        mov     dword ptr [tmpd+bx+D_PTR],eax
        mov     ax,[sln]
        mov     word ptr [tmpd+bx+D_LEN],ax
        add     [tsp],DSZ
        movzx   eax,bx
        add     eax,[tmplin]
        ret

; ::::::::::::::
; str_own - the block of sln chars at spt is owned by EAX.
;
; keeps:  EAX, SI, DI
; ::::::::::::::
str_own:
        push    eax
        movzx   eax,[sln]
        add     eax,[spt]
        call    mem_fs
        pop     eax
        mov     dword ptr fs:[bx+B_OWN],eax
        ret

; ::::::::::::::
; str_fit - the chars of a block of sn chars under sfr.
;
; out:    dpv  sfr - sn - BHDR
;         CF   set when it fits above aend
; keeps:  SI, DI
; ::::::::::::::
str_fit:
        movzx   eax,[sn]
        add     eax,BHDR
        neg     eax
        add     eax,[sfr]
        mov     [dpv],eax
        mov     ebx,[aend]
        cmp     ebx,eax
        ret

; ::::::::::::::
; str_alloc - a block for AX chars, no owner yet.
;
; in:     AX  the length
; out:    dpv  its chars
;         sn   AX
;         sfr  dpv
; keeps:  SI, DI
; error:  ?OUT OF MEMORY  no room after str_gc
; ::::::::::::::
str_alloc:
        mov     [sn],ax
        call    str_fit
        jc      @sal_o
        call    str_gc
        jc      @sal_o
        jmp     err_om
@sal_o:
        mov     eax,[dpv]
        mov     [sfr],eax
        movzx   ebx,[sn]
        add     eax,ebx
        call    mem_fs
        mov     ax,[sn]
        mov     fs:[bx+B_LEN],ax
        mov     dword ptr fs:[bx+B_OWN],0
        ret

; ::::::::::::::
; str_gc - collect the heap: from the top down the live blocks slide up,
; their owners follow; the dead are dropped.
;
; out:    sfr  under the live blocks
;         as str_fit
; keeps:  SI, DI
; ::::::::::::::
str_gc:
        mov     eax,[htop]
        mov     [gq],eax
        mov     [gd],eax
@sgc_l:
        mov     eax,[gq]
        cmp     eax,[sfr]
        jbe     @sgc_e
        sub     eax,BHDR
        call    mem_fs
        movzx   ecx,word ptr fs:[bx+B_LEN]
        mov     edx,dword ptr fs:[bx+B_OWN]
        sub     eax,ecx
        mov     [gq],eax                ; the chars, the next block's end
        test    edx,edx
        jz      @sgc_l
        mov     ebx,edx
        sub     ebx,[tmplin]
        cmp     ebx,DSZ*NTMP
        jae     @sgc_v
        cmp     bx,[tsp]
        jae     @sgc_l                  ; a popped temporary
@sgc_v:
        xchg    eax,edx
        call    mem_fs
        mov     ebx,dword ptr fs:[bx+D_PTR]
        cmp     ebx,edx
        jne     @sgc_l
        add     ecx,BHDR
        mov     ebx,[gd]
        sub     ebx,ecx
        mov     [gd],ebx
        push    eax                     ; the owner
        mov     eax,edx
        mov     edx,ebx
        call    mem_up
        pop     eax
        call    mem_fs
        mov     edx,[gd]
        mov     fs:[bx+D_PTR],edx
        jmp     @sgc_l
@sgc_e:
        mov     eax,[gd]
        mov     [sfr],eax
        jmp     str_fit

; ::::::::::::::
; str_set - store the string value in the Desc at bpo.
;
; Empty or in the program: pointed at. A temporary's own heap block: taken
; over. Else a copy.
;
; in:     EAX  the value
;         bpo  the Desc, linear
; error:  ?OUT OF MEMORY  no room for the copy
; ::::::::::::::
str_set:
        call    str_get
        jz      @sst_s
        mov     edx,[spt]
        sub     edx,[proglin]
        cmp     edx,PROGSZ
        jb      @sst_s
        mov     edx,[spt]
        cmp     edx,[sfr]
        jb      @sst_c
        cmp     edx,[htop]
        jae     @sst_c
        push    eax
        movzx   eax,[sln]
        add     eax,edx
        call    mem_fs
        pop     eax
        cmp     eax,fs:[bx+B_OWN]
        je      @sst_o
@sst_c:
        mov     [sac],eax
        mov     ax,[sln]
        call    str_alloc
        mov     eax,[sac]
        call    str_getd
        call    str_cpyd
        mov     eax,[dpv]
        mov     [spt],eax
@sst_o:
        mov     eax,[bpo]
        call    str_own
@sst_s:
        mov     eax,[bpo]
        call    mem_fs
        mov     eax,[spt]
        mov     fs:[bx+D_PTR],eax
        mov     ax,[sln]
        mov     fs:[bx+D_LEN],ax
        ret

; ::::::::::::::
; str_cpyd - copy sln chars from spt to dpv.
;
; in:     sln, spt, dpv  the count, the source, the destination
; keeps:  SI, DI
; ::::::::::::::
str_cpyd:
        mov     edx,[dpv]

; ::::::::::::::
; str_cpy - copy sln chars from spt to EDX.
;
; in:     sln, spt  the count, the source
;         EDX       the destination, linear
; keeps:  SI, DI
; ::::::::::::::
str_cpy:
        mov     eax,[spt]
        mov     cx,[sln]
        jmp     mem_cpy

; ::::::::::::::
; str_near - the first 255 chars to nbuf, a 0 after them.
;
; in:     sln, spt  the string
; out:    SI   nbuf
;         sln  255 at most
; keeps:  DI
; ::::::::::::::
str_near:
        cmp     [sln],255
        jbe     @snr_c
        mov     [sln],255
@snr_c:
        mov     edx,[dslin]
        add     edx,offset nbuf
        call    str_cpy
        mov     bx,[sln]
        mov     byte ptr [nbuf+bx],0
        mov     si,offset nbuf
        ret

; ::::::::::::::
; str_print - print sln chars from spt, 255 at a time through nbuf.
;
; in:     sln, spt  the count, the chars
; keeps:  SI, DI
; ::::::::::::::
str_print:
        push    si
        mov     dx,[sln]
@spr_c:
        test    dx,dx
        jz      @spr_e
        mov     [sln],dx
        push    dx
        call    str_near
        mov     cx,[sln]
        movzx   eax,cx
        add     [spt],eax
@spr_l:
        lodsb
        call    io_putc
        loop    @spr_l
        pop     dx
        sub     dx,[sln]
        jmp     @spr_c
@spr_e:
        pop     si
        ret

; ::::::::::::::
; str_lit - a "literal": a temporary for its text.
;
; in:     SI  at the opening quote
; out:    EAX  the temporary
;         SI   past the closing quote, or at the 0
; ::::::::::::::
str_lit:
        inc     si
        mov     ax,'""'
        call    str_scan

; ::::::::::::::
; str_skq - past the closing quote at SI, if there is one.
;
; in:     SI  at the quote or the 0
; keeps:  EAX
; ::::::::::::::
str_skq:
        cmp     byte ptr [si],0
        je      @skq_r
        inc     si
@skq_r:
        ret

; ::::::::::::::
; str_item - a READ or INPUT item: "quoted", or up to ',' ':' or the end.
;
; in:     SI  at it, or spaces before it
; out:    EAX  the temporary, the text in place
;         SI   at its end
; ::::::::::::::
str_item:
        call    st_get
        cmp     al,'"'
        je      str_lit
        mov     ax,':,'

; ::::::::::::::
; str_scan - a temporary for the text at SI up to 0, AL or AH.
;
; in:     SI      the first char
;         AL, AH  the stop bytes
; out:    EAX  the temporary
;         SI   at the stop
; ::::::::::::::
str_scan:
        call    str_span
        mov     [sln],ax
        movzx   ebx,bx
        add     ebx,[dslin]
        mov     [spt],ebx
        jmp     str_push

; ::::::::::::::
; str_span - the text at SI up to 0, AL or AH: its length.
;
; in:     SI      the first char
;         AL, AH  the stop bytes
; out:    AX  the length
;         BX  the start
;         SI  at the stop
; ::::::::::::::
str_span:
        mov     bx,si
@ssp_l:
        mov     cl,[si]
        test    cl,cl
        jz      @ssp_e
        cmp     cl,al
        je      @ssp_e
        cmp     cl,ah
        je      @ssp_e
        inc     si
        jmp     @ssp_l
@ssp_e:
        mov     ax,si
        sub     ax,bx
        ret

; ::::::::::::::
; str_dlen - the length in the Desc at EAX.
;
; out:    CX  the length
; keeps:  all but BX, CX, FS
; ::::::::::::::
str_dlen:
        call    mem_fs
        mov     cx,fs:[bx+D_LEN]
        ret

; ::::::::::::::
; str_cat - the left string + the right.
;
; in:     EDX  the left Desc
;         EAX  the right Desc
; out:    EAX  the joined temporary
; error:  ?STRING TOO LONG  over SMAX chars
; ::::::::::::::
str_cat:
        mov     [sac],eax
        mov     [sab],edx
        call    str_dlen
        mov     ax,cx
        xchg    eax,edx
        call    str_dlen
        add     cx,dx
        cmp     cx,SMAX
        ja      @sca_l
        mov     ax,cx
        call    str_alloc
        mov     eax,[sab]
        call    str_dlen
        movzx   edx,cx
        add     edx,[dpv]
        mov     eax,[sac]
        call    str_get
        call    str_cpy
        mov     eax,[sab]
        call    str_getd
        call    str_cpyd
        jmp     str_new
@sca_l:
        mov     al,E_LS
        jmp     err_raise

; ::::::::::::::
; str_cmp - compare the left string with the right; a prefix is less.
;
; in:     EDX  the left Desc
;         EAX  the right Desc
; out:    CL  1 left greater, 2 equal, 4 less
; keeps:  SI, DI
; ::::::::::::::
str_cmp:
        mov     [sab],edx
        call    str_get
        mov     dx,cx
        mov     ebx,[spt]
        mov     [sac],ebx
        mov     eax,[sab]
        call    str_getd
        push    si
        push    di
        push    es
        mov     eax,[spt]
        call    mem_fs
        mov     si,bx
        mov     eax,[sac]
        mov     ebx,eax
        shr     ebx,4
        mov     es,bx
        mov     di,ax
        and     di,15
        mov     cx,[sln]
@scm_l:
        jcxz    @scm_e
        test    dx,dx
        jz      @scm_g
        cmps    byte ptr fs:[si],byte ptr es:[di]
        jne     @scm_n
        dec     cx
        dec     dx
        jmp     @scm_l
@scm_e:
        mov     cl,2
        test    dx,dx
        jz      @scm_r
@scm_lt:
        mov     cl,4
        jmp     @scm_r
@scm_n:
        jb      @scm_lt
@scm_g:
        mov     cl,1
@scm_r:
        pop     es
        pop     di
        pop     si
        ret

; ::::::::::::::
; str_sub - a part of the string value: from s0v (0 the first), s1v at most.
;
; in:     EAX       the Desc
;         s0v, s1v  the start, the most chars
; out:    EAX  a new temporary
; ::::::::::::::
str_sub:
        mov     [sac],eax
        call    str_dlen
        mov     ax,cx
        sub     ax,[s0v]
        jnc     @ssb_a
        xor     ax,ax
@ssb_a:
        cmp     ax,[s1v]
        jb      @ssb_b
        mov     ax,[s1v]
@ssb_b:
        call    str_alloc
        mov     eax,[sac]
        call    str_get
        movzx   eax,[s0v]
        add     [spt],eax

; ::::::::::::::
; str_tods - copy sn chars from spt to the block: a new temporary.
;
; in:     spt, dpv, sn  the source, the block's chars, the count
; out:    EAX  the temporary
; ::::::::::::::
str_tods:
        mov     ax,[sn]
        mov     [sln],ax
        call    str_cpyd
        jmp     str_new

; ::::::::::::::
; str_buftmp - a new temporary for the first AX chars of sbuf.
;
; in:     AX  the length
; out:    EAX  the temporary
; ::::::::::::::
str_buftmp:
        call    str_alloc
        mov     eax,[dslin]
        add     eax,offset sbuf
        mov     [spt],eax
        jmp     str_tods
