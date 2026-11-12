# make [TARGET] [FP_BITS=8|16|24|32] [FPE=n FPM=n] [FAST_SQR=1]: the float is
# num_f$(FP_BITS).s; for 8 and 16, FPE exponent and FPM fraction bits (default 5
# and 2, 5 and 10). FAST_SQR=1: SQR exact and faster, 370 bytes more.
FP_BITS ?= 24
FPE ?= 5
ifeq ($(FP_BITS),8)
FPM ?= 2
else
FPM ?= 10
endif
ifeq ($(filter $(FP_BITS),8 16 24 32),)
$(error FP_BITS: 8, 16, 24 or 32)
endif
FAST_SQR ?= 0
export FP_BITS FPE FPM FAST_SQR

PY := python3
ISORT ?= isort
RUFF ?= ruff
TY ?= ty
TYFLAGS ?= --extra-search-path $(shell python3 -c 'import numpy, pathlib; print(pathlib.Path(numpy.__file__).parents[1])')
export ISORT RUFF TY TYFLAGS

.PHONY: x86 x86f x86f-ide ide test gate fpcheck fpall mathbench size repl style clean

test:
	$(PY) tools/harness.py test
	sh test/tools/size_stale.sh
	sh test/tools/routsize_fp.sh
	sh test/tools/routsize_data.sh
	sh test/tools/prof_branch.sh
	sh test/tools/prof_inside.sh
	sh test/tools/cache_slots.sh
	sh test/tools/prof_labels.sh
	sh test/tools/jmp_ind.sh
	sh test/tools/dupes.sh
	sh test/tools/entry_cost.sh
	sh test/tools/for_eol_cost.sh
	sh test/tools/f8tab_stale.sh
	sh test/tools/lexcheck.sh
	sh test/tools/x86dfa.sh
	sh test/tools/x86syms.sh
	sh test/tools/stock_norm.sh
	sh test/tools/build_stamp.sh

gate:                           # every gate at once; FULL=1 adds fpall and the benches
	$(PY) tools/harness.py gate $(if $(FULL),--full)

fpcheck:
	$(PY) tools/floats.py check

fpall:                          # every float, exhaustive for f8 (2/5 overflows INT(-3.06))
	$(PY) tools/floats.py all

mathbench:
	$(PY) tools/floats.py bench

size:
	$(PY) tools/romsize.py files

repl:
	tools/repl.sh

comma := ,
LLRM_BIN ?= /Users/alim/work/personal/llrm-qbmac/target/release

x86:                            # the DOS .COM, build/x86/basic.com
	mkdir -p build/x86
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -Fl=../../build/x86/main.lst -Fo ../../build/x86/main.obj main.asm
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -DCSOFF=$$(python3 ../../tools/x86csoff.py ../../build/x86/main.lst) \
		-Fo ../../build/x86/main.obj main.asm   # again, the data at the code's end
	$(LLRM_BIN)/jwlink option quiet format dos com file build/x86/main.obj name build/x86/raw.com
	tail -c +257 build/x86/raw.com > build/x86/basic.com   # jwlink keeps the 100h bytes before org

# llrm's pinned Open Watcom: its DOS/32A
DOS32A ?= $(HOME)/.cache/llrm/open-watcom-v2-703e1ae2f9a621dda2d28fb39b12d0d6d2788af6/bld/redist/dos32a
X86F_MODS := main vm/exec vm/flow vm/calls vm/builtins vm/bc comp/lex comp/decl comp/block comp/ctl comp/comp \
	comp/expr comp/link comp/list mem/far mem/prog mem/sym mem/str mem/arr io/io io/repl \
	io/file io/plat float/flt float/ftr float/fpu float/dbl vm/dbg
X86F_UI := ui/scr ui/mouse ui/compl ui/text ui/edit ui/draw ui/menu ui/dlg ui/sync ui/dbg ui/win ui/view ui/ui
X86F_INC := $(wildcard src/x86f/*.inc src/x86f/*/*.inc)
X86F_OBJ := $(X86F_MODS:%=build/x86f/%.obj) build/x86f/regions.obj
X86F_IDE_OBJ := $(X86F_MODS:%=build/x86f-ide/%.obj) $(X86F_UI:%=build/x86f-ide/%.obj) \
	build/x86f-ide/regions.obj
LE_LINK = $(LLRM_BIN)/jwlink option quiet format os2 le option stub=$(DOS32A)/stub32a.exe

# each module on its own, the shared header x86f.inc (tools/x86syms.py); regions.obj last:
# the zeroed state runs from main.asm's g_bss_s to its g_bss_e
build/x86f/%.obj: src/x86f/%.asm $(X86F_INC)
	@mkdir -p $(@D)
	cd src/x86f && $(LLRM_BIN)/jwasm -q -zf0 -Fl=$(abspath $(@:.obj=.lst)) -Fo $(abspath $@) $*.asm
build/x86f-ide/%.obj: src/x86f/%.asm $(X86F_INC)
	@mkdir -p $(@D)
	cd src/x86f && $(LLRM_BIN)/jwasm -q -zf0 -DIDE=1 -DAC=$(AC) -Fl=$(abspath $(@:.obj=.lst)) \
		-Fo $(abspath $@) $*.asm

x86f: $(X86F_OBJ)               # flat 32-bit, build/x86/basic32.exe, DOS32A.EXE beside it
	$(LE_LINK) file $(subst $(eval) ,$(comma),$(X86F_OBJ)) name build/x86f/basic32.exe
	cp build/x86f/basic32.exe build/x86/basic32.exe
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/x86/DOS32A.EXE

x86f-ide: $(X86F_IDE_OBJ)       # the flat build with the editor, build/x86/ide32.exe
	$(LE_LINK) file $(subst $(eval) ,$(comma),$(X86F_IDE_OBJ)) name build/x86f-ide/ide32.exe
	cp build/x86f-ide/ide32.exe build/x86/ide32.exe
	cp src/x86/ui/basic.hlp build/x86/BASIC.HLP
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/x86/DOS32A.EXE

AC ?= 1                         # ide: Tab completion, AC=0 leaves it out
ide:                            # with the full-screen editor, build/x86/basic-ide.com
	mkdir -p build/x86-ide
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -DIDE=1 -DAC=$(AC) -Fl=../../build/x86-ide/main.lst \
		-Fo ../../build/x86-ide/main.obj main.asm
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -DIDE=1 -DAC=$(AC) \
		-DCSOFF=$$(python3 ../../tools/x86csoff.py ../../build/x86-ide/main.lst) -Fo ../../build/x86-ide/main.obj main.asm
	$(LLRM_BIN)/jwlink option quiet format dos com file build/x86-ide/main.obj name build/x86-ide/raw.com
	tail -c +257 build/x86-ide/raw.com > build/x86/basic-ide.com
	cp src/x86/ui/basic.hlp build/x86/BASIC.HLP

style:
	$(PY) tools/asmstyle.py

clean:
	rm -rf build
