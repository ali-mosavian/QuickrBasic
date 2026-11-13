; ==========================================================================
; x86f.inc - every module's header: the model, the constants, the macros,
; the symbols the modules share (syms.inc, tools/x86syms.py)
; ==========================================================================
        .386
        .387
        .model  flat
        assume  gs:nothing              ; GS: the program store (prg_place)
        assume  fs:nothing              ; FS: the variables' TVARs (mem/sym.asm)
        option  casemap:none
include def.inc
include state.inc
include vm/isa.inc
include vm/vm.inc
include comp/comp.inc
include comp/dfa.inc
include io/msg.inc
if IDE
include ui/ui.inc
include ui/syms.inc
else
include syms.inc
endif
