; ==========================================================================
; compl.asm - Tab completes the word at the cursor, from what fits there
; ==========================================================================
; holds:    ac_tab
; uses:     kwlist, tx_get, le_insn, sc_*, dr_all, ui_getkey
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

AM_ST   = 1                             ; statements
AM_FN   = 2                             ; functions, NOT
AM_OP   = 4                             ; the words after an operand
AM_VR   = 8                             ; variables
AM_LB   = 10h                           ; line labels
AP_ST   = 0                             ; ac_pv: a statement's start
AP_ID   = 1                             ;   an operand
AP_SEP  = 2                             ;   an operator, a bracket
AP_COM  = 3                             ;   a comma, a semicolon
AC_ROWS = 8                             ; the list's rows
AC_LEN  = 10                            ; a name's characters at most
AE_KD   = 12                            ; an entry's kind
AE_RK   = 13                            ;   its rank
AE_XT   = 14                            ;   its token or line
AK_KW   = 0                             ; kinds: a keyword
AK_FN   = 1                             ;   a function: '(' put after
AK_VR   = 2                             ;   a variable
AK_LB   = 3                             ;   a line label
AT_ANY  = 0                             ; ac_ty: any type
AT_STR  = 1                             ;   a string
AT_NUM  = 2                             ;   a number
AC_PREV = 40                            ; a label's box: columns

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
        and     [ac_n],0
        mov     [ac_on],0
        cmp     [ac_m],AM_VR            ; NEXT: the open FOR's variable
        jne     @act_e
        cmp     [ac_st],TK_NEXT
        jne     @act_e
        call    ac_open
        cmp     [ac_pn],0
        jne     @act_c
        movzx   cx,[ac_on]              ; no word: that alone
        mov     si,offset ac_on+1
        mov     ax,AK_VR
        call    ac_add
        jmp     @act_g
@act_e:
        cmp     [ac_pn],0               ; no word: labels, or what has the
        jne     @act_c                  ; type wanted
        test    [ac_m],AM_LB
        jnz     @act_c
        cmp     [ac_ty],AT_ANY
        je      @act_n
        test    [ac_m],AM_FN+AM_VR
        jz      @act_n
@act_c:
        call    ac_keys
        call    ac_vars
@act_g:
        mov     cx,[ac_n]
        jcxz    @act_n
        call    ac_common
        cmp     [ac_n],1
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
        mov     bx,ds:[ed_lb+LB_COL]
        cmp     bx,ds:[ed_lb+LB_LEN]
        ja      @acw_n
        je      @acw_b
        mov     al,byte ptr ds:[ed_lb+LB_TXT+bx]
        call    ac_alnum
        jnc     @acw_n
@acw_b:
        mov     si,bx
@acw_l:
        test    si,si
        jz      @acw_s
        mov     al,byte ptr [ed_lb+LB_TXT-1+si]
        call    ac_alnum
        jc      @acw_s
        dec     si
        jmp     @acw_l
@acw_s:
        mov     [ac_ps],si
        mov     cx,bx
        sub     cx,si
        mov     [ac_pn],cx
        cmp     cx,AC_SZ-2
        jae     @acw_n
        mov     [ac_lc],0
        mov     di,offset ac_pfx
        jcxz    @acw_e
@acw_c:
        mov     al,byte ptr ds:[ed_lb+LB_TXT+si]
        mov     [ac_lc],0
        cmp     al,'a'
        jb      @acw_u
        mov     [ac_lc],1
@acw_u:
        call    ui_upper
        stosb
        inc     si
        loop    @acw_c
@acw_e:
        mov     byte ptr [di],0
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
        push    ax
        call    ui_upper
        cmp     al,'A'
        jb      @aal_n
        cmp     al,'Z'
        ja      @aal_n
        pop     ax
@aan_y:
        clc
        ret
@aal_n:
        pop     ax
@aan_n:
        stc
        ret

; ::::::::::::::
; ac_kw - the keyword that starts at SI, as the tokenizer finds it.
;
; out:    CF  clear: AL its token, CX its length
; keeps:  all but AX, CX
; ::::::::::::::
ac_kw:
        push    bx
        push    dx
        push    di
        mov     bx,offset kwlist
        mov     dl,80h
@akw_e:
        cmp     byte ptr [bx],0
        je      @akw_n
        mov     di,si
        xor     cx,cx
@akw_c:
        mov     al,[di]
        call    ui_upper
        mov     ah,[bx]
        and     ah,7Fh
        cmp     al,ah
        jne     @akw_s
        inc     cx
        inc     di
        test    byte ptr [bx],80h
        lea     bx,[bx+1]
        jz      @akw_c
        mov     al,dl
        clc
        jmp     @akw_x
@akw_s:
        test    byte ptr [bx],80h
        lea     bx,[bx+1]
        jz      @akw_s
        inc     dl
        jmp     @akw_e
@akw_n:
        stc
@akw_x:
        pop     di
        pop     dx
        pop     bx
        ret

; ::::::::::::::
; ac_ctx - what fits at the word: the line before it read as the
; tokenizer reads it, the type each operand gives (ac_lt) and the one the
; expression wants (ac_ty).
;
; out:    CF  set: nothing (a string, REM, DATA)
;         ac_m  what fits
; ::::::::::::::
ac_ctx:
        mov     si,offset ed_lb+LB_TXT
        mov     di,offset sy_buf
        mov     cx,[ac_ps]
        rep     movsb
        mov     byte ptr [di],0
        mov     si,offset sy_buf
@acx_ss:
        mov     [ac_st],0
        mov     [ac_pv],AP_ST
        mov     [ac_ty],AT_ANY
        mov     [ac_fn],0
@acx_l:
        lodsb
        test    al,al
        jz      @acx_e
        cmp     al,' '
        je      @acx_l
        cmp     al,'"'
        je      @acx_q
        cmp     al,':'
        je      @acx_ss
        dec     si
        call    ac_kw
        jc      @acx_nk
        add     si,cx
        cmp     al,TK_REM
        je      @acx_no
        cmp     al,TK_DATA
        je      @acx_no
        cmp     [ac_pv],TK_THEN         ; a statement's start: its keyword
        je      @acx_s
        cmp     [ac_pv],AP_ST
        jne     @acx_t
@acx_s:
        mov     [ac_st],al
@acx_t:
        mov     [ac_pv],al
        call    ac_tkty
        cmp     al,TK_PLUS              ; + - * / ^ AND OR > = <
        jb      @acx_l
        cmp     al,TK_PLUS+9
        ja      @acx_l
        mov     [ac_pv],AP_SEP
        jmp     @acx_l
@acx_nk:
        mov     al,[si]
        call    ac_alpha
        jc      @acx_d
        call    ac_name
        call    ac_nty
        mov     [ac_lt],al
        cmp     [ac_pv],AP_ST
        jne     @acx_ar
        mov     [ac_st],AP_ID           ; an assignment
@acx_ar:
        movzx   bx,[ac_nm]              ; an array's subscripts: numbers
        cmp     byte ptr [ac_nm+bx],'('
        jne     @acx_op
        mov     [ac_flt],al
        mov     [ac_fn],0FFh
        mov     [ac_ty],AT_NUM
        mov     [ac_pv],AP_SEP
        jmp     @acx_l
@acx_d:
        inc     si
        call    ac_digit
        jc      @acx_o
@acx_dl:
        lodsb                           ; a number; a label at the start
        call    ac_digit
        jnc     @acx_dl
        dec     si
        mov     [ac_lt],AT_NUM
        cmp     [ac_pv],AP_ST
        jne     @acx_op
        cmp     [ac_st],0
        je      @acx_l
@acx_op:
        mov     [ac_pv],AP_ID
        jmp     @acx_l
@acx_o:
        mov     ah,AP_SEP
        mov     dl,[ac_fn]
        cmp     al,'('                  ; a function's argument: its type
        jne     @acx_c
        test    dl,dl
        jz      @acx_p
        mov     [ac_ty],AT_STR
        cmp     dl,TK_LEN
        je      @acx_p
        cmp     dl,TK_LEN+2             ; VAL, ASC
        je      @acx_p
        cmp     dl,TK_LEN+3
        je      @acx_p
        cmp     dl,TK_LEN+5             ; LEFT$, RIGHT$, MID$
        jae     @acx_p
        mov     [ac_ty],AT_NUM
        jmp     @acx_p
@acx_c:
        cmp     al,')'
        jne     @acx_m
        mov     ah,AP_ID
        test    dl,dl
        jz      @acx_p
        mov     al,[ac_flt]             ; the call's value
        mov     [ac_lt],al
        mov     [ac_fn],0
        jmp     @acx_p
@acx_m:
        cmp     al,','
        je      @acx_cm
        cmp     al,';'
        jne     @acx_p
@acx_cm:
        mov     ah,AP_COM
        mov     [ac_ty],AT_ANY
        cmp     dl,TK_LEN+5             ; LEFT$( s$, n
        jb      @acx_p
        mov     [ac_ty],AT_NUM
@acx_p:
        mov     [ac_pv],ah
        jmp     @acx_l
@acx_q:
        lodsb
        test    al,al
        jz      @acx_no
        cmp     al,'"'
        jne     @acx_q
        mov     [ac_lt],AT_STR
        jmp     @acx_op
@acx_no:
        stc
        ret
@acx_e:
        call    ac_mask
        mov     al,[ac_pfx]             ; a number is a label
        call    ac_digit
        jc      @acx_x
        and     [ac_m],AM_LB
@acx_x:
        cmp     [ac_m],0
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
        mov     [ac_fn],al
        mov     [ac_flt],ah
        mov     [ac_lt],ah
        ret
@atk_o:
        mov     ah,[ac_lt]              ; + = < >: the other operand's type
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
        mov     [ac_ty],ah
@atk_x:
        ret

; ::::::::::::::
; ac_name - the name at SI, a letter first, to ac_nm: its length, then it
; in capitals, AC_LEN at most; a '(' after it kept.
;
; out:    SI  past it
; keeps:  all but AX, SI
; ::::::::::::::
ac_name:
        push    cx
        push    di
        mov     di,offset ac_nm+1
        mov     al,[si]
        jmp     @anm_c
@anm_l:
        mov     al,[si]
        call    ac_alnum
        jc      @anm_e
        push    ax
        call    ac_kw                   ; a keyword ends it
        pop     ax
        jnc     @anm_x
@anm_c:
        call    ac_put1
        inc     si
        jmp     @anm_l
@anm_e:
        cmp     al,'$'
        je      @anm_s
        cmp     al,'%'
        jne     @anm_x
@anm_s:
        call    ac_put1
        inc     si
@anm_x:
        cmp     byte ptr [si],'('
        jne     @anm_z
        mov     al,'('
        call    ac_put1
        inc     si
@anm_z:
        mov     ax,di
        sub     ax,offset ac_nm+1
        mov     [ac_nm],al
        mov     byte ptr [di],0
        pop     di
        pop     cx
        ret

; ::::::::::::::
; ac_put1 - AL in capitals at DI, while ac_nm has room.
; ::::::::::::::
ac_put1:
        cmp     di,offset ac_nm+1+AC_LEN
        jae     @ap1_r
        call    ui_upper
        stosb
@ap1_r:
        ret

; ::::::::::::::
; ac_nty - the type of the name in ac_nm: a string when it ends in $.
;
; out:    AL  AT_STR or AT_NUM
; keeps:  all but AX
; ::::::::::::::
ac_nty:
        push    bx
        movzx   bx,[ac_nm]
        cmp     byte ptr [ac_nm+bx],'('
        jne     @anty_d
        dec     bx
@anty_d:
        mov     al,AT_STR
        cmp     byte ptr [ac_nm+bx],'$'
        je      @anty_r
        mov     al,AT_NUM
@anty_r:
        pop     bx
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
        mov     al,[ac_pv]
        mov     ah,[ac_st]
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
        mov     si,offset ac_vst
@acm_l:
        mov     dl,[si]
        test    dl,dl
        jz      @acm_e
        inc     si
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
        mov     [ac_m],bl
        ret

DSEG    segment
ac_vst  db      TK_FOR,TK_NEXT,TK_DIM,TK_READ,TK_INPUT,TK_INPUTN,TK_GET,0
DSEG    ends

; ::::::::::::::
; ac_want - is token AL wanted by ac_m in statement ac_st?
;
; out:    CF  clear: it is
; ::::::::::::::
ac_want:
        test    [ac_m],AM_ST
        jz      @acn_f
        cmp     al,TK_NEW
        jbe     @acn_y
@acn_f:
        test    [ac_m],AM_FN
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
        test    [ac_m],AM_OP
        jz      @acn_n
        cmp     al,TK_PLUS+5            ; AND, OR
        je      @acn_y
        cmp     al,TK_PLUS+6
        je      @acn_y
        mov     dl,al
        mov     si,offset ac_ops
@acn_l:
        mov     al,[si]
        test    al,al
        jz      @acn_n
        add     si,3
        cmp     al,[ac_st]
        jne     @acn_l
        cmp     dl,[si-2]
        je      @acn_y
        cmp     dl,[si-1]
        je      @acn_y
        jmp     @acn_l
@acn_y:
        clc
        ret
@acn_n:
        stc
        ret

; the words that go on after an operand, by statement
DSEG    segment
ac_ops  db      TK_IF,TK_THEN,TK_GOTO
        db      TK_FOR,TK_TO,TK_STEP
        db      TK_ON,TK_GOTO,TK_GOSUB
        db      0
DSEG    ends

; ::::::::::::::
; ac_keys - the wanted keywords that start with the word, to ac_list: a
; function only of the type wanted.
; ::::::::::::::
ac_keys:
        mov     bx,offset kwlist
        mov     dl,80h
@ack_e:
        cmp     byte ptr [bx],0
        je      @ack_r
        mov     di,offset ac_tmp
@ack_c:
        mov     al,[bx]
        inc     bx
        mov     ah,al
        and     al,7Fh
        stosb
        test    ah,80h
        jz      @ack_c
        mov     byte ptr [di],0
        push    bx
        push    dx
        mov     al,dl
        call    ac_want
        jc      @ack_n
        mov     al,[ac_tmp]
        call    ac_alpha
        jc      @ack_n
        mov     cl,AK_KW
        mov     al,dl
        cmp     [ac_ty],AT_STR          ; FN and NOT give numbers
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
        cmp     [ac_st],TK_PRINT
        je      @ack_q
        cmp     [ac_st],TK_PRINTN
        jne     @ack_n
@ack_q:
        cmp     al,TK_TAB
        je      @ack_f
        cmp     al,TK_SPC
        je      @ack_f
        cmp     al,TK_PLUS+10
        jb      @ack_a
        call    ac_tkty                 ; a function: its value's type
        mov     al,[ac_ty]
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
        mov     si,offset ac_tmp
        mov     cx,di
        sub     cx,si
        call    ac_add
@ack_n:
        pop     dx
        pop     bx
        inc     dl
        jmp     @ack_e
@ack_r:
        ret

; ::::::::::::::
; ac_vars - the program's variables or labels that start with the word.
; GOSUB ranks first a label after a RETURN, END, STOP or GOTO: a start.
; ::::::::::::::
ac_vars:
        test    [ac_m],AM_VR+AM_LB
        jz      @acv_r
        mov     [ac_plk],0
        xor     ax,ax
@acv_l:
        cmp     ax,[tx_n]
        jae     @acv_r
        push    ax
        mov     di,offset sy_buf
        cmp     ax,[ed_line]
        jne     @acv_t
        mov     si,offset ed_lb+LB_TXT
        mov     cx,ds:[ed_lb+LB_LEN]
        rep     movsb
        jmp     @acv_z
@acv_t:
        call    tx_get
        add     di,cx
@acv_z:
        mov     byte ptr [di],0
        mov     si,offset sy_buf
        pop     dx
        push    dx
        test    [ac_m],AM_LB
        jz      @acv_v
        cmp     dx,[ed_line]            ; not the word's own line
        je      @acv_v
        call    ac_label
@acv_v:
        test    [ac_m],AM_VR
        jz      @acv_n
        call    ac_ids
@acv_n:
        mov     si,offset sy_buf
        call    ac_lastkw
        mov     [ac_plk],al
        pop     ax
        inc     ax
        jmp     @acv_l
@acv_r:
        ret

; ::::::::::::::
; ac_label - the label that starts line DX, at SI, to ac_list.
;
; keeps:  SI
; ::::::::::::::
ac_label:
        push    si
@acl_s:
        lodsb
        cmp     al,' '
        je      @acl_s
        dec     si
        mov     di,si
@acl_d:
        lodsb
        cmp     al,'0'
        jb      @acl_e
        cmp     al,'9'
        jbe     @acl_d
@acl_e:
        dec     si
        mov     cx,si
        sub     cx,di
        jcxz    @acl_r
        mov     si,di
        mov     ax,AK_LB+100h           ; GOSUB: a start first
        cmp     [ac_pv],TK_GOSUB
        jne     @acl_a
        mov     bl,[ac_plk]
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
@acl_r:
        pop     si
        ret

; ::::::::::::::
; ac_lastkw - the keyword that starts the last statement of the line at SI.
;
; out:    AL  it, 0 none
; keeps:  all but AX, CX
; ::::::::::::::
ac_lastkw:
        push    si
        push    dx
        xor     dx,dx                   ; DL the keyword, DH 0 at a start
@alk_l:
        lodsb
        test    al,al
        jz      @alk_e
        cmp     al,'"'
        je      @alk_q
        cmp     al,' '
        je      @alk_l
        cmp     al,':'
        jne     @alk_k
        xor     dx,dx
        jmp     @alk_l
@alk_k:
        dec     si
        call    ac_kw
        jc      @alk_n
        add     si,cx
        test    dh,dh
        jnz     @alk_t
        mov     dl,al
        inc     dh
@alk_t:
        cmp     al,TK_REM
        je      @alk_e
        jmp     @alk_l
@alk_n:
        lodsb
        call    ac_digit                ; a label keeps the start
        jnc     @alk_l
        mov     dh,1
        jmp     @alk_l
@alk_q:
        lodsb
        test    al,al
        jz      @alk_e
        cmp     al,'"'
        jne     @alk_q
        jmp     @alk_l
@alk_e:
        mov     al,dl
        pop     dx
        pop     si
        ret

; ::::::::::::::
; ac_ids - the names in the line at SI, to ac_list: not in strings, not
; after REM or DATA, not keywords; of the type wanted; no array after FOR
; or NEXT.
; ::::::::::::::
ac_ids:
        lodsb
        test    al,al
        jz      @aci_r
        cmp     al,'"'
        je      @aci_q
        dec     si
        call    ac_kw
        jc      @aci_n
        add     si,cx
        cmp     al,TK_REM
        je      @aci_r
        cmp     al,TK_DATA
        je      @aci_r
        jmp     ac_ids
@aci_n:
        mov     al,[si]
        call    ac_alpha
        jnc     @aci_i
        inc     si
        call    ac_digit                ; a number: its digits are no name
        jc      ac_ids
@aci_d:
        lodsb
        call    ac_alnum
        jnc     @aci_d
        dec     si
        jmp     ac_ids
@aci_i:
        call    ac_name
        call    ac_nty
        mov     ah,[ac_ty]
        test    ah,ah
        jz      @aci_t
        cmp     al,ah
        jne     ac_ids
@aci_t:
        movzx   bx,[ac_nm]
        cmp     byte ptr [ac_nm+bx],'('
        jne     @aci_a
        cmp     [ac_m],AM_VR
        jne     @aci_a
        cmp     [ac_st],TK_FOR
        je      ac_ids
        cmp     [ac_st],TK_NEXT
        je      ac_ids
@aci_a:
        push    si
        mov     si,offset ac_nm+1
        movzx   cx,[ac_nm]
        mov     ax,AK_VR
        call    ac_add
        pop     si
        jmp     ac_ids
@aci_q:
        lodsb
        test    al,al
        jz      @aci_r
        cmp     al,'"'
        jne     @aci_q
        jmp     ac_ids
@aci_r:
        ret

; ::::::::::::::
; ac_open - the variable of the innermost FOR that no NEXT closes before
; the word, to ac_on: its length, its text.
; ::::::::::::::
ac_open:
        mov     [ac_dp],0
        mov     si,offset sy_buf        ; the line to the word, as ac_ctx left it
        mov     [ac_cur],1
        mov     ax,[ed_line]
@aop_l:
        push    ax
        call    ac_evs
        pop     ax
        jnc     @aop_r
        mov     [ac_cur],0
        dec     ax
        js      @aop_r
        mov     di,offset sy_buf
        push    ax
        call    tx_get
        add     di,cx
        mov     byte ptr [di],0
        pop     ax
        mov     si,offset sy_buf
        jmp     @aop_l
@aop_r:
        ret

; ::::::::::::::
; ac_evs - the line at SI's FORs and NEXTs, taken last first against the
; NEXTs after them (ac_dp).
;
; out:    CF  clear: the open FOR found, in ac_on
; ::::::::::::::
ac_evs:
        mov     di,offset ac_ev         ; events: FOR its name; NEXT 0, a count
        xor     bx,bx
@aev_l:
        lodsb
        test    al,al
        jz      @aev_b
        cmp     al,'"'
        je      @aev_q
        dec     si
        call    ac_kw
        jc      @aev_n
        add     si,cx
        cmp     al,TK_REM
        je      @aev_b
        cmp     al,TK_DATA
        je      @aev_b
        cmp     al,TK_FOR
        je      @aev_f
        cmp     al,TK_NEXT
        jne     @aev_l
        xor     cx,cx                   ; NEXT: its names
@aev_x:
        lodsb
        cmp     al,' '
        je      @aev_x
        cmp     al,','
        je      @aev_x
        dec     si
        call    ac_alpha
        jc      @aev_c
        push    di
        call    ac_name
        pop     di
        inc     cx
        jmp     @aev_x
@aev_c:
        mov     byte ptr [di],0
        mov     [di+1],cl
        jmp     @aev_s
@aev_f:
        lodsb
        cmp     al,' '
        je      @aev_f
        dec     si
        call    ac_alpha
        jc      @aev_l
        push    di
        call    ac_name
        pop     di
        push    si
        mov     si,offset ac_nm
        mov     cx,AC_SZ
        rep     movsb
        sub     di,AC_SZ
        pop     si
@aev_s:
        cmp     bx,AC_EVN
        jae     @aev_l
        add     di,AC_SZ
        inc     bx
        jmp     @aev_l
@aev_n:
        inc     si
        jmp     @aev_l
@aev_q:
        lodsb
        test    al,al
        jz      @aev_b
        cmp     al,'"'
        jne     @aev_q
        jmp     @aev_l
@aev_b:
        test    bx,bx                   ; last first
        jz      @aev_no
        sub     di,AC_SZ
        dec     bx
        cmp     byte ptr [di],0
        jne     @aev_for
        movzx   ax,byte ptr [di+1]      ; NEXT: a bare one closes one, but
        test    ax,ax                   ; not the one being written
        jnz     @aev_a
        cmp     [ac_cur],0
        jne     @aev_a
        inc     ax
@aev_a:
        add     [ac_dp],ax
        mov     [ac_cur],0
        jmp     @aev_b
@aev_for:
        cmp     [ac_dp],0
        je      @aev_y
        dec     [ac_dp]
        jmp     @aev_b
@aev_y:
        mov     si,di
        mov     di,offset ac_on
        mov     cx,AC_SZ
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
        pusha
        mov     [ac_kd],ax
        mov     [ac_xt],dx
        cmp     cx,[ac_pn]
        jbe     @aad_r
        cmp     cx,AC_LEN
        ja      @aad_r
        mov     di,offset ac_tmp+AC_SZ  ; the name in capitals, 0-ended
        push    di
        push    cx
@aad_u:
        lodsb
        call    ui_upper
        stosb
        loop    @aad_u
        mov     byte ptr [di],0
        pop     cx
        pop     si
        mov     di,offset ac_pfx
        push    cx
        mov     cx,[ac_pn]
        push    si
        cmp     ax,ax                   ; no word: every name starts so
        repe    cmpsb
        pop     si
        pop     cx
        jne     @aad_r
        cmp     [ac_kd],AK_VR           ; the open FOR's variable first
        jne     @aad_o
        cmp     [ac_on],0
        je      @aad_o
        mov     [ac_rk],1
        push    si
        push    cx
        mov     di,offset ac_on+1
        inc     cx
        repe    cmpsb
        pop     cx
        pop     si
        jne     @aad_o
        mov     [ac_rk],0
@aad_o:
        mov     bx,offset ac_list       ; the place: before the first above it
        xor     dx,dx
@aad_p:
        cmp     dx,[ac_n]
        jae     @aad_i
        mov     al,[ac_rk]
        cmp     al,[bx+AE_RK]
        jb      @aad_i
        ja      @aad_x
        push    si
        lea     di,[bx+1]
@aad_c:
        lodsb
        mov     ah,[di]
        inc     di
        cmp     al,ah
        jne     @aad_d
        test    al,al
        jnz     @aad_c
        pop     si                      ; there already
        jmp     @aad_r
@aad_d:
        pop     si
        jb      @aad_i
@aad_x:
        add     bx,AC_SZ
        inc     dx
        jmp     @aad_p
@aad_i:
        cmp     [ac_n],AC_MAX
        jae     @aad_r
        push    si                      ; the rest up one
        push    cx
        mov     ax,[ac_n]
        sub     ax,dx
        imul    cx,ax,AC_SZ
        mov     ax,[ac_n]
        imul    si,ax,AC_SZ
        add     si,offset ac_list-1
        lea     di,[si+AC_SZ]
        std
        rep     movsb
        cld
        pop     cx
        pop     si
        mov     di,bx
        mov     al,cl
        stosb
        inc     cx
        rep     movsb
        mov     ax,[ac_kd]
        mov     [bx+AE_KD],ax
        mov     ax,[ac_xt]
        mov     [bx+AE_XT],ax
        inc     [ac_n]
@aad_r:
        popa
        ret

; ::::::::::::::
; ac_common - the word as long as all of ac_list agree: those letters put in.
; ::::::::::::::
ac_common:
        mov     bx,[ac_pn]
@acc_l:
        mov     si,offset ac_list
        cmp     bl,[si]
        jae     @acc_e
        mov     al,[si+1+bx]
        mov     dx,[ac_n]
@acc_n:
        cmp     bl,[si]
        jae     @acc_e
        cmp     al,[si+1+bx]
        jne     @acc_e
        add     si,AC_SZ
        dec     dx
        jnz     @acc_n
        inc     bx
        jmp     @acc_l
@acc_e:
        cmp     bx,[ac_pn]
        je      @acc_r
        mov     si,offset ac_list+1
        mov     dx,bx
        jmp     ac_put
@acc_r:
        ret

; ::::::::::::::
; ac_ins - entry AX of ac_list completes the word; a function gets its '('
; and its form on the status bar.
; ::::::::::::::
ac_ins:
        imul    si,ax,AC_SZ
        add     si,offset ac_list
        push    si
        movzx   dx,byte ptr [si]
        inc     si
        call    ac_put
        pop     si
        cmp     byte ptr [si+AE_KD],AK_FN
        jne     @acf_r
        movzx   bx,byte ptr [si]
        cmp     byte ptr [si+bx],'('
        je      @acf_h
        push    si
        mov     si,offset ac_lpar
        mov     cx,1
        mov     bx,offset ed_lb
        call    le_insn
        pop     si
@acf_h:
        call    ac_sig
        mov     [ui_help],offset ac_sigb
@acf_r:
        ret

DSEG    segment
ac_lpar db      '('
DSEG    ends

; ::::::::::::::
; ac_sig - the form of function entry SI in ac_sigb: LEFT$(s$, n).
; ::::::::::::::
ac_sig:
        push    si
        mov     dl,[si+AE_XT]
        mov     di,offset ac_sigb
        movzx   cx,byte ptr [si]
        inc     si
        rep     movsb
        cmp     byte ptr [di-1],'('
        je      @asg_a
        mov     byte ptr [di],'('
        inc     di
@asg_a:
        mov     si,offset ac_args       ; its arguments, else x
@asg_l:
        mov     ah,[si]
        test    ah,ah
        jz      @asg_x
        inc     si
        cmp     ah,dl
        je      @asg_c
@asg_s:
        lodsb
        test    al,al
        jnz     @asg_s
        jmp     @asg_l
@asg_x:
        mov     si,offset ac_x
@asg_c:
        lodsb
        stosb
        test    al,al
        jnz     @asg_c
        mov     word ptr [di-1],')'
        pop     si
        ret

; the arguments of the functions not of one number
DSEG    segment
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
DSEG    ends

; ::::::::::::::
; ac_put - characters ac_pn to DX of the name at SI typed after the word,
; in small letters when the word is; the word that long.
; ::::::::::::::
ac_put:
        mov     di,offset ac_tmp
        mov     cx,dx
        sub     cx,[ac_pn]
        jbe     @acp_r
        push    cx
        add     si,[ac_pn]
@acp_c:
        lodsb
        cmp     [ac_lc],0
        je      @acp_s
        call    ac_alpha
        jc      @acp_s
        or      al,20h
@acp_s:
        stosb
        loop    @acp_c
        pop     cx
        mov     [ac_pn],dx
        mov     si,offset ac_tmp
        mov     bx,offset ed_lb
        call    le_insn
        mov     [tx_mod],1
@acp_r:
        ret

; ::::::::::::::
; ac_pop - the list under the word: arrows choose, Enter or Tab takes, Esc
; or another key leaves it, the key then typed.
;
; out:    CF  clear
; ::::::::::::::
ac_pop:
        and     [ac_sel],0
        and     [ac_top],0
        mov     cl,4                    ; the box: the longest name and 4
        mov     si,offset ac_list
        mov     dx,[ac_n]
@aco_w:
        mov     al,AC_PREV              ; a label: its line after it
        cmp     byte ptr [si+AE_KD],AK_LB
        je      @aco_v
        mov     al,[si]
        add     al,4
@aco_v:
        cmp     al,cl
        jbe     @aco_x
        mov     cl,al
@aco_x:
        add     si,AC_SZ
        dec     dx
        jnz     @aco_w
        mov     [ac_bw],cl
        mov     ax,[ac_n]               ; its rows
        cmp     ax,AC_ROWS
        jbe     @aco_h
        mov     ax,AC_ROWS
@aco_h:
        mov     [ac_bn],al
        add     al,2
        mov     ch,al
        mov     ax,[ed_line]            ; under the line, else over it
        sub     ax,[ed_top]
        add     al,ED_R0+1
        mov     dh,al
        add     al,ch
        cmp     al,24
        jbe     @aco_c
        sub     dh,ch
        dec     dh
@aco_c:
        mov     ax,[ac_ps]
        sub     ax,[ed_left]
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
        mov     [ac_at],dx
@aco_d:
        mov     [ui_help],0             ; a function's form on the status bar
        imul    si,[ac_sel],AC_SZ
        add     si,offset ac_list
        cmp     byte ptr [si+AE_KD],AK_FN
        jne     @aco_e
        call    ac_sig
        mov     [ui_help],offset ac_sigb
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
        mov     dx,[ac_sel]
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
        mov     [ui_help],0
        clc
        ret
@aco_ok:
        mov     [ui_help],0
        mov     ax,[ac_sel]
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
        cmp     dx,[ac_n]
        jb      @aco_s
        mov     dx,[ac_n]
        dec     dx
@aco_s:
        mov     [ac_sel],dx
        cmp     dx,[ac_top]
        jae     @aco_t
        mov     [ac_top],dx
@aco_t:
        movzx   ax,[ac_bn]
        add     ax,[ac_top]
        cmp     dx,ax
        jb      @aco_d
        sub     dx,AC_ROWS-1
        mov     [ac_top],dx
        jmp     @aco_d
@aco_ms:
        cmp     [ms_ev],MS_PRESS
        jne     @aco_d
        mov     dx,[ms_pos]             ; a click on a name takes it
        mov     ax,[ac_at]
        sub     dh,ah
        jbe     @aco_q
        cmp     dh,[ac_bn]
        ja      @aco_q
        sub     dl,al
        jbe     @aco_q
        cmp     dl,[ac_bw]
        jae     @aco_q
        movzx   ax,dh
        dec     ax
        add     ax,[ac_top]
        mov     [ac_sel],ax
        jmp     @aco_ok

; ::::::::::::::
; ac_draw - the list's box at ac_at, ac_sel lit.
; ::::::::::::::
ac_draw:
        mov     dx,[ac_at]
        mov     ch,[ac_bn]
        add     ch,2
        mov     cl,[ac_bw]
        mov     ah,A_BAR
        xor     si,si
        call    sc_box
        call    sc_shadow
        xor     bx,bx
@acd_r:
        cmp     bl,[ac_bn]
        jae     @acd_e
        mov     dx,[ac_at]
        add     dh,bl
        inc     dh
        add     dl,2
        mov     ax,[ac_top]
        add     ax,bx
        imul    si,ax,AC_SZ
        add     si,offset ac_list
        movzx   cx,byte ptr [si]
        inc     si
        mov     ah,A_BAR
        push    dx
        push    si
        call    sc_strn
        pop     si
        pop     dx
        cmp     byte ptr [si-1+AE_KD],AK_LB
        jne     @acd_s
        call    ac_prev
@acd_s:
        mov     ax,[ac_top]
        add     ax,bx
        cmp     ax,[ac_sel]
        jne     @acd_n
        dec     dl
        movzx   cx,[ac_bw]
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
        push    bx
        push    dx
        mov     ax,[si-1+AE_XT]
        mov     di,offset sy_buf
        call    tx_get
        mov     bx,cx
        mov     byte ptr [di+bx],0
        mov     si,di
@apv_s:
        lodsb
        cmp     al,' '
        je      @apv_s
        cmp     al,'0'
        jb      @apv_t
        cmp     al,'9'
        jbe     @apv_s
@apv_t:
        dec     si
        mov     di,si                   ; its length, to the box's edge
@apv_n:
        lodsb
        test    al,al
        jnz     @apv_n
        mov     cx,si
        sub     cx,di
        dec     cx
        cmp     cx,AC_PREV-11
        jbe     @apv_w
        mov     cx,AC_PREV-11
@apv_w:
        mov     si,di
        add     dl,7
        mov     ah,A_BAR
        call    sc_strn
        pop     dx
        pop     bx
        ret
