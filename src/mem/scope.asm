; ==========================================================================
; scope.asm - the scoped name table: what a name is declared as, in the
; module and in each procedure
; ==========================================================================
; holds:    scp_init, scp_bind, scp_get, scp_look, scp_decl, scp_sfx, scp_mshr, scp_known, scp_con,
;           scp_label, scp_lfind
; data:     A binding (B_*) is a name's in one scope: the module's (0) or a
;           procedure's (its name). It holds the declared type, record and
;           STRING * n of the scalar and of the array, a label's line, and
;           flags: BF_SHR the module's, every procedure's (DIM SHARED,
;           CONST); BF_REF in a procedure, SHARED: the module's; BF_STAT
;           STATIC; BF_LBL a label; BF_CON a CONST. A procedure's binding,
;           any, makes the name its own there: it shadows the module's;
;           but BF_USE alone, a use with a suffix before any declaration
;           there, is none. A label is a binding too, a line number's in
;           scope + SC_NUM; one scope's in all (nflag's NM_LBL, lnbit).
;           The compiler's scopes are a stack (scs) of procedures, 0 the
;           module at its bottom; a name is looked up from the top down, its
;           first binding the scope seen from above sees. From a SUB or a
;           FUNCTION the module's names are seen only when BF_SHR, or when
;           SHARED there (BF_REF); from a DEF FN all. A line is compiled on
;           its own: scp_proc makes the stack its procedure's.
; ==========================================================================

include x86f.inc
        .code

NBND    = 16384                 ; numbered lines' too
NSCS    = 4
SCHASH  = 1024

; ::::::::::::::
; scp_init - no bindings.
;
; keeps:  all
; ::::::::::::::
scp_init:
        push    eax
        push    ecx
        push    edi
        mov     [g_nbnd],1
        mov     edi,offset g_bhead
        mov     ecx,SCHASH/2
        xor     eax,eax
        rep     stosd
        mov     edi,offset g_lnbit
        mov     ecx,64000/32
        rep     stosd
        pop     edi
        pop     ecx
        pop     eax
        ret

; ::::::::::::::
; scp_get - scope AX's binding of name BX.
;
; out:    EDI  it
;         CF   set: none
; keeps:  all but EDI
; ::::::::::::::
scp_get:
        push    ecx
        call    scp_hash
        movzx   ecx,word ptr [g_bhead+ecx*2]
@sgt_l:
        jecxz   @sgt_n
        imul    edi,ecx,BSZ
        add     edi,offset g_bnd
        cmp     ax,[edi+B_SC]
        jne     @F
        cmp     bx,[edi+B_NAM]
        je      @sgt_y
@@:
        movzx   ecx,word ptr [edi+B_NXT]
        jmp     @sgt_l
@sgt_n:
        pop     ecx
        stc
        ret
@sgt_y:
        pop     ecx
        clc
        ret

; ECX the hash chain of scope AX, name BX
scp_hash:
        movzx   ecx,ax
        imul    ecx,ecx,7
        add     cx,bx
        and     ecx,SCHASH-1
        ret

; ::::::::::::::
; scp_bind - scope AX's binding of name BX, made when there is none.
;
; out:    EDI  it
; keeps:  all but EDI
; error:  Out of memory  NBND bindings
; ::::::::::::::
scp_bind:
        call    scp_get
        jnc     @sbd_r
        push    ecx
        push    edx
        mov     edx,[g_nbnd]
        cmp     edx,NBND
        jae     err_om
        inc     [g_nbnd]
        imul    edi,edx,BSZ
        add     edi,offset g_bnd
        push    eax
        push    edi
        mov     ecx,BSZ
        xor     al,al
        rep     stosb
        pop     edi
        pop     eax
        mov     [edi+B_SC],ax
        mov     [edi+B_NAM],bx
        call    scp_hash
        mov     cx,[g_bhead+ecx*2]
        mov     [edi+B_NXT],cx
        call    scp_hash
        mov     [g_bhead+ecx*2],dx
        pop     edx
        pop     ecx
@sbd_r:
        ret

; ::::::::::::::
; scp_proc - procedure EAX compiled (cproc), 0 the module: the stack the
; module, then it.
;
; keeps:  all
; ::::::::::::::
scp_proc:
        mov     [g_state.cm.cproc],al
        mov     [g_scn],1
        and     word ptr [g_scs],0

; ::::::::::::::
; scp_enter - push procedure EAX's scope; none for 0.
;
; keeps:  all
; ::::::::::::::
scp_enter:
        test    eax,eax
        jz      @sen_r
        push    ecx
        mov     ecx,[g_scn]
        cmp     ecx,NSCS
        jae     err_om
        mov     [g_scs+ecx*2],ax
        inc     [g_scn]
        pop     ecx
@sen_r:
        ret

; ::::::::::::::
; scp_leave - pop a scope, the module's kept.
;
; keeps:  all
; ::::::::::::::
scp_leave:
        cmp     [g_scn],1
        jbe     @F
        dec     [g_scn]
@@:
        ret

; ::::::::::::::
; scp_frame - stack entry ECX's scope.
;
; out:    EAX  its procedure's name, 0 the module
;         DH   1 a DEF FN: it sees all of the scope under it
; keeps:  all but EAX, DH
; ::::::::::::::
scp_frame:
        movzx   eax,word ptr [g_scs+ecx*2]
        xor     dh,dh
        test    eax,eax
        jz      @sfr_r
        shl     eax,5
        test    byte ptr [g_prc+eax+P_FLAG],PF_DEF
        jz      @F
        inc     dh
@@:
        movzx   eax,word ptr [g_prc+eax+P_NAM]
@sfr_r:
        ret

; ::::::::::::::
; scp_own - the scope a new name is made in: the top one not a DEF FN's.
;
; out:    EAX  it
; keeps:  all but EAX
; ::::::::::::::
scp_own:
        push    ecx
        push    edx
        mov     ecx,[g_scn]
@sow_l:
        dec     ecx
        call    scp_frame
        test    dh,dh
        jz      @F
        test    ecx,ecx
        jnz     @sow_l
@@:
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; scp_look - name BX's binding, from the top of the stack down: the first a
; scope sees from the one above it.
;
; out:    EDI  the binding, a zero one for none
;         EAX  its scope, 0 the module's; none: scp_own's
; keeps:  all but EAX, EDI
; ::::::::::::::
scp_look:
        push    ecx
        push    edx
        mov     ecx,[g_scn]
        xor     dl,dl                   ; 1: only what the scope above sees
@slk_f:
        dec     ecx
        call    scp_frame
        call    scp_get
        jc      @slk_u
        cmp     byte ptr [edi+B_FLG],BF_USE
        je      @slk_u
        test    dl,dl
        jz      @F
        test    byte ptr [edi+B_FLG],BF_SHR
        jz      @slk_u
@@:
        test    byte ptr [edi+B_FLG],BF_REF     ; SHARED: the scope under it's,
        jz      @slk_r                  ; declared there or not
        test    ecx,ecx
        jz      @slk_r
        dec     ecx
        call    scp_frame
        call    scp_get
        jnc     @slk_r
        mov     edi,offset g_bzero
        jmp     @slk_r
@slk_u:                                 ; not here, or not seen: under it,
        mov     dl,1                    ; seen only when shared, unless a
        test    dh,dh                   ; DEF FN's
        jz      @slk_n
        xor     dl,dl
@slk_n:
        test    ecx,ecx
        jnz     @slk_f
        call    scp_own
        mov     edi,offset g_bzero
@slk_r:
        pop     edx
        pop     ecx
        ret

; ::::::::::::::
; scp_decl - name EAX's declared field CL (B_ET, B_REC, B_FIX), of its array
; when DL has 80h, as the procedure compiled sees it.
;
; out:    ECX  it, 0 none
; keeps:  all but ECX
; ::::::::::::::
scp_decl:
        push    eax
        push    ebx
        push    edi
        mov     ebx,eax
        call    scp_look
        movzx   eax,cl
        test    dl,dl
        jns     @sdc_s
        inc     eax                     ; the array's: the next byte
        cmp     cl,B_FIX
        jne     @sdc_s
        inc     eax                     ; the next word
@sdc_s:
        cmp     cl,B_FIX
        movzx   ecx,byte ptr [edi+eax]
        jne     @F
        movzx   ecx,word ptr [edi+eax]
@@:
        pop     edi
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; scp_sfx - name EAX with suffix type ECX (ET_*), an array's when DL has
; 80h, as the procedure compiled sees it: a CONST's is the CONST's type, any
; suffix; else not DIM AS another type, a record.
;
; out:    ECX  the type
; keeps:  all but ECX
; error:  Duplicate definition  DIM AS another
; ::::::::::::::
scp_sfx:
        push    eax
        push    ebx
        push    edi
        mov     ebx,eax
        call    scp_look
        test    byte ptr [edi+B_FLG],BF_CON
        jz      @F
        movzx   ecx,byte ptr [edi+B_ET]
        jmp     @scl_n
@@:
        xor     eax,eax
        test    dl,dl
        jns     @F
        inc     eax
@@:
        cmp     byte ptr [edi+B_REC+eax],0
        stc
        jne     @scl_r
        mov     al,[edi+B_ET+eax]
        test    al,al
        jz      @scl_n
        cmp     al,cl
        stc
        jne     @scl_r
@scl_n:
        clc
@scl_r:
        pop     edi                     ; EDI: a compile's ERR goes there
        pop     ebx
        pop     eax
        jnc     @F
        mov     al,E_RD
        jmp     err_raise
@@:
        ret

; ::::::::::::::
; scp_con - name EAX: a CONST, as the procedure compiled sees it?
;
; out:    CF  set: it is
; keeps:  all
; ::::::::::::::
scp_con:
        push    eax
        push    ebx
        push    edi
        mov     ebx,eax
        call    scp_look
        test    byte ptr [edi+B_FLG],BF_CON
        jz      @F
        stc
@@:
        pop     edi
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; scp_known - name EAX: bound, as the procedure compiled sees it?
;
; out:    CF  set: not
; keeps:  all
; ::::::::::::::
scp_known:
        push    eax
        push    ebx
        push    edi
        mov     ebx,eax
        call    scp_look
        cmp     edi,offset g_bzero
        clc
        jne     @F
        stc
@@:
        pop     edi
        pop     ebx
        pop     eax
        ret

; ::::::::::::::
; scp_mshr - name EBX: a variable the module shares, DIM SHARED, not a CONST?
;
; out:    CF  set: it is
; keeps:  all
; ::::::::::::::
scp_mshr:
        push    eax
        push    edi
        xor     eax,eax
        call    scp_get
        jc      @smr_n
        mov     al,[edi+B_FLG]
        and     al,BF_SHR or BF_CON
        cmp     al,BF_SHR
        stc
        je      @smr_r
@smr_n:
        clc
@smr_r:
        pop     edi
        pop     eax
        ret

; ::::::::::::::
; scp_lany - has a scope label BX: a name, a line number when AX has SC_NUM?
;
; out:    CF  set: one has
; keeps:  all
; ::::::::::::::
scp_lany:
        test    ah,SC_NUM shr 8
        jz      @F
        bt      dword ptr [g_lnbit],ebx
        ret
@@:
        test    [g_nflag+ebx],NM_LBL
        jz      @F
        stc
@@:
        ret

; ::::::::::::::
; scp_label - label BX of scope AX (SC_NUM: a line number) at line DX.
;
; keeps:  all but EDI
; error:  Duplicate label  a scope has it
; ::::::::::::::
scp_label:
        call    scp_lany
        jc      @slb_e
        test    ah,SC_NUM shr 8
        jz      @F
        bts     dword ptr [g_lnbit],ebx
        jmp     @slb_b
@@:
        or      [g_nflag+ebx],NM_LBL
@slb_b:
        call    scp_bind
        or      byte ptr [edi+B_FLG],BF_LBL
        mov     [edi+B_LINE],dx
        ret
@slb_e:
        mov     al,E_DL
        jmp     err_raise

; ::::::::::::::
; scp_lfind - label BX of scope AX (SC_NUM: a line number): its line.
;
; out:    EAX  it, 0 no scope's
; keeps:  all but EAX
; error:  Subprogram error  another scope's
; ::::::::::::::
scp_lfind:
        push    edi
        call    scp_get
        jc      @slf_o
        test    byte ptr [edi+B_FLG],BF_LBL
        jz      @slf_o
        movzx   eax,word ptr [edi+B_LINE]
        pop     edi
        ret
@slf_o:
        pop     edi                     ; EDI: a compile's ERR goes there
        call    scp_lany
        mov     eax,0
        jnc     @F
        mov     al,E_SPE
        jmp     err_raise
@@:
        ret

; ::::::::::::::
; scp_arec - name EAX's array's record TYPE, 0 none.
;
; out:    ECX  it
; keeps:  all but ECX
; ::::::::::::::
scp_arec:
        push    edx
        mov     dl,80h
        mov     ecx,B_REC
        call    scp_decl
        pop     edx
        ret

        .data
g_nbnd  dd      1
g_scn   dd      1                       ; the stack's depth, scopes
g_scs   dw      NSCS dup(0)
g_bzero db      BSZ dup(0)              ; none
        .data?
g_bhead dw      SCHASH dup(?)
g_lnbit dd      64000/32 dup(?)         ; the line numbers a scope has
g_bnd   db      NBND*BSZ dup(?)
        .code

        end
