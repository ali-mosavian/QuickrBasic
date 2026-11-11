; ==========================================================================
; lex.asm - the tokenizer: a typed line becomes the body of a stored line
; ==========================================================================
; holds:    lx_line, the scanner and the tokenizer's actions lt_*
; uses:     dfa.inc's tables (tools/x86dfa.py), sym_find
; data:     SI the text, DI the body. In the body:
;           M_VAR len name id  a name with its % or $, then its symbol id
;           M_INT lo hi        a whole number under 65536
;           Keywords crunch as stock: tools/lexer.py is the model.
;           Two automata. The scanner reads text bytes into a symbol: a
;           keyword's token, PRINT for ?, else the byte. The tokenizer's
;           states read a symbol each, as tk_mode says (scanned, raw, DATA
;           text); each entry is an action lt_* and the next state.
; calls:
;   lx_line            a symbol (lxrd by tk_mode), the entry, its action
;     lx_scan          the scanner: the lowest keyword that is a prefix
; ==========================================================================

include x86f.inc
DFA_DATA = 1
include comp/dfa.inc
        .code

; ::::::::::::::
; lx_line - tokenize the text at SI into the body at DI.
;
; in:     SI  inbuf, 0-terminated
;         DI  cbuf
; out:    DI  past the body's 0
; error:  ?OUT OF MEMORY  from sym_find
; ::::::::::::::
lx_line:
        xor     edx,edx
@lxl_s:
        mov     [lxst],dl
        movzx   eax,byte ptr [tk_mode+edx]
        call    [lxrd+eax*4]
        movzx   ebx,al
        movzx   ebx,byte ptr [tk_cls+ebx]
        movzx   edx,[lxst]
        imul    edx,edx,TK_NC
        add     ebx,edx
        movzx   edx,word ptr [tk_tab+ebx*2]
        test    dl,dl
        jns     @F
        mov     esi,[lxs]               ; the next state reads it again
@@:
        mov     [lxst],dh
        and     edx,7Fh
        call    [lxact+edx*4]
        movzx   edx,[lxst]
        cmp     dl,0FFh
        jne     @lxl_s
        ret

; ::::::::::::::
; lx_scan - the scanner: the symbol at SI.
;
; A letter it reads comes in upper case, folded in the text too: a state
; reads a byte only to compare it with a keyword's.
;
; out:    AL   the symbol
;         SI   past it; stays on the 0
;         lxs  where it began
; keeps:  all but EAX, SI
; ::::::::::::::
lx_scan:
        push    ebx
        push    ecx
        push    edx
@lsc_r:
        mov     [lxs],esi
        xor     edx,edx                 ; DL the state, DH the best token
@lsc_l:
        movzx   eax,byte ptr [esi]
        cmp     al,'a'
        jb      @F
        cmp     al,'z'
        ja      @F
        and     al,0DFh
        mov     [esi],al
@@:
        movzx   eax,byte ptr [sc_cls+eax]
        movzx   ebx,dl
        imul    ebx,ebx,SC_NC
        movzx   eax,byte ptr [sc_tab+ebx+eax]
        cmp     al,SC_NUL
        jae     @lsc_x
        inc     esi
        cmp     al,SC_LEAF
        jae     @lsc_h
        mov     dl,al
        mov     ah,[sc_acc+eax]
        test    ah,ah
        jz      @lsc_l
        mov     dh,ah                   ; a keyword ends here
        mov     ecx,esi
        jmp     @lsc_l
@lsc_h:
        mov     al,[sc_acc+eax]
        jmp     @lsc_o
@lsc_x:
        je      @lsc_0
        cmp     al,SC_SKIP
        jne     @F
        inc     esi
        jmp     @lsc_r
@@:
        cmp     al,SC_BYTE
        je      @lsc_b
        cmp     al,SC_PRINT
        jne     @lsc_k
        inc     esi
        mov     al,TK_PRINT
        jmp     @lsc_o
@lsc_k:
        test    dh,dh                   ; SC_BACK: the best keyword, else
        jz      @lsc_f                  ; the first byte
        mov     al,dh
        mov     esi,ecx
        jmp     @lsc_o
@lsc_f:
        mov     esi,[lxs]
@lsc_b:
        lodsb
        jmp     @lsc_o
@lsc_0:
        xor     al,al
@lsc_o:
        pop     edx
        pop     ecx
        pop     ebx
        ret

; ::::::::::::::
; lx_raw - the byte at SI, as it is.
; ::::::::::::::
lx_raw:
        mov     [lxs],esi
        mov     al,[esi]
        test    al,al
        jz      @F
        inc     esi
@@:
        ret

; ::::::::::::::
; lx_dat - DATA text: the byte at SI; bytes of 80h or more but pi skipped.
; ::::::::::::::
lx_dat:
        mov     [lxs],esi
        mov     al,[esi]
        test    al,al
        jz      @F
        inc     esi
        cmp     al,80h
        jb      @F
        cmp     al,TK_PI
        jne     lx_dat
@@:
        ret

; ::::::::::::::
; lt_put - the symbol to the body.
;
; in:     AL  the symbol, as for each lt_* action
; ::::::::::::::
lt_put:
; ::::::::::::::
; lt_end - the line's 0 to the body; the tokenizer stops.
; ::::::::::::::
lt_end:
        stosb
; ::::::::::::::
; lt_skip - nothing: a space before an array's '('.
; ::::::::::::::
lt_skip:
        ret

; ::::::::::::::
; lt_nbeg - a number's first byte, a digit or '.'.
; ::::::::::::::
lt_nbeg:
        mov     [lxn],edi
        mov     [lxf],1
        stosb
        cmp     al,'.'
        je      @F
        mov     [lxf],0
        and     eax,0Fh
        mov     [lxv],eax
@@:
        ret

; ::::::::::::::
; lt_ndig - a digit: a whole number's value, text from 65536.
; ::::::::::::::
lt_ndig:
        stosb
        cmp     [lxf],0
        jne     @F
        and     eax,0Fh
        imul    ecx,[lxv],10
        add     eax,ecx
        mov     [lxv],eax
        cmp     eax,65535
        jbe     @F
        mov     [lxf],1
@@:
        ret

; ::::::::::::::
; lt_nput - '.', E or E's sign: the number is text.
; ::::::::::::::
lt_nput:
        stosb
        mov     [lxf],1
        ret

; ::::::::::::::
; lt_nend - the number's end: a whole one is M_INT lo hi.
; ::::::::::::::
lt_nend:
        cmp     [lxf],0
        jne     @F
        mov     edi,[lxn]
        mov     al,M_INT
        stosb
        mov     ax,word ptr [lxv]
        stosw
@@:
        ret

; ::::::::::::::
; lt_vbeg - a name's first letter: M_VAR, its length's place, the letter.
; ::::::::::::::
lt_vbeg:
        mov     [nkind],0
        jmp     @lt_nb
; ::::::::::::::
; lt_fbeg - FN's name's first letter.
; ::::::::::::::
lt_fbeg:
        mov     [nkind],3
@lt_nb:
        mov     byte ptr [nkey],al
        mov     byte ptr [nkey+1],0
        mov     ah,al
        mov     al,M_VAR
        stosb
        stosb
        mov     [nlenp],edi
        mov     al,ah
        stosb
        ret

; ::::::::::::::
; lt_npct - the name's %: its end.
; ::::::::::::::
lt_npct:
        mov     [nkind],1
        jmp     @lt_ns
; ::::::::::::::
; lt_ndol - the name's $: its end.
; ::::::::::::::
lt_ndol:
        mov     [nkind],2
@lt_ns:
        call    lt_nchr
        jmp     lt_nlen

; ::::::::::::::
; lt_nchr - a letter or digit of the name; nkey keeps its second byte.
; ::::::::::::::
lt_nchr:
        cmp     byte ptr [nkey+1],0
        jne     @F
        mov     byte ptr [nkey+1],al
@@:
        stosb
        ret

; ::::::::::::::
; lt_nlen - the name's end at SI: its length; lt_nvar goes on from SI.
; ::::::::::::::
lt_nlen:
        mov     ebx,[nlenp]
        mov     eax,edi
        sub     eax,ebx
        mov     [ebx-1],al
        mov     [lxm],esi
        ret

; ::::::::::::::
; lt_fend - FN's name's end: its id.
; ::::::::::::::
lt_fend:
        call    lt_nlen
        xor     al,al
        jmp     @lt_f

; ::::::::::::::
; lt_narr - a '(' after the name: an array's id; the text again from the
; name's end.
; ::::::::::::::
lt_narr:
        mov     al,80h
        jmp     @lt_id
; ::::::::::::::
; lt_nvar - no '(' after the name: a variable's id; the text again from
; the name's end.
; ::::::::::::::
lt_nvar:
        xor     al,al
@lt_id:
        mov     esi,[lxm]
@lt_f:
        call    sym_find
        stosb
        ret

        .data
lxrd    dd      lx_scan,lx_raw,lx_dat   ; by tk_mode: RD_KW, RD_RAW, RD_DATA
lxact   dd      lt_put,lt_end,lt_nbeg,lt_ndig,lt_nput,lt_nend,lt_vbeg,lt_fbeg
        dd      lt_nchr,lt_npct,lt_ndol,lt_nlen,lt_fend,lt_nvar,lt_narr,lt_skip
        .code
.erre LT_SKIP eq 15, <lxact: the order of tools/x86dfa.py's TK_ACTS>

        end
