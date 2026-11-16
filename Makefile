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
	comp/expr comp/fold comp/link comp/list mem/far mem/alloc mem/tab mem/prog mem/sym mem/scope mem/str mem/arr io/io io/errmsg io/diag io/srd io/repl \
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
	$(PY) tools/x86fold_test.py build/x86/basic32.exe
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
	-DARENA_AT_ZERO=1 -Wall -Wextra -Wno-unused-parameter -isystem csrc/lib/include \
	-Icsrc -Icsrc/core
COWN := $(foreach f,memcpy memmove memset,--redefine-sym _$(f)=_lib_$(f))

# llrm's QB runtime (graphics, sound), ours since llrm-next f4197e19b2 (csrc/qbrt/llrm): the portable
# part, then each target's
QBRT := csrc/qbrt/llrm
QBRT_PORTABLE := $(addprefix qb/,console.c cntext.c cngfx.c gfx.c gline.c gcirc.c gpaint.c gputget.c \
	gdraw.c play.c screen.c scrfn.c sleep.c sticks.c timer.c varptr.c error.c rtinit.c nhstutil.c nheap.c)
# DOS32: its own device and asm, by clang and jwasm; the asm's names given to clang's C (names.h)
QBRT_DSRC := $(QBRT_PORTABLE) $(addprefix qb/,dos/m32/ad.c posix/block.c dev/dos/gfxdev.c \
	$(addprefix dev/dos/m32/,clock.c con.c key.c prn.c text.c tone.c))
QBRT_DASM := $(addprefix qb/dev/dos/m32/,bios.asm gdcirc.asm gdfill.asm gdline.asm gdscan.asm gdxfer.asm \
	ports.asm tick.asm) qb/dos/m32/block.asm
QBRT_DINC := -Icsrc/qbrt/include $(addprefix -I$(QBRT)/qb/,dos/m32 . dev dev/dos/m32 dev/dos) \
	-I$(QBRT)/os/x86-m32
QBRT_DOBJ := $(QBRT_DSRC:%.c=build/c/qbrt/%.obj) $(QBRT_DASM:%.asm=build/c/qbrt/%.obj) \
	build/c/obj/qbrt/qbrt.obj build/c/obj/qbrt/qbrt_dos.obj
build/c/qbrt/names.h: $(QBRT)/qb/dev/dos/m32/device.h $(QBRT)/qb/dev/dos/m32/devsym.h
	@mkdir -p $(@D)
	sed -n 's/^#pragma aux \([a-z_0-9]*\) "\(B\$$[A-Z0-9]*\)"/#pragma redefine_extname \1 \2/p' $^ > $@
	echo '#pragma redefine_extname music_tick B$$MUSICTICK' >> $@
build/c/qbrt/%.obj: $(QBRT)/%.c build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(subst -std=c23,-std=gnu11,$(CFLAGS)) -w -D__cdecl= -include build/c/qbrt/names.h $(QBRT_DINC) \
		-c $< -o $@
	$(OBJCOPY) $(COWN) $@
build/c/qbrt/%.obj: $(QBRT)/%.asm
	@mkdir -p $(@D)
	$(LLRM)/target/release/jwasm -q -coff -I$(QBRT)/qb/dev/dos/m32 -Fo$@ $<
build/c/obj/qbrt/%.obj: csrc/qbrt/%.c $(CHDR) build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -D__cdecl= -include build/c/qbrt/names.h $(subst -I,-isystem ,$(QBRT_DINC)) -c $< -o $@
	$(OBJCOPY) $(COWN) $@

CSRC := $(shell find csrc/core csrc/lib -name '*.c' -not -path 'csrc/core/ui/*') \
	$(wildcard csrc/plat/dos32/*.c) csrc/boot.c
CUI := $(shell find csrc/core/ui -name '*.c' 2>/dev/null)
CASM := $(wildcard csrc/plat/dos32/*.asm)
CHDR := $(shell find csrc -name '*.h' -o -name '*.def')
CCORE := $(CSRC:csrc/%.c=build/c/obj/%.obj) $(CASM:csrc/%.asm=build/c/obj/%.obj) $(QBRT_DOBJ)
COBJ := $(CCORE) build/c/obj/app/basic32.obj
CIDE_OBJ := $(CCORE) $(CUI:csrc/%.c=build/c/obj/%.obj) build/c/obj/app/ide32.obj
C_LINK = python3 -I $(LLRM)/tools/dosbatch/link_objects.py x86-m32

.PHONY: c cbasic32 cide32 ctest cstyle
cstyle:                         # formats the C sources (tools/cfmt.py)
	git ls-files 'csrc/*.[ch]' 'tests/*.[ch]' | grep -v -e qbrt/llrm/ -e comp/dfa.h -e io/gen.h -e io/msg.h | xargs $(PY) tools/cfmt.py
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

# ---- LTO (make clto): the C objects as bitcode (build/clto), one module optimized whole by
# tools/clto.py, linked with the asm's objects: build/clto/basic32.exe and ide32.exe. PGO=1: what
# a training run calls not is cold (tools/x86pgo.py, build/clto/*.hot)
CASM_OBJ := $(CASM:csrc/%.asm=build/c/obj/%.obj) $(QBRT_DASM:%.asm=build/c/qbrt/%.obj)
CBC = $(patsubst build/c/%.obj,build/clto/%.bc,$(filter-out $(CASM_OBJ),$1))
.PHONY: clto
clto: build/clto/basic32.exe build/clto/ide32.exe

build/clto/qbrt/%.bc: $(QBRT)/%.c build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(subst -std=c23,-std=gnu11,$(CFLAGS)) -flto -w -D__cdecl= -include build/c/qbrt/names.h \
		$(QBRT_DINC) -c $< -o $@
build/clto/obj/qbrt/%.bc: csrc/qbrt/%.c $(CHDR) build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto -D__cdecl= -include build/c/qbrt/names.h $(subst -I,-isystem ,$(QBRT_DINC)) \
		-c $< -o $@
build/clto/obj/core/ui/%.bc: csrc/core/ui/%.c $(CHDR)
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto -DAC=$(AC) -c $< -o $@
build/clto/obj/%.bc: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto -c $< -o $@

build/clto/prof/%.obj: build/clto/%.bclist
	@mkdir -p $(@D)
	$(PY) tools/clto.py $(CLANG) $@ --profiling $(shell cat $<) -- $(CASM_OBJ)
build/clto/basic32.bclist: $(call CBC,$(COBJ))
	echo $^ > $@
build/clto/ide32.bclist: $(call CBC,$(CIDE_OBJ))
	echo $^ > $@
build/clto/%.hot: build/clto/prof/%.obj $(CASM_OBJ)
	$(PY) tools/x86pgo.py $@ build/clto/prof/$*.exe $(if $(findstring ide,$*),ide) -- $< $(CASM_OBJ)
CHOT = $(if $(PGO),--hot build/clto/$1.hot)
.SECONDARY: build/clto/prof/basic32.obj build/clto/prof/ide32.obj

build/clto/basic32.exe: $(call CBC,$(COBJ)) $(CASM_OBJ) $(if $(PGO),build/clto/basic32.hot)
	$(PY) tools/clto.py $(CLANG) build/clto/basic32.obj $(call CHOT,basic32) $(call CBC,$(COBJ)) -- $(CASM_OBJ)
	$(C_LINK) $(abspath $@) $(abspath build/clto/basic32.obj $(CASM_OBJ))
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/clto/DOS32A.EXE
	$(PY) tools/x86lto_test.py $@
build/clto/ide32.exe: $(call CBC,$(CIDE_OBJ)) $(CASM_OBJ) $(if $(PGO),build/clto/ide32.hot)
	$(PY) tools/clto.py $(CLANG) build/clto/ide32.obj $(call CHOT,ide32) $(call CBC,$(CIDE_OBJ)) -- $(CASM_OBJ)
	$(C_LINK) $(abspath $@) $(abspath build/clto/ide32.obj $(CASM_OBJ))
	cp src/ui/basic.hlp build/clto/BASIC.HLP
	$(PY) tools/x86d32a.py $(DOS32A)/dos32a.exe build/clto/DOS32A.EXE

# ---- The host build: the same core on this machine's own CPU (plat/host/pfpu.c: the x87's on x86-64, C's on another) ----
SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
HOST_CC ?= $(CLANG) -isysroot $(SDK)
HOST_CFLAGS := -std=c23 -O1 -g -Werror=undef -DARENA_AT_ZERO=0 -DEXEC_COUNT -Wall -Wextra -Wno-unused-parameter \
	-Icsrc/plat/host/include -Icsrc -Icsrc/core
HSRC := $(shell find csrc/core -name '*.c' -not -path 'csrc/core/ui/*') csrc/core/ui/tree.c csrc/core/ui/indent.c \
	$(wildcard csrc/plat/host/*.c) csrc/boot.c
HOBJ := $(HSRC:csrc/%.c=build/host/obj/%.o)

# the host: llrm's headless device
QBRT_HSRC := $(QBRT_PORTABLE) $(addprefix qb/,posix/block.c posix/ad.c posix/mathops.c \
	dev/headless/headless.c dev/host/video.c dev/host/vgadata.c dev/host/tick.c) shared/posix/os.c
QBRT_HINC := -Icsrc/qbrt/include $(addprefix -I$(QBRT)/,qb/posix qb qb/dev qb/dev/headless qb/dev/host shared/posix)
QBRT_HOBJ := $(QBRT_HSRC:%.c=build/host/qbrt/%.o) build/host/obj/qbrt/qbrt.o build/host/obj/qbrt/qbrt_host.o
# play.c's waits for the tick come to qbrt_idle (BREAK, the tick in tests); the flag is here, so is its rebuild
build/host/qbrt/qb/play.o: QBRT_DEF = '-DQB_IDLE()=do { extern void qbrt_idle(void); qbrt_idle(); } while (0)'
build/host/qbrt/qb/play.o: Makefile
build/host/qbrt/%.o: $(QBRT)/%.c
	@mkdir -p $(@D)
	$(HOST_CC) -std=gnu11 -w -D__cdecl= $(QBRT_DEF) $(QBRT_HINC) -c $< -o $@
build/host/obj/qbrt/%.o: csrc/qbrt/%.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(subst -I,-isystem ,$(QBRT_HINC)) -c $< -o $@

.PHONY: chost chosttest
build/host/obj/%.o: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@

chost: build/host/basic
build/host/basic: $(HOBJ) $(QBRT_HOBJ) build/host/obj/app/basic32.o build/host/obj/plat/host/app/term.o
	$(HOST_CC) $^ -o $@

# tools/bench.py's native build: optimized, no op count;
# QB_INSNS reports the instructions it ran
.PHONY: chostperf
HPERF_CFLAGS := $(filter-out -O1 -g -DEXEC_COUNT,$(HOST_CFLAGS)) -O2
HPERF_OBJ := $(HSRC:csrc/%.c=build/hostperf/obj/%.o)
HPERF_QBRT := $(QBRT_HSRC:%.c=build/hostperf/qbrt/%.o) build/hostperf/obj/qbrt/qbrt.o build/hostperf/obj/qbrt/qbrt_host.o
build/hostperf/qbrt/qb/play.o: QBRT_DEF = '-DQB_IDLE()=do { extern void qbrt_idle(void); qbrt_idle(); } while (0)'
build/hostperf/qbrt/%.o: $(QBRT)/%.c
	@mkdir -p $(@D)
	$(HOST_CC) -O2 -std=gnu11 -w -D__cdecl= $(QBRT_DEF) $(QBRT_HINC) -c $< -o $@
build/hostperf/obj/qbrt/%.o: csrc/qbrt/%.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HPERF_CFLAGS) $(subst -I,-isystem ,$(QBRT_HINC)) -c $< -o $@
build/hostperf/obj/%.o: csrc/%.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HPERF_CFLAGS) -c $< -o $@
chostperf: build/hostperf/basic
build/hostperf/basic: $(HPERF_OBJ) $(HPERF_QBRT) build/hostperf/obj/app/basic32.o build/hostperf/obj/plat/host/app/insns.o
	$(HOST_CC) $^ -o $@

UNIT_SRC := $(wildcard tests/unit/*.c)
UNIT_OBJ := $(UNIT_SRC:tests/unit/%.c=build/host/unit/%.o)
build/host/unit/%.o: tests/unit/%.c tests/unit/unit.h $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@
build/host/unit_tests: $(HOBJ) $(QBRT_HOBJ) $(UNIT_OBJ)
	$(HOST_CC) $^ -o $@

.PHONY: cunit
cunit: build/host/unit_tests   # the unit tests, on the host
	build/host/unit_tests

build/host/golden/dump.o: tests/golden/dump.c $(CHDR)
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -c $< -o $@
build/host/golden_dump: $(HOBJ) $(QBRT_HOBJ) build/host/golden/dump.o
	$(HOST_CC) $^ -o $@

.PHONY: cgolden cgolden-update
cgolden: build/host/golden_dump   # each corpus line's code against tests/golden/code.txt
	$(PY) tools/golden.py
cgolden-update: build/host/golden_dump
	$(PY) tools/golden.py --update

chosttest: chost chostperf cunit cgolden  # the unit tests and the suites against the host build, natively
	$(PY) tools/qbconf_test.py
	X86_COM=build/host/basic $(PY) tools/x86f_test.py build/host/basic
	X86_COM=build/host/basic $(PY) tools/qbconf.py
	$(PY) tools/x86fold_test.py build/host/basic
	$(PY) tools/pterm_test.py build/host/basic
	$(PY) tools/bench_test.py

ctest:                          # the suites against the C build
	X86_COM=build/c/basic32.exe $(PY) tools/x86f_test.py build/c/basic32.exe
	X86_COM=build/c/basic32.exe $(PY) tools/qbconf.py
	$(PY) tools/x86fold_test.py build/c/basic32.exe
	X86_COM=build/c/basic32.exe $(PY) tools/x86cost_test.py build/c/basic32.exe
	X86_COM=build/c/basic32.exe $(PY) tools/x86gfx_test.py
	X86_IDE=build/c/ide32.exe $(PY) tools/x86ide_test.py
