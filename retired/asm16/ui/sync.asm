; ==========================================================================
; sync.asm - the text to the program store, line numbers both ways
; ==========================================================================
; holds:    sy_build, sy_find
; uses:     tx_get, rpl_enter, prg_init, dl_ok
; data:     A line's number is its label when it starts with one; a line
;           without one takes the number after the line before (0 for the
;           first). The labels must rise; a blank line is not stored. A
;           label alone is stored as REM, to be jumped to.
; calls:
;   sy_build           NEW, then each line through rpl_enter
;     sy_pass          the numbering, shared with sy_find
; ==========================================================================

; ::::::::::::::
; sy_build - the program from the text.
;
; out:    CF  set: a numbering error was shown, sy_i its line
; error:  as rpl_enter, through ui_err with sy_on set
; ::::::::::::::
sy_build:
        call    prg_init
        mov     [sy_md],0
        call    sy_pass
        jc      @syb_r
        and     [tx_mod],0
@syb_r:
        ret

; ::::::::::::::
; sy_find - the text line numbered AX.
;
; out:    AX  its index
;         CF  set: none
; ::::::::::::::
sy_find:
        mov     [sy_tg],ax
        mov     [sy_md],1
        call    sy_pass
        jc      @syf_n
        cmp     [sy_md],2
        jne     @syf_n
        mov     ax,[sy_i]
        clc
        ret
@syf_n:
        stc
        ret

; ::::::::::::::
; sy_pass - number each line; store it (sy_md 0) or stop at sy_tg (sy_md 1,
; then 2 when found).
;
; out:    CF  set: a numbering error, shown when storing
; ::::::::::::::
sy_pass:
        mov     [sy_prev],0FFFFh
        mov     [sy_i],0
        mov     [sy_on],1
@syp_l:
        mov     ax,[sy_i]
        cmp     ax,[tx_n]
        jae     @syp_d
        mov     di,offset sy_buf
        call    tx_get
        mov     si,offset sy_buf
        mov     bx,si
        add     bx,cx
        mov     byte ptr [bx],0
@syp_b:
        lodsb
        cmp     al,' '
        je      @syp_b
        test    al,al
        jz      @syp_n                  ; blank
        dec     si
        mov     [sy_hid],1
        cmp     al,'0'
        jb      @syp_h
        cmp     al,'9'
        ja      @syp_h
        mov     [sy_hid],0
        xor     eax,eax
@syp_g:
        movzx   edx,byte ptr [si]
        sub     dl,'0'
        cmp     dl,9
        ja      @syp_x
        inc     si
        imul    eax,10
        add     eax,edx
        cmp     eax,64000
        jb      @syp_g
        mov     ax,offset u_range
        jmp     @syp_e
@syp_x:
        mov     dx,ax
        cmp     [sy_prev],0FFFFh
        je      @syp_k
        cmp     dx,[sy_prev]
        ja      @syp_k
        mov     ax,offset u_order
        jmp     @syp_e
@syp_h:
        mov     dx,[sy_prev]
        inc     dx
        cmp     dx,64000
        jb      @syp_k
        mov     ax,offset u_range
        jmp     @syp_e
@syp_k:
        mov     [sy_prev],dx
        cmp     [sy_md],0
        je      @syp_s
        cmp     dx,[sy_tg]
        jne     @syp_n
        mov     [sy_md],2
        jmp     @syp_d
@syp_s:
        mov     di,offset inbuf
        mov     ax,dx
        push    si
        call    ui_dec
        pop     si
        mov     al,' '
        stosb
@syp_y:
        lodsb
        cmp     al,' '
        je      @syp_y
        dec     si
        test    al,al
        jnz     @syp_c
        mov     dword ptr [di],'MER'
        add     di,3
        jmp     @syp_t
@syp_c:
        lodsb
        test    al,al
        jz      @syp_t0
        stosb
        cmp     di,offset inbuf+250
        jb      @syp_c
        jmp     @syp_t
@syp_t0:
@syp_t:
        mov     ax,di
        sub     ax,offset inbuf
        call    rpl_enter
@syp_n:
        inc     [sy_i]
        jmp     @syp_l
@syp_d:
        and     [sy_on],0
        clc
        ret
@syp_e:
        and     [sy_on],0
        cmp     [sy_md],0
        jne     @syp_f
        mov     si,ax
        call    dl_ok
@syp_f:
        stc
        ret
