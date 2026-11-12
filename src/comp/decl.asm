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
;                          parameters' AS types as DIM's
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
        cmp     al,TK_SUB
        je      @dcl_sub
        cmp     al,TK_FUNCTION
        je      @dcl_fun
        cmp     al,TK_DECLARE
        je      @dcl_dec
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
        movzx   eax,byte ptr [esi+1]
        cmp     al,M_VAR
        je      @dcf_t
        sub     al,TK_INTEGER
        cmp     al,TK_STRING-TK_INTEGER
        ja      @dcl_n
        mov     al,[g_dast+eax]
        mov     [edi+F_ET],al
        add     esi,2
        cmp     byte ptr [esi],TK_MUL
        jne     @dcl_n
        cmp     byte ptr [esi+1],M_INT
        jne     @dcl_n
        mov     eax,[esi+2]
        mov     [edi+F_FIX],ax
        jmp     @dcl_n
@dcf_t:
        inc     esi                     ; another TYPE's
        call    dc_nam
        mov     al,[g_ntype+ebx]
        mov     [edi+F_SUB],al
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
@dcm_t:                                 ; AS a TYPE: each field a name n.f
        push    ebx
        inc     esi
        call    dc_nam
        movzx   eax,[g_ntype+ebx]
        pop     ebx
        test    eax,eax
        jz      @dcm_r
        mov     edx,offset g_nrec
        cmp     edi,offset g_ntyp
        je      @F
        mov     edx,offset g_narec
@@:
        mov     [edx+ebx],al
        push    esi
        push    edi
        call    rc_leaves
        pop     edi
        pop     esi
        mov     dl,[g_nflag+ebx]
        and     dl,NM_SHR
        xor     ecx,ecx
@dct_l:
        cmp     ecx,[g_rln]
        jae     @dcm_r
        movzx   eax,word ptr [g_rlid+ecx*2]
        or      [g_nflag+eax],dl
        mov     ebp,[g_rlfp+ecx*4]
        push    edx
        mov     dl,[ebp+F_ET]
        mov     [edi+eax],dl
        mov     dx,[ebp+F_FIX]
        cmp     edi,offset g_ntyp
        jne     @F
        mov     [g_nfix+eax*2],dx
        jmp     @dct_n
@@:
        mov     [g_nafix+eax*2],dx
@dct_n:
        pop     edx
        inc     ecx
        jmp     @dct_l
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
; rc_leaves - the names n.f of each field of TYPE EAX for name EBX, the
; fields of a field's TYPE in turn: rlid the names, rlfp the fields.
;
; out:    rln  how many
; keeps:  EBX
; error:  ?OUT OF MEMORY  from nam_find
; ::::::::::::::
rc_leaves:
        and     [g_rln],0
        push    ebx
        mov     esi,[g_nname+ebx*4]
        movzx   ecx,byte ptr [esi]
        inc     esi
        mov     edi,offset g_rnam
        mov     [g_rnl],ecx
        rep     movsb
        call    rc_walk
        pop     ebx
        ret

; ::::::::::::::
; rc_walk - rc_leaves under the text rnam, rnl long, of TYPE EAX.
; ::::::::::::::
rc_walk:
        movzx   esi,word ptr [g_typ+eax*8+T_FST]
        movzx   ecx,word ptr [g_typ+eax*8+T_NF]
@rcw_l:
        test    ecx,ecx
        jz      @rcw_r
        push    ecx
        push    esi
        push    [g_rnl]
        mov     edi,[g_rnl]
        cmp     edi,72
        jae     err_om
        add     edi,offset g_rnam
        mov     al,'.'
        stosb
        movzx   eax,word ptr [g_fld+esi*8+F_NAM]
        push    esi
        mov     esi,[g_nname+eax*4]
        movzx   ecx,byte ptr [esi]
        inc     esi
        lea     eax,[ecx+1]
        add     [g_rnl],eax
        rep     movsb
        pop     esi
        movzx   eax,byte ptr [g_fld+esi*8+F_SUB]
        test    eax,eax
        jz      @rcw_f
        call    rc_walk
        jmp     @rcw_n
@rcw_f:
        mov     ecx,[g_rln]
        cmp     ecx,NRLF
        jae     err_om
        lea     eax,[g_fld+esi*8]
        mov     [g_rlfp+ecx*4],eax
        mov     [g_state.lx.nptr],offset g_rnam
        mov     eax,[g_rnl]
        mov     [g_state.lx.nlen],al
        call    nam_find
        mov     ecx,[g_rln]
        mov     [g_rlid+ecx*2],ax
        inc     [g_rln]
@rcw_n:
        pop     [g_rnl]
        pop     esi
        pop     ecx
        inc     esi
        dec     ecx
        jmp     @rcw_l
@rcw_r:
        ret

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
g_rnam  db      80 dup(?)               ; rc_leaves: a name n.f
g_rnl   dd      ?
g_rln   dd      ?
g_rlid  dw      NRLF dup(?)
g_rlfp  dd      NRLF dup(?)
g_nfix  dw      NNAM dup(?)             ; a name's STRING * n, its array's
g_nafix dw      NNAM dup(?)
g_obase db      ?                       ; OPTION BASE
g_shr   db      NSHR*8 dup(?)           ; SHARED, STATIC: proc name:2, name:2, 1|2
        .data
g_dett  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; DEFINT .. DEFSTR
g_dast  db      ET_I2,ET_I4,ET_R4,ET_R8,ET_SD   ; AS INTEGER .. AS STRING
        .code

        end
