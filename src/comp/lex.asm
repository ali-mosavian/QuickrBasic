; ==========================================================================
; lex.asm - the tokenizer: a typed line becomes the body of a stored line
; ==========================================================================
; holds:    lx_line, lx_word, lx_kw, kw_step, lx_colof, g_lxcol
; uses:     dfa.inc's tables (tools/x86dfa.py), nam_find
; data:     SI the text, DI the body. In the body:
;           M_VAR len name nam:2  a name as typed, its suffix too; nam its
;                                 entry in the name table
;           M_INT n:4            a whole number under 2^31, &H, &O
;           a token              80h..FEh, or TK_X and a byte
;           Spaces go, but in strings, REM and DATA. A number with a point,
;           an exponent or 2^31 and over stays text. A keyword is a whole
;           word (the scanner's automaton), any other word a name.
;           lxcol: an item's column in the text, by its body offset.
; calls:
;   lx_line            each item of the line
;     lx_word          a word: its keyword, or a name for nam_find
; ==========================================================================

include x86f.inc
DFA_DATA = 1
include comp/dfa.inc
        .code

; lxroom - Out of memory: the body at DI past a line's record.
lxroom  macro
        cmp     edi,[g_lxend]
        jae     err_om
endm

; ::::::::::::::
; lx_line - tokenize the text at SI into the body at DI.
;
; in:     SI  inbuf, 0-terminated
;         DI  cbuf
; out:    DI  past the body's 0
; error:  Out of memory  from nam_find
; The body is at most 5 times the text: DI has room for it.
; ::::::::::::::
lx_line:
        mov     [g_state.lx.lxsrc],esi
        mov     [g_state.lx.lxdst],edi
        lea     eax,[edi+LMAX]
        mov     [g_lxend],eax
        or      [g_lxlst],-1            ; no item's column yet
lx_item:
        lodsb
        test    al,al
        jz      @lxl_e
        cmp     al,' '
        je      lx_item
        cmp     al,9
        je      lx_item
        call    lx_col
        cmp     al,'"'
        je      @lxl_q
        cmp     al,"'"
        je      @lxl_c
        cmp     al,'&'
        je      @lxl_h
        movzx   ebx,al
        cmp     al,'.'
        je      @lxl_d
        sub     bl,'0'
        cmp     bl,10
        jb      @lxl_n
        mov     bl,al
        upcase  ebx
        sub     bl,'A'
        cmp     bl,26
        jb      @lxl_w
        movzx   ebx,al
        mov     ah,[g_chr_tok+ebx]
        test    ah,ah
        jz      @F
        mov     al,ah
@@:
        stosb
        jmp     lx_item
@lxl_e:
        call    lx_col                  ; the end too has a column
        stosb
        ret
@lxl_q:
        stosb                           ; a string: to its quote, as it is
@lxq_l:
        lxroom
        lodsb
        test    al,al
        jz      @lxl_e
        stosb
        cmp     al,'"'
        jne     @lxq_l
        jmp     lx_item
@lxl_c:
        mov     al,TK_REM
        stosb
        mov     al,REM_Q
        stosb
@lxl_r:                                 ; REM's text: the rest, as it is
@lxr_l:
        lxroom
        lodsb
        stosb
        test    al,al
        jnz     @lxr_l
        ret
@lxl_d:
        mov     bl,[esi]
        sub     bl,'0'
        cmp     bl,10
        jb      @lxl_n
        stosb                           ; a '.' alone
        jmp     lx_item
@lxl_n:
        dec     esi
        call    lx_num
        jmp     lx_item
@lxl_h:
        call    lx_hex
        jmp     lx_item
@lxl_w:
        dec     esi
        call    lx_fn
        call    lx_word
        cmp     al,TK_REM
        je      @lxl_r
        cmp     al,TK_DATA
        jne     lx_item
        xor     cl,cl                   ; DATA's text: to a ':' out of quotes
@lxd_l:
        lxroom
        lodsb
        test    al,al
        jz      @lxl_e
        cmp     al,'"'
        jne     @F
        xor     cl,1
@@:
        test    cl,cl
        jnz     @lxd_p
        cmp     al,':'
        je      @lxd_x
@lxd_p:
        stosb
        jmp     @lxd_l
@lxd_x:
        stosb
        jmp     lx_item

; ::::::::::::::
; lx_col - the item at SI-1: its column to lxcol at the body's offset; the
; body within a line's record.
;
; keeps:  all
; ::::::::::::::
lx_col:
        push    eax
        push    ebx
        push    ecx
        mov     ebx,edi
        sub     ebx,[g_state.lx.lxdst]
        cmp     ebx,LMAX                ; past a line's record: the store's
        jae     err_om
        cmp     ebx,LXCMAX
        jae     @lxc_r
        cmp     [g_lxall],0             ; the bytes since the last item: its
        je      @lxc_s                  ; column, when lx_colof is to be asked
        mov     ecx,[g_lxlst]
        mov     ax,[g_lxlc]
@@:
        inc     ecx
        cmp     ecx,ebx
        jae     @lxc_s
        mov     [g_lxcol+ecx*2],ax
        jmp     @B
@lxc_s:
        lea     eax,[esi-1]
        sub     eax,[g_state.lx.lxsrc]
        mov     [g_lxcol+ebx*2],ax
        mov     [g_lxlc],ax
        mov     [g_lxlst],ebx
@lxc_r:
        pop     ecx
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; lx_colof - the column in lx_line's text of the body's byte EAX: its item's,
; else the one's before it. lx_line ran with lxall set.
;
; out:    ECX  the column
; keeps:  all but ECX
; ::::::::::::::
lx_colof:
        xor     ecx,ecx
        cmp     [g_lxlst],0
        jl      @F
        push    eax
        cmp     eax,[g_lxlst]
        jbe     @lxo_c
        mov     eax,[g_lxlst]
@lxo_c:
        movzx   ecx,word ptr [g_lxcol+eax*2]
        pop     eax
@@:
        ret

        .data?
g_lxcol dw      LXCMAX dup(?)
g_lxlst dd      ?                       ; the last item's offset, -1 none
g_lxend dd      ?                       ; the body's limit: a line's record
g_lxall db      ?                       ; 1: every byte's column, for lx_colof
g_lxlc  dw      ?                       ; its column
        .code

; ::::::::::::::
; lx_fn - DEF FN name, FN name: the word FN and the name after its spaces
; made one name, the spaces moved before it.
;
; in:     SI  at a word
; out:    SI  at the word, FN joined
; ::::::::::::::
lx_fn:
        mov     ax,[esi]
        and     ax,0DFDFh
        cmp     ax,'NF'
        jne     @lxf_r
        mov     al,[esi+2]
        cmp     al,' '
        je      @F
        cmp     al,9
        jne     @lxf_r
@@:
        lea     ebx,[esi+2]
@lxf_s:
        movzx   eax,byte ptr [ebx]
        cmp     al,' '
        je      @lxf_n
        cmp     al,9
        jne     @lxf_l
@lxf_n:
        inc     ebx
        jmp     @lxf_s
@lxf_l:
        upcase  eax
        sub     al,'A'
        cmp     al,26
        jae     @lxf_r
        mov     ax,[esi]
        mov     word ptr [esi],'  '     ; spaces first: one space overlaps
        mov     [ebx-2],ax
        lea     esi,[ebx-2]
@lxf_r:
        ret

; ::::::::::::::
; lx_num - a number: M_INT n:4 when whole and under 2^31, else its text.
;
; in:     SI  at its first digit or point
; out:    SI  past it
; ::::::::::::::
lx_num:
        mov     [g_state.lx.lxn],edi
        xor     edx,edx                 ; the value; DH bit: text
        xor     ecx,ecx
        mov     [g_state.lx.lxf],0
@lxn_d:
        lxroom
        movzx   eax,byte ptr [esi]
        sub     al,'0'
        cmp     al,10
        jae     @lxn_o
        movsb
        cmp     [g_state.lx.lxf],0
        jne     @lxn_d
        imul    ecx,ecx,10
        jc      @lxn_t
        add     ecx,eax
        jns     @lxn_d
@lxn_t:
        mov     [g_state.lx.lxf],1      ; 2^31 and over: text
        jmp     @lxn_d
@lxn_o:
        mov     al,[esi]
        cmp     al,'.'
        je      @lxn_p
        upcase  eax
        cmp     al,'E'
        je      @lxn_e
        cmp     al,'D'
        jne     @lxn_x
@lxn_e:
        mov     bl,[esi+1]              ; E, D: an exponent when a digit or sign
        cmp     bl,'+'                  ; follows
        je      @lxn_s
        cmp     bl,'-'
        je      @lxn_s
        sub     bl,'0'
        cmp     bl,10
        jae     @lxn_x
        mov     byte ptr [esi],al       ; E or D, upper case
        movsb
        mov     [g_state.lx.lxf],1
        jmp     @lxn_d
@lxn_s:
        mov     byte ptr [esi],al
        movsw
        mov     [g_state.lx.lxf],1
        jmp     @lxn_d
@lxn_p:
        movsb
        mov     [g_state.lx.lxf],1
        jmp     @lxn_d
@lxn_x:
        mov     al,[esi]                ; # a DOUBLE's, ! a SINGLE's
        cmp     al,'#'
        je      @F
        cmp     al,'!'
        jne     @lxn_y
@@:
        movsb
        mov     [g_state.lx.lxf],1
@lxn_y:
        cmp     [g_state.lx.lxf],0
        jne     @lxn_r
        mov     al,[esi]                ; % an INTEGER's, & a LONG's: one type
        cmp     al,'%'
        je      @F
        cmp     al,'&'
        jne     @lxn_i
@@:
        inc     esi
@lxn_i:
        mov     edi,[g_state.lx.lxn]
        mov     al,M_INT
        stosb
        mov     eax,ecx
        stosd
@lxn_r:
        ret

; ::::::::::::::
; lx_hex - &H or &O, its digits, a % or & after: M_INT n:4; '&' alone is
; the byte.
;
; in:     SI  after the '&'
; out:    SI  past it
; ::::::::::::::
lx_hex:
        call    num_amp
        jnc     @F
        mov     al,'&'
        stosb
        ret
@@:
        mov     al,[esi]                ; % an INTEGER's, & a LONG's: one type
        cmp     al,'%'
        je      @lxh_s
        cmp     al,'&'
        jne     @F
@lxh_s:
        inc     esi
@@:
        mov     al,M_INT
        stosb
        mov     eax,edx
        stosd
        ret

; ::::::::::::::
; lx_word - a word: letters, digits and points. Its keyword's token, or a
; name (lx_name).
;
; in:     SI  at its first letter
; out:    SI  past it
;         AL  the token written, else 0
; error:  ?OUT OF MEMORY  from nam_find
; ::::::::::::::
lx_word:
        mov     [g_state.lx.lxs],esi
        call    lx_kw
        test    eax,eax
        jz      lx_name
        cmp     eax,100h
        jb      @lxw_1
        mov     byte ptr [edi],TK_X
        inc     edi
@lxw_1:
        stosb
        ret

; ::::::::::::::
; lx_kw - the keyword a word spells: its letters, digits and points, a '$'
; when the keyword has one.
;
; in:     SI   at its first letter
; out:    SI   past it
;         EAX  the keyword's token (100h+ FFh's page), 0: a name
; keeps:  all but EAX, EBX, EDX, ESI
; ::::::::::::::
lx_kw:
        mov     edx,KW_START
@lxw_l:
        movzx   eax,byte ptr [esi]
        movzx   ebx,al
        upcase  ebx
        sub     bl,'A'
        cmp     bl,26
        jb      @lxw_c
        sub     al,'0'
        cmp     al,10
        jb      @lxw_c
        cmp     byte ptr [esi],'.'
        jne     @lxw_e
@lxw_c:
        movzx   eax,byte ptr [esi]
        upcase  eax
        movzx   eax,byte ptr [g_kw_cls+eax]
        call    kw_step
        inc     esi
        jmp     @lxw_l
@lxw_e:
        cmp     byte ptr [esi],'$'
        jne     @lxw_k
        movzx   eax,byte ptr [g_kw_cls+'$']
        push    edx
        call    kw_step
        mov     ebx,edx
        pop     edx
        cmp     word ptr [g_kw_acc+ebx*2],0
        je      @lxw_k
        mov     edx,ebx
        inc     esi
@lxw_k:
        movzx   eax,word ptr [g_kw_acc+edx*2]
        ret

; ::::::::::::::
; kw_step - the scanner's state EDX on character class AL: the state's
; transition of that class in its list (kw_off), else 0, the dead state.
;
; out:    EDX  the state
; keeps:  all but EDX
; ::::::::::::::
kw_step:
        push    ecx
        push    ebx
        movzx   ecx,word ptr [g_kw_off+edx*2]
        movzx   ebx,word ptr [g_kw_off+edx*2+2]
        xor     edx,edx
@kws_l:
        cmp     ecx,ebx
        jae     @kws_r
        cmp     al,[g_kw_tc+ecx]
        je      @F
        inc     ecx
        jmp     @kws_l
@@:
        movzx   edx,word ptr [g_kw_tn+ecx*2]
@kws_r:
        pop     ebx
        pop     ecx
        ret

; ::::::::::::::
; lx_name - the word from lxs to SI is a name: M_VAR len name nam:2, a suffix
; % & ! # $ in the text but not the name's key. What the name is, the
; compiler finds in the name table.
;
; in:     lxs  its start
;         SI   its end
; out:    AL   0
; error:  ?OUT OF MEMORY  from nam_find
; ::::::::::::::
lx_name:
        mov     al,M_VAR
        stosb
        stosb                           ; the length's place
        mov     [g_state.lx.nptr],edi
        mov     ecx,esi
        sub     ecx,[g_state.lx.lxs]
        mov     [g_state.lx.nlen],cl
        push    esi
        mov     esi,[g_state.lx.lxs]
        rep     movsb
        pop     esi
        mov     al,[esi]
        cmp     al,'%'
        je      @lxm_s
        cmp     al,'&'
        je      @lxm_s
        cmp     al,'!'
        je      @lxm_s
        cmp     al,'#'
        je      @lxm_s
        cmp     al,'$'
        jne     @lxm_l
@lxm_s:
        movsb
@lxm_l:
        mov     ebx,[g_state.lx.nptr]
        mov     eax,edi
        sub     eax,ebx
        mov     [ebx-1],al
        call    nam_find
        stosw
        xor     al,al
        ret

        end
