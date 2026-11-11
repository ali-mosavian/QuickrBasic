; ==========================================================================
; list.asm - LIST, SAVE and VERIFY: a stored body back to text
; ==========================================================================
; holds:    ls_all, ls_range, ls_line
; uses:     mem_line, io_kw, io_num, io_putc
; data:     A line lists as typed: keywords from their tokens, a name from its
;           M_VAR, a whole number from its M_INT; strings, REM and DATA
;           verbatim.
; calls:
;   ls_all             ls_range
;     ls_line          a line at a time
; ==========================================================================

include x86f.inc
        .code

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
        cmp     byte ptr [g_progb+ebx],0
        je      @ls_e
        mov     ax,word ptr [g_progb+ebx+1]
        cmp     ax,[g_state.rn.lfrom]
        jb      @ls_sk
        cmp     ax,[g_state.rn.lto]
        ja      @ls_e
        push    ebx
        call    ls_line
        pop     ebx
@ls_sk:
        movzx   ecx,byte ptr [g_progb+ebx]
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
        mov     ax,word ptr [g_progb+ebx+1]
        call    io_num
        call    io_space
        pop     ebx
        mov     edi,offset g_dlb
        call    mem_line
        mov     esi,offset g_dlb+HDR
if IDE                                 ; the editor lists a body alone
        call    ls_body
        pop     esi
        jmp     io_nl

; ::::::::::::::
; ls_body - print the token body at SI.
; ::::::::::::::
ls_body:
endif
        mov     [g_state.io.lq],0
@ls_c:
        lodsb
        test    al,al
        jz      @ls_end
        cmp     al,'"'
        je      @ls_qt
        cmp     [g_state.io.lq],0
        je      @ls_tok
        cmp     al,':'
        jne     @ls_o
        cmp     [g_state.io.lq],2
        jne     @ls_o
        mov     [g_state.io.lq],0
        jmp     @ls_o
@ls_tok:
        cmp     al,M_INT
        jne     @ls_t1
        lodsw
        call    io_num
        jmp     @ls_c
@ls_t1:
        cmp     al,M_VAR
        jne     @ls_t2
        lodsb
        movzx   ecx,al
@@:
        lodsb
        call    io_putc
        loop    @B
        inc     esi
        jmp     @ls_c
@ls_t2:
        mov     cl,80h
        cmp     al,TK_REM
        je      @ls_rw
        mov     cl,2
        cmp     al,TK_DATA
        jne     @ls_t3
@ls_rw:
        mov     [g_state.io.lq],cl
@ls_t3:
        test    al,al
        jns     @ls_o
        cmp     al,TK_PI
        je      @ls_o
        call    io_kw
        jmp     @ls_c
@ls_qt:
        xor     [g_state.io.lq],1
        mov     al,'"'
@ls_o:
        call    io_putc
        jmp     @ls_c
@ls_end:
if IDE
        ret
else
        pop     esi
        jmp     io_nl
endif

        .data?
g_dlb   db      256 dup(?)
        .code

        end
