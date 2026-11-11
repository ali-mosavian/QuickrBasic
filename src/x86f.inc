; ==========================================================================
; x86f.inc - every module's header: the model, the constants, the macros,
; the symbols the modules share (syms.inc, tools/x86syms.py)
; ==========================================================================
        .386
        .387
        .model  flat
        option  casemap:none
include def.inc
include vm/isa.inc
include vm/vm.inc
include comp/comp.inc
include comp/dfa.inc
if IDE
include ui/uibuf.inc
include ui/syms.inc
else
include syms.inc
endif
