; ==========================================================================
; syntax.asm - syntax highlighting: a line's keywords, strings, numbers,
; comments and names in the theme's colours
; ==========================================================================
; holds:    hl_row, hl_toggle, g_hlon
; uses:     lx_line, g_lxcol, sc_attr
; data:     The line is tokenized as the compiler does it; each item of the
;           body, at its column (lxcol), gives its slot (A_KW ..) to the
;           columns from there to the next item's. hlc: a slot a column.
; ==========================================================================

include x86f.inc

        .code

; ::::::::::::::
; hl_toggle - Options, Syntax Highlighting: on, off.
; ::::::::::::::
hl_toggle:
        xor     [g_hlon],1
        ret

; ::::::::::::::
; hl_row - the edit window's row at DH, DL: its line, ECX chars at ESI,
; coloured from column ed_left, ed_tw of them.
;
; keeps:  all
; ::::::::::::::
hl_row:
        cmp     [g_hlon],0
        je      @hxr_r
        pushad
        push    edx
        call    hl_line
        pop     edx
        movzx   ebx,[g_ui.ed.ed_left]   ; EBX the column, ECX those left
        movzx   ecx,[g_ui.ed.ed_tw]
@hxr_l:
        cmp     ebx,[g_hlen]
        jae     @hxr_x
        mov     ah,[g_hlc+ebx]
        cmp     ah,A_ED
        je      @F
        push    ecx
        mov     cx,1
        call    sc_attr
        pop     ecx
@@:
        inc     dl
        inc     ebx
        loop    @hxr_l
@hxr_x:
        popad
@hxr_r:
        ret

; ::::::::::::::
; hl_line - hlc for the ECX chars at ESI.
; ::::::::::::::
hl_line:
        cmp     ecx,LNMAX
        jbe     @F
        mov     ecx,LNMAX
@@:
        mov     [g_hlen],ecx
        mov     edi,offset g_hlsrc      ; a copy: the lexer changes its text
        push    ecx
        rep     movsb
        mov     byte ptr [edi],0
        pop     ecx
        mov     edi,offset g_hlc
        mov     al,A_ED
        rep     stosb
        mov     esi,offset g_hlsrc
        mov     edi,offset g_hlbody
        call    lx_line
        mov     esi,offset g_hlbody
@hxl_l:
        mov     ebx,esi                 ; EBX the item's column
        sub     ebx,offset g_hlbody
        cmp     ebx,255
        ja      @hxl_r
        movzx   ebx,byte ptr [g_lxcol+ebx]
        lodsb
        test    al,al
        jz      @hxl_r
        cmp     al,M_VAR
        je      @hxl_v
        cmp     al,M_INT
        je      @hxl_i
        cmp     al,'"'
        je      @hxl_s
        cmp     al,TK_REM
        je      @hxl_c
        cmp     al,TK_DATA
        je      @hxl_d
        cmp     al,TK_X
        je      @hxl_x
        cmp     al,80h
        jae     @hxl_k
        cmp     al,'.'
        je      @hxl_n
        sub     al,'0'
        cmp     al,10
        jb      @hxl_n
        mov     ah,A_ED                 ; a ',' ':' '(' and the like
        jmp     @hxl_p
@hxl_v:
        movzx   eax,byte ptr [esi]      ; len name nam:2
        lea     esi,[esi+eax+3]
        mov     ah,A_NAM
        jmp     @hxl_p
@hxl_i:
        add     esi,4
        mov     ah,A_NUM
        jmp     @hxl_p
@hxl_n:                                 ; a number kept as its text
        mov     al,[esi]
        cmp     al,'0'
        jb      @F
        cmp     al,'9'
        jbe     @hxl_m
@@:
        cmp     al,'.'
        je      @hxl_m
        cmp     al,'E'
        je      @hxl_m
        cmp     al,'D'
        je      @hxl_m
        cmp     al,'+'
        je      @hxl_m
        cmp     al,'-'
        je      @hxl_m
        cmp     al,'#'
        je      @hxl_m
        cmp     al,'!'
        jne     @F
@hxl_m:
        inc     esi
        jmp     @hxl_n
@@:
        mov     ah,A_NUM
        jmp     @hxl_p
@hxl_s:
        lodsb
        test    al,al
        jz      @hxl_e
        cmp     al,'"'
        jne     @hxl_s
        mov     ah,A_STR
        jmp     @hxl_p
@hxl_e:
        dec     esi
        mov     ah,A_STR
        jmp     @hxl_p
@hxl_c:
        mov     ah,A_COM                ; REM, ': the rest
        jmp     @hxl_t
@hxl_d:
        mov     ah,A_KW                 ; DATA's word; its items plain, to a
        call    @hxl_f                  ; ':' out of quotes
        push    ebx
        add     ebx,4
        mov     ah,A_ED
        call    @hxl_f
        pop     ebx
        xor     cl,cl
@hxd_l:
        lodsb
        test    al,al
        jz      @hxl_r
        cmp     al,'"'
        jne     @F
        xor     cl,1
@@:
        test    cl,cl
        jnz     @hxd_l
        cmp     al,':'
        jne     @hxd_l
        jmp     @hxl_l
@hxl_x:
        inc     esi
@hxl_k:
        mov     ah,A_KW
        cmp     al,TK_PLUS              ; + - * / ^ > = <: plain
        jb      @hxl_p
        cmp     al,TK_POW
        jbe     @F
        cmp     al,TK_GT
        jb      @hxl_p
        cmp     al,TK_LT
        ja      @hxl_p
@@:
        mov     ah,A_ED
@hxl_p:
        call    @hxl_f
        jmp     @hxl_l
@hxl_t:
        call    @hxl_f
@hxl_r:
        ret
@hxl_f:                                 ; AH from column EBX to the end
        mov     ecx,[g_hlen]
        sub     ecx,ebx
        jbe     @F
        lea     edi,[g_hlc+ebx]
        mov     al,ah
        rep     stosb
@@:
        ret

        .data
g_hlon  db      1                       ; Syntax Highlighting
        .data?
g_hlen  dd      ?
g_hlc   db      LNMAX+1 dup(?)
g_hlsrc db      LNBUF dup(?)
g_hlbody db     LNBUF*5 dup(?)
        .code

        end
