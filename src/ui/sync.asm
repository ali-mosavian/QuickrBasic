; ==========================================================================
; sync.asm - the text to the program store, line numbers both ways
; ==========================================================================
; holds:    sy_build, sy_find, sy_walk
; uses:     tx_get, rpl_enter, prg_init, dl_ok
; data:     A line's number is its label when it starts with one; a line
;           without one takes the number after the line before (0 for the
;           first). The labels must rise; a blank line is not stored. A
;           label alone is stored as REM, to be jumped to.
; calls:
;   sy_build           NEW, then each line through rpl_enter
;     sy_pass          the numbering, shared with sy_find, sy_walk
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; sy_build - the program from the text.
;
; out:    CF  set: a numbering error was shown, sy_i its line
; error:  as rpl_enter, through ui_err with sy_on set
; ::::::::::::::
sy_build:
        call    prg_init
        mov     [g_ui.sy.sy_md],0
        mov     [g_state.io.lmode],1    ; as a file loads: labels, its order
        call    sy_pass
        mov     [g_state.io.lmode],0
        jc      @syb_r
        call    prg_drop
        and     [g_ui.tx.tx_mod],0
@syb_r:
        ret

; ::::::::::::::
; sy_find - the text line numbered AX.
;
; out:    AX  its index
;         CF  set: none
; ::::::::::::::
sy_find:
        mov     [g_ui.sy.sy_tg],ax
        mov     [g_ui.sy.sy_md],1
        call    sy_pass
        jc      @syf_n
        cmp     [g_ui.sy.sy_md],2
        jne     @syf_n
        mov     ax,[g_ui.sy.sy_i]
        clc
        ret
@syf_n:
        stc
        ret

; ::::::::::::::
; sy_walk - each line's index and number to the routine at EDX.
;
; in:     EDX  called with AX the index, DX the number
; ::::::::::::::
sy_walk:
        mov     [g_ui.sy.sy_cb],edx
        mov     [g_ui.sy.sy_md],3
        jmp     sy_pass

; ::::::::::::::
; sy_pass - number each line; store it (sy_md 0), stop at sy_tg (sy_md 1,
; then 2 when found) or call sy_cb (sy_md 3).
;
; out:    CF  set: a numbering error, shown when storing
; ::::::::::::::
sy_pass:
        mov     [g_ui.sy.sy_prev],0FFFFh
        mov     [g_ui.sy.sy_i],0
        mov     [g_ui.sy.sy_on],1
@syp_l:
        mov     ax,[g_ui.sy.sy_i]
        cmp     ax,[g_ui.tx.tx_n]
        jae     @syp_d
        mov     edi,offset g_ui.bf.sy_buf
        call    tx_get
        mov     esi,offset g_ui.bf.sy_buf
        mov     byte ptr [esi+ecx],0
@syp_b:
        lodsb
        cmp     al,' '
        je      @syp_b
        test    al,al
        jz      @syp_n                  ; blank
        dec     esi
        mov     [g_ui.sy.sy_hid],1
        cmp     al,'0'
        jb      @syp_h
        cmp     al,'9'
        ja      @syp_h
        mov     [g_ui.sy.sy_hid],0
        xor     eax,eax
@syp_g:
        movzx   edx,byte ptr [esi]
        sub     dl,'0'
        cmp     dl,9
        ja      @syp_x
        inc     esi
        imul    eax,10
        add     eax,edx
        cmp     eax,64000
        jb      @syp_g
        mov     eax,offset u_range
        jmp     @syp_e
@syp_x:
        mov     dx,ax
        cmp     [g_ui.sy.sy_prev],0FFFFh
        je      @syp_k
        cmp     dx,[g_ui.sy.sy_prev]
        ja      @syp_k
        mov     eax,offset u_order
        jmp     @syp_e
@syp_h:
        mov     dx,[g_ui.sy.sy_prev]
        inc     dx
        cmp     dx,64000
        jb      @syp_k
        mov     eax,offset u_range
        jmp     @syp_e
@syp_k:
        mov     [g_ui.sy.sy_prev],dx
        cmp     [g_ui.sy.sy_md],0
        je      @syp_s
        cmp     [g_ui.sy.sy_md],3
        jne     @F
        mov     ax,[g_ui.sy.sy_i]
        call    [g_ui.sy.sy_cb]
        jmp     @syp_n
@@:
        cmp     dx,[g_ui.sy.sy_tg]
        jne     @syp_n
        mov     [g_ui.sy.sy_md],2
        jmp     @syp_d
@syp_s:
        mov     edi,offset g_inbuf
        mov     ax,dx
        push    esi
        call    ui_dec
        pop     esi
        mov     al,' '
        stosb
@syp_y:
        lodsb
        cmp     al,' '
        je      @syp_y
        dec     esi
        test    al,al
        jnz     @syp_c
        mov     dword ptr [edi],'MER'
        add     edi,3
        jmp     @syp_t
@syp_c:
        lodsb
        test    al,al
        jz      @syp_t0
        stosb
        cmp     edi,offset g_inbuf+250
        jb      @syp_c
        jmp     @syp_t
@syp_t0:
@syp_t:
        mov     eax,edi
        sub     eax,offset g_inbuf
        call    rpl_enter
@syp_n:
        inc     [g_ui.sy.sy_i]
        jmp     @syp_l
@syp_d:
        and     [g_ui.sy.sy_on],0
        clc
        ret
@syp_e:
        and     [g_ui.sy.sy_on],0
        cmp     [g_ui.sy.sy_md],0
        jne     @syp_f
        mov     esi,eax
        call    dl_ok
@syp_f:
        stc
        ret

        end
