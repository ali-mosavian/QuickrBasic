; ==========================================================================
; regions.asm - the 64K regions, linked last
; ==========================================================================
; holds:    g_bss_e, the end of the zeroed state; the regions after it
; uses:     nothing: data only
; calls:
;   none
; ==========================================================================

include x86f.inc

        .data?
g_bss_e label   byte
        align   4
g_dsb   db      10000h dup(?)            ; the 16-bit build's DS, as one block
g_pokeb db      10000h dup(?)
g_stkb  db      STKSZ dup(?)
if IDE
g_clpb  db      10000h dup(?)            ; the editor's clipboard
endif
        .code

        end
