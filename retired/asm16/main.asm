; ==========================================================================
; main.asm - the .COM image: startup, then every module
; ==========================================================================
; holds:    rt_start; the include list
; uses:     every module, as one translation unit
; data:     CS the code, below CSOFF, and the code cache over the data's
;           free part; DS=SS=ES CS+CSOFF/16, the data, its image moved there
;           at the start. FS the POKE segment; GS the program; the heap above.
; calls:
;   rt_start           the data moved, zeroed, plt_init, prg_init, rpl_ready
; ==========================================================================
        .model  tiny
        .386
        .387
        option  casemap:none
include def.inc

        .code
DSEG    segment para public 'DSEG'      ; the data, after the code
DSEG    ends
DBSS    segment byte public 'BSS'       ; zeroed at the start, not in the file
DBSS    ends
DATA    group   DSEG,DBSS
        assume  ds:DATA, ss:DATA
        org     100h

; ::::::::::::::
; rt_start - zero the data and the POKE segment, then the READY loop.
; ::::::::::::::
rt_start:
        cld
        mov     ax,cs
        add     ax,HEAPSEG+100h         ; 4K of heap at least
        cmp     ax,ds:[2]
        ja      @st_nm
        mov     ax,offset codeend       ; the code below the data
        cmp     ax,CSOFF
        ja      @st_nm
        add     ax,15                   ; the data's image: the next paragraph
        and     al,0F0h
        mov     si,ax
        mov     ax,cs
        add     ax,CSOFF/16
        mov     [dsv],ax
        mov     es,ax
        mov     cx,offset bss_s         ; moved up, from its end: they overlap
        add     si,cx
        dec     si
        mov     di,cx
        dec     di
        std
        rep     movsb
        cld
        mov     ds,ax
        mov     ss,ax
        mov     sp,STKTOP
        cmp     word ptr [dsmark],'SD'  ; the move found the data
        jne     @st_nm
        mov     di,offset bss_s
        mov     cx,offset bss_e
        sub     cx,di
        xor     al,al
        rep     stosb
        mov     ax,cs
        add     ax,POKESEG
        mov     fs,ax
        mov     es,ax
        xor     di,di
        mov     cx,8000h
        xor     ax,ax
        rep     stosw
        push    ds
        pop     es
        mov     word ptr [sysv+2],fs
        mov     [peekseg],fs
        mov     ax,ds
        movzx   eax,ax
        shl     eax,4
        mov     [dslin],eax
        lea     edx,[eax+offset tmpd]
        mov     [tmplin],edx
        lea     edx,[eax+(PSEG-CSOFF/16)*16]
        mov     [pglin],edx
        add     edx,PROGBASE
        mov     [proglin],edx
        add     eax,(HEAPSEG-CSOFF/16)*16
        mov     [hbot],eax
        movzx   eax,word ptr cs:[2]
        shl     eax,4
        mov     [htop],eax
        mov     sp,STKTOP
        mov     [rsp],sp
        mov     [cstk],CSTOP
        mov     ax,STKTOP-STKROOM
        mov     [stklim],ax
        mov     ax,cs
        add     ax,PSEG
        mov     gs,ax
        mov     dword ptr gs:[DLINE],0FF0000h
        mov     [cxb],offset xbuf
        mov     [rs],0FFFFFF01h         ; the 6502 build's boot state under sim65
        call    flt_finit
        call    plt_init
        call    prg_init
if IDE
        jmp     ui_start
else
        jmp     rpl_ready
endif
@st_nm:
        mov     ax,4C01h
        int     21h

dsv     dw      ?                       ; DS, for code that runs without it
old1b   dd      ?                       ; the vectors plt_init replaced
old1c   dd      ?

include vm/isa.inc
include vm/exec.asm
include vm/flow.asm
include vm/calls.asm
include vm/builtins.asm
include comp/lex.asm
include comp/comp.asm
include comp/expr.asm
include comp/link.asm
include comp/native.asm
include comp/list.asm
if IDE
include ui/scr.asm
include ui/mouse.asm
if AC
include ui/compl.asm
endif
include ui/text.asm
include ui/edit.asm
include ui/draw.asm
include ui/menu.asm
include ui/dlg.asm
include ui/sync.asm
include ui/ui.asm
endif
include mem/far.asm
include mem/prog.asm
include mem/sym.asm
include mem/str.asm
include mem/arr.asm
include io/io.asm
include io/repl.asm
include io/file.asm
include io/plat.asm
include float/flt.asm
include float/ftr.asm
include float/fpu.asm
codeend label   byte                    ; the code's end: the data follows
include gen.inc
include data.inc

        end     rt_start
