; ==========================================================================
; diag.asm - a compile error in words, for the person who wrote the line
; ==========================================================================
; holds:    dx_at, dx_what, dx_todo, dx_shape, dx_say, dx_raise
; uses:     er_text, cm_expn, cm_expect, lx_kw, io_putc, io_puts, io_nl; msg.inc
;           (tools/x86msg.py)
; data:     An error is its E_*, its DG_ (0: found by BC's name and the text
;           at the error) and its column in the line's text. dx_at reads the
;           place: the text there, the statement it is in, the name it is
;           about; dx_say prints what is wrong, then what to do, each a
;           sentence, then the statement's form, if it has words for it.
;           Nothing here knows how the error was found; ported from llrm's
;           front/explain.rs.
; ==========================================================================

include x86f.inc
DX_DATA = 1
include io/msg.inc

        .code

; ::::::::::::::
; dx_at - the place of an error.
;
; in:     ESI  the line's text, 0-ended
;         ECX  the error's column in it
;         AL   the error, E_*
;         AH   its DG_, 0: by its text
;         DX   the statement automaton's state it failed in, FFFFh none
; out:    EAX  the column of the text it is about
; ::::::::::::::
dx_at:
        pushad
        mov     [g_dxln],esi
        mov     word ptr [g_dxcd],ax
        mov     [g_dxst],dx
        xor     edx,edx                 ; the column, at most the end
@@:
        cmp     byte ptr [esi+edx],0
        je      @F
        inc     edx
        jmp     @B
@@:
        cmp     ecx,edx
        jbe     @xdga_s
        mov     ecx,edx
@xdga_s:
        cmp     byte ptr [esi+ecx],' '
        jne     @F
        inc     ecx
        jmp     @xdga_s
@@:
        mov     [g_dxp],ecx
        lea     ebx,[esi+ecx]           ; the text: a word, a string or a sign
        xor     edx,edx
        mov     al,[ebx]
        test    al,al
        jz      @xdga_t
        inc     edx
        cmp     al,'"'
        jne     @xdga_w
@xdga_q:
        mov     al,[ebx+edx]
        test    al,al
        jz      @xdga_t
        inc     edx
        cmp     al,'"'
        jne     @xdga_q
        jmp     @xdga_t
@xdga_w:
        call    dx_isw
        jnc     @xdga_t
@@:
        mov     al,[ebx+edx]
        call    dx_isw
        jnc     @xdga_t
        inc     edx
        jmp     @B
@xdga_t:
        test    edx,edx                 ; nothing here, past what it read: that
        jnz     @F                      ; item, but for a syntax error
        cmp     [g_dxcd],E_SN
        je      @F
        call    dx_back
@@:
        mov     [g_dxtl],edx
        call    dx_stmt
        call    dx_name
        call    dx_res
        popad
        mov     eax,[g_dxp]
        ret

;::::::::::::::
; dx_raise - an error found off the code, as if an ERR op raised it: never
; trapped.
;
; in:     AL   the error, E_*
;         AH   its DG_
;         ECX  its offset in the line's body
;         DX   the line
;         EBX  {other}'s text, EDI its length
; ::::::::::::::
dx_raise:
        mov     [g_dxr],O_ERR
        mov     word ptr [g_dxr+1],ax
        mov     word ptr [g_dxr+3],cx
        or      word ptr [g_dxr+5],-1
        mov     [g_state.rn.ln],dx
        mov     [g_state.rn.opip],offset g_dxr
        mov     [g_state.rn.inerr],1
        mov     ecx,edi
        cmp     ecx,31
        jbe     @F
        mov     ecx,31
@@:
        mov     esi,ebx
        mov     edi,offset g_dxoth
        rep     movsb
        mov     byte ptr [edi],0
        jmp     err_raise

; dx_back - the item before column ECX of the line at ESI: p at it, EDX
; its length; none: EDX 0.
dx_back:
        mov     ebx,ecx
@@:
        test    ebx,ebx
        jz      @xdb_n
        cmp     byte ptr [esi+ebx-1],' '
        jne     @F
        dec     ebx
        jmp     @B
@@:
        mov     edx,ebx
        dec     ebx
        mov     al,[esi+ebx]
        cmp     al,'"'
        jne     @xdb_w
@xdb_q:
        test    ebx,ebx
        jz      @xdb_s
        dec     ebx
        cmp     byte ptr [esi+ebx],'"'
        jne     @xdb_q
        jmp     @xdb_s
@xdb_w:
        call    dx_isw
        jnc     @xdb_s
@@:
        test    ebx,ebx
        jz      @xdb_s
        mov     al,[esi+ebx-1]
        call    dx_isw
        jnc     @xdb_s
        dec     ebx
        jmp     @B
@xdb_s:
        mov     [g_dxp],ebx
        sub     edx,ebx
        ret
@xdb_n:
        xor     edx,edx
        ret

; dx_stmt - stm the statement's first word, if a keyword; dxbeg
; set when the error is at that word.
dx_stmt:
        mov     esi,[g_dxln]
        mov     ecx,[g_dxp]
        xor     edx,edx                 ; the statement's start: past the
        xor     ebx,ebx                 ; last ':' out of quotes
        xor     edi,edi
@xdgs_l:
        cmp     edi,ecx
        jae     @xdgs_h
        mov     al,[esi+edi]
        inc     edi
        cmp     al,'"'
        jne     @F
        xor     bl,1
@@:
        cmp     al,':'
        jne     @xdgs_l
        test    bl,bl
        jnz     @xdgs_l
        mov     edx,edi
        jmp     @xdgs_l
@xdgs_h:                                 ; past blanks and a line number
        cmp     edx,ecx
        jae     @xdgs_w
        mov     al,[esi+edx]
        cmp     al,' '
        je      @F
        sub     al,'0'
        cmp     al,9
        ja      @xdgs_w
@@:
        inc     edx
        jmp     @xdgs_h
@xdgs_w:
        lea     ebx,[esi+edx]           ; its word before the error
        lea     edx,[esi+ecx]
        sub     edx,ebx                 ; at most up to the error
        call    dx_alpha
        mov     [g_dxbeg],0
        test    ecx,ecx
        jnz     @F
        mov     [g_dxbeg],1
@@:
        cmp     ecx,2
        jae     @xdgs_k
        cmp     [g_dxbeg],0             ; at the start: the word at the error
        je      @xdgs_n
        mov     ebx,esi
        add     ebx,[g_dxp]
        mov     edx,255
        call    dx_alpha
        cmp     ecx,2
        jb      @xdgs_n
@xdgs_k:
        cmp     ecx,15
        jbe     @F
        mov     ecx,15
@@:
        mov     edi,offset g_dxstm
        call    dx_upper
        mov     esi,offset g_dxstm
        call    dx_form
        jnc     @xdgs_r
        call    lx_kw                   ; a keyword, if no form for it
        test    eax,eax
        jz      @xdgs_n
        cmp     byte ptr [esi],0
        je      @xdgs_r
@xdgs_n:
        mov     [g_dxstm],0
@xdgs_r:
        ret

; dx_alpha - ECX the letters at EBX, at most EDX.
dx_alpha:
        xor     ecx,ecx
@@:
        cmp     ecx,edx
        jae     @xdal_r
        mov     al,[ebx+ecx]
        or      al,20h
        sub     al,'a'
        cmp     al,25
        ja      @xdal_r
        inc     ecx
        jmp     @B
@xdal_r:
        ret

; dx_upper - the ECX bytes at EBX to EDI in capitals, 0 after.
dx_upper:
        push    ecx
        push    ebx
@@:
        jecxz   @xdgu_e
        mov     al,[ebx]
        inc     ebx
        cmp     al,'a'
        jb      @xdgu_c
        cmp     al,'z'
        ja      @xdgu_c
        sub     al,20h
@xdgu_c:
        stosb
        dec     ecx
        jmp     @B
@xdgu_e:
        mov     byte ptr [edi],0
        pop     ebx
        pop     ecx
        ret

; dx_name - nm, nml: the first word from the error on that is no leading
; keyword (p in SUB p (a%)).
dx_name:
        mov     ebx,[g_dxln]
        add     ebx,[g_dxp]
        and     [g_dxnl],0
@xdgn_l:
        cmp     byte ptr [ebx],' '
        jne     @F
        inc     ebx
        jmp     @xdgn_l
@@:
        xor     ecx,ecx
@@:
        mov     al,[ebx+ecx]
        call    dx_isw
        jnc     @F
        inc     ecx
        jmp     @B
@@:
        jecxz   @xdgn_r
        mov     edi,offset g_dxkey
        call    dx_upper
        push    ebx
        mov     edi,offset g_dxlead
@xdgn_k:                                 ; a leading keyword: the next word
        cmp     byte ptr [edi],0
        je      @xdgn_f
        mov     esi,offset g_dxkey
        call    dx_streq
        je      @xdgn_s
@@:
        inc     edi
        cmp     byte ptr [edi-1],0
        jne     @B
        jmp     @xdgn_k
@xdgn_s:
        pop     ebx
        add     ebx,ecx
        jmp     @xdgn_l
@xdgn_f:
        pop     ebx
        mov     [g_dxnm],ebx
        mov     [g_dxnl],ecx
@xdgn_r:
        ret

; dx_isw - CF set when AL is a name's or a number's: a letter, a digit or
; . _ $ % & ! # @.
dx_isw:
        push    eax
        cmp     al,'0'
        jb      @xdgi_p
        cmp     al,'9'
        jbe     @xdgi_y
        or      al,20h
        cmp     al,'a'
        jb      @xdgi_p
        cmp     al,'z'
        jbe     @xdgi_y
@xdgi_p:
        pop     eax
        push    edi
        push    ecx
        mov     edi,offset g_dxwc
        mov     ecx,8
        repne   scasb
        pop     ecx
        pop     edi
        je      @xdgi_y2
        clc
        ret
@xdgi_y:
        pop     eax
@xdgi_y2:
        stc
        ret

; dx_streq - ZF set when the 0-ended text at ESI is the one at EDI.
dx_streq:
        push    esi
        push    edi
@@:
        mov     al,[esi]
        cmp     al,[edi]
        jne     @F
        inc     esi
        inc     edi
        test    al,al
        jnz     @B
@@:
        pop     edi
        pop     esi
        ret

; dx_skip - EDI past its 0-ended text.
dx_skip:
        push    eax
        push    ecx
        xor     al,al
        or      ecx,-1
        repne   scasb
        pop     ecx
        pop     eax
        ret

; dx_form - EBX the form of the statement word at ESI, CF set: none.
dx_form:
        push    edi
        mov     edi,offset g_dxfrm
@xdgf_l:
        cmp     byte ptr [edi],0
        je      @xdgf_n
        call    dx_streq
        pushf
        call    dx_skip
        popf
        je      @xdgf_y
        call    dx_skip
        jmp     @xdgf_l
@xdgf_y:
        mov     ebx,edi
        pop     edi
        clc
        ret
@xdgf_n:
        pop     edi
        stc
        ret

; dx_find - EDI the entry named as the text at ESI, CF set: none.
dx_find:
        mov     edi,offset g_dxmsg
@xdgd_l:
        cmp     byte ptr [edi],0
        je      @xdgd_n
        call    dx_streq
        je      @xdgd_y
        call    dx_skip
        call    dx_skip
        call    dx_skip
        jmp     @xdgd_l
@xdgd_y:
        clc
        ret
@xdgd_n:
        stc
        ret

; dx_res - the error's words: wt what is wrong, td what to do (0 none), nm2
; the name, for an error the catalogue has no words for.
dx_res:
        and     [g_dxfm],0
        movzx   eax,[g_dxid]
        test    eax,eax
        jz      @xdsy_b
        mov     edi,offset g_dxmsg      ; by its number
@@:
        dec     eax
        jz      @xdsy_f
        call    dx_skip
        call    dx_skip
        call    dx_skip
        jmp     @B
@xdsy_b:                                ; by BC's name: name /TEXT, name /end,
        mov     al,[g_dxcd]             ; name
        call    er_text
        mov     edi,offset g_dxkey
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        dec     edi
        mov     [g_dxke],edi
        mov     ax,'/ '
        stosw
        mov     ebx,[g_dxln]
        add     ebx,[g_dxp]
        mov     ecx,[g_dxtl]
        call    dx_upper
        mov     esi,offset g_dxkey
        call    dx_find
        jnc     @xdsy_f
        cmp     [g_dxtl],0
        jne     @F
        mov     edi,[g_dxke]
        mov     dword ptr [edi],'e/ '
        mov     dword ptr [edi+3],'dn'
        call    dx_find
        jnc     @xdsy_f
@@:
        mov     edi,[g_dxke]
        mov     byte ptr [edi],0
        call    dx_find
        jnc     @xdsy_f
        mov     [g_dxnm2],offset g_dxkey
        mov     [g_dxwt],offset g_dxat
        and     [g_dxtd],0
        cmp     [g_dxcd],E_SN
        jne     @F
        call    dx_syn
@@:
        ret
@xdsy_f:                                ; EDI the entry
        mov     [g_dxnm2],edi
        call    dx_skip
        mov     [g_dxwt],edi
        call    dx_skip
        mov     [g_dxtd],edi
        ret

; ::::::::::::::
; dx_what - print what is wrong, a sentence.
; ::::::::::::::
dx_what:
        pushad
        mov     ebx,[g_dxwt]
        call    dx_sent
        popad
        ret

; ::::::::::::::
; dx_todo - print what to do, a sentence, if the error has words for it.
;
; out:    ZF  set: none
; ::::::::::::::
dx_todo:
        pushad
        mov     ebx,[g_dxtd]
        test    ebx,ebx
        jz      @F
        cmp     byte ptr [ebx],0
        je      @F
        call    dx_sent
        or      ebx,ebx                 ; ZF clear
@@:
        popad
        ret

; ::::::::::::::
; dx_shape - print how the statement is written, if known.
;
; out:    ZF  set: not known
; ::::::::::::::
dx_shape:
        pushad
        mov     ebx,[g_dxfm]
        test    ebx,ebx
        jz      @F
        call    io_puts
        or      ebx,ebx                 ; ZF clear
@@:
        popad
        ret

; ::::::::::::::
; dx_say - the error dx_at placed, in words: what is wrong, what to do, the
; statement's form indented; a line each.
; ::::::::::::::
dx_say:
        call    dx_what
        call    io_nl
        call    dx_todo
        jz      @F
        call    io_nl
@@:
        cmp     [g_dxfm],0
        je      @F
        call    io_space
        call    io_space
        call    dx_shape
        call    io_nl
@@:
        ret

; dx_syn - a syntax error in words: what was found where, then the
; statement's form.
dx_syn:
        mov     [g_dxnm2],offset g_dxsyn
        cmp     [g_dxbeg],0             ; at the statement's first word, a
        je      @xdsn_s                 ; word: that one is the statement
        mov     ecx,[g_dxtl]
        jecxz   @xdsn_s
        mov     ebx,[g_dxln]
        add     ebx,[g_dxp]
        mov     edx,ecx
        call    dx_alpha
        cmp     ecx,[g_dxtl]
        jne     @xdsn_s
        cmp     ecx,15
        ja      @xdsn_s
        mov     edi,offset g_dxstm
        call    dx_upper
        mov     [g_dxwt],offset g_dxsfst
        jmp     @xdsn_w
@xdsn_s:
        movzx   edx,[g_dxst]            ; the automaton's state: what it wanted
        cmp     dx,-1
        je      @F
        call    cm_expn
        mov     [g_dxwt],offset g_dxsexp
        test    ecx,ecx
        jnz     @xdsn_w
@@:
        mov     [g_dxwt],offset g_dxsend
        cmp     [g_dxtl],0
        je      @xdsn_w
        mov     [g_dxwt],offset g_dxsfit
@xdsn_w:
        mov     [g_dxtd],offset g_dxchk
        mov     esi,offset g_dxstm
        call    dx_form
        jc      @xdsn_r
        mov     [g_dxfm],ebx
        mov     edi,offset g_dxkey      ; STATEMENT is written:
        mov     [g_dxtd],edi
        mov     esi,offset g_dxstm
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        dec     edi
        mov     esi,offset g_dxwrit
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
@xdsn_r:
        ret

; dx_sent - dx_fill as a sentence: a capital first, a period last unless it
; ends in one or a ':'.
dx_sent:
        mov     [g_dxcap],1
        call    dx_fill
        mov     [g_dxcap],0
        mov     al,[g_dxlast]
        cmp     al,'.'
        je      @F
        cmp     al,':'
        je      @F
        mov     al,'.'
        call    io_putc
@@:
        ret

; dx_fill - print the words at EBX, {text} {name} {statement} {stm}
; {other} filled.
dx_fill:
        push    esi
        push    edi
        mov     esi,ebx
@xdfl_l:
        lodsb
        test    al,al
        jz      @xdfl_r
        cmp     al,'{'
        je      @xdfl_k
@xdfl_c:
        cmp     [g_dxcap],0
        je      @F
        mov     [g_dxcap],0
        cmp     al,'a'
        jb      @F
        cmp     al,'z'
        ja      @F
        sub     al,20h
@@:
        mov     [g_dxlast],al
        call    io_putc
        jmp     @xdfl_l
@xdfl_k:
        mov     [g_dxcap],0
        mov     [g_dxlast],'x'
        mov     edi,offset g_dxph       ; the placeholders, 0 each
        xor     edx,edx
@xdfl_m:
        cmp     byte ptr [edi],0
        je      @xdfl_x
        push    esi
@@:
        mov     al,[edi]
        inc     edi
        test    al,al
        jz      @xdfl_y
        cmp     al,[esi]
        jne     @F
        inc     esi
        jmp     @B
@@:
        pop     esi
        call    dx_skip
        inc     edx
        jmp     @xdfl_m
@xdfl_y:
        add     esp,4                   ; ESI past it
        call    [g_dxphf+edx*4]
        jmp     @xdfl_l
@xdfl_x:
        mov     al,'{'
        jmp     @xdfl_c
@xdfl_r:
        pop     edi
        pop     esi
        ret

dx_ptext:
        mov     ebx,[g_dxln]
        add     ebx,[g_dxp]
        mov     ecx,[g_dxtl]
dx_pn:                                  ; ECX bytes at EBX
        jecxz   @xdpn_r
@@:
        mov     al,[ebx]
        call    io_putc
        inc     ebx
        loop    @B
@xdpn_r:
        ret
dx_pname:
        mov     ecx,[g_dxnl]
        jecxz   dx_ptext
        mov     ebx,[g_dxnm]
        jmp     dx_pn
dx_pstmt:
        mov     ebx,offset g_dxstm
        cmp     byte ptr [ebx],0
        jne     @F
        mov     ebx,offset g_dxthis
@@:
        jmp     io_puts
dx_pstm:                                ; STATEMENT statement, or statement
        cmp     [g_dxstm],0
        je      @F
        mov     ebx,offset g_dxstm
        call    io_puts
        mov     al,' '
        call    io_putc
@@:
        mov     ebx,offset g_dxstw
        jmp     io_puts
dx_pbc:
        mov     ebx,[g_dxnm2]
        jmp     io_puts
dx_pexp:
        movzx   edx,[g_dxst]
        jmp     cm_expect
dx_pfound:                              ; `text`, or the statement's end
        mov     ebx,offset g_dxeos
        cmp     [g_dxtl],0
        je      io_puts
        mov     al,'`'
        call    io_putc
        call    dx_ptext
        mov     al,'`'
        jmp     io_putc
dx_pother:
        mov     ebx,offset g_dxoth
        jmp     io_puts

        .data
g_dxwc  db      "._$%&!#@"
g_dxlead db     "SUB",0,"FUNCTION",0,"TYPE",0,"DECLARE",0,"CALL",0,"STATIC",0
        db      "SHARED",0,"DIM",0,"REDIM",0,"COMMON",0,"CONST",0,"DEF",0,"END",0
        db      "EXIT",0,"ERASE",0,"SELECT",0,"CASE",0,"GOTO",0,"GOSUB",0
        db      "RETURN",0,0
g_dxph  db      "text}",0,"name}",0,"statement}",0,"stm}",0,"other}",0,"bc}",0
        db      "expected}",0,"found}",0,0
g_dxphf dd      dx_ptext,dx_pname,dx_pstmt,dx_pstm,dx_pother,dx_pbc,dx_pexp,dx_pfound
g_dxsyn db      "Syntax error",0
g_dxsfst db     "`{text}` cannot be used here: it does not start a valid statement at "
        db      "this point",0
g_dxsfit db     "`{text}` does not fit the {stm} here",0
g_dxsend db     "the {stm} is incomplete: the line ends where more is needed",0
g_dxsexp db     "expected {expected} in the {stm}, but found {found}",0
g_dxeos db      "the end of the statement",0
g_dxwrit db     " is written:",0
g_dxchk db      "check the statement against the manual; the mark shows where it "
        db      "stopped being understood",0
g_dxat  db      "{bc} (at `{text}`)",0
g_dxthis db     "this",0
g_dxstw db      "statement",0
        .data?
g_dxln  dd      ?                       ; the line's text
g_dxst  dw      ?                       ; the automaton's state, FFFFh none
g_dxcd  db      ?                       ; the error, E_*
g_dxid  db      ?                       ; its DG_, 0 none
g_dxp   dd      ?                       ; the text's column
g_dxtl  dd      ?                       ; its length
g_dxbeg db      ?                       ; at the statement's first word
g_dxnm  dd      ?                       ; the name, its length
g_dxnl  dd      ?
g_dxwt  dd      ?                       ; what is wrong, its words
g_dxtd  dd      ?                       ; what to do, 0 none
g_dxnm2 dd      ?                       ; its name, when no words
g_dxke  dd      ?                       ; the key's end before " /"
g_dxstm db      16 dup(?)               ; the statement's word
g_dxkey db      160 dup(?)              ; a key, a word; a form's line
g_dxr   db      7 dup(?)                ; dx_raise's ERR op
g_dxoth db      32 dup(?)               ; {other}
g_dxfm  dd      ?                       ; the statement's form, 0 none
g_dxcap db      ?                       ; dx_fill: a capital next
g_dxlast db     ?                       ; dx_fill: the last it put
        .code

        end
