; ==========================================================================
; decl.asm - a line's declarations, into the name table as it is entered
; ==========================================================================
; holds:    dc_line
; uses:     g_ntyp, g_natyp, g_deft
; data:     As QBasic, a line's declarations go into the table when it is
;           entered, so lines compiled before it know them:
;           DEFtype a-b    deft, the type of a name with no suffix by its
;                          first letter
;           DIM [SHARED] n[( )] AS type   ntyp, natyp: n's, n()'s type
;           CONST n = e    ntyp: the value's type, when n has no suffix
;           SUB, FUNCTION, DECLARE n (params)   nflag NM_SUB, NM_FUNC; the
;                          parameters' AS types as DIM's; DEF FNn's too
;           SHARED, STATIC n   g_shr: the procedure being entered (dproc), n
;           DIM SHARED, a module's CONST   nflag NM_SHR
;           AS STRING * n  nfix, nafix: n's, n()'s length
;           OPTION BASE n  obase
;           A type is ET_*; 0 in ntyp, natyp is none declared. DEFtype holds
;           for the whole program, not from its line on.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; dc_proc - the body in cbuf: a procedure's first line, its END, or neither.
;
; out:    AL   PK_SUB, PK_FN, PK_END, else 0
;         ESI  the name's M_VAR, for PK_SUB, PK_FN
; keeps:  all but EAX, ESI
; ::::::::::::::
dc_proc:
        mov     esi,offset g_cbuf
        cmp     byte ptr [esi],M_INT    ; a line number
        jne     @F
        add     esi,5
@@:
        lodsb
        mov     ah,PK_SUB
        cmp     al,TK_SUB
        je      @dcq_n
        mov     ah,PK_FN
        cmp     al,TK_FUNCTION
        je      @dcq_n
        cmp     al,TK_END
        jne     @dcq_0
        mov     ah,PK_END
        cmp     byte ptr [esi],TK_SUB
        je      @dcq_r
        cmp     byte ptr [esi],TK_FUNCTION
        je      @dcq_r
@dcq_0:
        xor     ah,ah
        jmp     @dcq_r
@dcq_n:
        cmp     byte ptr [esi],M_VAR
        jne     @dcq_0
@dcq_r:
        mov     al,ah
        ret

if AC
; ::::::::::::::
; dc_tail - the statement a body ends in.
;
; in:     SI  the body
; out:    SI  that statement's first item, past a line number
;         CF  set: the body ends in a string, REM's or DATA's text
; keeps:  all but EAX, ESI
; ::::::::::::::
dc_tail:
        push    edx
        cmp     byte ptr [esi],M_INT
        jne     @dct_s
        add     esi,5
@dct_s:
        mov     edx,esi
@dct_l:
        mov     al,[esi]
        test    al,al
        jz      @dct_x
        cmp     al,':'
        je      @dct_c
        cmp     al,TK_REM
        je      @dct_y
        cmp     al,'"'
        je      @dct_q
        cmp     al,TK_DATA
        je      @dct_d
        call    dc_item
        jmp     @dct_l
@dct_c:
        inc     esi
        jmp     @dct_s
@dct_q:
        inc     esi                     ; a string: open at the end?
        mov     al,[esi]
        test    al,al
        jz      @dct_y
        cmp     al,'"'
        jne     @dct_q
        inc     esi
        jmp     @dct_l
@dct_d:
        xor     ah,ah                   ; DATA's text: to a ':' out of quotes
@dct_dl:
        inc     esi
        mov     al,[esi]
        test    al,al
        jz      @dct_y
        cmp     al,'"'
        jne     @F
        xor     ah,1
@@:
        test    ah,ah
        jnz     @dct_dl
        cmp     al,':'
        jne     @dct_dl
        jmp     @dct_c
@dct_y:
        stc
@dct_x:
        mov     esi,edx
        pop     edx
        ret

; ::::::::::::::
; dc_step - dc_item, a number kept as text stepped whole: digits, '.', an
; exponent and its sign, '#' or '!'.
;
; keeps:  all but ESI, EAX
; ::::::::::::::
dc_step:
        mov     al,[esi]
        cmp     al,'.'
        je      @dcs_l
        sub     al,'0'
        cmp     al,10
        jae     dc_item
@dcs_l:
        inc     esi
        mov     al,[esi]
        cmp     al,'.'
        je      @dcs_l
        cmp     al,'E'
        je      @dcs_e
        cmp     al,'D'
        je      @dcs_e
        cmp     al,'#'
        je      @dcs_t
        cmp     al,'!'
        je      @dcs_t
        sub     al,'0'
        cmp     al,10
        jb      @dcs_l
        ret
@dcs_e:
        mov     al,[esi+1]
        cmp     al,'+'
        je      @dcs_s
        cmp     al,'-'
        jne     @dcs_l
@dcs_s:
        inc     esi
        jmp     @dcs_l
@dcs_t:
        inc     esi
        ret
endif

; ::::::::::::::
; dc_line - enter the declarations of the body in cbuf.
;
; keeps:  all
; ::::::::::::::
dc_line:
        pushad
        call    dc_warn
        mov     esi,offset g_cbuf
        cmp     byte ptr [esi],M_INT    ; a line number
        jne     @dcl_s
        add     esi,5
@dcl_s:
        mov     al,[esi]
        cmp     al,TK_DIM
        je      @dcl_dim
        cmp     al,TK_REDIM
        je      @dcl_dim
        cmp     al,TK_CONST
        je      @dcl_con
        cmp     al,TK_SUB
        je      @dcl_sub
        cmp     al,TK_FUNCTION
        je      @dcl_fun
        cmp     al,TK_DECLARE
        je      @dcl_dec
        cmp     al,TK_DEF
        je      @dcl_dfn
        cmp     al,TK_SHARED
        je      @dcl_shr
        cmp     al,TK_STATIC
        je      @dcl_sta
        cmp     al,TK_END
        je      @dcl_end
        cmp     al,TK_OPTION
        je      @dcl_opt
        cmp     al,TK_TYPE
        je      @dcl_typ
        cmp     al,M_VAR
        je      @dcl_fld
        cmp     al,TK_X
        jne     @F
        cmp     byte ptr [esi+1],TK1_COMMON
        je      @dcl_cmn
@@:
        sub     al,TK_DEFINT
        cmp     al,TK_DEFSTR-TK_DEFINT
        jbe     @dcl_def
@dcl_n:
        call    dc_next
        jnz     @dcl_s
        popad
        ret

@dcl_typ:                               ; TYPE t: its fields from here
        cmp     byte ptr [esi+1],M_VAR
        jne     @dcl_n
        inc     esi
        call    dc_nam
        movzx   eax,[g_state.lx.ntyps]
        cmp     eax,NTYPE-1
        jae     err_om
        inc     eax
        mov     [g_state.lx.ntyps],ax
        mov     [g_state.lx.dtype],ax
        mov     [g_ntype+ebx],al
        mov     word ptr [g_typ+eax*8+T_NAM],bx
        mov     cx,[g_state.lx.nflds]
        mov     word ptr [g_typ+eax*8+T_FST],cx
        and     word ptr [g_typ+eax*8+T_NF],0
        and     word ptr [g_typ+eax*8+T_SIZE],0
        jmp     @dcl_n
@dcl_fld:                               ; in a TYPE: f AS type [* n]
        movzx   edx,[g_state.lx.dtype]
        test    edx,edx
        jz      @dcl_n
        call    dc_nam
        cmp     byte ptr [esi],TK_AS
        jne     @dcl_n
        movzx   eax,[g_state.lx.nflds]
        cmp     eax,NFLD
        jae     err_om
        inc     [g_state.lx.nflds]
        inc     word ptr [g_typ+edx*8+T_NF]
        lea     edi,[g_fld+eax*8]
        mov     [edi+F_NAM],bx
        xor     eax,eax
        mov     [edi+F_ET],ax
        mov     [edi+F_FIX],ax
        mov     ax,word ptr [g_typ+edx*8+T_SIZE] ; at the TYPE's end
        mov     [edi+F_OFF],ax
        movzx   eax,byte ptr [esi+1]
        cmp     al,M_VAR
        je      @dcf_t
        cmp     al,TK_X
        jne     @F
        cmp     byte ptr [esi+2],TK1_SHORT
        jne     @dcl_n
        mov     byte ptr [edi+F_ET],ET_SH
        mov     eax,2
        jmp     @dcf_z
@@:
        sub     al,TK_INTEGER
        cmp     al,TK_STRING-TK_INTEGER
        ja      @dcl_n
        movzx   ecx,[g_dast+eax]
        mov     [edi+F_ET],cl
        movzx   eax,[g_etsz+ecx]
        add     esi,2
        cmp     cl,ET_SD
        jne     @dcf_z
        xor     eax,eax
        cmp     byte ptr [esi],TK_MUL   ; STRING * n
        jne     @dcf_z
        cmp     byte ptr [esi+1],M_INT
        jne     @dcf_z
        mov     eax,[esi+2]
        mov     [edi+F_FIX],ax
        jmp     @dcf_z
@dcf_t:
        push    edx
        inc     esi                     ; another TYPE's
        call    dc_nam
        pop     edx
        movzx   eax,[g_ntype+ebx]
        mov     [edi+F_SUB],al
        mov     ax,word ptr [g_typ+eax*8+T_SIZE]
@dcf_z:                                 ; AX its size: the TYPE's grows
        add     word ptr [g_typ+edx*8+T_SIZE],ax
        jmp     @dcl_n

@dcl_opt:                               ; OPTION BASE n
        cmp     byte ptr [esi+1],TK_BASE
        jne     @dcl_n
        cmp     byte ptr [esi+2],M_INT
        jne     @dcl_n
        mov     al,[esi+3]
        mov     [g_obase],al
        add     esi,7
        jmp     @dcl_n
@dcl_dec:                               ; DECLARE SUB|FUNCTION n: its flag
        inc     esi
        mov     dl,NM_SUB
        cmp     byte ptr [esi],TK_SUB
        je      @dcp_f
        mov     dl,NM_FUNC
        cmp     byte ptr [esi],TK_FUNCTION
        jne     @dcl_n
@dcp_f:
        inc     esi
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        or      [g_nflag+ebx],dl
        jmp     @dcp_p
@dcl_dfn:                               ; DEF FNn (params): their types
        inc     esi
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        jmp     @dcp_p
@dcl_sub:                               ; SUB n, FUNCTION n: in it from here
        mov     dl,NM_SUB
        jmp     @dcs_g
@dcl_fun:
        mov     dl,NM_FUNC
@dcs_g:
        inc     esi
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        or      [g_nflag+ebx],dl
        mov     [g_state.lx.dproc],bx
@dcp_p:                                 ; ( [BYVAL] n[()] [AS type] {, ..} )
        cmp     byte ptr [esi],'('      ; as DIM's
        jne     @dcl_n
        inc     esi
@dcp_l:
        cmp     byte ptr [esi],TK_BYVAL
        jne     @F
        inc     esi
@@:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        call    @dcm_1
        cmp     byte ptr [esi],','
        jne     @dcl_n
        inc     esi
        jmp     @dcp_l
@dcl_end:                               ; END SUB, END FUNCTION: the module
        mov     al,[esi+1]
        cmp     al,TK_TYPE
        jne     @F
        and     [g_state.lx.dtype],0
        jmp     @dcl_n
@@:
        cmp     al,TK_SUB
        je      @F
        cmp     al,TK_FUNCTION
        jne     @dcl_n
@@:
        and     [g_state.lx.dproc],0
        jmp     @dcl_n
@dcl_shr:                               ; SHARED n[()] [AS t] {, ..}
        mov     dl,1
        jmp     @dch_g
@dcl_sta:                               ; STATIC n[()] [AS t] {, ..}
        mov     dl,2
@dch_g:
        inc     esi
@dch_l:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        movzx   eax,[g_state.lx.nshr]
        cmp     eax,NSHR
        jae     @dch_s
        inc     [g_state.lx.nshr]
        lea     edi,[g_shr+eax*8]
        mov     ax,[g_state.lx.dproc]
        mov     [edi],ax
        mov     [edi+2],bx
        mov     [edi+4],dl
@dch_s:
        mov     al,[esi]                ; past ( ) and AS type
        test    al,al
        jz      @dcl_n
        cmp     al,':'
        je      @dcl_n
        cmp     al,','
        je      @dch_c
        call    dc_item
        jmp     @dch_s
@dch_c:
        inc     esi
        jmp     @dch_l

@dcl_def:                               ; DEFtype l[-l] {, l[-l]}
        movzx   eax,al
        mov     dl,[g_dett+eax]
        inc     esi
@dcd_l:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        movzx   ebx,byte ptr [esi+2]
        upcase  ebx
        mov     ecx,ebx
        call    dc_item
        cmp     byte ptr [esi],TK_MINUS
        jne     @dcd_s
        inc     esi
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        movzx   ecx,byte ptr [esi+2]
        upcase  ecx
        call    dc_item
@dcd_s:
        sub     ebx,'A'
        sub     ecx,'A'
        cmp     ecx,25
        ja      @dcl_n
@dcd_f:
        cmp     ebx,ecx
        ja      @dcd_c
        mov     [g_deft+ebx],dl
        inc     ebx
        jmp     @dcd_f
@dcd_c:
        cmp     byte ptr [esi],','
        jne     @dcl_n
        inc     esi
        jmp     @dcd_l

@dcl_cmn:                               ; COMMON [SHARED] [/b/]: as DIM's
        add     esi,2
        xor     ecx,ecx
        cmp     byte ptr [esi],TK_SHARED
        jne     @F
        inc     esi
        mov     cl,NM_SHR
@@:
        cmp     byte ptr [esi],TK_DIV
        jne     @dcm_l
        cmp     byte ptr [esi+1],M_VAR
        jne     @dcl_n
        movzx   eax,byte ptr [esi+2]
        lea     esi,[esi+eax+5]
        cmp     byte ptr [esi],TK_DIV
        jne     @dcl_n
        inc     esi
        jmp     @dcm_l

@dcl_dim:                               ; DIM [SHARED] n[( )] [AS type] {, ..}
        inc     esi
        xor     ecx,ecx
        cmp     byte ptr [esi],TK_SHARED
        jne     @dcm_l
        inc     esi
        mov     cl,NM_SHR
@dcm_l:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        or      [g_nflag+ebx],cl
        push    ecx
        call    @dcm_1
        pop     ecx
        jmp     @dcm_c
@dcm_1:
        mov     edi,offset g_ntyp
        cmp     byte ptr [esi],'('
        jne     @dcm_a
        mov     edi,offset g_natyp
        xor     ecx,ecx                 ; to the ')' that closes it
@dcm_p:
        mov     al,[esi]
        test    al,al
        jz      @dcm_r
        cmp     al,'('
        jne     @F
        inc     ecx
@@:
        cmp     al,')'
        jne     @F
        dec     ecx
@@:
        call    dc_item
        test    ecx,ecx
        jnz     @dcm_p
@dcm_a:
        cmp     byte ptr [esi],TK_AS
        jne     @dcm_r
        movzx   eax,byte ptr [esi+1]
        cmp     al,M_VAR
        je      @dcm_t
        cmp     al,TK_X                 ; AS SHORT
        jne     @F
        cmp     byte ptr [esi+2],TK1_SHORT
        jne     @dcm_r
        add     esi,3
        mov     byte ptr [edi+ebx],ET_SH
        ret
@@:
        sub     al,TK_INTEGER
        cmp     al,TK_STRING-TK_INTEGER
        ja      @dcm_r
        add     esi,2
        mov     al,[g_dast+eax]
        mov     [edi+ebx],al
        cmp     byte ptr [esi],TK_MUL   ; STRING * n
        jne     @dcm_r
        inc     esi
        cmp     byte ptr [esi],M_INT
        jne     @dcm_r
        mov     eax,[esi+1]
        cmp     edi,offset g_ntyp
        jne     @F
        mov     [g_nfix+ebx*2],ax
        jmp     @dcm_x
@@:
        mov     [g_nafix+ebx*2],ax
@dcm_x:
        call    dc_item
@dcm_r:
        ret
@dcm_t:                                 ; AS a TYPE: a record, its array
        push    ebx
        inc     esi
        call    dc_nam
        movzx   eax,[g_ntype+ebx]
        pop     ebx
        mov     edx,offset g_nrec
        cmp     edi,offset g_ntyp
        je      @F
        mov     edx,offset g_narec
@@:
        mov     [edx+ebx],al
        ret
@dcm_c:
        cmp     byte ptr [esi],','
        jne     @dcl_n
        inc     esi
        jmp     @dcm_l

@dcl_con:                               ; CONST n = e {, n = e}
        inc     esi
@dcc_l:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        cmp     byte ptr [esi],TK_EQ
        jne     @dcl_n
        inc     esi
        mov     dl,ET_I4                ; the value's type: its items'
        xor     ecx,ecx
@dcc_e:
        mov     al,[esi]
        test    al,al
        jz      @dcc_t
        cmp     al,':'
        je      @dcc_t
        test    ecx,ecx
        jnz     @dcc_i
        cmp     al,','
        je      @dcc_t
@dcc_i:
        cmp     al,'('
        jne     @F
        inc     ecx
@@:
        cmp     al,')'
        jne     @F
        dec     ecx
@@:
        cmp     al,'"'
        jne     @F
        mov     dl,ET_SD
@@:
        cmp     al,'.'
        je      @dcc_f
        sub     al,'0'
        cmp     al,10
        jb      @dcc_f
        cmp     byte ptr [esi],TK_DIV
        jne     @dcc_n
@dcc_f:
        cmp     dl,ET_SD
        je      @dcc_n
        mov     dl,ET_R4                ; a point, an exponent, a /
@dcc_n:
        call    dc_item
        jmp     @dcc_e
@dcc_t:
        cmp     [g_state.lx.dproc],0
        jne     @F
        or      [g_nflag+ebx],NM_SHR    ; the module's: every procedure's
@@:
        test    ebp,ebp                 ; a suffix: its own type
        jnz     @dcc_c
        mov     [g_ntyp+ebx],dl
@dcc_c:
        cmp     byte ptr [esi],','
        jne     @dcl_n
        inc     esi
        jmp     @dcc_l

; ::::::::::::::
; dc_warn - on standard error, a warning for each DEF SEG, VARSEG, SSEG in
; cbuf: no effect, flat. Not in the editor.
; ::::::::::::::
dc_warn:
if IDE eq 0
        mov     esi,offset g_cbuf
@dcw_l:
        mov     al,[esi]
        test    al,al
        jz      @dcw_r
        cmp     al,TK_REM
        je      @dcw_r
        cmp     al,TK_DATA
        je      @dcw_r
        mov     edx,offset g_wdseg
        cmp     al,TK_DEF
        jne     @F
        cmp     word ptr [esi+1],TK_X+TK1_SEG*256
        je      @dcw_w
@@:
        mov     edx,offset g_wvseg
        cmp     al,TK_X
        jne     @dcw_n
        cmp     byte ptr [esi+1],TK1_VARSEG
        je      @dcw_w
        mov     edx,offset g_wsseg
        cmp     byte ptr [esi+1],TK1_SSEG
        jne     @dcw_n
@dcw_w:
        push    esi
        push    edx
        mov     edi,offset g_wbuf       ; "Warning: line n: " what
        mov     esi,offset g_wpre
        mov     ecx,9
        rep     movsb
        movzx   eax,[g_state.io.fline]
        test    eax,eax
        jz      @dcw_t
        mov     esi,offset g_wlin
        mov     ecx,5
        rep     movsb
        mov     ebx,10
        xor     ecx,ecx
@@:
        xor     edx,edx
        div     ebx
        add     dl,'0'
        push    edx
        inc     ecx
        test    eax,eax
        jnz     @B
@@:
        pop     eax
        stosb
        loop    @B
        mov     ax,' :'
        stosw
@dcw_t:
        pop     esi
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        dec     edi
        mov     esi,offset g_wtail
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        lea     ecx,[edi-1]
        sub     ecx,offset g_wbuf
        mov     edx,offset g_wbuf
        mov     ebx,2
        mov     ah,40h
        int     21h
        pop     esi
@dcw_n:
        call    dc_item
        jmp     @dcw_l
@dcw_r:
endif
        ret

        .data
g_wpre  db      "Warning: "
g_wlin  db      "line "
g_wdseg db      "DEF SEG",0
g_wvseg db      "VARSEG",0
g_wsseg db      "SSEG",0
g_wtail db      " has no effect in 32-bit mode",13,10,0
        .data?
g_wbuf  db      80 dup(?)
        .code

; ::::::::::::::
; dc_nam - the name at SI, stepped past.
;
; in:     SI  at M_VAR
; out:    EBX  its name
;         EBP  its suffix, 0 none
; keeps:  ECX, EDX, EDI
; ::::::::::::::
dc_nam:
        movzx   eax,byte ptr [esi+1]
        movzx   ebx,word ptr [esi+eax+2]
        movzx   ebp,byte ptr [esi+eax+1]
        cmp     ebp,'.'
        jb      dc_item                 ; % & ! # $, under '.' and the digits
        xor     ebp,ebp

; ::::::::::::::
; dc_item - step past the item at SI: a string, a number, a name, a token.
;
; keeps:  all but ESI, EAX
; ::::::::::::::
dc_item:
        movzx   eax,byte ptr [esi]
        cmp     al,'"'
        je      @dci_q
        cmp     al,M_INT
        je      @dci_i
        cmp     al,M_VAR
        je      @dci_v
        cmp     al,TK_X
        jne     @F
        inc     esi
@@:
        inc     esi
        ret
@dci_i:
        add     esi,5
        ret
@dci_v:
        movzx   eax,byte ptr [esi+1]
        lea     esi,[esi+eax+4]
        ret
@dci_q:
        inc     esi
        mov     al,[esi]
        test    al,al
        jz      @dci_r
        cmp     al,'"'
        jne     @dci_q
        inc     esi
@dci_r:
        ret

; ::::::::::::::
; dc_next - to the next statement: past a ':' out of strings; REM's and
; DATA's text are skipped.
;
; out:    ZF   clear at a statement, set at the end
;         SI   the statement
; ::::::::::::::
dc_next:
        mov     al,[esi]
        test    al,al
        jz      @dcn_r
        cmp     al,TK_REM
        je      @dcn_e
        cmp     al,':'
        je      @dcn_c
        cmp     al,TK_DATA
        je      @dcn_d
        call    dc_item
        jmp     dc_next
@dcn_c:
        inc     esi
        or      esp,esp                 ; ZF clear
@dcn_r:
        ret
@dcn_e:
        cmp     al,al                   ; ZF set
        ret
@dcn_d:
        xor     ah,ah                   ; DATA's text: to a ':' out of quotes
@dcn_dl:
        inc     esi
        mov     al,[esi]
        test    al,al
        jz      @dcn_r
        cmp     al,'"'
        jne     @F
        xor     ah,1
@@:
        test    ah,ah
        jnz     @dcn_dl
        cmp     al,':'
        jne     @dcn_dl
        jmp     @dcn_c

        .data?
g_typ   db      NTYPE*8 dup(?)          ; a TYPE: T_*
g_fld   db      NFLD*8 dup(?)           ; a field: F_*
g_ntype db      NNAM dup(?)             ; a name's TYPE, 0 none
g_nrec  db      NNAM dup(?)             ; a name's record's TYPE, its array's
g_narec db      NNAM dup(?)
g_nfix  dw      NNAM dup(?)             ; a name's STRING * n, its array's
g_nafix dw      NNAM dup(?)
g_obase db      ?                       ; OPTION BASE
g_shr   db      NSHR*8 dup(?)           ; SHARED, STATIC: proc name:2, name:2, 1|2
        .data
g_dett  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; DEFINT .. DEFSTR
g_dast  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; AS INTEGER .. AS STRING
g_etsz  db      0,4,4,4,8,0,2               ; a record field's size by ET_*
        .code

        end
