; ==========================================================================
; block.asm - the block statements paired across lines, before a run
; ==========================================================================
; holds:    cm_blocks, bk_find
; uses:     dc_item, err_sn, err_om
; data:     A position is a line (its offset in progb) and a statement's
;           offset in its body. cm_blocks walks every statement in program
;           order, a stack of the open blocks, and gives each block statement
;           an entry in g_blk, in that order (bk_find's binary search):
;             B_POS   its position, line:2 boff:1, then BK_* its kind
;             B_NXT   where its false test, or its loop, goes: a target
;             B_END   where it leaves the block: a target
;             B_LINK  the next entry+1 of a chain: IF's clauses, a loop's EXITs
;             B_AUX   CASE: its SELECT's entry; EXIT: the block's BK_*
;           A target is a position and a mode: 0 the statement, 1 past the
;           JMP it starts with, 2 the statement after it. A jump op holds one
;           and the address it resolves to (vm/flow.asm, ex_btgt).
; calls:
;   cm_blocks          each line
;     bk_line          each statement, and those after THEN and ELSE
;       bk_stmt        a block statement's entry, the stack
; ==========================================================================

include x86f.inc
        .code

; a stack entry: the block's kind, its opener's entry, its last clause, its
; chain's head+1
S_KIND  = 0
S_OPEN  = 2
S_LAST  = 4
S_CHAIN = 6
SSIZE   = 8
NBST    = 64

; ::::::::::::::
; cm_blocks - pair the program's block statements into g_blk.
;
; out:    nblk  the entries
; error:  ?SYNTAX ERROR  a block left open, a closer with no opener; ln its
;                        line
;         ?OUT OF MEMORY NBLK entries
; ::::::::::::::
cm_blocks:
        pushad
        and     [g_state.lnk.nblk],0
        and     [g_state.lnk.bsp],0
        mov     ebx,PROGBASE
@cbk_l:
        movzx   ecx,byte ptr [g_progb+ebx]
        jecxz   @cbk_d
        mov     [g_state.lnk.bln],bx
        lea     esi,[g_progb+ebx+HDR]
        push    ebx
        call    bk_line
        pop     ebx
        movzx   ecx,byte ptr [g_progb+ebx]
        add     ebx,ecx
        jmp     @cbk_l
@cbk_d:
        movzx   eax,[g_state.lnk.bsp]
        test    eax,eax
        jz      @cbk_r
        movzx   eax,word ptr [g_bst+eax*SSIZE-SSIZE+S_OPEN]
        shl     eax,4
        mov     ax,word ptr [g_blk+eax]
        mov     [g_state.rn.ln],ax
        jmp     err_sn
@cbk_r:
        popad
        ret

; ::::::::::::::
; bk_line - the statements of the body at SI.
;
; in:     SI   the body
;         bln  its line
; ::::::::::::::
bk_line:
        mov     ebp,esi
        cmp     byte ptr [esi],M_VAR    ; a label: its statement after the ':'
        jne     @bkl_s
        movzx   eax,byte ptr [esi+1]
        cmp     byte ptr [esi+eax+4],':'
        jne     @bkl_s
        lea     esi,[esi+eax+5]
@bkl_s:
        mov     al,[esi]
        cmp     al,':'
        jne     @F
        inc     esi
        jmp     @bkl_s
@@:
        test    al,al
        jz      @bkl_r
        call    bk_stmt
@bkl_i:
        mov     al,[esi]
        test    al,al
        jz      @bkl_r
        cmp     al,TK_REM
        je      @bkl_r
        cmp     al,':'
        je      @bkl_c
        cmp     al,TK_ELSE              ; a one-line IF's ELSE: a statement after
        je      @bkl_c
        cmp     al,TK_THEN
        je      @bkl_c
        cmp     al,TK_DATA
        je      @bkl_d
        call    dc_item
        jmp     @bkl_i
@bkl_c:
        inc     esi
        jmp     @bkl_s
@bkl_r:
        ret
@bkl_d:
        xor     ah,ah                   ; DATA's text: to a ':' out of quotes
@bkl_dl:
        inc     esi
        mov     al,[esi]
        test    al,al
        jz      @bkl_r
        cmp     al,'"'
        jne     @F
        xor     ah,1
@@:
        test    ah,ah
        jnz     @bkl_dl
        cmp     al,':'
        jne     @bkl_dl
        jmp     @bkl_i

; ::::::::::::::
; bk_stmt - the statement at SI: a block statement's entry and the stack.
;
; in:     SI   the statement
;         BP   its body
;         bln  its line
; keeps:  ESI, EBP
; ::::::::::::::
bk_stmt:
        mov     ecx,esi
        sub     ecx,ebp                 ; CL its offset
        mov     al,[esi]
        cmp     al,TK_IF
        je      @bks_if
        cmp     al,TK_ELSEIF
        je      @bks_ei
        cmp     al,TK_ELSE
        je      @bks_el
        cmp     al,TK_ENDIF
        je      @bks_ef
        cmp     al,TK_SELECT
        je      @bks_sl
        cmp     al,TK_CASE
        je      @bks_cs
        cmp     al,TK_DO
        je      @bks_do
        cmp     al,TK_LOOP
        je      @bks_lp
        cmp     al,TK_WHILE
        je      @bks_wh
        cmp     al,TK_WEND
        je      @bks_wn
        cmp     al,TK_FOR
        je      @bks_fr
        cmp     al,TK_NEXT
        je      @bks_nx
        cmp     al,TK_EXIT
        je      @bks_ex
        cmp     al,TK_END
        jne     @bks_r
        mov     al,[esi+1]
        cmp     al,TK_IF
        je      @bks_ef
        cmp     al,TK_SELECT
        je      @bks_es
@bks_r:
        ret

@bks_if:                                ; IF: a block when THEN ends the line
        push    esi
@bif_l:
        mov     al,[esi]
        test    al,al
        jz      @bif_n
        cmp     al,TK_THEN
        je      @bif_t
        call    dc_item
        jmp     @bif_l
@bif_t:
        mov     al,[esi+1]
        test    al,al
        jz      @bif_b
        cmp     al,TK_REM
        je      @bif_b
@bif_n:
        pop     esi
        ret
@bif_b:
        pop     esi
        mov     al,BK_IF
        call    bk_new
        mov     dl,BK_IF
        jmp     bk_push

@bks_ei:                                ; ELSEIF: the clause before goes here
        mov     dl,BK_IF
        call    bk_top
        mov     al,BK_ELSEIF
        call    bk_new
        mov     dl,1
        call    bk_clause
        ret

@bks_el:                                ; ELSE at a statement's start: a block's
        mov     dl,BK_IF
        call    bk_top
        mov     al,BK_ELSE
        call    bk_new
        mov     dl,1
        jmp     bk_clause

@bks_ef:                                ; END IF
        mov     dl,BK_IF
        call    bk_top
        mov     al,BK_ENDIF
        call    bk_new
        jmp     bk_close

@bks_sl:                                ; SELECT CASE: its depth, for its value
        mov     al,BK_SELECT
        call    bk_new
        movzx   ebx,[g_state.lnk.bsp]
@bsl_l:
        test    ebx,ebx
        jz      @bsl_d
        dec     ebx
        cmp     [g_bst+ebx*SSIZE+S_KIND],BK_SELECT
        jne     @bsl_l
        inc     byte ptr [edi+B_AUX]
        jmp     @bsl_l
@bsl_d:
        mov     dl,BK_SELECT
        jmp     bk_push

@bks_cs:                                ; CASE: SELECT's or the case before's
        mov     dl,BK_SELECT            ; test goes here
        call    bk_top
        mov     al,BK_CASE
        call    bk_new
        mov     dx,[ebx+S_OPEN]
        mov     [edi+B_AUX],dx
        mov     dl,1
        jmp     bk_clause

@bks_es:                                ; END SELECT
        mov     dl,BK_SELECT
        call    bk_top
        mov     al,BK_ENDSEL
        call    bk_new
        movzx   edx,word ptr [ebx+S_OPEN]
        shl     edx,4
        call    bk_here0
        mov     dword ptr [g_blk+edx+B_END],eax
        jmp     bk_close

@bks_do:
        mov     al,BK_DO
        call    bk_new
        mov     dl,BK_DO
        jmp     bk_push
@bks_wh:
        mov     al,BK_WHILE
        call    bk_new
        mov     dl,BK_WHILE
        jmp     bk_push

@bks_lp:                                ; LOOP, WEND: back to the opener, which
        mov     dl,BK_DO                ; leaves past it, as its EXITs do
        mov     al,BK_LOOP
        jmp     @bks_lw
@bks_wn:
        mov     dl,BK_WHILE
        mov     al,BK_WEND
@bks_lw:
        call    bk_top
        call    bk_new
        movzx   edx,word ptr [ebx+S_OPEN]
        shl     edx,4
        mov     eax,dword ptr [g_blk+edx+B_POS]
        and     eax,0FFFFFFh            ; mode 0
        mov     [edi+B_NXT],eax
        call    bk_here2
        mov     dword ptr [g_blk+edx+B_END],eax
        jmp     bk_exits

@bks_fr:                                ; FOR: past its NEXT when not entered
        mov     al,BK_FOR
        call    bk_new
        mov     dl,BK_FOR
        jmp     bk_push

@bks_nx:                                ; NEXT a, b: a FOR closed for each
        mov     edx,1
        push    esi
@bnx_l:
        mov     al,[esi]
        test    al,al
        jz      @bnx_e
        cmp     al,':'
        je      @bnx_e
        cmp     al,TK_ELSE
        je      @bnx_e
        cmp     al,TK_REM
        je      @bnx_e
        cmp     al,','
        jne     @F
        inc     edx
@@:
        call    dc_item
        jmp     @bnx_l
@bnx_e:
        pop     esi
@bnx_p:
        push    edx
        mov     dl,BK_FOR
        call    bk_top
        call    bk_here2
        movzx   edx,word ptr [ebx+S_OPEN]
        shl     edx,4
        mov     dword ptr [g_blk+edx+B_END],eax
        call    bk_exits
        pop     edx
        dec     edx
        jnz     @bnx_p
        ret

@bks_ex:                                ; EXIT FOR, EXIT DO: on the chain of
        mov     dl,BK_FOR               ; the innermost such block
        cmp     byte ptr [esi+1],TK_FOR
        je      @F
        mov     dl,BK_DO
        cmp     byte ptr [esi+1],TK_DO
        jne     @bks_r                  ; EXIT SUB ..: not a block's
@@:
        movzx   ebx,[g_state.lnk.bsp]
@bex_l:
        test    ebx,ebx
        jz      @bk_er
        dec     ebx
        cmp     [g_bst+ebx*SSIZE+S_KIND],dl
        jne     @bex_l
        lea     ebx,[g_bst+ebx*SSIZE]
        push    edx
        mov     al,BK_EXIT
        call    bk_new
        pop     edx
        mov     [edi+B_AUX],dl
        mov     dx,[ebx+S_CHAIN]
        mov     [edi+B_LINK],dx
        inc     eax
        mov     [ebx+S_CHAIN],ax
        ret

@bk_er:
        mov     ax,[g_state.lnk.bln]
        mov     [g_state.rn.ln],ax
        jmp     err_sn

; ::::::::::::::
; bk_new - a new entry here, of kind AL.
;
; in:     CL   the statement's offset
; out:    EAX  its index
;         EDI  it
; keeps:  EBX, ECX, ESI, EBP
; error:  ?OUT OF MEMORY  NBLK entries
; ::::::::::::::
bk_new:
        movzx   edi,[g_state.lnk.nblk]
        cmp     edi,NBLK
        jae     err_om
        inc     [g_state.lnk.nblk]
        push    edi
        shl     edi,4
        add     edi,offset g_blk
        push    eax
        mov     ax,[g_state.lnk.bln]
        mov     [edi+B_POS],ax
        mov     [edi+B_POS+2],cl
        pop     eax
        mov     [edi+B_POS+3],al
        xor     eax,eax
        mov     [edi+B_NXT],eax
        mov     [edi+B_END],eax
        mov     [edi+B_LINK],eax
        pop     eax
        ret

; ::::::::::::::
; bk_push - open a block of kind DL, its opener entry EAX.
;
; error:  ?SYNTAX ERROR  NBST deep
; ::::::::::::::
bk_push:
        movzx   ebx,[g_state.lnk.bsp]
        cmp     ebx,NBST
        jae     @bk_er
        inc     [g_state.lnk.bsp]
        lea     ebx,[g_bst+ebx*SSIZE]
        mov     [ebx+S_KIND],dl
        mov     [ebx+S_OPEN],ax
        mov     [ebx+S_LAST],ax
        and     word ptr [ebx+S_CHAIN],0
        ret

; ::::::::::::::
; bk_top - the innermost block, which must be of kind DL.
;
; out:    EBX  its stack entry
; keeps:  EAX, ECX, ESI, EDI, EBP
; error:  ?SYNTAX ERROR  none, or another kind
; ::::::::::::::
bk_top:
        movzx   ebx,[g_state.lnk.bsp]
        test    ebx,ebx
        jz      @bk_er
        lea     ebx,[g_bst+ebx*SSIZE-SSIZE]
        cmp     [ebx+S_KIND],dl
        jne     @bk_er
        ret

; ::::::::::::::
; bk_clause - a clause EAX (EDI) of the block at EBX: the last clause's
; false test comes here, mode DL; it joins the chain that END closes.
; ::::::::::::::
bk_clause:
        test    word ptr [ebx+S_LAST],8000h
        jnz     @bk_er                  ; a clause after ELSE
        push    eax
        movzx   eax,word ptr [ebx+S_LAST]
        shl     eax,4
        push    edi
        lea     edi,[g_blk+eax+B_NXT]
        call    bk_here
        pop     edi
        pop     eax
        mov     [ebx+S_LAST],ax
        cmp     byte ptr [edi+B_POS+3],BK_ELSE
        jne     @F
        or      word ptr [ebx+S_LAST],8000h     ; after ELSE: no false test
@@:
        mov     dx,[ebx+S_CHAIN]
        mov     [edi+B_LINK],dx
        inc     eax
        mov     [ebx+S_CHAIN],ax
        ret

; ::::::::::::::
; bk_close - END IF, END SELECT here: the last clause's false test comes
; here, every clause leaves here; the block is closed.
;
; in:     EBX  its stack entry
; ::::::::::::::
bk_close:
        movzx   eax,word ptr [ebx+S_LAST]
        test    ah,80h
        jnz     @bkc_c
        shl     eax,4
        lea     edi,[g_blk+eax+B_NXT]
        mov     dl,0
        call    bk_here
@bkc_c:
        call    bk_here0
        movzx   edx,word ptr [ebx+S_CHAIN]
@bkc_l:
        test    edx,edx
        jz      @bkc_p
        dec     edx
        shl     edx,4
        mov     dword ptr [g_blk+edx+B_END],eax
        movzx   edx,word ptr [g_blk+edx+B_LINK]
        jmp     @bkc_l
@bkc_p:
        dec     [g_state.lnk.bsp]
        ret

; ::::::::::::::
; bk_exits - the block at EBX closes: its EXITs go to target EAX.
; ::::::::::::::
bk_exits:
        movzx   edx,word ptr [ebx+S_CHAIN]
        jmp     @bkc_l

; ::::::::::::::
; bk_here - the target here, mode DL, to [EDI].
;
; in:     CL  the statement's offset
; keeps:  all
; ::::::::::::::
bk_here:
        push    eax
        mov     al,dl
        call    bk_herem
        mov     [edi],eax
        pop     eax
        ret

; ::::::::::::::
; bk_here0, bk_here2 - EAX the target here, mode 0 or 2.
;
; in:     CL  the statement's offset
; keeps:  all but EAX
; ::::::::::::::
bk_here0:
        xor     al,al
        jmp     bk_herem
bk_here2:
        mov     al,2
bk_herem:
        movzx   eax,al
        shl     eax,8
        mov     al,cl
        shl     eax,16
        mov     ax,[g_state.lnk.bln]    ; line:2 boff:1 mode:1
        ret

; ::::::::::::::
; bk_find - the entry at line BX, offset CL.
;
; out:    EAX  its index; CF set: none
; keeps:  all but EAX
; ::::::::::::::
bk_find:
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        movzx   ebx,bx
        shl     ebx,8
        movzx   ecx,cl
        or      ebx,ecx                 ; the key
        xor     ecx,ecx                 ; [ECX, EDX)
        movzx   edx,[g_state.lnk.nblk]
@bkf_l:
        cmp     ecx,edx
        jae     @bkf_n
        lea     eax,[ecx+edx]
        shr     eax,1
        mov     edi,eax
        shl     edi,4
        movzx   esi,word ptr [g_blk+edi+B_POS]
        shl     esi,8
        movzx   edi,byte ptr [g_blk+edi+B_POS+2]
        or      esi,edi
        cmp     esi,ebx
        je      @bkf_y
        jb      @F
        mov     edx,eax
        jmp     @bkf_l
@@:
        lea     ecx,[eax+1]
        jmp     @bkf_l
@bkf_n:
        stc
        jmp     @bkf_r
@bkf_y:
        clc
@bkf_r:
        pop     edi
        pop     esi
        pop     edx
        pop     ecx
        pop     ebx
        ret

        .data?
g_blk   db      NBLK*16 dup(?)
g_bst   db      NBST*SSIZE dup(?)
        .code

        end
