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
;           A type is ET_*; 0 in ntyp, natyp is none declared. DEFtype holds
;           for the whole program, not from its line on.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; dc_line - enter the declarations of the body in cbuf.
;
; keeps:  all
; ::::::::::::::
dc_line:
        pushad
        mov     esi,offset g_cbuf
        cmp     byte ptr [esi],M_INT    ; a line number
        jne     @dcl_s
        add     esi,5
@dcl_s:
        mov     al,[esi]
        cmp     al,TK_DIM
        je      @dcl_dim
        cmp     al,TK_CONST
        je      @dcl_con
        sub     al,TK_DEFINT
        cmp     al,TK_DEFSTR-TK_DEFINT
        jbe     @dcl_def
@dcl_n:
        call    dc_next
        jnz     @dcl_s
        popad
        ret

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

@dcl_dim:                               ; DIM [SHARED] n[( )] [AS type] {, ..}
        inc     esi
        cmp     byte ptr [esi],TK_SHARED
        jne     @dcm_l
        inc     esi
@dcm_l:
        cmp     byte ptr [esi],M_VAR
        jne     @dcl_n
        call    dc_nam
        mov     edi,offset g_ntyp
        cmp     byte ptr [esi],'('
        jne     @dcm_a
        mov     edi,offset g_natyp
        xor     ecx,ecx                 ; to the ')' that closes it
@dcm_p:
        mov     al,[esi]
        test    al,al
        jz      @dcl_n
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
        jne     @dcm_c
        movzx   eax,byte ptr [esi+1]
        sub     al,TK_INTEGER
        cmp     al,TK_STRING-TK_INTEGER
        ja      @dcl_n
        add     esi,2
        mov     al,[g_dast+eax]
        mov     [edi+ebx],al
        cmp     byte ptr [esi],TK_MUL   ; STRING * n
        jne     @dcm_c
        inc     esi
        call    dc_item
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
        test    ebp,ebp                 ; a suffix: its own type
        jnz     @dcc_c
        mov     [g_ntyp+ebx],dl
@dcc_c:
        cmp     byte ptr [esi],','
        jne     @dcl_n
        inc     esi
        jmp     @dcc_l

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

        .data
g_dett  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; DEFINT .. DEFSTR
g_dast  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; AS INTEGER .. AS STRING
        .code

        end
