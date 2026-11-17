; ==========================================================================
; native.asm - ops as machine code in the stream
; ==========================================================================
; holds:    cm_native; cm_ds, nt_* the pieces
; uses:     DI the stream, SI at the op in xbuf, BL the op
; data:     The simple ops are written out in place, no handler: loads and
;           stores of variables and constants, INT and FLOAT operators,
;           compares, IF, conversions. Their routines (flt_*, int_div) are
;           called, their errors jumped to. The data stack is SP as in the
;           handlers; BP aims at the top cell where they used it.
; calls:
;   cm_native          one op, or CF set for none
;     cm_ds            the bytes after its call
;     cm_nisrc, cm_nfsrc the right operand into AX or EAX
; ==========================================================================

; ::::::::::::::
; cm_ds - copy the bytes after the call to the stream: a count, then them.
;
; in:     DI  the stream
; out:    DI  past them
; keeps:  all but DI, FLAGS
; ::::::::::::::
cm_ds:
        push    bx
        push    cx
        push    si
        mov     bx,sp
        mov     si,[bx+6]
        movzx   cx,byte ptr cs:[si]     ; the bytes are in the code
        inc     si
        rep     movs byte ptr es:[di],byte ptr cs:[si]
        mov     [bx+6],si
        pop     si
        pop     cx
        pop     bx
        ret

; ::::::::::::::
; cm_nvar - the slot of the variable id at [SI+1].
;
; out:    AX  its address
; ::::::::::::::
cm_nvar:
        movzx   ax,byte ptr [si+1]
        shl     ax,2
        add     ax,VARBASE
        ret

; ::::::::::::::
; cm_nrel - store the relative target, a routine, of a word at DI.
;
; in:     AX  the target
; ::::::::::::::
cm_nrel:
        sub     ax,di
        sub     ax,2+CSOFF              ; DI in DS, the target in CS
        stosw
        ret

; ::::::::::::::
; cm_ncall - E8 and the target.
;
; in:     AX  the routine
; ::::::::::::::
cm_ncall:
        mov     dx,ax
        mov     al,0E8h
        stosb
        mov     ax,dx
        jmp     cm_nrel

; ::::::::::::::
; cm_njo - JO to the ?OVERFLOW.
; ::::::::::::::
cm_njo:
        call    cm_ds
        db      2,0Fh,80h
        mov     ax,offset err_ov
        jmp     cm_nrel

; ::::::::::::::
; cm_nisrc - an INT right operand into AX.
;
; in:     AL  0 popped, 1 a variable, 2 the U8
; ::::::::::::::
cm_nisrc:
        test    al,al
        jnz     @nis_1
        call    cm_ds
        db      2,66h,58h
        ret
@nis_1:
        cmp     al,1
        jne     @nis_2
        mov     al,0A1h
        stosb
        call    cm_nvar
        stosw
        ret
@nis_2:
        mov     al,0B8h
        stosb
        movzx   ax,byte ptr [si+1]
        stosw
        ret

; ::::::::::::::
; cm_nfsrc - a FLOAT right operand into EAX.
;
; in:     AL  0 popped, 1 a variable, 2 an INT variable, 3 the U8, 4 the F32
; ::::::::::::::
cm_nfsrc:
        cmp     al,1
        ja      @nfs_2
        jb      cm_nisrc
        call    cm_ds
        db      2,66h,0A1h
        call    cm_nvar
        stosw
        ret
@nfs_2:
        cmp     al,4
        jne     @nfs_3
        call    cm_ds
        db      2,66h,0B8h
        mov     eax,[si+1]
        stosd
        ret
@nfs_3:
        cmp     al,3
        je      cm_nfk
        dec     al
        dec     al
        call    cm_nisi
        mov     ax,offset flt_from16
        jmp     cm_ncall

; ::::::::::::::
; cm_nfk - EAX = the U8 at [SI+1] as a FLOAT, worked out now.
; ::::::::::::::
cm_nfk:
        push    si
        push    di
        movzx   ax,byte ptr [si+1]
        call    flt_from16
        pop     di
        pop     si
        mov     edx,eax
        call    cm_ds
        db      2,66h,0B8h
        mov     eax,edx
        stosd
        ret

; ::::::::::::::
; cm_nisi - AL 0 an INT variable, 1 the U8, into AX.
; ::::::::::::::
cm_nisi:
        inc     al
        jmp     cm_nisrc

; ::::::::::::::
; cm_native - the op as machine code, if it has a template.
;
; in:     BL  the op
;         SI  at it in xbuf
;         CX  its size
;         DI  the stream
; out:    DI  past the code
;         CF  set: no template, nothing written
; keeps:  BX, CX, SI
; ::::::::::::::
cm_native:
        push    bx
        push    cx
        mov     [ntsz],cl
        call    cm_nop
        pop     cx
        pop     bx
        ret

; ::::::::::::::
; cm_nop - cm_native's work.
; ::::::::::::::
cm_nop:
        cmp     bl,O_I_LD_V
        je      @nt_pv
        cmp     bl,O_F_LD_V
        je      @nt_pv
        cmp     bl,O_I_LD_I16
        je      @nt_i16
        cmp     bl,O_I_LD_U8
        je      @nt_u8
        cmp     bl,O_F_LD_F32
        je      @nt_f32
        cmp     bl,O_S_LD_V
        je      @nt_ps
        cmp     bl,O_F_LD_U8
        je      @nt_fu8
        cmp     bl,O_F_LD_VI
        je      @nt_fvi
        cmp     bl,O_F_LD_PI
        je      @nt_pi2
        cmp     bl,O_I_NEG
        je      @nt_ineg
        cmp     bl,O_I_NOT
        je      @nt_inot
        cmp     bl,O_F_NEG
        je      @nt_fneg
        cmp     bl,O_I_ST_V
        je      @nt_sti
        cmp     bl,O_F_ST_V
        je      @nt_stf
        cmp     bl,O_I_IF
        je      @nt_iif
        cmp     bl,O_F_IF
        je      @nt_fif
        cmp     bl,O_DROP
        je      @nt_drop
        cmp     bl,O_NEXT_V
        je      cm_nnext
        mov     dl,0
        cmp     bl,O_I_LD_A1
        je      cm_narr
        mov     dl,1
        cmp     bl,O_F_LD_A1
        je      cm_narr
        mov     dl,2
        cmp     bl,O_I_ST_A1
        je      cm_narr
        mov     dl,3
        cmp     bl,O_F_ST_A1
        je      cm_narr
        mov     al,bl
        sub     al,O_I_ADD
        cmp     al,O_I_OR_U8-O_I_ADD+1
        jb      cm_nibin
        mov     al,bl
        sub     al,O_F_ADD
        cmp     al,O_F_DIV_F32-O_F_ADD+1
        jb      cm_nfbin
        mov     al,bl
        sub     al,O_I_CMP
        mov     dl,0
        cmp     al,O_I_CMP_U8-O_I_CMP+1
        jb      cm_nicmp
        mov     al,bl
        sub     al,O_I_CMPIF
        mov     dl,1
        cmp     al,O_I_CMPIF_U8-O_I_CMPIF+1
        jb      cm_nicmp
        mov     al,bl
        sub     al,O_F_CMP
        mov     dl,0
        cmp     al,O_F_CMP_F32-O_F_CMP+1
        jb      cm_nfcmp
        mov     al,bl
        sub     al,O_F_CMPIF
        mov     dl,1
        cmp     al,O_F_CMPIF_F32-O_F_CMPIF+1
        jb      cm_nfcmp
        cmp     bl,O_I_TOF
        jb      @nt_no
        cmp     bl,O_I_TOA
        jbe     cm_nconv
@nt_no:
        stc
        ret
@nt_pv:
        call    cm_ds
        db      3,66h,0FFh,36h
        call    cm_nvar
        jmp     @nt_w
@nt_i16:
        mov     ax,[si+1]
        jmp     @nt_pi
@nt_u8:
        movzx   ax,byte ptr [si+1]
@nt_pi:
        mov     dx,ax
        call    cm_ds
        db      2,66h,68h
        mov     ax,dx
        stosw
        xor     ax,ax
        jmp     @nt_w
@nt_fu8:
        mov     al,3
        call    cm_nfk
        call    cm_ds
        db      2,66h,50h
        clc
        ret
@nt_fvi:
        mov     al,0A1h
        stosb
        call    cm_nvar
        stosw
        mov     ax,offset flt_from16
        jmp     @nt_fp
@nt_pi2:
        mov     ax,offset flt_pi
@nt_fp:
        call    cm_ncall
        call    cm_ds
        db      2,66h,50h
        clc
        ret
@nt_ineg:
        call    cm_ds
        db      5,89h,0E5h,0F7h,5Eh,0
        call    cm_njo
        clc
        ret
@nt_inot:
        call    cm_ds
        db      5,89h,0E5h,0F7h,56h,0
        clc
        ret
@nt_fneg:
        call    cm_ds
        db      16,89h,0E5h,66h,0F7h,46h,0,0,0,80h,7Fh,74h,4,80h,76h,3,80h
        clc
        ret
@nt_f32:
        call    cm_ds
        db      2,66h,68h
        mov     eax,[si+1]
        stosd
        clc
        ret
@nt_ps:
        call    cm_ds
        db      2,66h,68h
        movzx   eax,byte ptr [si+1]
        shl     eax,3
        add     eax,SVBASE
        add     eax,[dslin]
        stosd
        clc
        ret
@nt_sti:
        call    cm_ds
        db      3,66h,58h,0A3h
        call    cm_nvar
        jmp     @nt_w
@nt_stf:
        call    cm_ds
        db      3,66h,8Fh,06h
        call    cm_nvar
@nt_w:
        stosw
        clc
        ret
@nt_iif:
        call    cm_ds
        db      6,66h,58h,85h,0C0h,0Fh,84h
        jmp     @nt_nl
@nt_fif:
        call    cm_ds
        db      10,66h,58h,66h,0A9h,0,0,80h,7Fh,0Fh,84h
@nt_nl:
        mov     ax,offset ex_nextln
        call    cm_nrel
        clc
        ret
@nt_drop:
        call    cm_ds
        db      3,83h,0C4h,4
        clc
        ret

; ::::::::::::::
; cm_nibin - an INT operator: the right in AX, the left the top cell.
;
; in:     AL  the op - I_ADD: the source * 6 + the operator
; ::::::::::::::
cm_nibin:
        mov     ah,0
        mov     dl,6
        div     dl
        mov     dh,ah
        call    cm_nisrc
        call    cm_ds
        db      2,89h,0E5h
        movzx   bx,dh
        cmp     bl,3
        je      @nib_d
        cmp     bl,2
        je      @nib_m
        mov     al,[ntib+bx]
        stosb
        call    cm_ds
        db      2,46h,0
        cmp     bl,4
        jae     @nib_x
        call    cm_njo
@nib_x:
        clc
        ret
@nib_m:
        call    cm_ds
        db      4,0Fh,0AFh,46h,0
        call    cm_njo
        jmp     @nib_s
@nib_d:
        mov     ax,offset int_div
        call    cm_ncall
@nib_s:
        call    cm_ds
        db      3,89h,46h,0
        clc
        ret
ntib    db      01h,29h,0,0,21h,09h

; ::::::::::::::
; cm_nfbin - a FLOAT operator: the right in EAX, the left popped to EDX.
;
; in:     AL  the op - F_ADD: the source * 4 + the operator
; ::::::::::::::
cm_nfbin:
        shr     al,2
        mov     ah,bl
        sub     ah,O_F_ADD
        and     ah,3
        mov     dh,ah
        push    dx
        call    cm_nfsrc
        call    cm_ds
        db      2,66h,5Ah
        pop     dx
        call    cm_ds
        db      2,0FFh,16h
        movzx   ax,dh
        add     ax,ax
        add     ax,offset bflt
        stosw
        call    cm_ds
        db      2,66h,50h
        clc
        ret

; ::::::::::::::
; cm_nicmp - an INT compare or CMPIF: the right in AX, the left popped to DX.
; The value is -1 or 0; a CMPIF's false goes to ex_nextln.
;
; in:     AL  the source
;         DL  1 a CMPIF
; error:  CF set  the mask is 0 or 7: not here
; ::::::::::::::
cm_nicmp:
        movzx   bx,[ntsz]
        mov     ah,[si+bx-1]
        dec     ah
        cmp     ah,5
        ja      @nic_n
        movzx   bx,ah
        mov     ah,[ntcc+bx]
        push    ax
        push    dx
        call    cm_nisrc
        call    cm_ds
        db      2,66h,5Ah
        pop     dx
        pop     ax
        test    dl,dl
        jnz     @nic_i
        push    ax
        call    cm_ds
        db      3,66h,31h,0C9h
        call    cm_ds
        db      2,39h,0C2h
        pop     ax
        mov     al,0Fh
        mov     dl,ah
        or      dl,90h
        mov     ah,dl
        stosw
        mov     al,0C1h
        stosb
        call    cm_ds
        db      5,66h,0F7h,0D9h,66h,51h
        clc
        ret
@nic_i:
        push    ax
        call    cm_ds
        db      2,39h,0C2h
        pop     ax
        xor     ah,1
        or      ah,80h
        mov     al,0Fh
        stosw
        mov     ax,offset ex_nextln
        call    cm_nrel
        clc
        ret
@nic_n:
        stc
        ret
ntcc    db      0Fh,4,0Dh,0Ch,5,0Eh

; ::::::::::::::
; cm_nfcmp - a FLOAT compare or CMPIF: flt_cmp, then the mask's bit for its
; result, 0 below, 1 equal, 2 above.
;
; in:     AL  the source, as cm_nfsrc
;         DL  1 a CMPIF
; ::::::::::::::
cm_nfcmp:
        push    dx
        movzx   bx,[ntsz]
        mov     ah,[si+bx-1]
        push    ax
        call    cm_nfsrc
        call    cm_ds
        db      2,66h,5Ah
        mov     ax,offset flt_cmp
        call    cm_ncall
        call    cm_ds
        db      5,0FEh,0C0h,88h,0C1h,0B0h
        pop     ax
        mov     al,ah
        stosb
        call    cm_ds
        db      4,0D2h,0E8h,24h,1
        pop     dx
        test    dl,dl
        jnz     @nfc_i
        call    cm_ds
        db      9,66h,0Fh,0B6h,0C0h,66h,0F7h,0D8h,66h,50h
        clc
        ret
@nfc_i:
        call    cm_ds
        db      2,0Fh,84h
        mov     ax,offset ex_nextln
        call    cm_nrel
        clc
        ret

; ::::::::::::::
; cm_nconv - a conversion of the top cell.
;
; in:     BL  I_TOF .. I_TOA
; ::::::::::::::
cm_nconv:
        cmp     bl,O_I_TOB
        jae     @ncv_b
        call    cm_ds
        db      2,66h,58h
        mov     ax,offset flt_from16
        cmp     bl,O_I_TOF
        je      @ncv_c
        mov     ax,offset flt_fix16
        cmp     bl,O_F_TOA
        jne     @ncv_c
        mov     ax,offset flt_fixu16
@ncv_c:
        call    cm_ncall
        cmp     bl,O_F_TOB
        jne     @ncv_p
        call    cm_ds
        db      4,84h,0E4h,0Fh,85h
        mov     ax,offset err_iq
        call    cm_nrel
@ncv_p:
        call    cm_ds
        db      2,66h,50h
        clc
        ret
@ncv_b:
        cmp     bl,O_I_TOA
        je      @ncv_a
        call    cm_ds
        db      8,89h,0E5h,80h,7Eh,1,0,0Fh,85h
        jmp     @ncv_e
@ncv_a:
        call    cm_ds
        db      8,89h,0E5h,0F6h,46h,1,80h,0Fh,85h
@ncv_e:
        mov     ax,offset err_iq
        call    cm_nrel
        clc
        ret

; ::::::::::::::
; cm_nnext - NEXT of an INT variable: nx_t copied to the stream, its
; addresses filled in; its last byte is the id. Else CF set.
;
; in:     SI  at the op
; out:    DI  past the template
; ::::::::::::::
cm_nnext:
        movzx   bx,byte ptr [si+1]
        mov     al,[sinfo+bx]
        and     al,3
        cmp     al,1
        jne     @nt_no
        push    si
        push    cx
        push    bp
        mov     dx,di                   ; the template's copy, from the code
        mov     si,offset nx_t
        mov     cx,nx_len
        rep     movs byte ptr es:[di],byte ptr cs:[si]
        mov     bp,dx
        lea     ax,[bp+6]
        mov     [bp+nx_x-nx_t+1],ax     ; ex_brk finds the element from SI
        lea     ax,[bp+nx_d-nx_t]
        mov     [bp+nx_s-nx_t+1],ax     ; the slow path: the id
        mov     [bp+nx_d-nx_t],bl
        mov     [bp+nx_i-nx_t+3],bl
        shl     bx,2
        add     bx,VARBASE
        mov     [bp+nx_a-nx_t+1],bx
        mov     [bp+nx_b-nx_t+1],bx
        mov     bx,offset nx_t-CSOFF
        sub     bx,dx                   ; the rel16s: the move, into CS's view
        mov     si,offset nx_fix
        mov     cx,NXFIX
@ncn_f:
        lods    word ptr cs:[si]
        mov     bp,ax
        add     bp,dx
        add     [bp],bx
        loop    @ncn_f
        pop     bp
        pop     cx
        pop     si
        clc
        ret

; ::::::::::::::
; nx_t - NEXT of an INT variable, as copied: the stop key, the top frame
; being that variable's, the step, the test; a loop goes on at the frame's
; ip, else the next element follows. Other frames: op_nextv.
; ::::::::::::::
nx_t:
nx_x      label   byte
        db      0BEh,0,0                ; mov si,element + 6, for ex_brk
        cmp     [stpflg],0
        jne     ex_brk
nx_f1   label   byte
        and     [tsp],0
        mov     di,[cstk]
        cmp     di,CSTOP
        jae     @nx_sl
        cmp     byte ptr [di+F_MARK],M_FR
        jne     @nx_sl
nx_i      label   byte
        db      80h,7Dh,F_ID,0          ; cmp byte ptr [di+F_ID],id
        jne     @nx_sl
nx_a      label   byte
        db      0A1h,0,0                ; mov ax,[var]
        add     ax,[di+F_STEP]
        jno     @F
        jmp     err_iq
nx_f2   label   byte
@@:
nx_b      label   byte
        db      0A3h,0,0                ; mov [var],ax
        test    byte ptr [di+F_STEP+1],80h
        js      @nx_dn
        cmp     ax,[di+F_LIMIT]
        jle     @nx_lp
        jmp     @nx_dn1
@nx_dn:
        cmp     ax,[di+F_LIMIT]
        jge     @nx_lp
@nx_dn1:
        add     [cstk],FRSIZE
        jmp     @nx_ov
@nx_lp:
        mov     bx,di
        jmp     for_lp
nx_f3   label   byte
@nx_sl:
nx_s      label   byte
        db      0BEh,0,0                ; mov si,the id
        jmp     op_nextv
nx_f4   label   byte
nx_d    label   byte
        db      0
@nx_ov:
nx_len  = @nx_ov - nx_t
nx_fix  dw      nx_f1-nx_t-2
        dw      nx_f2-nx_t-2
        dw      nx_f3-nx_t-2
        dw      nx_f4-nx_t-2
NXFIX   = 4

; ::::::::::::::
; cm_narr - a 1-D array's element, INT or FLOAT, load or store: the template
; copied, its variable's slot and id filled in. The unusual case calls arr_r1.
;
; in:     DL  0 I.LD.A1, 1 F.LD.A1, 2 I.ST.A1, 3 F.ST.A1
;         SI  at the op
; out:    DI  past the template
; ::::::::::::::
cm_narr:
        movzx   bx,byte ptr [si+1]
        add     bx,bx
        push    si
        push    cx
        push    bp
        movzx   ax,dl
        imul    ax,10
        add     ax,offset na_tab
        mov     bp,ax
        mov     dx,di                   ; na_tab and the template in the code
        mov     si,cs:[bp]
        mov     cx,cs:[bp+2]
        rep     movs byte ptr es:[di],byte ptr cs:[si]
        mov     ax,offset vt
        add     ax,bx
        mov     si,cs:[bp+4]
        add     si,dx
        mov     [si],ax
        mov     si,cs:[bp+6]
        add     si,dx
        mov     [si],bx
        mov     si,cs:[bp+8]
        add     si,dx
        mov     ax,offset arr_r1
        sub     ax,si
        sub     ax,2+CSOFF
        mov     [si],ax
        pop     bp
        pop     cx
        pop     si
        clc
        ret

; The element: the id's array in vt, the subscript below the count (AH_FAST),
; one segment (AH_SEG); else arr_r1 finds it.
arrt    macro   nm, kind
nm&_t   label   byte
  if kind ge 2
        pop     dword ptr [stv]
  endif
        pop     eax
nm&_v   label   byte
        db      8Bh,3Eh,0,0
        cmp     byte ptr [di],1
        jne     nm&_s
        cmp     ax,[di+AH_FAST]
        jae     nm&_s
        mov     fs,[di+AH_SEG]
  if kind and 1
        shl     ax,2
  else
        add     ax,ax
  endif
        mov     di,ax
nm&_g   label   near
  if kind eq 0
        push    dword ptr fs:[di]
  elseif kind eq 1
        push    dword ptr fs:[di]
  elseif kind eq 2
        mov     ax,word ptr [stv]
        mov     fs:[di],ax
  else
        mov     eax,[stv]
        mov     fs:[di],eax
  endif
        jmp     short nm&_o
nm&_s   label   near
nm&_b   label   byte
        db      0BBh,0,0
        call    arr_r1
nm&_c   label   byte
        jmp     nm&_g
nm&_o   label   near
        endm

; ::::::::::::::
; The four templates, and na_tab: where each is, its size and its patch places.
; ::::::::::::::
        arrt    ila,0
        arrt    fla,1
        arrt    isa,2
        arrt    fsa,3
na_tab  dw      offset ila_t,ila_o-ila_t,ila_v-ila_t+2,ila_b-ila_t+1,ila_c-ila_t-2
        dw      offset fla_t,fla_o-fla_t,fla_v-fla_t+2,fla_b-fla_t+1,fla_c-fla_t-2
        dw      offset isa_t,isa_o-isa_t,isa_v-isa_t+2,isa_b-isa_t+1,isa_c-isa_t-2
        dw      offset fsa_t,fsa_o-fsa_t,fsa_v-fsa_t+2,fsa_b-fsa_t+1,fsa_c-fsa_t-2
