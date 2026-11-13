; ==========================================================================
; list.asm - LIST, SAVE and VERIFY: a stored body back to text
; ==========================================================================
; holds:    ls_all, ls_range, ls_line, ls_body, ls_text
; uses:     mem_line, io_kw, io_num, io_putc
; data:     A line lists as QBasic's editor spaces it: keywords from their
;           tokens, a name from its M_VAR, a whole number from its M_INT;
;           strings, REM and DATA verbatim. A space between words, around an
;           operator but a sign, after , ; : and none inside ( ).
;           lq the last item: LS_NONE no space follows (the start, '(', a
;           sign), LS_WORD a name, number, string or ')', LS_KW a keyword,
;           LS_FN a function's, LS_SEP an operator or , ; :, LS_REL a
;           relation.
; calls:
;   ls_all             ls_range
;     ls_line          a line at a time
; ==========================================================================

include x86f.inc
        .code

LS_NONE = 0
LS_WORD = 1
LS_KW   = 2
LS_FN   = 3
LS_SEP  = 4
LS_REL  = 5

; ::::::::::::::
; ls_all - list every line.
; ::::::::::::::
ls_all:
        xor     ax,ax
        mov     [g_state.rn.lfrom],ax
        dec     ax
        mov     [g_state.rn.lto],ax
; ::::::::::::::
; ls_range - list lfrom to lto.
;
; in:     lfrom, lto  the numbers
; keeps:  SI
; ::::::::::::::
ls_range:
        mov     ebx,PROGBASE
@ls_l:
        cmp     word ptr gs:[ebx],0
        je      @ls_e
        mov     ax,word ptr gs:[ebx+L_NUM]
        cmp     ax,[g_state.rn.lfrom]
        jb      @ls_sk
        cmp     ax,[g_state.rn.lto]
        ja      @ls_e
        push    ebx
        call    ls_line
        pop     ebx
@ls_sk:
        movzx   ecx,word ptr gs:[ebx]
        add     ebx,ecx
        jmp     @ls_l
@ls_e:
        ret

; ::::::::::::::
; ls_line - print the line at BX.
;
; in:     BX  the line
; keeps:  SI
; ::::::::::::::
ls_line:
        push    esi
        push    ebx
        mov     ax,word ptr gs:[ebx+L_NUM]
        call    io_num
        call    io_space
        pop     ebx
        lea     esi,[ebx+HDR]
        add     esi,[g_state.mm.pglin]
        call    ls_body
        pop     esi
        jmp     io_nl

; ::::::::::::::
; ls_text - the body at ESI as LIST spells it, in sbuf, sbx long, a 0
; after; and where its byte EAX on is spelled.
;
; out:    ECX  that column
; keeps:  ESI
; ::::::::::::::
ls_text:
        push    esi
        add     eax,esi
        mov     [g_lsat],eax
        mov     [g_state.io.ocap],80h
        and     [g_state.io.sbx],0
        call    ls_body
        and     [g_state.io.ocap],0
        mov     ecx,[g_state.io.sbx]
        mov     byte ptr [g_sbuf+ecx],0
        cmp     [g_lsat],-1             ; not reached: the end
        jne     @F
        mov     ecx,[g_lscol]
@@:
        or      [g_lsat],-1
        pop     esi
        ret

; ::::::::::::::
; ls_body - print the token body at SI.
; ::::::::::::::
ls_body:
        mov     [g_state.io.lq],LS_NONE
@ls_c:
        cmp     esi,[g_lsat]            ; ls_text's byte: its column
        jb      @F
        or      [g_lsat],-1
        mov     eax,[g_state.io.sbx]
        mov     [g_lscol],eax
@@:
        lodsb
        test    al,al
        jz      @ls_end
        cmp     al,M_INT
        je      @ls_int
        cmp     al,M_VAR
        je      @ls_var
        cmp     al,'"'
        je      @ls_qt
        cmp     al,'('
        je      @ls_op
        cmp     al,')'
        je      @ls_pn
        cmp     al,','
        je      @ls_sp
        cmp     al,';'
        je      @ls_sp
        cmp     al,':'
        je      @ls_sp
        test    al,al
        js      @ls_tok
        cmp     al,'.'
        je      @ls_num
        cmp     al,'0'
        jb      @F
        cmp     al,'9'
        jbe     @ls_num
@@:
        mov     cl,LS_WORD              ; any other byte: as a word
        call    ls_lead
        call    io_putc
        mov     [g_state.io.lq],cl
        jmp     @ls_c
@ls_int:
        mov     cl,LS_WORD
        call    ls_lead
        lodsd
        call    io_u24
        mov     [g_state.io.lq],cl
        jmp     @ls_c
@ls_num:                                ; a number kept as text: one word,
        mov     cl,LS_WORD              ; its exponent's sign too
        call    ls_lead
@lsn_l:
        mov     dl,al
        call    io_putc
        mov     al,[esi]
        cmp     al,'.'
        je      @lsn_t
        cmp     al,'#'
        je      @lsn_t
        cmp     al,'!'
        je      @lsn_t
        cmp     al,'E'
        je      @lsn_t
        cmp     al,'D'
        je      @lsn_t
        cmp     al,'+'
        je      @lsn_s
        cmp     al,'-'
        je      @lsn_s
        sub     al,'0'
        cmp     al,9
        ja      @lsn_x
        mov     al,[esi]
        jmp     @lsn_t
@lsn_s:
        cmp     dl,'E'
        je      @lsn_t
        cmp     dl,'D'
        jne     @lsn_x
@lsn_t:
        inc     esi
        jmp     @lsn_l
@lsn_x:
        mov     [g_state.io.lq],cl
        jmp     @ls_c
@ls_var:
        mov     cl,LS_WORD
        call    ls_lead
        lodsb
        movzx   ecx,al
@@:
        lodsb
        call    io_putc
        loop    @B
        add     esi,2
        mov     [g_state.io.lq],LS_WORD
        jmp     @ls_c
@ls_qt:
        mov     cl,LS_WORD
        call    ls_lead
@@:
        call    io_putc                 ; a string: to its quote, as it is
        lodsb
        test    al,al
        jz      @ls_end
        cmp     al,'"'
        jne     @B
        call    io_putc
        mov     [g_state.io.lq],LS_WORD
        jmp     @ls_c
@ls_op:
        cmp     [g_state.io.lq],LS_KW   ; '(': after a name or a function, close
        jb      @F
        cmp     [g_state.io.lq],LS_FN
        je      @F
        call    ls_space
@@:
        call    io_putc
        mov     [g_state.io.lq],LS_NONE
        jmp     @ls_c
@ls_pn:
        call    io_putc
        mov     [g_state.io.lq],LS_WORD
        jmp     @ls_c
@ls_sp:
        call    io_putc                 ; , ; : and a space before what follows
        mov     [g_state.io.lq],LS_SEP
        jmp     @ls_c
@ls_tok:
        cmp     al,TK_X
        je      @ls_x
        cmp     al,TK_PLUS
        jb      @ls_kw
        cmp     al,TK_LT
        jbe     @ls_bin
        cmp     al,TK_MOD
        jb      @ls_kw
        cmp     al,TK_IMP
        jbe     @ls_bin
@ls_kw:
        mov     cl,LS_KW
        cmp     al,TK_SGN
        jb      @F
        cmp     al,TK_MID
        jbe     @ls_kf
@@:
        cmp     al,TK_TAB
        je      @ls_kf
        cmp     al,TK_SPC
        je      @ls_kf
        cmp     al,TK_LBOUND
        jb      @ls_kp
@ls_kf:
        mov     cl,LS_FN
@ls_kp:
        call    ls_lead
        cmp     al,TK_REM               ; ' as it was written
        jne     @F
        cmp     byte ptr [esi],REM_Q
        jne     @F
        inc     esi
        mov     al,"'"
        call    io_putc
        jmp     @ls_rem
@@:
        push    eax
        call    io_kw
        pop     eax
        mov     [g_state.io.lq],cl
        cmp     al,TK_REM
        je      @ls_rem
        cmp     al,TK_DATA
        jne     @ls_c
        xor     cl,cl                   ; DATA's text: to a ':' out of quotes
@ls_dl:
        lodsb
        test    al,al
        jz      @ls_end
        cmp     al,'"'
        jne     @F
        xor     cl,1
@@:
        test    cl,cl
        jnz     @F
        cmp     al,':'
        je      @ls_sp
@@:
        call    io_putc
        jmp     @ls_dl
@ls_rem:
        lodsb                           ; REM's text: the rest, as it is
        test    al,al
        jz      @ls_end
        call    io_putc
        jmp     @ls_rem
@ls_x:
        lodsb                           ; FFh n: a function when '(' follows
        mov     cl,LS_KW
        cmp     byte ptr [esi],'('
        jne     @F
        mov     cl,LS_FN
@@:
        call    ls_lead
        call    io_kw1
        mov     [g_state.io.lq],cl
        jmp     @ls_c
@ls_bin:
        mov     cl,LS_SEP               ; an operator: spaced, but a sign
        cmp     al,TK_MINUS
        je      @F
        cmp     al,TK_PLUS
        jne     @ls_b2
@@:
        mov     ah,[g_state.io.lq]
        cmp     ah,LS_WORD
        je      @ls_b2
        cmp     ah,LS_REL
        je      @F
        cmp     ah,LS_NONE
        je      @ls_un
@@:
        call    ls_space
@ls_un:
        call    io_kw
        mov     [g_state.io.lq],LS_NONE
        jmp     @ls_c
@ls_b2:
        cmp     al,TK_GT
        jb      @ls_b3
        cmp     al,TK_LT
        ja      @ls_b3
        mov     cl,LS_REL
        cmp     [g_state.io.lq],LS_REL  ; <= <> >=: one operator
        je      @ls_b4
@ls_b3:
        cmp     [g_state.io.lq],LS_NONE
        je      @ls_b4
        call    ls_space
@ls_b4:
        call    io_kw
        mov     [g_state.io.lq],cl
        jmp     @ls_c
@ls_end:
        ret

        .data
g_lsat  dd      -1                      ; ls_text's byte, -1 none
        .data?
g_lscol dd      ?
        .code

; ::::::::::::::
; ls_lead - the space before a word, as the last item wants.
;
; in:     lq  the last item's LS_*
; keeps:  all
; ::::::::::::::
ls_lead:
        cmp     [g_state.io.lq],LS_NONE
        je      @lsl_r

; ::::::::::::::
; ls_space - a space.
;
; keeps:  all
; ::::::::::::::
ls_space:
        push    eax
        call    io_space
        pop     eax
@lsl_r:
        ret

        .data?
        .code

        end
