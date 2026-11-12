; ==========================================================================
; compl.asm - Tab completes the word at the cursor, from what fits there
; ==========================================================================
; holds:    ac_tab
; uses:     lx_line, dc_tail, dc_step, kwlist, tx_get, le_insn, sc_*, dr_all,
;           ui_getkey
; data:     The line before the word tells what fits (ac_m): a statement
;           or a variable at a statement's start; a line label after GOTO,
;           GOSUB, THEN, RUN, LIST, ON's list; a variable after FOR, NEXT,
;           DIM, READ, INPUT, GET; after an operand the words that go on
;           (AND, OR, THEN, TO, STEP); else a function or a variable, of
;           the type the expression wants (ac_ty). Nothing in a string, REM
;           or DATA. NEXT offers the open FOR's variable first, GOSUB the
;           lines after a RETURN, END, STOP or GOTO. An entry of ac_list
;           (AC_SZ bytes): the length, the text and 0 (10 at most); the
;           kind (AK_*); the rank, 0 first; a token or a line.
; calls:
;   ac_tab             ac_word, ac_ctx, ac_keys, ac_vars, ac_ins, ac_pop
; ==========================================================================

include x86f.inc
if AC

        .code


; ::::::::::::::
; ac_tab - Tab in the program window: the word completed, or a list of
; what fits.
;
; out:    CF  set: nothing fits, Tab indents
; ::::::::::::::
ac_tab:
        call    ac_word
        jc      @act_r
        call    ac_ctx
        jc      @act_r
        and     [g_ui.ac.ac_n],0
        mov     [g_ui.ac.ac_on],0
        cmp     [g_ui.ac.ac_m],AM_VR             ; NEXT: the open FOR's variable
        jne     @act_e
        cmp     [g_ui.ac.ac_st],TK_NEXT
        jne     @act_e
        call    ac_open
        cmp     [g_ui.ac.ac_pn],0
        jne     @act_c
        movzx   ecx,[g_ui.ac.ac_on]  ; no word: that alone
        mov     esi,offset g_ui.ac.ac_on+1
        mov     ax,AK_VR
        call    ac_add
        jmp     @act_g
@act_e:
        cmp     [g_ui.ac.ac_pn],0                ; no word: labels, or what has the
        jne     @act_c                  ; type wanted
        test    [g_ui.ac.ac_m],AM_LB
        jnz     @act_c
        cmp     [g_ui.ac.ac_ty],AT_ANY
        je      @act_n
        test    [g_ui.ac.ac_m],AM_FN+AM_VR
        jz      @act_n
@act_c:
        call    ac_keys
        call    ac_vars
@act_g:
        movzx   ecx,word ptr [g_ui.ac.ac_n]
        jcxz    @act_n
        call    ac_common
        cmp     [g_ui.ac.ac_n],1
        je      @act_1
        jmp     ac_pop
@act_1:
        xor     ax,ax
        call    ac_ins
        clc
        ret
@act_n:
        stc
@act_r:
        ret

; ::::::::::::::
; ac_word - the word before the cursor: ac_ps its column, ac_pn its length,
; ac_pfx it in capitals, ac_lc set when typed in small letters.
;
; out:    CF  set: the cursor is inside a word or past the line
; ::::::::::::::
ac_word:
        movzx   ebx,word ptr [g_ui.bf.ed_lb.col]
        cmp     bx,[g_ui.bf.ed_lb.len]
        ja      @acw_n
        je      @acw_b
        mov     al,byte ptr [g_ui.bf.ed_lb.txt+ebx]
        call    ac_alnum
        jnc     @acw_n
@acw_b:
        movzx   esi,bx
@acw_l:
        test    esi,esi
        jz      @acw_s
        mov     al,byte ptr [g_ui.bf.ed_lb.txt-1+esi]
        call    ac_alnum
        jc      @acw_s
        dec     esi
        jmp     @acw_l
@acw_s:
        mov     [g_ui.ac.ac_ps],si
        movzx   ecx,bx
        sub     cx,si
        mov     [g_ui.ac.ac_pn],cx
        cmp     cx,AC_SZ-2
        jae     @acw_n
        mov     [g_ui.ac.ac_lc],0
        mov     edi,offset g_ui.ac.ac_pfx
        jcxz    @acw_e
@acw_c:
        mov     al,byte ptr [g_ui.bf.ed_lb.txt+esi]
        mov     [g_ui.ac.ac_lc],0
        cmp     al,'a'
        jb      @acw_u
        mov     [g_ui.ac.ac_lc],1
@acw_u:
        call    ui_upper
        stosb
        inc     esi
        loop    @acw_c
@acw_e:
        mov     byte ptr [edi],0
        clc
        ret
@acw_n:
        stc
        ret

; ::::::::::::::
; ac_alnum - is AL a letter or a digit?
;
; out:    CF  clear: it is
; keeps:  all
; ::::::::::::::
ac_alnum:
        cmp     al,'0'
        jb      @aan_n
        cmp     al,'9'
        jbe     @aan_y
; ::::::::::::::
; ac_alpha - is AL a letter?
;
; out:    CF  clear: it is
; keeps:  all
; ::::::::::::::
ac_alpha:
        push    eax
        call    ui_upper
        cmp     al,'A'
        jb      @aal_n
        cmp     al,'Z'
        ja      @aal_n
        pop     eax
@aan_y:
        clc
        ret
@aal_n:
        pop     eax
@aan_n:
        stc
        ret

; ::::::::::::::
; ac_ctx - what fits at the word: the line before it tokenized, its last
; statement read item by item; the type each operand gives (ac_lt) and the
; one the expression wants (ac_ty).
;
; out:    CF  set: nothing (a string, REM, DATA)
;         ac_m  what fits
; ::::::::::::::
ac_ctx:
        mov     esi,offset g_ui.bf.ed_lb.txt
        mov     edi,offset g_ui.bf.sy_buf
        movzx   ecx,word ptr [g_ui.ac.ac_ps]
        rep     movsb
        mov     byte ptr [edi],0
        mov     esi,offset g_ui.bf.sy_buf
        call    ac_lex
        call    dc_tail
        jc      @acx_no
        mov     [g_ui.ac.ac_st],0
        mov     [g_ui.ac.ac_pv],AP_ST
        mov     [g_ui.ac.ac_ty],AT_ANY
        mov     [g_ui.ac.ac_fn],0
@acx_l:
        mov     al,[esi]
        test    al,al
        jz      @acx_e
        js      @acx_k
        cmp     al,M_VAR
        je      @acx_v
        cmp     al,'"'
        je      @acx_q
        cmp     al,M_INT
        je      @acx_d
        cmp     al,'.'
        je      @acx_d
        cmp     al,'0'
        jb      @acx_o
        cmp     al,'9'
        jbe     @acx_d
@acx_o:
        inc     esi
        mov     ah,AP_SEP
        mov     dl,[g_ui.ac.ac_fn]
        cmp     al,'('                  ; a function's argument: its type
        jne     @acx_c
        test    dl,dl
        jz      @acx_p
        mov     [g_ui.ac.ac_ty],AT_STR
        cmp     dl,TK_LEN
        je      @acx_p
        cmp     dl,TK_LEN+2             ; VAL, ASC
        je      @acx_p
        cmp     dl,TK_LEN+3
        je      @acx_p
        cmp     dl,TK_LEN+5             ; LEFT$, RIGHT$, MID$
        jae     @acx_p
        mov     [g_ui.ac.ac_ty],AT_NUM
        jmp     @acx_p
@acx_c:
        cmp     al,')'
        jne     @acx_m
        mov     ah,AP_ID
        test    dl,dl
        jz      @acx_p
        mov     al,[g_ui.ac.ac_flt]              ; the call's value
        mov     [g_ui.ac.ac_lt],al
        mov     [g_ui.ac.ac_fn],0
        jmp     @acx_p
@acx_m:
        cmp     al,','
        je      @acx_cm
        cmp     al,';'
        jne     @acx_p
@acx_cm:
        mov     ah,AP_COM
        mov     [g_ui.ac.ac_ty],AT_ANY
        cmp     dl,TK_LEN+5             ; LEFT$( s$, n
        jb      @acx_p
        mov     [g_ui.ac.ac_ty],AT_NUM
@acx_p:
        mov     [g_ui.ac.ac_pv],ah
        jmp     @acx_l
@acx_k:
        push    eax                     ; a keyword
        call    dc_item
        pop     eax
        cmp     [g_ui.ac.ac_pv],TK_THEN          ; a statement's start: its keyword
        je      @acx_s
        cmp     [g_ui.ac.ac_pv],AP_ST
        jne     @acx_t
@acx_s:
        mov     [g_ui.ac.ac_st],al
@acx_t:
        mov     [g_ui.ac.ac_pv],al
        call    ac_tkty
        cmp     al,TK_PLUS              ; + - * / ^ AND OR > = <
        jb      @acx_l
        cmp     al,TK_PLUS+9
        ja      @acx_l
        mov     [g_ui.ac.ac_pv],AP_SEP
        jmp     @acx_l
@acx_v:
        call    ac_vnm
        call    ac_nty
        mov     [g_ui.ac.ac_lt],al
        cmp     [g_ui.ac.ac_pv],AP_ST
        jne     @acx_ar
        mov     [g_ui.ac.ac_st],AP_ID            ; an assignment
@acx_ar:
        cmp     byte ptr [esi],'('      ; an array's subscripts: numbers
        jne     @acx_op
        inc     esi
        mov     [g_ui.ac.ac_flt],al
        mov     [g_ui.ac.ac_fn],0FFh
        mov     [g_ui.ac.ac_ty],AT_NUM
        mov     [g_ui.ac.ac_pv],AP_SEP
        jmp     @acx_l
@acx_d:
        call    dc_step
        mov     [g_ui.ac.ac_lt],AT_NUM
@acx_op:
        mov     [g_ui.ac.ac_pv],AP_ID
        jmp     @acx_l
@acx_q:
        call    dc_item
        mov     [g_ui.ac.ac_lt],AT_STR
        jmp     @acx_op
@acx_no:
        stc
        ret
@acx_e:
        call    ac_mask
        mov     al,[g_ui.ac.ac_pfx]              ; a number is a label
        call    ac_digit
        jc      @acx_x
        and     [g_ui.ac.ac_m],AM_LB
@acx_x:
        cmp     [g_ui.ac.ac_m],0
        je      @acx_no
        clc
        ret

; ::::::::::::::
; ac_tkty - the types keyword AL gives: a function's value, what an
; operator wants.
;
; keeps:  AL
; ::::::::::::::
ac_tkty:
        mov     ah,AT_NUM
        cmp     al,TK_FN
        je      @atk_f
        cmp     al,TK_PLUS+10           ; SGN to MID$
        jb      @atk_o
        cmp     al,TK_MID
        ja      @atk_o
        cmp     al,TK_LEN+1             ; STR$
        je      @atk_s
        cmp     al,TK_LEN+4             ; CHR$, LEFT$, RIGHT$, MID$
        jb      @atk_f
@atk_s:
        mov     ah,AT_STR
@atk_f:
        mov     [g_ui.ac.ac_fn],al
        mov     [g_ui.ac.ac_flt],ah
        mov     [g_ui.ac.ac_lt],ah
        ret
@atk_o:
        mov     ah,[g_ui.ac.ac_lt]               ; + = < >: the other operand's type
        cmp     al,TK_PLUS
        je      @atk_t
        cmp     al,TK_PLUS+7
        jae     @atk_m
        mov     ah,AT_NUM               ; - * / ^ AND OR NOT TO STEP FOR NEXT
        cmp     al,TK_PLUS
        ja      @atk_t
        cmp     al,TK_NOT
        je      @atk_t
        cmp     al,TK_TO
        je      @atk_t
        cmp     al,TK_STEP
        je      @atk_t
        cmp     al,TK_FOR
        je      @atk_t
        cmp     al,TK_NEXT
        je      @atk_t
        mov     ah,AT_ANY
        jmp     @atk_t
@atk_m:
        cmp     al,TK_PLUS+9
        ja      @atk_x
@atk_t:
        mov     [g_ui.ac.ac_ty],ah
@atk_x:
        ret

; ::::::::::::::
; ac_vnm - the name at SI's M_VAR to ac_nm: its length, then it in
; capitals, AC_LEN at most; a '(' after it added.
;
; out:    SI  past it, at the '('
; keeps:  all but AX, SI
; ::::::::::::::
ac_vnm:
        push    ecx
        push    edi
        movzx   ecx,byte ptr [esi+1]
        lea     eax,[esi+ecx+4]
        push    eax
        add     esi,2
        cmp     ecx,AC_LEN
        jbe     @avn_c
        mov     ecx,AC_LEN
@avn_c:
        mov     edi,offset g_ui.ac.ac_nm+1
        jecxz   @avn_p
@avn_l:
        lodsb
        call    ui_upper
        stosb
        loop    @avn_l
@avn_p:
        pop     esi
        cmp     byte ptr [esi],'('
        jne     @avn_z
        cmp     edi,offset g_ui.ac.ac_nm+1+AC_LEN
        jae     @avn_z
        mov     al,'('
        stosb
@avn_z:
        mov     eax,edi
        sub     eax,offset g_ui.ac.ac_nm+1
        mov     [g_ui.ac.ac_nm],al
        mov     byte ptr [edi],0
        pop     edi
        pop     ecx
        ret

; ::::::::::::::
; ac_line - text line AX tokenized, the cursor's from the editor: its text
; in sy_buf, its body in cbuf.
;
; out:    SI  the body
; keeps:  all but SI
; ::::::::::::::
ac_line:
        pushad
        mov     edi,offset g_ui.bf.sy_buf
        cmp     ax,[g_ui.ed.ed_line]
        jne     @alx_t
        mov     esi,offset g_ui.bf.ed_lb.txt
        movzx   ecx,word ptr [g_ui.bf.ed_lb.len]
        rep     movsb
        jmp     @alx_z
@alx_t:
        call    tx_get
        movzx   ecx,cx
        add     edi,ecx
@alx_z:
        mov     byte ptr [edi],0
        popad
        mov     esi,offset g_ui.bf.sy_buf

; ::::::::::::::
; ac_lex - the 0-ended text at SI tokenized into cbuf.
;
; out:    SI  the body
; keeps:  all but SI
; ::::::::::::::
ac_lex:
        pushad
        mov     edi,offset g_cbuf
        call    lx_line
        popad
        mov     esi,offset g_cbuf
        ret
; ::::::::::::::
; ac_nty - the type of the name in ac_nm: a string when it ends in $.
;
; out:    AL  AT_STR or AT_NUM
; keeps:  all but AX
; ::::::::::::::
ac_nty:
        push    ebx
        movzx   ebx,[g_ui.ac.ac_nm]
        cmp     byte ptr [g_ui.ac.ac_nm+ebx],'('
        jne     @anty_d
        dec     bx
@anty_d:
        mov     al,AT_STR
        cmp     byte ptr [g_ui.ac.ac_nm+ebx],'$'
        je      @anty_r
        mov     al,AT_NUM
@anty_r:
        pop     ebx
        ret

; ::::::::::::::
; ac_digit - is AL a digit or a point?
;
; out:    CF  clear: it is
; keeps:  all
; ::::::::::::::
ac_digit:
        cmp     al,'.'
        je      @adg_y
        cmp     al,'0'
        jb      @adg_n
        cmp     al,'9'
        ja      @adg_n
@adg_y:
        clc
        ret
@adg_n:
        stc
        ret

; ::::::::::::::
; ac_mask - ac_m from the statement (ac_st) and what came last (ac_pv).
; ::::::::::::::
ac_mask:
        mov     al,[g_ui.ac.ac_pv]
        mov     ah,[g_ui.ac.ac_st]
        mov     bl,AM_ST+AM_VR
        cmp     al,AP_ST
        je      @acm_s
        mov     bl,AM_ST+AM_VR+AM_LB
        cmp     al,TK_THEN
        je      @acm_s
        mov     bl,AM_LB
        cmp     al,TK_GOTO
        je      @acm_s
        cmp     al,TK_GOSUB
        je      @acm_s
        cmp     al,TK_RUN
        je      @acm_s
        cmp     al,TK_LIST
        je      @acm_s
        cmp     ah,TK_ON
        jne     @acm_v
        cmp     al,AP_COM
        je      @acm_s
@acm_v:
        mov     bl,AM_VR                ; the statements that name variables
        mov     esi,offset ac_vst
@acm_l:
        mov     dl,[esi]
        test    dl,dl
        jz      @acm_e
        inc     esi
        cmp     ah,dl
        jne     @acm_l
        cmp     al,dl
        je      @acm_s
        cmp     al,AP_COM
        je      @acm_s
        cmp     dl,TK_FOR               ; FOR's list is one
        jne     @acm_l
@acm_e:
        mov     bl,AM_OP
        cmp     al,AP_ID
        je      @acm_s
        mov     bl,AM_FN+AM_VR
@acm_s:
        mov     [g_ui.ac.ac_m],bl
        ret

        .data
ac_vst  db      TK_FOR,TK_NEXT,TK_DIM,TK_READ,TK_INPUT,TK_INPUTN,TK_GET,0
        .code

; ::::::::::::::
; ac_want - is token AL wanted by ac_m in statement ac_st?
;
; out:    CF  clear: it is
; ::::::::::::::
ac_want:
        test    [g_ui.ac.ac_m],AM_ST
        jz      @acn_f
        cmp     al,TK_NEW
        jbe     @acn_y
@acn_f:
        test    [g_ui.ac.ac_m],AM_FN
        jz      @acn_o
        cmp     al,TK_TAB
        je      @acn_y
        cmp     al,TK_FN
        je      @acn_y
        cmp     al,TK_SPC
        je      @acn_y
        cmp     al,TK_NOT
        je      @acn_y
        cmp     al,TK_PLUS+10
        jb      @acn_o
        cmp     al,TK_MID
        jbe     @acn_y
@acn_o:
        test    [g_ui.ac.ac_m],AM_OP
        jz      @acn_n
        cmp     al,TK_PLUS+5            ; AND, OR
        je      @acn_y
        cmp     al,TK_PLUS+6
        je      @acn_y
        mov     dl,al
        mov     esi,offset ac_ops
@acn_l:
        mov     al,[esi]
        test    al,al
        jz      @acn_n
        add     esi,3
        cmp     al,[g_ui.ac.ac_st]
        jne     @acn_l
        cmp     dl,[esi-2]
        je      @acn_y
        cmp     dl,[esi-1]
        je      @acn_y
        jmp     @acn_l
@acn_y:
        clc
        ret
@acn_n:
        stc
        ret

; the words that go on after an operand, by statement
        .data
ac_ops  db      TK_IF,TK_THEN,TK_GOTO
        db      TK_FOR,TK_TO,TK_STEP
        db      TK_ON,TK_GOTO,TK_GOSUB
        db      0
        .code

; ::::::::::::::
; ac_keys - the wanted keywords that start with the word, to ac_list: a
; function only of the type wanted.
; ::::::::::::::
ac_keys:
        mov     ebx,offset g_kwlist
        mov     dl,80h
@ack_e:
        cmp     byte ptr [ebx],0
        je      @ack_r
        mov     edi,offset g_ui.ac.ac_tmp
@ack_c:
        mov     al,[ebx]
        inc     bx
        mov     ah,al
        and     al,7Fh
        stosb
        test    ah,80h
        jz      @ack_c
        mov     byte ptr [edi],0
        push    ebx
        push    edx
        mov     al,dl
        call    ac_want
        jc      @ack_n
        mov     al,[g_ui.ac.ac_tmp]
        call    ac_alpha
        jc      @ack_n
        mov     cl,AK_KW
        mov     al,dl
        cmp     [g_ui.ac.ac_ty],AT_STR           ; FN and NOT give numbers
        jne     @ack_k
        cmp     al,TK_FN
        je      @ack_n
        cmp     al,TK_NOT
        je      @ack_n
@ack_k:
        cmp     al,TK_TAB               ; TAB( SPC( in PRINT only
        je      @ack_p
        cmp     al,TK_SPC
        jne     @ack_q
@ack_p:
        cmp     [g_ui.ac.ac_st],TK_PRINT
        je      @ack_q
        cmp     [g_ui.ac.ac_st],TK_PRINTN
        jne     @ack_n
@ack_q:
        cmp     al,TK_TAB
        je      @ack_f
        cmp     al,TK_SPC
        je      @ack_f
        cmp     al,TK_PLUS+10
        jb      @ack_a
        call    ac_tkty                 ; a function: its value's type
        mov     al,[g_ui.ac.ac_ty]
        test    al,al
        jz      @ack_f
        cmp     al,ah
        jne     @ack_n
@ack_f:
        mov     cl,AK_FN
@ack_a:
        mov     al,cl                   ; a function after the variables
        xor     ah,ah
        cmp     dl,TK_NEW
        jbe     @ack_d
        inc     ah
@ack_d:
        movzx   dx,dl
        mov     esi,offset g_ui.ac.ac_tmp
        mov     ecx,edi
        sub     ecx,esi
        call    ac_add
@ack_n:
        pop     edx
        pop     ebx
        inc     dl
        jmp     @ack_e
@ack_r:
        ret

; ::::::::::::::
; ac_vars - the program's variables or labels that start with the word.
; GOSUB ranks first a label after a RETURN, END, STOP or GOTO: a start.
; ::::::::::::::
ac_vars:
        test    [g_ui.ac.ac_m],AM_VR+AM_LB
        jz      @acv_r
        mov     [g_ui.ac.ac_plk],0
        xor     ax,ax
@acv_l:
        cmp     ax,[g_ui.tx.tx_n]
        jae     @acv_r
        push    eax
        call    ac_line
        pop     edx
        push    edx
        test    [g_ui.ac.ac_m],AM_LB
        jz      @acv_v
        cmp     dx,[g_ui.ed.ed_line]             ; not the word's own line
        je      @acv_v
        call    ac_label
@acv_v:
        test    [g_ui.ac.ac_m],AM_VR
        jz      @acv_n
        call    ac_ids
@acv_n:
        mov     esi,offset g_cbuf
        call    dc_tail                 ; its last statement's keyword
        mov     al,[esi]
        test    al,al
        js      @acv_k
        xor     al,al
@acv_k:
        mov     [g_ui.ac.ac_plk],al
        pop     eax
        inc     ax
        jmp     @acv_l
@acv_r:
        ret

; ::::::::::::::
; ac_label - the label that starts line DX, the body at SI, to ac_list.
;
; keeps:  all
; ::::::::::::::
ac_label:
        cmp     byte ptr [esi],M_INT
        jne     @acl_n
        cmp     dword ptr [esi+1],0FFFFh
        ja      @acl_n
        pushad
        mov     eax,[esi+1]
        mov     edi,offset g_ui.ac.ac_nm
        call    ui_dec
        mov     esi,offset g_ui.ac.ac_nm
        mov     ecx,edi
        sub     ecx,esi
        mov     edx,[esp+20]            ; pushad's DX
        mov     ax,AK_LB+100h           ; GOSUB: a start first
        cmp     [g_ui.ac.ac_pv],TK_GOSUB
        jne     @acl_a
        mov     bl,[g_ui.ac.ac_plk]
        cmp     bl,TK_RET
        je      @acl_f
        cmp     bl,TK_END
        je      @acl_f
        cmp     bl,TK_STOP
        je      @acl_f
        cmp     bl,TK_GOTO
        jne     @acl_a
@acl_f:
        xor     ah,ah
@acl_a:
        call    ac_add
        popad
@acl_n:
        ret

; ::::::::::::::
; ac_ids - the names in the body at SI, to ac_list: not after REM or DATA;
; of the type wanted; no array after FOR or NEXT.
; ::::::::::::::
ac_ids:
        mov     al,[esi]
        test    al,al
        jz      @aci_r
        cmp     al,TK_REM
        je      @aci_r
        cmp     al,TK_DATA
        je      @aci_r
        cmp     al,M_VAR
        je      @aci_i
        call    dc_step
        jmp     ac_ids
@aci_i:
        call    ac_vnm
        call    ac_nty
        mov     ah,[g_ui.ac.ac_ty]
        test    ah,ah
        jz      @aci_t
        cmp     al,ah
        jne     ac_ids
@aci_t:
        movzx   ebx,[g_ui.ac.ac_nm]
        cmp     byte ptr [g_ui.ac.ac_nm+ebx],'('
        jne     @aci_a
        cmp     [g_ui.ac.ac_m],AM_VR
        jne     @aci_a
        cmp     [g_ui.ac.ac_st],TK_FOR
        je      ac_ids
        cmp     [g_ui.ac.ac_st],TK_NEXT
        je      ac_ids
@aci_a:
        push    esi
        mov     esi,offset g_ui.ac.ac_nm+1
        movzx   ecx,[g_ui.ac.ac_nm]
        mov     ax,AK_VR
        call    ac_add
        pop     esi
        jmp     ac_ids
@aci_r:
        ret

; ::::::::::::::
; ac_open - the variable of the innermost FOR that no NEXT closes before
; the word, to ac_on: its length, its text.
; ::::::::::::::
ac_open:
        mov     [g_ui.ac.ac_dp],0
        mov     esi,offset g_cbuf       ; the line to the word, as ac_ctx left it
        mov     [g_ui.ac.ac_cur],1
        mov     ax,[g_ui.ed.ed_line]
@aop_l:
        push    eax
        call    ac_evs
        pop     eax
        jnc     @aop_r
        mov     [g_ui.ac.ac_cur],0
        dec     ax
        js      @aop_r
        call    ac_line
        jmp     @aop_l
@aop_r:
        ret

; ::::::::::::::
; ac_evs - the body at SI's FORs and NEXTs, taken last first against the
; NEXTs after them (ac_dp).
;
; out:    CF  clear: the open FOR found, in ac_on
; ::::::::::::::
ac_evs:
        mov     edi,offset g_ui.bf.ac_ev  ; events: FOR its name; NEXT 0, a count
        xor     ebx,ebx
@aev_l:
        mov     al,[esi]
        test    al,al
        jz      @aev_b
        cmp     al,TK_REM
        je      @aev_b
        cmp     al,TK_DATA
        je      @aev_b
        push    eax
        call    dc_step
        pop     eax
        cmp     al,TK_FOR
        je      @aev_f
        cmp     al,TK_NEXT
        jne     @aev_l
        xor     ecx,ecx                 ; NEXT: its names
@aev_x:
        mov     al,[esi]
        cmp     al,','
        je      @aev_m
        cmp     al,M_VAR
        jne     @aev_c
        call    dc_item
        inc     cx
        jmp     @aev_x
@aev_m:
        inc     esi
        jmp     @aev_x
@aev_c:
        mov     byte ptr [edi],0
        mov     [edi+1],cl
        jmp     @aev_s
@aev_f:
        cmp     byte ptr [esi],M_VAR
        jne     @aev_l
        call    ac_vnm
        push    esi
        mov     esi,offset g_ui.ac.ac_nm
        mov     ecx,AC_SZ
        rep     movsb
        sub     edi,AC_SZ
        pop     esi
@aev_s:
        cmp     bx,AC_EVN
        jae     @aev_l
        add     edi,AC_SZ
        inc     bx
        jmp     @aev_l
@aev_b:
        test    bx,bx                   ; last first
        jz      @aev_no
        sub     edi,AC_SZ
        dec     bx
        cmp     byte ptr [edi],0
        jne     @aev_for
        movzx   ax,byte ptr [edi+1] ; NEXT: a bare one closes one, but
        test    ax,ax                   ; not the one being written
        jnz     @aev_a
        cmp     [g_ui.ac.ac_cur],0
        jne     @aev_a
        inc     ax
@aev_a:
        add     [g_ui.ac.ac_dp],ax
        mov     [g_ui.ac.ac_cur],0
        jmp     @aev_b
@aev_for:
        cmp     [g_ui.ac.ac_dp],0
        je      @aev_y
        dec     [g_ui.ac.ac_dp]
        jmp     @aev_b
@aev_y:
        mov     esi,edi
        mov     edi,offset g_ui.ac.ac_on
        mov     ecx,AC_SZ
        rep     movsb
        clc
        ret
@aev_no:
        stc
        ret
; ::::::::::::::
; ac_add - CX characters at SI to ac_list, in capitals, once, in order of
; rank then text; when they start with the word and are longer. A variable
; ranks first when it is the open FOR's (ac_on).
;
; in:     AL  the kind (AK_*)
;         AH  the rank
;         DX  the token or the line
; keeps:  all
; ::::::::::::::
ac_add:
        pushad
        movzx   ecx,cx
        mov     word ptr [g_ui.ac.ac_kd],ax  ; with ac_rk
        mov     [g_ui.ac.ac_xt],dx
        cmp     cx,[g_ui.ac.ac_pn]
        jbe     @aad_r
        cmp     cx,AC_LEN
        ja      @aad_r
        mov     edi,offset g_ui.ac.ac_tmp+AC_SZ  ; the name in capitals, 0-ended
        push    edi
        push    ecx
@aad_u:
        lodsb
        call    ui_upper
        stosb
        loop    @aad_u
        mov     byte ptr [edi],0
        pop     ecx
        pop     esi
        mov     edi,offset g_ui.ac.ac_pfx
        push    ecx
        movzx   ecx,word ptr [g_ui.ac.ac_pn]
        push    esi
        cmp     ax,ax                   ; no word: every name starts so
        repe    cmpsb
        pop     esi
        pop     ecx
        jne     @aad_r
        cmp     [g_ui.ac.ac_kd],AK_VR            ; the open FOR's variable first
        jne     @aad_o
        cmp     [g_ui.ac.ac_on],0
        je      @aad_o
        mov     [g_ui.ac.ac_rk],1
        push    esi
        push    ecx
        mov     edi,offset g_ui.ac.ac_on+1
        inc     cx
        repe    cmpsb
        pop     ecx
        pop     esi
        jne     @aad_o
        mov     [g_ui.ac.ac_rk],0
@aad_o:
        mov     ebx,offset g_ui.bf.ac_list  ; the place: before the first above it
        xor     dx,dx
@aad_p:
        cmp     dx,[g_ui.ac.ac_n]
        jae     @aad_i
        mov     al,[g_ui.ac.ac_rk]
        cmp     al,[ebx+TACE.rk]
        jb      @aad_i
        ja      @aad_x
        push    esi
        lea     edi,[ebx+1]
@aad_c:
        lodsb
        mov     ah,[edi]
        inc     edi
        cmp     al,ah
        jne     @aad_d
        test    al,al
        jnz     @aad_c
        pop     esi                      ; there already
        jmp     @aad_r
@aad_d:
        pop     esi
        jb      @aad_i
@aad_x:
        add     ebx,AC_SZ
        inc     dx
        jmp     @aad_p
@aad_i:
        cmp     [g_ui.ac.ac_n],AC_MAX
        jae     @aad_r
        push    esi                      ; the rest up one
        push    ecx
        mov     ax,[g_ui.ac.ac_n]
        sub     ax,dx
        movzx   ecx,ax
        imul    ecx,ecx,AC_SZ
        movzx   esi,word ptr [g_ui.ac.ac_n]
        imul    esi,esi,AC_SZ
        add     esi,offset g_ui.bf.ac_list-1
        lea     edi,[esi+AC_SZ]
        std
        rep     movsb
        cld
        pop     ecx
        pop     esi
        movzx   ecx,cx
        mov     edi,ebx
        mov     al,cl
        stosb
        inc     cx
        rep     movsb
        mov     ax,word ptr [g_ui.ac.ac_kd]
        mov     word ptr [ebx+TACE.kd],ax
        mov     ax,[g_ui.ac.ac_xt]
        mov     [ebx+TACE.xt],ax
        inc     [g_ui.ac.ac_n]
@aad_r:
        popad
        ret

; ::::::::::::::
; ac_common - the word as long as all of ac_list agree: those letters put in.
; ::::::::::::::
ac_common:
        movzx   ebx,word ptr [g_ui.ac.ac_pn]
@acc_l:
        mov     esi,offset g_ui.bf.ac_list
        cmp     bl,[esi]
        jae     @acc_e
        mov     al,[esi+1+ebx]
        mov     dx,[g_ui.ac.ac_n]
@acc_n:
        cmp     bl,[esi]
        jae     @acc_e
        cmp     al,[esi+1+ebx]
        jne     @acc_e
        add     esi,AC_SZ
        dec     dx
        jnz     @acc_n
        inc     bx
        jmp     @acc_l
@acc_e:
        cmp     bx,[g_ui.ac.ac_pn]
        je      @acc_r
        mov     esi,offset g_ui.bf.ac_list+1
        mov     dx,bx
        jmp     ac_put
@acc_r:
        ret

; ::::::::::::::
; ac_ins - entry AX of ac_list completes the word; a function gets its '('
; and its form on the status bar.
; ::::::::::::::
ac_ins:
        movzx   esi,ax
        imul    esi,esi,AC_SZ
        add     esi,offset g_ui.bf.ac_list
        push    esi
        movzx   dx,byte ptr [esi]
        inc     esi
        call    ac_put
        pop     esi
        cmp     byte ptr [esi+TACE.kd],AK_FN
        jne     @acf_r
        movzx   ebx,byte ptr [esi]
        cmp     byte ptr [esi+ebx],'('
        je      @acf_h
        push    esi
        mov     esi,offset ac_lpar
        mov     ecx,1
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_insn
        pop     esi
@acf_h:
        call    ac_sig
        mov     [g_ui.lp.ui_help],offset g_ui.ac.ac_sigb
@acf_r:
        ret

        .data
ac_lpar db      '('
        .code

; ::::::::::::::
; ac_sig - the form of function entry SI in ac_sigb: LEFT$(s$, n).
; ::::::::::::::
ac_sig:
        push    esi
        mov     dl,byte ptr [esi+TACE.xt]
        mov     edi,offset g_ui.ac.ac_sigb
        movzx   ecx,byte ptr [esi]
        inc     esi
        rep     movsb
        cmp     byte ptr [edi-1],'('
        je      @asg_a
        mov     byte ptr [edi],'('
        inc     edi
@asg_a:
        mov     esi,offset ac_args ; its arguments, else x
@asg_l:
        mov     ah,[esi]
        test    ah,ah
        jz      @asg_x
        inc     esi
        cmp     ah,dl
        je      @asg_c
@asg_s:
        lodsb
        test    al,al
        jnz     @asg_s
        jmp     @asg_l
@asg_x:
        mov     esi,offset ac_x
@asg_c:
        lodsb
        stosb
        test    al,al
        jnz     @asg_c
        mov     word ptr [edi-1],')'
        pop     esi
        ret

; the arguments of the functions not of one number
        .data
ac_args db      TK_LEN-1,'address',0
        db      TK_LEN,'s$',0
        db      TK_LEN+2,'s$',0
        db      TK_LEN+3,'s$',0
        db      TK_LEN+4,'code',0
        db      TK_LEN+5,'s$, n',0
        db      TK_LEN+6,'s$, n',0
        db      TK_MID,'s$, start[, n]',0
        db      TK_TAB,'column',0
        db      TK_SPC,'n',0
        db      0
ac_x    db      'x',0
        .code

; ::::::::::::::
; ac_put - characters ac_pn to DX of the name at SI typed after the word,
; in small letters when the word is; the word that long.
; ::::::::::::::
ac_put:
        mov     edi,offset g_ui.ac.ac_tmp
        movzx   ecx,dx
        sub     cx,[g_ui.ac.ac_pn]
        jbe     @acp_r
        push    ecx
        movzx   eax,word ptr [g_ui.ac.ac_pn]
        add     esi,eax
@acp_c:
        lodsb
        cmp     [g_ui.ac.ac_lc],0
        je      @acp_s
        call    ac_alpha
        jc      @acp_s
        or      al,20h
@acp_s:
        stosb
        loop    @acp_c
        pop     ecx
        mov     [g_ui.ac.ac_pn],dx
        mov     esi,offset g_ui.ac.ac_tmp
        mov     ebx,offset g_ui.bf.ed_lb
        call    le_insn
        mov     [g_ui.tx.tx_mod],1
@acp_r:
        ret

; ::::::::::::::
; ac_pop - the list under the word: arrows choose, Enter or Tab takes, Esc
; or another key leaves it, the key then typed.
;
; out:    CF  clear
; ::::::::::::::
ac_pop:
        and     [g_ui.ac.ac_sel],0
        and     [g_ui.ac.ac_top],0
        mov     cl,4                    ; the box: the longest name and 4
        mov     esi,offset g_ui.bf.ac_list
        mov     dx,[g_ui.ac.ac_n]
@aco_w:
        mov     al,AC_PREV              ; a label: its line after it
        cmp     byte ptr [esi+TACE.kd],AK_LB
        je      @aco_v
        mov     al,[esi]
        add     al,4
@aco_v:
        cmp     al,cl
        jbe     @aco_x
        mov     cl,al
@aco_x:
        add     esi,AC_SZ
        dec     dx
        jnz     @aco_w
        mov     [g_ui.ac.ac_bw],cl
        mov     ax,[g_ui.ac.ac_n]                ; its rows
        cmp     ax,AC_ROWS
        jbe     @aco_h
        mov     ax,AC_ROWS
@aco_h:
        mov     [g_ui.ac.ac_bn],al
        add     al,2
        mov     ch,al
        mov     ax,[g_ui.ed.ed_line]             ; under the line, else over it
        sub     ax,[g_ui.ed.ed_top]
        add     al,byte ptr [g_ui.ed.ed_r0]
        inc     al
        mov     dh,al
        add     al,ch
        cmp     al,24
        jbe     @aco_c
        sub     dh,ch
        dec     dh
@aco_c:
        mov     ax,[g_ui.ac.ac_ps]
        sub     ax,[g_ui.ed.ed_left]
        jae     @aco_l
        xor     ax,ax
@aco_l:
        mov     dl,al
        add     al,cl
        cmp     al,80
        jbe     @aco_p
        mov     dl,80
        sub     dl,cl
@aco_p:
        mov     [g_ui.ac.ac_at],dx
@aco_d:
        mov     [g_ui.lp.ui_help],0              ; a function's form on the status bar
        movzx   esi,word ptr [g_ui.ac.ac_sel]
        imul    esi,esi,size TACE
        add     esi,offset g_ui.bf.ac_list
        cmp     byte ptr [esi+TACE.kd],AK_FN
        jne     @aco_e
        call    ac_sig
        mov     [g_ui.lp.ui_help],offset g_ui.ac.ac_sigb
@aco_e:
        call    dr_all
        call    ac_draw
        call    ui_getkey
        cmp     ax,MS_KEY
        je      @aco_ms
        cmp     al,0Dh
        je      @aco_ok
        cmp     al,09h
        je      @aco_ok
        cmp     al,1Bh
        je      @aco_q
        test    al,al
        jnz     @aco_k
        mov     dx,[g_ui.ac.ac_sel]
        cmp     ah,48h
        je      @aco_up
        cmp     ah,50h
        je      @aco_dn
        cmp     ah,49h
        je      @aco_pu
        cmp     ah,51h
        je      @aco_pd
@aco_k:
        call    ui_key                  ; another key: typed as ever
@aco_q:
        mov     [g_ui.lp.ui_help],0
        clc
        ret
@aco_ok:
        mov     [g_ui.lp.ui_help],0
        mov     ax,[g_ui.ac.ac_sel]
        call    ac_ins
        clc
        ret
@aco_up:
        dec     dx
        jns     @aco_s
        xor     dx,dx
        jmp     @aco_s
@aco_pu:
        sub     dx,AC_ROWS
        jns     @aco_s
        xor     dx,dx
        jmp     @aco_s
@aco_pd:
        add     dx,AC_ROWS-1
@aco_dn:
        inc     dx
        cmp     dx,[g_ui.ac.ac_n]
        jb      @aco_s
        mov     dx,[g_ui.ac.ac_n]
        dec     dx
@aco_s:
        mov     [g_ui.ac.ac_sel],dx
        cmp     dx,[g_ui.ac.ac_top]
        jae     @aco_t
        mov     [g_ui.ac.ac_top],dx
@aco_t:
        movzx   ax,[g_ui.ac.ac_bn]
        add     ax,[g_ui.ac.ac_top]
        cmp     dx,ax
        jb      @aco_d
        sub     dx,AC_ROWS-1
        mov     [g_ui.ac.ac_top],dx
        jmp     @aco_d
@aco_ms:
        cmp     [g_ui.ms.ms_ev],MS_PRESS
        jne     @aco_d
        mov     dx,[g_ui.ms.ms_pos]              ; a click on a name takes it
        mov     ax,[g_ui.ac.ac_at]
        sub     dh,ah
        jbe     @aco_q
        cmp     dh,[g_ui.ac.ac_bn]
        ja      @aco_q
        sub     dl,al
        jbe     @aco_q
        cmp     dl,[g_ui.ac.ac_bw]
        jae     @aco_q
        movzx   ax,dh
        dec     ax
        add     ax,[g_ui.ac.ac_top]
        mov     [g_ui.ac.ac_sel],ax
        jmp     @aco_ok

; ::::::::::::::
; ac_draw - the list's box at ac_at, ac_sel lit.
; ::::::::::::::
ac_draw:
        mov     dx,[g_ui.ac.ac_at]
        mov     ch,[g_ui.ac.ac_bn]
        add     ch,2
        mov     cl,[g_ui.ac.ac_bw]
        mov     ah,A_BAR
        xor     esi,esi
        call    sc_box
        call    sc_shadow
        xor     ebx,ebx
@acd_r:
        cmp     bl,[g_ui.ac.ac_bn]
        jae     @acd_e
        mov     dx,[g_ui.ac.ac_at]
        add     dh,bl
        inc     dh
        add     dl,2
        mov     ax,[g_ui.ac.ac_top]
        add     ax,bx
        movzx   esi,ax
        imul    esi,esi,AC_SZ
        add     esi,offset g_ui.bf.ac_list
        movzx   ecx,byte ptr [esi]
        inc     esi
        mov     ah,A_BAR
        push    edx
        push    esi
        call    sc_strn
        pop     esi
        pop     edx
        cmp     byte ptr [esi-1+TACE.kd],AK_LB
        jne     @acd_s
        call    ac_prev
@acd_s:
        mov     ax,[g_ui.ac.ac_top]
        add     ax,bx
        cmp     ax,[g_ui.ac.ac_sel]
        jne     @acd_n
        dec     dl
        movzx   ecx,[g_ui.ac.ac_bw]
        sub     cx,2
        mov     ah,07h
        call    sc_attr
@acd_n:
        inc     bx
        jmp     @acd_r
@acd_e:
        ret

; ::::::::::::::
; ac_prev - label entry SI-1's line after the label, at DH, DL+7 in the box.
;
; keeps:  BX, DX
; ::::::::::::::
ac_prev:
        push    ebx
        push    edx
        mov     ax,[esi-1+TACE.xt]
        call    ac_line
        xor     edi,edi                 ; its text from the item after the label
        cmp     byte ptr [esi],M_INT
        jne     @apv_t
        cmp     byte ptr [esi+5],0
        je      @apv_e
        movzx   edi,byte ptr [g_lxcol+5]
@apv_t:
        add     edi,offset g_ui.bf.sy_buf
        mov     esi,edi                 ; its length, to the box's edge
@apv_n:
        lodsb
        test    al,al
        jnz     @apv_n
        mov     ecx,esi
        sub     ecx,edi
        dec     cx
        cmp     cx,AC_PREV-11
        jbe     @apv_w
        mov     ecx,AC_PREV-11
@apv_w:
        mov     esi,edi
        add     dl,7
        mov     ah,A_BAR
        call    sc_strn
@apv_e:
        pop     edx
        pop     ebx
        ret
endif

        end
