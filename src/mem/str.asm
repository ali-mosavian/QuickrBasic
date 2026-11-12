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

include x86f.inc
        .code

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
        movzx   ebx,[g_state.sg.tsp]
        add     ebx,[g_state.sg.tmplin]
        sub     ebx,DSZ
        cmp     ebx,eax
        jne     @sgd_n
        sub     [g_state.sg.tsp],DSZ
@sgd_n:
        mov     cx,[eax+D_LEN]
        mov     ebx,[eax+D_PTR]
        mov     [g_state.sg.spt],ebx
        mov     [g_state.sg.sln],cx
        test    cx,cx
        ret

; ::::::::::::::
; str_new - str_push of the block str_alloc made; the temporary owns it.
;
; in:     dpv, sn  from str_alloc
; out:    as str_push
; ::::::::::::::
str_new:
        mov     eax,[g_state.sg.dpv]
        mov     [g_state.sg.spt],eax
        mov     ax,[g_state.sg.sn]
        mov     [g_state.sg.sln],ax
        movzx   eax,[g_state.sg.tsp]
        add     eax,[g_state.sg.tmplin]
        call    str_own

; ::::::::::::::
; str_push - the string value is a new temporary for sln/spt.
;
; in:     sln, spt  the length, the chars
; out:    EAX  the temporary
; error:  ?FORMULA TOO COMPLEX  NTMP in use
; ::::::::::::::
str_push:
        movzx   ebx,[g_state.sg.tsp]
        cmp     ebx,DSZ*NTMP
        jae     err_st
        mov     eax,[g_state.sg.spt]
        mov     dword ptr [g_tmpd+ebx+D_PTR],eax
        mov     ax,[g_state.sg.sln]
        mov     word ptr [g_tmpd+ebx+D_LEN],ax
        add     [g_state.sg.tsp],DSZ
        mov     eax,ebx
        add     eax,[g_state.sg.tmplin]
        ret

; ::::::::::::::
; str_own - the block of sln chars at spt is owned by EAX.
;
; keeps:  EAX, SI, DI
; ::::::::::::::
str_own:
        push    eax
        movzx   eax,[g_state.sg.sln]
        add     eax,[g_state.sg.spt]
        mov     ebx,eax
        pop     eax
        mov     [ebx+B_OWN],eax
        ret

; ::::::::::::::
; str_fit - the chars of a block of sn chars under sfr.
;
; out:    dpv  sfr - sn - BHDR
;         CF   set when it fits above aend
; keeps:  SI, DI
; ::::::::::::::
str_fit:
        movzx   eax,[g_state.sg.sn]
        add     eax,BHDR
        neg     eax
        add     eax,[g_state.mm.sfr]
        mov     [g_state.sg.dpv],eax
        mov     ebx,[g_state.mm.aend]
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
        mov     [g_state.sg.sn],ax
        call    str_fit
        jc      @sal_o
        call    str_gc
        jc      @sal_o
        jmp     err_om
@sal_o:
        mov     eax,[g_state.sg.dpv]
        mov     [g_state.mm.sfr],eax
        movzx   ebx,[g_state.sg.sn]
        add     eax,ebx
        mov     ebx,eax
        mov     ax,[g_state.sg.sn]
        mov     [ebx+B_LEN],ax
        mov     dword ptr [ebx+B_OWN],0
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
        mov     eax,[g_state.mm.htop]
        mov     [g_state.sg.gq],eax
        mov     [g_state.sg.gd],eax
@sgc_l:
        mov     eax,[g_state.sg.gq]
        cmp     eax,[g_state.mm.sfr]
        jbe     @sgc_e
        sub     eax,BHDR
        movzx   ecx,word ptr [eax+B_LEN]
        mov     edx,[eax+B_OWN]
        sub     eax,ecx
        mov     [g_state.sg.gq],eax                ; the chars, the next block's end
        test    edx,edx
        jz      @sgc_l
        mov     ebx,edx
        sub     ebx,[g_state.sg.tmplin]
        cmp     ebx,DSZ*NTMP
        jae     @sgc_v
        cmp     bx,[g_state.sg.tsp]
        jae     @sgc_l                  ; a popped temporary
@sgc_v:
        xchg    eax,edx
        mov     ebx,[eax+D_PTR]
        cmp     ebx,edx
        jne     @sgc_l
        add     ecx,BHDR
        mov     ebx,[g_state.sg.gd]
        sub     ebx,ecx
        mov     [g_state.sg.gd],ebx
        push    eax                     ; the owner
        mov     eax,edx
        mov     edx,ebx
        call    mem_up
        pop     eax
        mov     edx,[g_state.sg.gd]
        mov     [eax+D_PTR],edx
        jmp     @sgc_l
@sgc_e:
        mov     eax,[g_state.sg.gd]
        mov     [g_state.mm.sfr],eax
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
        mov     edx,[g_state.sg.spt]
        sub     edx,[g_state.mm.proglin]
        cmp     edx,PROGSZ
        jb      @sst_s
        mov     edx,[g_state.sg.spt]
        cmp     edx,[g_state.mm.sfr]
        jb      @sst_c
        cmp     edx,[g_state.mm.htop]
        jae     @sst_c
        mov     ebx,eax                 ; a variable's block stays its own
        sub     ebx,[g_state.sg.tmplin]
        cmp     ebx,DSZ*NTMP
        jae     @sst_c
        push    eax
        movzx   eax,[g_state.sg.sln]
        add     eax,edx
        mov     ebx,eax
        pop     eax
        cmp     eax,[ebx+B_OWN]
        je      @sst_o
@sst_c:
        mov     [g_state.sg.sac],eax
        mov     ax,[g_state.sg.sln]
        call    str_alloc
        mov     eax,[g_state.sg.sac]
        call    str_getd
        call    str_cpyd
        mov     eax,[g_state.sg.dpv]
        mov     [g_state.sg.spt],eax
@sst_o:
        mov     eax,[g_state.sg.bpo]
        call    str_own
@sst_s:
        mov     ebx,[g_state.sg.bpo]
        mov     eax,[g_state.sg.spt]
        mov     [ebx+D_PTR],eax
        mov     ax,[g_state.sg.sln]
        mov     [ebx+D_LEN],ax
        ret

; ::::::::::::::
; str_cpyd - copy sln chars from spt to dpv.
;
; in:     sln, spt, dpv  the count, the source, the destination
; keeps:  SI, DI
; ::::::::::::::
str_cpyd:
        mov     edx,[g_state.sg.dpv]

; ::::::::::::::
; str_cpy - copy sln chars from spt to EDX.
;
; in:     sln, spt  the count, the source
;         EDX       the destination, linear
; keeps:  SI, DI
; ::::::::::::::
str_cpy:
        mov     eax,[g_state.sg.spt]
        mov     cx,[g_state.sg.sln]
        jmp     mem_cpy

; ::::::::::::::
; str_near - the first 255 chars to nbuf, a 0 after them.
;
; in:     sln, spt  the string
; out:    ESI  nbuf
;         sln  255 at most
; keeps:  DI
; ::::::::::::::
str_near:
        cmp     [g_state.sg.sln],255
        jbe     @snr_c
        mov     [g_state.sg.sln],255
@snr_c:
        mov     edx,[g_state.mm.dslin]
        add     edx,offset g_nbuf
        call    str_cpy
        movzx   ebx,[g_state.sg.sln]
        mov     byte ptr [g_nbuf+ebx],0
        mov     esi,offset g_nbuf
        ret

; ::::::::::::::
; str_print - print sln chars from spt, 255 at a time through nbuf.
;
; in:     sln, spt  the count, the chars
; keeps:  SI, DI
; ::::::::::::::
str_print:
        push    esi
        mov     dx,[g_state.sg.sln]
@spr_c:
        test    dx,dx
        jz      @spr_e
        mov     [g_state.sg.sln],dx
        push    edx
        call    str_near
        movzx   ecx,[g_state.sg.sln]
        mov     eax,ecx
        add     [g_state.sg.spt],eax
@spr_l:
        lodsb
        call    io_putc
        loop    @spr_l
        pop     edx
        sub     dx,[g_state.sg.sln]
        jmp     @spr_c
@spr_e:
        pop     esi
        ret

; ::::::::::::::
; str_lit - a "literal": a temporary for its text.
;
; in:     SI  at the opening quote
; out:    EAX  the temporary
;         SI   past the closing quote, or at the 0
; ::::::::::::::
str_lit:
        inc     esi
        mov     ax,'""'
        call    str_scan

; ::::::::::::::
; str_skq - past the closing quote at SI, if there is one.
;
; in:     SI  at the quote or the 0
; keeps:  EAX
; ::::::::::::::
str_skq:
        cmp     byte ptr [esi],0
        je      @skq_r
        inc     esi
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
        mov     [g_state.sg.sln],ax
        add     ebx,[g_state.mm.dslin]
        mov     [g_state.sg.spt],ebx
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
        mov     ebx,esi
@ssp_l:
        mov     cl,[esi]
        test    cl,cl
        jz      @ssp_e
        cmp     cl,al
        je      @ssp_e
        cmp     cl,ah
        je      @ssp_e
        inc     esi
        jmp     @ssp_l
@ssp_e:
        mov     eax,esi
        sub     eax,ebx
        ret

; ::::::::::::::
; str_dlen - the length in the Desc at EAX.
;
; out:    CX  the length
; keeps:  all but BX, CX, FS
; ::::::::::::::
str_dlen:
        mov     cx,[eax+D_LEN]
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
        mov     [g_state.sg.sac],eax
        mov     [g_state.sg.sab],edx
        call    str_dlen
        mov     ax,cx
        xchg    eax,edx
        call    str_dlen
        add     cx,dx
        cmp     cx,SMAX
        ja      @sca_l
        mov     ax,cx
        call    str_alloc
        mov     eax,[g_state.sg.sab]
        call    str_dlen
        movzx   edx,cx
        add     edx,[g_state.sg.dpv]
        mov     eax,[g_state.sg.sac]
        call    str_get
        call    str_cpy
        mov     eax,[g_state.sg.sab]
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
        mov     [g_state.sg.sab],edx
        call    str_get
        mov     dx,cx
        mov     ebx,[g_state.sg.spt]
        mov     [g_state.sg.sac],ebx
        mov     eax,[g_state.sg.sab]
        call    str_getd
        push    esi
        push    edi
        mov     esi,[g_state.sg.spt]
        mov     edi,[g_state.sg.sac]
        mov     cx,[g_state.sg.sln]
@scm_l:
        test    cx,cx
        jz      @scm_e
        test    dx,dx
        jz      @scm_g
        cmpsb
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
        pop     edi
        pop     esi
        ret

; ::::::::::::::
; str_sub - a part of the string value: from s0v (0 the first), s1v at most.
;
; in:     EAX       the Desc
;         s0v, s1v  the start, the most chars
; out:    EAX  a new temporary
; ::::::::::::::
str_sub:
        mov     [g_state.sg.sac],eax
        call    str_dlen
        mov     ax,cx
        sub     ax,[g_state.sg.s0v]
        jnc     @ssb_a
        xor     ax,ax
@ssb_a:
        cmp     ax,[g_state.sg.s1v]
        jb      @ssb_b
        mov     ax,[g_state.sg.s1v]
@ssb_b:
        call    str_alloc
        mov     eax,[g_state.sg.sac]
        call    str_get
        movzx   eax,[g_state.sg.s0v]
        add     [g_state.sg.spt],eax

; ::::::::::::::
; str_tods - copy sn chars from spt to the block: a new temporary.
;
; in:     spt, dpv, sn  the source, the block's chars, the count
; out:    EAX  the temporary
; ::::::::::::::
str_tods:
        mov     ax,[g_state.sg.sn]
        mov     [g_state.sg.sln],ax
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
        mov     eax,[g_state.mm.dslin]
        add     eax,offset g_sbuf
        mov     [g_state.sg.spt],eax
        jmp     str_tods

        .data?
g_tmpd  db      DSZ*NTMP dup(?)
g_nbuf  db      257 dup(?)
        .code

        end
