; ==========================================================================
; chan.asm - channels: the files OPEN makes, by number, and where output
; and input go
; ==========================================================================
; holds:    ch_init, ch_look, ch_find, ch_open, ch_close, ch_clall, ch_out, ch_in,
;           ch_clr, ch_putc, ch_getb, ch_flush, ch_seek, ch_lof, ch_rw
; uses:     INT 21h's handles, plt_putc (the console)
; data:     NCH channels, g_ch: channel 0 is the console, always open; the
;           rest free (cnum 0) or file cnum's. A channel's kind (CK_*) says
;           where its bytes go: the console, a DOS handle, nowhere. Its
;           buffer holds the file's bytes from cpos: read ahead (cbn of
;           them, cbp read) or written (cbn, cdirty). och, ich: the
;           channels output and input use now, 0 the console; io's pcol is
;           och's column, kept in ccol while another is used.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; ch_init - the console the only channel.
; ::::::::::::::
ch_init:
        mov     edi,offset g_ch
        mov     ecx,NCH*(size TCHAN)
        xor     al,al
        rep     stosb
        mov     byte ptr [g_ch+TCHAN.ckind],CK_CON
        mov     byte ptr [g_ch+TCHAN.cmode],FM_OUT+FM_IN
        mov     [g_och],0
        mov     [g_ich],0
        ret

; ch_ptr - EDI = channel EBX's TCHAN. Keeps all but EDI.
ch_ptr:
        imul    edi,ebx,size TCHAN
        add     edi,offset g_ch
        ret

; ::::::::::::::
; ch_look - the channel of file number EAX, if open.
;
; out:    CF   set: none
;         EBX  its index; EDI its TCHAN
; ::::::::::::::
ch_look:
        dec     eax
        cmp     eax,254
        ja      @chf_n
        inc     eax
        mov     ebx,NCH-1
@chf_l:
        call    ch_ptr
        cmp     al,[edi].TCHAN.cnum
        je      @chf_r
        dec     ebx
        jnz     @chf_l
@chf_n:
        stc
        ret
@chf_r:
        clc
        ret

; ::::::::::::::
; ch_find - ch_look, or the error.
;
; error:  52 Bad file name or number  not open, or out of 1..255
; ::::::::::::::
ch_find:
        call    ch_look
        jc      ch_e52
        ret
ch_e52:
        mov     al,52
        jmp     ch_err

; ::::::::::::::
; ch_err - QBasic's error AL, as ERROR AL raises it.
; ::::::::::::::
ch_err:
        movzx   eax,al
        mov     [g_state.rn.uerr],ax
        mov     al,E_USR
        jmp     err_raise

; ch_doserr - a DOS error AX as QBasic's: raised.
ch_doserr:
        mov     ecx,offset g_dose
@chd_l:
        cmp     byte ptr [ecx],0
        je      @chd_x
        cmp     al,[ecx]
        je      @chd_f
        add     ecx,2
        jmp     @chd_l
@chd_f:
        mov     al,[ecx+1]
        jmp     ch_err
@chd_x:
        mov     al,75                   ; Path/File access error
        jmp     ch_err

; ::::::::::::::
; ch_open - open file number EAX on the name at ESI (0-ended), mode DL
; (FM_*), records of ECX bytes.
;
; out:    EBX, EDI  the channel
; error:  52, 55 already open, 67 none free, 59 a bad length; DOS's
; ::::::::::::::
ch_open:
        dec     eax
        cmp     eax,254
        ja      ch_e52
        inc     eax
        push    ecx
        push    eax
        mov     ebx,NCH-1               ; open already: 55; EBX a free one
        xor     ecx,ecx
@cho_l:
        call    ch_ptr
        cmp     al,[edi].TCHAN.cnum
        mov     ah,55
        je      @cho_e
        cmp     [edi].TCHAN.cnum,0
        jne     @F
        mov     ecx,ebx
@@:
        dec     ebx
        jnz     @cho_l
        mov     ah,67
        test    ecx,ecx
        jz      @cho_e
        mov     ebx,ecx
        call    ch_ptr
        mov     ecx,[esp+4]
        dec     ecx
        cmp     ecx,RECMAX
        mov     ah,59
        jae     @cho_e
        inc     ecx
        push    edi                     ; a fresh channel
        push    ecx
        mov     ecx,size TCHAN
        xor     al,al
        rep     stosb
        pop     ecx
        pop     edi
        mov     [edi].TCHAN.creclen,cx
        mov     [edi].TCHAN.cmode,dl
        mov     [edi].TCHAN.ckind,CK_DISK
        call    ch_dev                  ; a device's name: no DOS file
        jnc     @cho_k
        mov     edx,esi
        mov     al,[edi].TCHAN.cmode
        test    al,FM_OUT
        jnz     @cho_c                  ; OUTPUT: created
        mov     ax,3D00h                ; INPUT: read
        test    [edi].TCHAN.cmode,FM_IN
        jnz     @F
        mov     ax,3D02h                ; the rest: read and write
@@:
        int     21h
        jnc     @cho_h
        cmp     ax,2                    ; not found: INPUT's error, the
        jne     @cho_d                  ; rest create it
        test    [edi].TCHAN.cmode,FM_IN
        jnz     @cho_d
@cho_c:
        mov     ah,3Ch
        xor     ecx,ecx
        int     21h
        jc      @cho_d
        test    [edi].TCHAN.cmode,FM_OUT
        jnz     @cho_h
        mov     ebx,eax                 ; reopened to read and write
        mov     ah,3Eh
        int     21h
        mov     ax,3D02h
        int     21h
        jc      @cho_d
@cho_h:
        mov     [edi].TCHAN.chnd,ax
        test    [edi].TCHAN.cmode,FM_APP
        jz      @cho_k
        movzx   ebx,ax                  ; APPEND: at the end
        mov     ax,4202h
        xor     ecx,ecx
        xor     edx,edx
        int     21h
        shl     edx,16
        mov     dx,ax
        mov     [edi].TCHAN.cpos,edx
@cho_k:
        pop     eax
        mov     [edi].TCHAN.cnum,al
        mov     eax,edi                 ; EBX the index again
        sub     eax,offset g_ch
        xor     edx,edx
        mov     ecx,size TCHAN
        div     ecx
        mov     ebx,eax
        pop     ecx
        ret
@cho_d:
        add     esp,8
        jmp     ch_doserr
@cho_e:
        add     esp,8
        mov     al,ah
        jmp     ch_err

; ::::::::::::::
; ch_dev - the name at ESI a device: SCRN: CONS: the console, NUL; its kind
; in the channel at EDI.
;
; out:    CF  set: not one, a DOS file
; keeps:  ESI, EDI
; ::::::::::::::
ch_dev:
        push    esi
        mov     ebx,offset g_devs
@chv_l:
        mov     cl,[ebx]
        test    cl,cl
        jz      @chv_n
        push    esi
        inc     ebx
@chv_c:
        mov     al,[esi]
        upcase  al
        cmp     al,[ebx]
        jne     @chv_x
        inc     esi
        inc     ebx
        cmp     byte ptr [ebx],0
        jne     @chv_c
        cmp     byte ptr [esi],0        ; the whole name
        pop     esi
        jne     @chv_s
        mov     [edi].TCHAN.ckind,cl
        pop     esi
        clc
        ret
@chv_x:
        pop     esi
@chv_s:
        inc     ebx                     ; to the next
        cmp     byte ptr [ebx-1],0
        jne     @chv_s
        jmp     @chv_l
@chv_n:
        pop     esi
        stc
        ret

; ::::::::::::::
; ch_close - close channel EBX (EDI); the console stays.
; ::::::::::::::
ch_close:
        test    ebx,ebx
        jz      @chc_r
        call    ch_flush
        cmp     [edi].TCHAN.ckind,CK_DISK
        jne     @F
        push    ebx
        movzx   ebx,[edi].TCHAN.chnd
        mov     ah,3Eh
        int     21h
        pop     ebx
@@:
        mov     [edi].TCHAN.cnum,0
        cmp     [g_och],bl
        jne     @F
        call    ch_clr
@@:
@chc_r:
        ret

; ::::::::::::::
; ch_clall - close every file: CLOSE alone, RESET, END, RUN.
; ::::::::::::::
ch_clall:
        call    ch_clr
        mov     ebx,NCH-1
@cca_l:
        call    ch_ptr
        cmp     [edi].TCHAN.cnum,0
        je      @F
        push    ebx
        call    ch_close
        pop     ebx
@@:
        dec     ebx
        jnz     @cca_l
        ret

; ::::::::::::::
; ch_out - output to channel EBX, its column io's.
;
; error:  54 Bad file mode  not one written to
; ::::::::::::::
ch_out:
        call    ch_ptr
        test    [edi].TCHAN.cmode,FM_OUT+FM_APP+FM_RND+FM_BIN
        mov     al,54
        jz      ch_err
        call    ch_swcol
        mov     [g_och],bl
        mov     al,[edi].TCHAN.ccol
        mov     [g_state.io.pcol],al
        ret

; ch_swcol - io's column to och's ccol. Keeps all.
ch_swcol:
        push    eax
        push    edi
        push    ebx
        movzx   ebx,[g_och]
        call    ch_ptr
        mov     al,[g_state.io.pcol]
        mov     [edi].TCHAN.ccol,al
        pop     ebx
        pop     edi
        pop     eax
        ret

; ::::::::::::::
; ch_in - input from channel EBX.
;
; error:  54 Bad file mode  not one read
; ::::::::::::::
ch_in:
        call    ch_ptr
        test    [edi].TCHAN.cmode,FM_IN+FM_RND+FM_BIN
        mov     al,54
        jz      ch_err
        mov     [g_ich],bl
        mov     [g_state.io.cin],bl
        ret

; ::::::::::::::
; ch_clr - output and input the console's again.
;
; keeps:  all
; ::::::::::::::
ch_clr:
        call    ch_swcol
        push    eax
        push    edi
        mov     [g_och],0
        mov     [g_ich],0
        mov     [g_state.io.cin],0FFh
        and     [g_state.io.infl],7Fh
        mov     al,byte ptr [g_ch+TCHAN.ccol]
        mov     [g_state.io.pcol],al
        pop     edi
        pop     eax
        ret

; ::::::::::::::
; ch_putc - AL to the output channel, och; NL is CR LF.
;
; keeps:  all
; ::::::::::::::
ch_putc:
        pushad
        movzx   ebx,[g_och]
        call    ch_ptr
        cmp     [edi].TCHAN.ckind,CK_DISK
        jne     @chp_o
        cmp     al,NL
        jne     @F
        mov     al,13
        call    ch_wb
        mov     al,10
@@:
        call    ch_wb
        popad
        ret
@chp_o:
        cmp     [edi].TCHAN.ckind,CK_CON
        jne     @F
        call    plt_putc
@@:
        popad
        ret

; ch_wb - byte AL into channel EDI's buffer, written when full.
ch_wb:
        cmp     [edi].TCHAN.cdirty,0    ; read ahead: dropped, the place kept
        jne     @F
        call    ch_drop
        mov     [edi].TCHAN.cdirty,1
@@:
        movzx   ecx,[edi].TCHAN.cbn
        mov     [edi+ecx].TCHAN.cbuf,al
        inc     ecx
        mov     [edi].TCHAN.cbn,cx
        cmp     ecx,CBSZ
        jb      @F
        call    ch_flush
@@:
        ret

; ::::::::::::::
; ch_flush - channel EDI's written bytes to its file.
;
; keeps:  all
; error:  61 Disk full, DOS's
; ::::::::::::::
ch_flush:
        cmp     [edi].TCHAN.cdirty,0
        je      @chl_r
        pushad
        call    ch_at
        movzx   ecx,[edi].TCHAN.cbn
        lea     edx,[edi].TCHAN.cbuf
        movzx   ebx,[edi].TCHAN.chnd
        mov     ah,40h
        int     21h
        jc      ch_doserr
        cmp     ax,cx
        mov     al,61
        jne     ch_err
        add     [edi].TCHAN.cpos,ecx
        xor     eax,eax
        mov     [edi].TCHAN.cbn,ax
        mov     [edi].TCHAN.cdirty,al
        popad
@chl_r:
        ret

; ch_drop - channel EDI's read-ahead forgotten, cpos where reading was.
; Keeps all.
ch_drop:
        push    eax
        movzx   eax,[edi].TCHAN.cbp
        add     [edi].TCHAN.cpos,eax
        xor     eax,eax
        mov     [edi].TCHAN.cbn,ax
        mov     [edi].TCHAN.cbp,ax
        pop     eax
        ret

; ch_at - channel EDI's handle at cpos. Keeps all.
ch_at:
        pushad
        movzx   ebx,[edi].TCHAN.chnd
        mov     edx,[edi].TCHAN.cpos
        mov     ecx,edx
        shr     ecx,16
        mov     ax,4200h
        int     21h
        popad
        ret

; ::::::::::::::
; ch_getb - the next byte of the input channel, ich.
;
; out:    AL  the byte
;         CF  set at the end, or a ^Z (QBasic's end of a text file)
; keeps:  all but EAX
; ::::::::::::::
ch_getb:
        push    edi
        push    ebx
        movzx   ebx,[g_ich]
        call    ch_ptr
        call    ch_peek
        jc      @F
        inc     word ptr [edi].TCHAN.cbp
@@:
        pop     ebx
        pop     edi
        ret

; ch_skiplf - past the input channel's next byte if it is LF. Keeps all.
ch_skiplf:
        push    eax
        push    edi
        push    ebx
        movzx   ebx,[g_ich]
        call    ch_ptr
        call    ch_peek
        jc      @F
        cmp     al,10
        jne     @F
        inc     word ptr [edi].TCHAN.cbp
@@:
        pop     ebx
        pop     edi
        pop     eax
        ret

; ::::::::::::::
; ch_peek - channel EDI's next byte, not read.
;
; out:    AL  it
;         CF  set: none, the end, or ^Z in a sequential file
; keeps:  all but EAX
; ::::::::::::::
ch_peek:
        cmp     [edi].TCHAN.ckind,CK_DISK
        jne     @chk_e
        call    ch_flush
        movzx   eax,[edi].TCHAN.cbp
        cmp     ax,[edi].TCHAN.cbn
        jb      @chk_b
        call    ch_drop                 ; the next block
        pushad
        call    ch_at
        mov     ecx,CBSZ
        lea     edx,[edi].TCHAN.cbuf
        movzx   ebx,[edi].TCHAN.chnd
        mov     ah,3Fh
        int     21h
        jc      ch_doserr
        mov     [edi].TCHAN.cbn,ax
        popad
        xor     eax,eax
        cmp     ax,[edi].TCHAN.cbn
        je      @chk_e
@chk_b:
        mov     al,[edi+eax].TCHAN.cbuf
        cmp     al,1Ah
        jne     @F
        test    [edi].TCHAN.cmode,FM_IN
        jnz     @chk_e
@@:
        clc
        ret
@chk_e:
        stc
        ret

; ::::::::::::::
; ch_tell - channel EDI's place: the next byte's offset.
;
; out:    EAX  it
; keeps:  all but EAX
; ::::::::::::::
ch_tell:
        mov     eax,[edi].TCHAN.cpos
        cmp     [edi].TCHAN.cdirty,0
        jne     @F
        push    ecx
        movzx   ecx,[edi].TCHAN.cbp
        add     eax,ecx
        pop     ecx
        ret
@@:
        push    ecx
        movzx   ecx,[edi].TCHAN.cbn
        add     eax,ecx
        pop     ecx
        ret

; ::::::::::::::
; ch_seek - channel EDI to offset EAX.
;
; keeps:  all
; ::::::::::::::
ch_seek:
        call    ch_flush
        push    eax
        xor     eax,eax
        mov     [edi].TCHAN.cbn,ax
        mov     [edi].TCHAN.cbp,ax
        pop     eax
        mov     [edi].TCHAN.cpos,eax
        ret

; ::::::::::::::
; ch_lof - channel EDI's file's length.
;
; out:    EAX  it
; keeps:  all but EAX
; ::::::::::::::
ch_lof:
        xor     eax,eax
        cmp     [edi].TCHAN.ckind,CK_DISK
        jne     @F
        call    ch_flush
        pushad
        movzx   ebx,[edi].TCHAN.chnd
        mov     ax,4202h
        xor     ecx,ecx
        xor     edx,edx
        int     21h
        shl     edx,16
        mov     dx,ax
        mov     [esp+28],edx
        popad
@@:
        ret

; ::::::::::::::
; ch_rw - ECX bytes at EDX read from (AH 3Fh) or written to (40h) channel
; EDI at its place, which moves past them.
;
; out:    EAX  how many were read
; error:  DOS's
; ::::::::::::::
ch_rw:
        cmp     [edi].TCHAN.ckind,CK_DISK
        jne     @crw_n
        push    eax
        call    ch_tell
        call    ch_seek
        call    ch_at
        pop     eax
        push    ebx
        movzx   ebx,[edi].TCHAN.chnd
        int     21h
        pop     ebx
        jc      ch_doserr
        movzx   eax,ax
        add     [edi].TCHAN.cpos,eax
        ret
@crw_n:
        xor     eax,eax
        ret

        .data
; DOS's errors, QBasic's numbers: not found, no path, too many, denied,
; the handle, the drive
g_dose  db      2,53, 3,76, 4,67, 5,70, 6,52, 15,68, 0
; a device's kind, its name with its ':'
g_devs  db      CK_CON,'SCRN:',0, CK_CON,'CONS:',0, CK_NUL,'NUL:',0, 0
        .data?
g_ch    db      NCH*(size TCHAN) dup(?)
g_och   db      ?
g_ich   db      ?
        .code

        end
