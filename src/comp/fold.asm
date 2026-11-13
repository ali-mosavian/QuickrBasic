; ==========================================================================
; fold.asm - constant folding: an expression of constants becomes its value
; ==========================================================================
; holds:    cm_fold, op_fold, cf_fail, cm_krec, cm_klit, var_kclr
; uses:     the VM's handlers, cm_lint, cm_op, cm_dw, cm_shid
; data:     Code whose every op is FK (vm/isa.inc) runs as it is compiled,
;           through the VM's own handlers, so the value is the run's to the
;           bit. Its error leaves the code as it was, to raise when run.
;           A CONST whose value folds to one literal load keeps it (TVX.kop,
;           kval): a use compiled after loads it in place of the variable.
;           The CONST still stores the variable, for uses compiled before.
; ==========================================================================

include x86f.inc
        .code

; ::::::::::::::
; cm_fold - the code from EBX to EDI, of type cty: when all FK ops and more
; than one, one load of its value instead.
;
; in:     EBX  its start
; keeps:  all but EDI
; ::::::::::::::
cm_fold:
        pushad
        mov     al,[g_state.cm.cty]
        cmp     al,T_INT
        je      @F
        cmp     al,T_FLT
        je      @F
        cmp     al,T_DBL
        jne     @cf_r
@@:
        mov     ecx,ebx
        xor     edx,edx                 ; EDX the ops
@cf_s:
        cmp     ecx,edi
        jae     @cf_e
        movzx   eax,byte ptr [ecx]
        test    [g_opfl+eax],FK
        jz      @cf_r
        movzx   eax,[g_opsz+eax]
        add     ecx,eax
        inc     edx
        jmp     @cf_s
@cf_e:
        cmp     edx,2
        jb      @cf_r
        cmp     edi,[g_state.cm.cxlim]
        jae     @cf_r
        mov     byte ptr [edi],O_FOLD
        push    [g_state.rn.opip]
        mov     [g_cfsp],esp
        mov     [g_cfold],1
        mov     esi,ebx
        dispatch

; ::::::::::::::
; op_fold - FOLD ( x -- ): the folded code's end; its value replaces it.
; ::::::::::::::
op_fold:
        pop     eax                     ; the cell: a DOUBLE's both dwords
        pop     edx
        mov     esp,[g_cfsp]
        mov     [g_cfold],0
        pop     [g_state.rn.opip]
        mov     edi,[esp+16]            ; the code's start: pushad's EBX
        cmp     [g_state.cm.clle],edi   ; marks inside the code go
        jbe     @F
        and     [g_state.cm.cll],0
        and     [g_state.cm.clle],0
@@:
        cmp     [g_state.cm.ccmpe],edi  ; no relation left for an IF to take
        jbe     @F
        and     [g_state.cm.ccmpe],0
@@:
        cmp     [g_state.cm.cty],T_INT
        jne     @cf_f
        call    cm_lint
        jmp     @cf_c
@cf_f:
        push    eax
        mov     al,O_F_LD_F32
        cmp     [g_state.cm.cty],T_FLT
        je      @F
        mov     al,O_D_LD_K
@@:
        call    cm_op
        pop     eax
        call    cm_dw
        cmp     [g_state.cm.cty],T_DBL
        jne     @F
        mov     eax,edx
        call    cm_dw
@@:
        mov     [g_ckon],edi
@cf_c:
        mov     ecx,[esp]               ; the code's end: pushad's EDI
        cmp     [g_cshe],ecx            ; the SHORT's class as it was
        jne     @F
        mov     [g_cshe],edi
@@:
        mov     [esp],edi
@cf_r:
        popad
        ret

; ::::::::::::::
; cf_fail - err_raise while folding: the code stays as it is, cm_fold
; returns. Not folding: back.
; ::::::::::::::
cf_fail:
        cmp     [g_cfold],0
        je      @cff_r
        mov     esp,[g_cfsp]
        mov     [g_cfold],0
        pop     [g_state.rn.opip]
        popad
@cff_r:
        ret

; ::::::::::::::
; cm_krec - a CONST's store: its value, folded, one literal load? Kept as
; the variable's.
;
; in:     cvat  the value's code's start
; keeps:  all but EDI
; ::::::::::::::
cm_krec:
        cmp     [g_state.cm.ctok],TK_CONST
        jne     @ckr_x
        cmp     [g_state.cm.ctn],0
        jne     @ckr_x
        pushad
        mov     ebx,[g_cvat]
        call    cm_fold
        mov     [esp],edi
        movzx   eax,byte ptr [ebx]
        call    @ck_lit
        jne     @ckr_r
        movzx   ecx,[g_opsz+eax]
        lea     edx,[ebx+ecx]
        cmp     edx,edi
        jne     @ckr_r
        movzx   edx,[g_state.cm.ctv]
        vxat    edi
        mov     [edi+edx*8].TVX.kop,al
        mov     edi,[g_state.sym.vars.at+12]
        lea     edi,[edi+edx*8]
        lea     esi,[ebx+1]
        dec     ecx
        rep     movsb
@ckr_r:
        popad
@ckr_x:
        ret

; ::::::::::::::
; cm_klit - variable EBX a CONST with a literal: its load instead of the
; variable's, typed and classed as the variable's.
;
; out:    CF  set: done
; keeps:  all but EDI
; ::::::::::::::
cm_klit:
        push    eax
        push    ecx
        push    esi
        vxat    eax
        movzx   eax,[eax+ebx*8].TVX.kop
        test    eax,eax
        jz      @ckl_n
        mov     cl,T_DBL
        cmp     al,O_D_LD_K
        je      @ckl_t
        mov     cl,T_FLT
        cmp     al,O_F_LD_F32
        je      @ckl_t
        cmp     al,O_F_LD_U8
        je      @ckl_t
        cmp     al,O_F_LD_PI
        je      @ckl_t
        mov     cl,T_INT
        mov     [g_state.cm.cll],edi
@ckl_t:
        mov     [g_state.cm.cty],cl
        movzx   ecx,[g_opsz+eax]
        call    cm_op
        mov     esi,[g_state.sym.vars.at+12]
        lea     esi,[esi+ebx*8]
@@:
        dec     ecx
        jz      @F
        lodsb
        call    cm_byt
        jmp     @B
@@:
        cmp     [g_state.cm.cty],T_INT
        jne     @F
        mov     [g_state.cm.clle],edi
@@:
        mov     [g_ckon],edi
        call    cm_shid
        stc
        jmp     @ckl_r
@ckl_n:
        clc
@ckl_r:
        pop     esi
        pop     ecx
        pop     eax
        ret

@ck_lit:                                ; ZF: op AL a literal's load
        cmp     al,O_I_LD_U8
        je      @F
        cmp     al,O_I_LD_I16
        je      @F
        cmp     al,O_I_LD_I32
        je      @F
        cmp     al,O_F_LD_F32
        je      @F
        cmp     al,O_F_LD_U8
        je      @F
        cmp     al,O_F_LD_PI
        je      @F
        cmp     al,O_D_LD_K
@@:
        ret

; ::::::::::::::
; var_kclr - no CONST's literal kept: a new link's CONSTs record theirs.
;
; keeps:  ESI
; ::::::::::::::
var_kclr:
        vxat    eax
        movzx   ecx,[g_state.lx.nsyms]
@@:
        dec     ecx
        js      @F
        mov     [eax+ecx*8].TVX.kop,0
        jmp     @B
@@:
        ret

        .data?
g_cvat  dd      ?                       ; an assignment's value's code start
g_cfsp  dd      ?                       ; ESP while folding
g_cfold db      ?                       ; 1 while folding
        .code

        end
