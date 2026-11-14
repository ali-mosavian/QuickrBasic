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
	-fno-asynchronous-unwind-tables -fno-stack-protector -mno-stack-arg-probe -mstack-alignment=4 -Werror=undef \
	-DARENA_AT_ZERO=1 -Wall -Wextra -Wno-unused-parameter -isystem csrc/lib/include -Icsrc -Icsrc/core
COWN := $(foreach f,memcpy memmove memset,--redefine-sym _$(f)=_lib_$(f))
CSRC := $(shell find csrc/core csrc/lib -name '*.c' -not -path 'csrc/core/ui/*') \
	$(wildcard csrc/plat/dos32/*.c) csrc/boot.c
CUI := $(shell find csrc/core/ui -name '*.c' 2>/dev/null)
CASM := $(wildcard csrc/plat/dos32/*.asm)
CHDR := $(shell find csrc -name '*.h' -o -name '*.def')
CCORE := $(CSRC:csrc/%.c=build/c/obj/%.obj) $(CASM:csrc/%.asm=build/c/obj/%.obj)
COBJ := $(CCORE) build/c/obj/app/basic32.obj
CIDE_OBJ := $(CCORE) $(CUI:csrc/%.c=build/c/obj/%.obj) build/c/obj/app/ide32.obj
C_LINK = python3 -I $(LLRM)/tools/dosbatch/link_objects.py x86-m32

.PHONY: c cbasic32 cide32 ctest
c: cbasic32 cide32

build/c/obj/core/ui/%.obj: csrc/core/ui/%.c $(CHDR)   # the editor: AC, its Tab completion
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -DAC=$(AC) -c $< -o $@
	$(OBJCOPY) $(COWN) $@
build/c/obj/%.obj: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -c $< -o $@
	$(OBJCOPY) $(COWN) $@
build/c/obj/%.obj: csrc/%.asm
	@mkdir -p $(@D)
	$(LLRM)/target/release/jwasm -q -zf0 -Fo $@ $<

cbasic32: $(COBJ)
	$(C_LINK) $(abspath build/c/basic32.exe) $(abspath $(COBJ))
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/c/DOS32A.EXE

cide32: $(CIDE_OBJ)
	$(C_LINK) $(abspath build/c/ide32.exe) $(abspath $(CIDE_OBJ))
	cp src/ui/basic.hlp build/c/BASIC.HLP
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/c/DOS32A.EXE

# ---- The host build: the same core on this machine (x86-64; Rosetta's x87 on Apple silicon) ----
HOST_CC ?= $(CLANG) -arch x86_64 -isysroot $(shell xcrun --show-sdk-path 2>/dev/null)
HOST_CFLAGS := -std=c23 -O1 -g -Werror=undef -DARENA_AT_ZERO=0 -Wall -Wextra -Wno-unused-parameter \
	-Icsrc/plat/host/include -Icsrc -Icsrc/core
HSRC := $(shell find csrc/core -name '*.c' -not -path 'csrc/core/ui/*') \
	$(wildcard csrc/plat/host/*.c) csrc/boot.c
HOBJ := $(HSRC:csrc/%.c=build/host/obj/%.o)

.PHONY: chost chosttest
build/host/obj/%.o: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

chost: build/host/basic
build/host/basic: $(HOBJ) build/host/obj/app/basic32.o
	$(HOST_CC) $^ -o $@

UNIT_SRC := $(wildcard tests/unit/*.c)
UNIT_OBJ := $(UNIT_SRC:tests/unit/%.c=build/host/unit/%.o)
build/host/unit/%.o: tests/unit/%.c tests/unit/unit.h $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@
build/host/unit_tests: $(HOBJ) $(UNIT_OBJ)
	$(HOST_CC) $^ -o $@

.PHONY: cunit
cunit: build/host/unit_tests   # the unit tests, on the host
	build/host/unit_tests

chosttest: chost cunit         # the unit tests and the suites against the host build, natively
	X86_COM=build/host/basic $(PY) tools/x86f_test.py build/host/basic
	X86_COM=build/host/basic $(PY) tools/qbconf.py

ctest:                          # the suites against the C build
	X86_COM=build/c/basic32.exe $(PY) tools/x86f_test.py build/c/basic32.exe
	X86_COM=build/c/basic32.exe $(PY) tools/qbconf.py
	X86_IDE=build/c/ide32.exe $(PY) tools/x86ide_test.py
