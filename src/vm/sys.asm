; ==========================================================================
; sys.asm - QBasic's system routines: the clock, the keyboard, the command
; line, the environment, RANDOMIZE
; ==========================================================================
; holds:    sys_init, sc_timer, sc_date, sc_time, sc_sdate, sc_stime,
;           sc_inkey, sc_cmd, sc_envs, sc_envset, sc_setmem, sc_erdev,
;           sc_erdevs, sc_sleep, sc_rndq, sc_rndz
; uses:     INT 21h's clock, INT 16h, the PSP, str_*, io_*
; data:     g_env: the environment, copied at the start: NAME=value 0 each,
;           a 0 after the last; ENVIRON changes the copy, as QBasic's own.
;           g_cmd: the command line after the program's file, upper case.
; ==========================================================================

include x86f.inc
        .code

ENVSZ   = 4096

; ::::::::::::::
; sys_init - the environment and the command line, from the PSP.
; ::::::::::::::
sys_init:
        assume  fs:nothing
        push    fs
        mov     fs,[g_plat.pspsel]
        mov     fs,fs:[2Ch]             ; DOS/32A: the environment's selector
        xor     esi,esi
        mov     edi,offset g_env
        mov     ecx,ENVSZ-2
@sxi_e:
        mov     al,fs:[esi]
        inc     esi
        stosb
        test    al,al
        jnz     @F
        cmp     byte ptr fs:[esi],0     ; two 0s: the end
        je      @sxi_d
@@:
        loop    @sxi_e
@sxi_d:
        mov     byte ptr [edi],0
        mov     fs,[g_plat.pspsel]      ; the tail, past the file's name
        movzx   ecx,byte ptr fs:[80h]
        mov     esi,81h
        call    @sxi_sp
@sxi_w:
        jecxz   @sxi_c
        cmp     byte ptr fs:[esi],' '
        jbe     @sxi_c
        inc     esi
        dec     ecx
        jmp     @sxi_w
@sxi_c:
        call    @sxi_sp
        mov     edi,offset g_cmd
@sxi_t:
        jecxz   @sxi_x
        mov     al,fs:[esi]
        inc     esi
        dec     ecx
        cmp     al,0Dh
        je      @sxi_x
        upcase  al
        stosb
        jmp     @sxi_t
@sxi_x:
        dec     edi                     ; no trailing blanks
        cmp     edi,offset g_cmd
        jb      @F
        cmp     byte ptr [edi],' '
        jbe     @sxi_x
@@:
        inc     edi
        sub     edi,offset g_cmd
        mov     [g_cmdn],di
        pop     fs
        ret
@sxi_sp:                                ; ESI past blanks, ECX left
        jecxz   @F
        cmp     byte ptr fs:[esi],' '
        jne     @F
        inc     esi
        dec     ecx
        jmp     @sxi_sp
@@:
        ret

; ::::::::::::::
; sx_sret - the first ECX chars of sbuf a string, pushed; the next op.
; ::::::::::::::
sx_sret:
        mov     eax,ecx
        call    str_buftmp
        dpush   eax
        jmp     ex_lp

; sx_d2 - AL as two digits at EDI. Keeps all but EAX, EDI.
sx_d2:
        aam
        add     ax,'00'
        xchg    al,ah
        stosw
        ret

; ::::::::::::::
; sc_timer - TIMER ( -- f ): the seconds since midnight, to 1/100.
; ::::::::::::::
sc_timer:
        mov     ah,2Ch
        int     21h
        movzx   eax,ch
        imul    eax,60
        movzx   ebx,cl
        add     eax,ebx
        imul    eax,60
        movzx   ebx,dh
        add     eax,ebx
        imul    eax,100
        movzx   ebx,dl
        add     eax,ebx
        sub     esp,CELL
        mov     [esp],eax
        fild    dword ptr [esp]
        fdiv    [g_k100]
        fstp    dword ptr [esp]
        jmp     ex_lp

; ::::::::::::::
; sc_date - DATE$ ( -- s ): mm-dd-yyyy.
; ::::::::::::::
sc_date:
        mov     ah,2Ah
        int     21h
        mov     edi,offset g_sbuf
        mov     al,dh
        call    sx_d2
        mov     al,'-'
        stosb
        mov     al,dl
        call    sx_d2
        mov     al,'-'
        stosb
        mov     ax,cx
        mov     bl,100
        div     bl
        push    eax
        call    sx_d2
        pop     eax
        mov     al,ah
        call    sx_d2
        mov     ecx,10
        jmp     sx_sret

; ::::::::::::::
; sc_time - TIME$ ( -- s ): hh:mm:ss.
; ::::::::::::::
sc_time:
        mov     ah,2Ch
        int     21h
        mov     edi,offset g_sbuf
        mov     al,ch
        call    sx_d2
        mov     al,':'
        stosb
        mov     al,cl
        call    sx_d2
        mov     al,':'
        stosb
        mov     al,dh
        call    sx_d2
        mov     ecx,8
        jmp     sx_sret

; ::::::::::::::
; sx_parts - the string value at the stack's top as up to 3 numbers apart by
; any one of the 2 chars in DX.
;
; out:    g_part  the numbers, FFFFh none
;         ECX     how many
; keeps:  ESI
; error:  ?ILLEGAL QUANTITY  another char, a number past 9999
; ::::::::::::::
sx_parts:
        push    esi
        mov     eax,[esp+8]
        call    str_get
        movzx   ecx,cx
        mov     esi,[g_state.sg.spt]
        mov     dword ptr [g_part],-1
        mov     word ptr [g_part+4],-1
        xor     ebx,ebx                 ; EBX the part
        xor     edi,edi                 ; EDI digits in it
@sxp_l:
        jecxz   @sxp_e
        lodsb
        dec     ecx
        cmp     al,dl
        je      @sxp_s
        cmp     al,dh
        je      @sxp_s
        sub     al,'0'
        cmp     al,9
        ja      err_iq
        cmp     word ptr [g_part+ebx*2],-1
        jne     @F
        and     word ptr [g_part+ebx*2],0
@@:
        movzx   eax,al
        push    eax
        movzx   eax,word ptr [g_part+ebx*2]
        imul    eax,10
        add     eax,[esp]
        add     esp,4
        cmp     eax,9999
        ja      err_iq
        mov     [g_part+ebx*2],ax
        inc     edi
        jmp     @sxp_l
@sxp_s:
        test    edi,edi
        jz      err_iq
        xor     edi,edi
        inc     ebx
        cmp     ebx,3
        jae     err_iq
        jmp     @sxp_l
@sxp_e:
        test    edi,edi
        jz      err_iq
        lea     ecx,[ebx+1]
        pop     esi
        ret

; ::::::::::::::
; sc_sdate - DATE$ = s ( s -- ): mm-dd-yy or mm-dd-yyyy, - or /.
; ::::::::::::::
sc_sdate:
        mov     dx,'-/'
        call    sx_parts
        cmp     ecx,3
        jne     err_iq
        movzx   ecx,word ptr [g_part+4]
        cmp     ecx,100
        jae     @F
        add     ecx,1900
        cmp     ecx,1980
        jae     @F
        add     ecx,100
@@:
        mov     dh,byte ptr [g_part]
        mov     dl,byte ptr [g_part+2]
        mov     ah,2Bh
        int     21h
        test    al,al
        jnz     err_iq
        add     esp,CELL
        jmp     ex_lp

; ::::::::::::::
; sc_stime - TIME$ = s ( s -- ): hh[:mm[:ss]].
; ::::::::::::::
sc_stime:
        mov     dx,'::'
        call    sx_parts
        xor     eax,eax
        cmp     word ptr [g_part+2],-1
        jne     @F
        mov     word ptr [g_part+2],ax
@@:
        cmp     word ptr [g_part+4],-1
        jne     @F
        mov     word ptr [g_part+4],ax
@@:
        mov     ch,byte ptr [g_part]
        mov     cl,byte ptr [g_part+2]
        mov     dh,byte ptr [g_part+4]
        xor     dl,dl
        mov     ah,2Dh
        int     21h
        test    al,al
        jnz     err_iq
        add     esp,CELL
        jmp     ex_lp

; ::::::::::::::
; sc_inkey - INKEY$ ( -- s ): a key waiting, else empty; an extended key
; is CHR$(0) and its scan code.
; ::::::::::::::
sc_inkey:
        call    plt_flush
        mov     edi,offset g_sbuf
        xor     ecx,ecx
        mov     ah,11h
        int     16h
        jz      sx_sret
        mov     ah,10h
        int     16h
        inc     ecx
        test    al,al
        jz      @F
        cmp     al,0E0h
        jne     @sik_c
        test    ah,ah
        jz      @sik_c
@@:
        xor     al,al
        stosb
        inc     ecx
        mov     al,ah
@sik_c:
        stosb
        jmp     sx_sret

; ::::::::::::::
; sc_cmd - COMMAND$ ( -- s ).
; ::::::::::::::
sc_cmd:
        push    esi
        mov     esi,offset g_cmd
        mov     edi,offset g_sbuf
        movzx   ecx,[g_cmdn]
        push    ecx
        rep     movsb
        pop     ecx
        pop     esi
        jmp     sx_sret

; ::::::::::::::
; sx_find - the entry named as the CX chars at EBX: EDI it, CF clear; CF
; set: none, EDI at the last 0. Upper and lower case are one.
; ::::::::::::::
sx_find:
        mov     edi,offset g_env
@sxf_l:
        cmp     byte ptr [edi],0
        je      @sxf_n
        push    ecx
        xor     edx,edx
@sxf_c:
        cmp     edx,ecx
        jae     @sxf_e
        mov     al,[edi+edx]
        upcase  al
        mov     ah,[ebx+edx]
        upcase  ah
        cmp     al,ah
        jne     @sxf_x
        inc     edx
        jmp     @sxf_c
@sxf_e:
        cmp     byte ptr [edi+edx],'='
        jne     @sxf_x
        pop     ecx
        clc
        ret
@sxf_x:
        pop     ecx
        xor     al,al                   ; to the next
        push    ecx
        mov     ecx,-1
        repne   scasb
        pop     ecx
        jmp     @sxf_l
@sxf_n:
        stc
        ret

; ::::::::::::::
; sc_envs - ENVIRON$ ( x flag -- s ): flag 1: x a name, its value; 0: x the
; entry, from 1, whole.
;
; error:  ?ILLEGAL QUANTITY  an entry under 1
; ::::::::::::::
sc_envs:
        dpop    eax
        test    eax,eax
        jz      @sev_n
        dpop    eax
        call    str_get
        mov     ebx,[g_state.sg.spt]
        movzx   ecx,cx
        jecxz   @sev_z
        call    sx_find
        jc      @sev_z
        movzx   ecx,cx
        lea     edi,[edi+ecx+1]         ; past NAME=
        jmp     @sev_c
@sev_n:
        dpop    eax
        cmp     eax,1
        jl      err_iq
        mov     edi,offset g_env
@sev_s:
        cmp     byte ptr [edi],0
        je      @sev_z
        dec     eax
        jz      @sev_c
        push    eax
        xor     al,al
        mov     ecx,-1
        repne   scasb
        pop     eax
        jmp     @sev_s
@sev_c:                                 ; the 0-ended text at EDI
        push    esi
        mov     esi,edi
        mov     edi,offset g_sbuf
        xor     ecx,ecx
@@:
        lodsb
        test    al,al
        jz      @F
        stosb
        inc     ecx
        cmp     ecx,255
        jb      @B
@@:
        pop     esi
        jmp     sx_sret
@sev_z:
        xor     ecx,ecx
        jmp     sx_sret

; ::::::::::::::
; sc_envset - ENVIRON s ( s -- ): NAME=value set, replaced, or with no
; value, removed.
;
; error:  ?ILLEGAL QUANTITY  no '=' or no name
;         ?OUT OF MEMORY     no room
; ::::::::::::::
sc_envset:
        dpop    eax
        call    str_get
        mov     ebx,[g_state.sg.spt]
        push    esi
        mov     esi,ebx                 ; ESI the text, ECX its length
        movzx   ecx,cx
        xor     edx,edx                 ; EDX the name's length
@ses_q:
        cmp     edx,ecx
        jae     @ses_e
        cmp     byte ptr [esi+edx],'='
        je      @F
        inc     edx
        jmp     @ses_q
@@:
        test    edx,edx
        jz      @ses_e
        push    ecx
        mov     ecx,edx
        call    sx_find
        pop     ecx
        jc      @ses_a
        push    ecx                     ; the old one out
        push    esi
        mov     esi,edi
@@:
        lodsb
        test    al,al
        jnz     @B
@@:
        lodsb
        stosb
        test    al,al
        jnz     @B
        cmp     byte ptr [esi],0        ; the end: two 0s
        jne     @B
        mov     byte ptr [edi],0
        pop     esi
        pop     ecx
        mov     edi,offset g_env        ; EDI to the last 0
@@:
        cmp     byte ptr [edi],0
        je      @ses_a
        push    ecx
        xor     al,al
        mov     ecx,-1
        repne   scasb
        pop     ecx
        jmp     @B
@ses_a:                                 ; EDI the end's 0: appended
        lea     eax,[edx+1]
        cmp     eax,ecx
        jae     @ses_r                  ; no value: removed
        lea     eax,[edi+ecx+2]
        cmp     eax,offset g_env+ENVSZ
        ja      @ses_m
        rep     movsb
        xor     al,al
        stosb
        stosb
@ses_r:
        pop     esi
        jmp     ex_lp
@ses_m:
        pop     esi
        jmp     err_om
@ses_e:
        pop     esi
        jmp     err_iq

; ::::::::::::::
; sc_setmem - SETMEM ( n -- n ): the free bytes; the heap is fixed.
; ::::::::::::::
sc_setmem:
        add     esp,CELL
        call    str_gc
        mov     eax,[g_state.mm.sfr]
        sub     eax,[g_state.mm.aend]
        dpush   eax
        jmp     ex_lp

; ::::::::::::::
; sc_erdev, sc_erdevs - ERDEV, ERDEV$: no device error is kept.
; ::::::::::::::
sc_erdev:
        xor     eax,eax
        dpush   eax
        jmp     ex_lp
sc_erdevs:
        xor     ecx,ecx
        jmp     sx_sret

; ::::::::::::::
; sc_sleep - SLEEP ( n -- ): n seconds or a key, which stays; 0 a key.
; ::::::::::::::
sc_sleep:
        call    plt_flush
        dpop    eax
        imul    eax,eax,182             ; ticks * 10
        mov     ebx,10
        cdq
        idiv    ebx
        mov     ebx,eax                 ; EBX the ticks, 0 none
        mov     edx,ds:[46Ch]
@sxl_l:
        cmp     [g_state.rn.stpflg],0
        jne     ex_lp
        mov     ah,11h
        int     16h
        jnz     ex_lp
        test    ebx,ebx
        jz      @sxl_l
        mov     eax,ds:[46Ch]
        sub     eax,edx
        cmp     eax,ebx
        jb      @sxl_l
        jmp     ex_lp

; ::::::::::::::
; sc_rndq - RANDOMIZE alone ( -- d ): the seed asked for.
; ::::::::::::::
sc_rndq:
        push    esi
@srq_a:
        mov     ebx,offset u_seed
        call    io_puts
        call    io_read
        mov     esi,offset g_inbuf
        call    dbl_in
        cmp     byte ptr [esi],0
        jne     @srq_r                  ; QBasic's ?Redo from start
        pop     esi
        sub     esp,CELL
        fld     [g_dval]
        fstp    qword ptr [esp]
        jmp     ex_lp
@srq_r:
        mov     ebx,offset u_redo
        call    io_puts
        jmp     @srq_a

; ::::::::::::::
; sc_rndz - RANDOMIZE ( d -- ): the DOUBLE's two high words, xor-ed, the
; seed's middle word (QBasic's B$RNZP).
; ::::::::::::::
sc_rndz:
        mov     ax,[esp+4]
        xor     ax,[esp+6]
        add     esp,CELL
        mov     ecx,[g_state.ft.rs]
        and     ecx,0FF0000FFh
        movzx   eax,ax
        shl     eax,8
        or      eax,ecx
        mov     [g_state.ft.rs],eax
        jmp     ex_lp

        .data
g_k100  dd      100.0
u_seed  db      'Random-number seed (-32768 to 32767)? ',0
u_redo  db      'Redo from start',NL,0
        .data?
g_env   db      ENVSZ dup(?)
g_cmd   db      128 dup(?)
g_cmdn  dw      ?
g_part  dw      3 dup(?)
        .code

        end
