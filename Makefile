# make [all|basic32|ide32|test|style|clean]: build/x86/basic32.exe (the compiler and runtime) and
# build/x86/ide32.exe (with the editor), DOS/32A executables; DOS32A.EXE beside them.
PY := python3
comma := ,
LLRM_BIN ?= /Users/alim/work/personal/llrm-qbmac/target/release
# llrm's pinned Open Watcom: its DOS/32A
DOS32A ?= $(HOME)/.cache/llrm/open-watcom-v2-703e1ae2f9a621dda2d28fb39b12d0d6d2788af6/bld/redist/dos32a
AC ?= 1                         # ide32: Tab completion, AC=0 leaves it out

.PHONY: all basic32 ide32 syms test style clean

MODS := main vm/exec vm/flow vm/calls vm/builtins vm/sys vm/bin vm/bc comp/lex comp/decl comp/block comp/ctl comp/comp \
	comp/expr comp/link comp/list mem/far mem/alloc mem/tab mem/prog mem/sym mem/scope mem/str mem/arr io/io io/errmsg io/diag io/srd io/repl \
	io/chan io/fio vm/using io/plat float/flt float/ftr float/fpu float/dbl vm/dbg vm/event vm/chain
UI := ui/scr ui/mouse ui/compl ui/text ui/edit ui/draw ui/menu ui/dlg ui/lmap ui/dbg ui/dpane ui/win ui/view ui/theme ui/syntax ui/ui
INC := $(wildcard src/*.inc src/*/*.inc)
OBJ := $(MODS:%=build/basic32/%.obj) build/basic32/regions.obj
IDE_OBJ := $(MODS:%=build/ide32/%.obj) $(UI:%=build/ide32/%.obj) build/ide32/regions.obj
LE_LINK = $(LLRM_BIN)/jwlink option quiet format os2 le option stub=$(DOS32A)/stub32a.exe

all: syms basic32 ide32

syms:                           # the shared symbols, src/syms.inc and src/ui/syms.inc
	$(PY) tools/x86syms.py
	$(PY) tools/x86syms.py IDE

# each module on its own, the shared header x86f.inc (tools/x86syms.py); regions.obj last:
# the zeroed state runs from main.asm's g_bss_s to its g_bss_e
build/basic32/%.obj: src/%.asm $(INC)
	@mkdir -p $(@D)
	cd src && $(LLRM_BIN)/jwasm -q -zf0 -Fl=$(abspath $(@:.obj=.lst)) -Fo $(abspath $@) $*.asm
build/ide32/%.obj: src/%.asm $(INC)
	@mkdir -p $(@D)
	cd src && $(LLRM_BIN)/jwasm -q -zf0 -DIDE=1 -DAC=$(AC) -Fl=$(abspath $(@:.obj=.lst)) \
		-Fo $(abspath $@) $*.asm

basic32: $(OBJ)
	@mkdir -p build/x86
	$(LE_LINK) file $(subst $(eval) ,$(comma),$(OBJ)) name build/x86/basic32.exe
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/x86/DOS32A.EXE

ide32: $(IDE_OBJ)
	@mkdir -p build/x86
	$(LE_LINK) file $(subst $(eval) ,$(comma),$(IDE_OBJ)) name build/x86/ide32.exe
	cp src/ui/basic.hlp build/x86/BASIC.HLP
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/x86/DOS32A.EXE

test:                           # the regressions, QB 4.5 conformance, the generated files
	$(PY) tools/x86f_test.py build/x86/basic32.exe
	$(PY) tools/qbconf.py
	sh test/tools/x86msg.sh
	sh test/tools/x86syms.sh
	sh test/tools/x86dfa.sh
	X86_IDE=build/x86/ide32.exe $(PY) tools/x86ide_test.py

style:
	$(PY) tools/asmstyle.py

clean:
	rm -rf build

# ---- The C build: csrc/, clang for i386 (COFF), build/c/basic32.exe and ide32.exe ----
# LLRM: llrm release/next's tree, for its DOS32 start-up and runtime, link recipe (jwlink) and
# jwasm. The C library's subset is csrc/lib; its mem routines are renamed in each object.
LLRM ?= $(HOME)/work/personal/llrm-next
CLANG ?= /opt/homebrew/opt/llvm/bin/clang
OBJCOPY ?= $(dir $(CLANG))llvm-objcopy
CC_OPT ?= -O2
CFLAGS := --target=i386-pc-win32 -march=i386 -mregparm=3 -std=c23 $(CC_OPT) -ffreestanding \
	-fno-asynchronous-unwind-tables -fno-stack-protector -mno-stack-arg-probe -mstack-alignment=4 \
	-DARENA_AT_ZERO=1 -Wall -Wextra -Wno-unused-parameter -isystem csrc/lib/include -Icsrc -Icsrc/core
COWN := $(foreach f,memcpy memmove memset,--redefine-sym _$(f)=_lib_$(f))
CSRC := $(shell find csrc/core csrc/lib -name '*.c' -not -path 'csrc/core/ui/*') \
	$(wildcard csrc/plat/dos32/*.c) csrc/main.c
CUI := $(shell find csrc/core/ui -name '*.c' 2>/dev/null)
CASM := $(wildcard csrc/plat/dos32/*.asm)
CHDR := $(shell find csrc -name '*.h' -o -name '*.def')
COBJ := $(CSRC:csrc/%.c=build/c/basic32/%.obj) $(CASM:csrc/%.asm=build/c/basic32/%.obj)
CIDE_OBJ := $(CSRC:csrc/%.c=build/c/ide32/%.obj) $(CUI:csrc/%.c=build/c/ide32/%.obj) \
	$(CASM:csrc/%.asm=build/c/ide32/%.obj)
C_LINK = python3 -I $(LLRM)/tools/dosbatch/link_objects.py x86-m32

.PHONY: c cbasic32 cide32 ctest
c: cbasic32 cide32

build/c/basic32/%.obj: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -DIDE=0 -c $< -o $@
	$(OBJCOPY) $(COWN) $@
build/c/ide32/%.obj: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -DIDE=1 -c $< -o $@
	$(OBJCOPY) $(COWN) $@
build/c/basic32/%.obj: csrc/%.asm
	@mkdir -p $(@D)
	$(LLRM)/target/release/jwasm -q -zf0 -Fo $@ $<
build/c/ide32/%.obj: csrc/%.asm
	@mkdir -p $(@D)
	$(LLRM)/target/release/jwasm -q -zf0 -Fo $@ $<

cbasic32: $(COBJ)
	$(C_LINK) $(abspath build/c/basic32.exe) $(abspath $(COBJ))
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/c/DOS32A.EXE

cide32: $(CIDE_OBJ)
	$(C_LINK) $(abspath build/c/ide32.exe) $(abspath $(CIDE_OBJ))
	cp src/ui/basic.hlp build/c/BASIC.HLP
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/c/DOS32A.EXE

ctest:                          # the suites against the C build
	X86_COM=build/c/basic32.exe $(PY) tools/x86f_test.py build/c/basic32.exe
	X86_COM=build/c/basic32.exe $(PY) tools/qbconf.py
	X86_IDE=build/c/ide32.exe $(PY) tools/x86ide_test.py
