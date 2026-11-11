; ==========================================================================
; main.asm - the .COM image: startup, then every module
; ==========================================================================
; holds:    rt_start; the regions at the end
; uses:     plt_init, prg_init, flt_finit, rpl_ready
; data:     CS the code, below CSOFF, and the code cache over the data's
;           free part; DS=SS=ES CS+CSOFF/16, the data, its image moved there
;           at the start. FS the POKE segment; GS the program; the heap above.
; calls:
;   rt_start           the data moved, zeroed, plt_init, prg_init, rpl_ready
; ==========================================================================
;           Flat: CS, DS, ES, SS the one 4G segment. The regions at the end.
; ==========================================================================
include x86f.inc
        .stack  4096

        .code

; ::::::::::::::
; rt_start - the regions zeroed, the vectors, then the READY loop.
; ::::::::::::::
rt_start:
        cld
        mov     ax,ds
        mov     [g_plat.dsv],ax
        mov     es,ax
        mov     fs,ax
        mov     gs,ax
        mov     [g_plat.pspsel],es              ; DOS/32A: ES the PSP at the start
        mov     esp,STKTOP
        mov     edi,offset g_bss_s
        mov     ecx,offset g_bss_e
        sub     ecx,edi
        xor     al,al
        rep     stosb
        mov     edi,offset g_pokeb
        mov     ecx,10000h/4
        xor     eax,eax
        rep     stosd
        mov     [g_state.mm.peekseg],0
        mov     [g_state.mm.dslin],0
        mov     [g_state.sg.tmplin],offset g_tmpd
        mov     [g_state.mm.pglin],offset g_progb
        mov     [g_state.mm.proglin],offset g_progb+PROGBASE
        mov     [g_state.mm.hbot],offset g_heapb
        mov     [g_state.mm.htop],offset g_heapb+HEAPSZ
        mov     [g_state.rn.rsp],esp
        mov     [g_state.rn.cstk],CSTOP
        mov     [g_state.rn.stklim],STKTOP-STKROOM
        mov     [g_state.rn.cplim],STKTOP-2*STKROOM ; compiler frames: twice the 16-bit's
        mov     dword ptr [g_progb+DLINE],0FF0000h
        mov     [g_state.cm.cxb],offset g_xbuf
        mov     [g_state.ft.rs],0FFFFFF01h         ; the 6502 build's boot state under sim65
        call    flt_finit
        call    plt_init
        call    prg_init
if IDE
        jmp     ui_start
else
        jmp     rpl_ready
endif

        .data?
g_bss_s label   byte                    ; from here to g_bss_e (regions.asm): zeroed
g_state TSTATE  <>
        .code


        end     rt_start
