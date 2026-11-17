; ==========================================================================
; lex.asm - the tokenizer: a typed line becomes the body of a stored line
; ==========================================================================
; holds:    lx_line
; uses:     kwlist, inbuf, cbuf, sym_find
; data:     SI the text, DI the body. In the body:
;           M_VAR len name id  a name with its % or $, then its symbol id
;           M_INT lo hi        a whole number under 65536
;           Keywords crunch as stock: tools/lexer.py is the model.
; calls:
;   lx_line            a byte at a time; " and REM raw, DATA up to a colon
;     lx_get           a keyword's token, PRINT for ?, else the character
;     lx_num           . 0-9: the text, or M_INT lo hi
;     sym_find         A-Z: M_VAR len name, then the id
; ==========================================================================

; ::::::::::::::
; lx_num - a number: its text, or M_INT lo hi for a whole one under 65536.
;
; At the start of the line only digits are read: the line number.
;
; in:     AL  the first character, a digit or '.'
;         SI  past it
; ::::::::::::::
lx_num:
        mov     [lxn],di
        xor     ebx,ebx
        mov     [lxf],0

; ::::::::::::::
; lx_nc - lx_num's loop: the byte in AL.
;
; in:     AL  the byte
; ::::::::::::::
lx_nc:
        stosb
        xor     al,'0'
        cmp     al,10
        jae     @lxn_d
        cmp     [lxf],0
        jne     @lxn_n
        movzx   eax,al
        imul    ebx,ebx,10
        add     ebx,eax
        cmp     ebx,65535
        jbe     @lxn_n
        mov     [lxf],1
        jmp     @lxn_n
@lxn_d:
        mov     [lxf],1
        cmp     al,'E' xor '0'
        jne     @lxn_n
        call    lx_get
        cmp     al,TK_PLUS
        je      lx_nc
        cmp     al,TK_MINUS
        je      lx_nc
        jmp     @lxn_y
@lxn_n:
        call    lx_get
@lxn_y:
        cmp     [lxn],offset cbuf
        je      @lxn_g
        cmp     al,'E'
        je      lx_nc
        cmp     al,'.'
        jmp     @lxn_h
@lxn_g:
        cmp     al,'0'
@lxn_h:
        jb      @lxn_e
        cmp     al,'9'
        jbe     lx_nc
@lxn_e:
        mov     si,[lxs]
        cmp     [lxf],0
        jne     lx_line
        mov     di,[lxn]
        mov     al,M_INT
        stosb
        mov     ax,bx
        stosw

; ::::::::::::::
; lx_line - tokenize the text at SI into the body at DI.
;
; in:     SI  inbuf, 0-terminated
;         DI  cbuf
; out:    DI  past the body's 0
; error:  ?OUT OF MEMORY  from sym_find
; ::::::::::::::
lx_line:
        call    lx_get
        cmp     al,'"'
        je      @lxl_q
        cmp     [lxd],0
        jne     @lxl_d
        cmp     al,'.'
        jb      @lxl_p
        cmp     al,'9'
        jbe     lx_num
        cmp     al,TK_REM
        je      @lxl_r
        cmp     al,TK_DATA
        jne     @lxl_n
        mov     [lxd],al
@lxl_n:
        cmp     al,'A'
        jb      @lxl_p
        cmp     al,'Z'
        jbe     @lxl_nm
@lxl_p:
        stosb
        test    al,al
        jnz     lx_line
        ret
@lxl_d:
        cmp     al,':'
        jne     @lxl_p
        mov     [lxd],0
        jmp     @lxl_p
@lxl_q:
        mov     [lxe],al
        jmp     @lxl_w
@lxl_r:
        mov     [lxe],0
@lxl_w:
        stosb
        mov     al,[si]
        test    al,al
        jz      lx_line
        inc     si
        cmp     al,[lxe]
        jne     @lxl_w
        jmp     @lxl_p

; A name: letters and digits, then % or $; an array when '(' follows.
@lxl_nm:
        mov     byte ptr [nkey],al
        mov     byte ptr [nkey+1],0
        mov     [nkind],0
        mov     bx,di
@lxl_b:
        dec     bx
        cmp     byte ptr [bx],' '
        je      @lxl_b
        cmp     byte ptr [bx],TK_FN
        jne     @lxl_0
        mov     [nkind],3
@lxl_0:
        mov     ah,M_VAR
        xchg    al,ah
        stosb
        stosb                           ; the length's place
        mov     [nlenp],di
        mov     al,ah
        stosb
@lxl_c:
        call    lx_get
        cmp     al,'0'
        jb      @lxl_x
        cmp     al,'9'
        jbe     @lxl_l
        cmp     al,'A'
        jb      @lxl_x
        cmp     al,'Z'
        ja      @lxl_x
@lxl_l:
        call    @lxl_k
        jmp     @lxl_c
@lxl_x:
        mov     dl,1
        cmp     al,'%'
        je      @lxl_s
        inc     dl
        cmp     al,'$'
        jne     @lxl_ns
@lxl_s:
        mov     [nkind],dl
        call    @lxl_k
        jmp     @lxl_st
@lxl_ns:
        mov     si,[lxs]
@lxl_st:
        push    si
        mov     ax,di
        mov     bx,[nlenp]
        sub     ax,bx
        mov     [bx-1],al
        cmp     [nkind],3
        je      @lxl_v
@lxl_sp:
        call    lx_get
        cmp     al,' '
        je      @lxl_sp
        cmp     al,'('
        jne     @lxl_v
        mov     al,80h
        jmp     @lxl_f
@lxl_v:
        xor     al,al
@lxl_f:
        call    sym_find
        pop     si
        stosb
        jmp     lx_line
@lxl_k:
        cmp     byte ptr [nkey+1],0
        jne     @lxl_kp
        mov     byte ptr [nkey+1],al
@lxl_kp:
        stosb
        ret

; ::::::::::::::
; lx_get - the next text byte: a keyword's token, PRINT for ?, or the char.
;
; Bytes of 80h or more but pi are skipped. A letter comes in upper case,
; folded in the text too. In DATA text no keyword is searched.
;
; in:     SI  the text
; out:    AL   the byte
;         SI   past it; stays on the 0
;         lxs  where the byte began
; keeps:  BX, CX, DX, DI
; ::::::::::::::
lx_get:
        mov     [lxs],si
        mov     al,[si]
        test    al,al
        jz      @lxg_r
        jns     @lxg_lo
        cmp     al,TK_PI
        je      @lxg_o
        inc     si
        jmp     lx_get
@lxg_lo:
        cmp     [lxd],0
        jne     @lxg_o
        cmp     al,' '
        je      @lxg_o
        cmp     al,'0'
        jb      @lxg_f
        cmp     al,'<'
        jb      @lxg_o
        cmp     al,'?'
        jne     @lxg_f
        mov     al,TK_PRINT
@lxg_o:
        inc     si
@lxg_r:
        ret
@lxg_f:
        push    bx
        mov     ah,80h
        mov     bx,offset kwlist
@lxg_c:
        mov     al,[si]
        cmp     al,'a'
        jb      @lxg_u
        cmp     al,'z'
        ja      @lxg_u
        and     al,0DFh
        mov     [si],al
@lxg_u:
        xor     al,[bx]
        jz      @lxg_m
        cmp     al,80h
        jne     @lxg_nt
        mov     al,ah
        inc     si
        pop     bx
        ret
@lxg_m:
        inc     si
        inc     bx
        jmp     @lxg_c
@lxg_nt:
        mov     si,[lxs]
        inc     ah
@lxg_sk:
        test    byte ptr [bx],80h
        lea     bx,[bx+1]
        jz      @lxg_sk
        cmp     byte ptr [bx],0
        jne     @lxg_c
        lodsb
        pop     bx
        ret
