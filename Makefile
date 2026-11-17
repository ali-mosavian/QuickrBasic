# QuickrBasic. `make` builds build/c/basic32.exe (the compiler and runtime) and build/c/ide32.exe
# (with the editor): DOS/32A programs, with DOS32A.EXE and BASIC.HLP beside them.
#
#   make             the DOS build; GCC=1 builds it with i686-elf-gcc instead, in build/gcc
#   make test        the DOS build's suites, under DOSBox-X
#   make host        the same core for this machine: build/host/basic
#   make host-test   the unit tests, the golden code and the suites on the host build
#   make wasm        the same core as WebAssembly: build/wasm/basic (node runs it), build/web (the IDE's page)
#   make lto         build/lto: the whole program optimized as one module (LLVM); PGO=1 trains it first
#   make format      the C sources formatted
#   make clean
#
# Tools: clang (any with the i386 target), python3, git and a C compiler for the pinned jwasm,
# jwlink and DOSBox-X, which tools/deps.sh builds into build/deps on first use.
# The retired asm builds on its own: make -C retired/asm32, make -C retired/asm16.

PY ?= python3
CLANG ?= clang
# the editor's Tab completion; AC=0 leaves it out
AC ?= 1

DEPS := build/deps/bin
JWASM ?= $(DEPS)/jwasm
JWLINK ?= $(DEPS)/jwlink
DOSBOX_BIN ?= $(DEPS)/dosbox-x
DOS32A ?= third_party/dos32a
export JWASM JWLINK DOSBOX_BIN DOS32A

.PHONY: all test host host-test wasm wasm-test web mac mac-zip mac-test dos-zip lto format clean
all:

# one run builds both: two would clone into the same directory at once under -j
$(DEPS)/jwasm: tools/deps.sh
	sh tools/deps.sh build
$(DEPS)/jwlink: $(DEPS)/jwasm
$(DEPS)/dosbox-x: tools/deps.sh
	sh tools/deps.sh dosbox

# ---- The DOS build: clang for i386 (COFF), the start-up and OS layer by jwasm, linked by jwlink ----

B := $(if $(GCC),build/gcc,build/c)
CFLAGS := --target=i386-pc-win32 -march=i386 -mregparm=3 -std=c23 -O2 -ffreestanding -fno-asynchronous-unwind-tables \
	-fno-stack-protector -mno-stack-arg-probe -mstack-alignment=4 -Wall -Wextra -Wno-unused-parameter -Werror=undef \
	-DARENA_AT_ZERO=1 -isystem src/lib/include -Isrc -Isrc/core
DEPFLAGS = -MMD -MP -MF $@.d -MT $@

# GCC=1: i686-elf-gcc's assembly, made COFF by tools/gas2coff.py and assembled by clang
GCC_CC ?= i686-elf-gcc
GCCFLAGS = $(filter-out --target=i386-pc-win32 -mno-stack-arg-probe -mstack-alignment=4,$1) -mpreferred-stack-boundary=2 \
	-fleading-underscore -fno-pic -fno-delete-null-pointer-checks --param=min-pagesize=0  # data at address 0
ifdef GCC
compile = $(GCC_CC) $(call GCCFLAGS,$1) $(DEPFLAGS) -S $< -o $@.s && $(PY) tools/gas2coff.py $@.s $@.s && \
	$(CLANG) --target=i386-pc-win32 -march=i386 -c $@.s -o $@
else
compile = $(CLANG) $1 $(DEPFLAGS) -c $< -o $@
endif

# llrm's QB runtime (graphics, sound, the screen), src/qbrt/llrm: its portable part, then DOS's
QBRT := src/qbrt/llrm
QBRT_PORTABLE := $(addprefix qb/,console.c cntext.c cngfx.c gfx.c gline.c gcirc.c gpaint.c gputget.c gdraw.c play.c music.c \
	screen.c scrfn.c sleep.c sticks.c timer.c varptr.c error.c rtinit.c nhstutil.c nheap.c)
QBRT_DOS_C := $(QBRT_PORTABLE) $(addprefix qb/,dos/m32/ad.c posix/block.c dev/dos/gfxdev.c \
	$(addprefix dev/dos/m32/,clock.c con.c key.c prn.c text.c tone.c))
QBRT_DOS_ASM := $(addprefix qb/dev/dos/m32/,bios.asm gdcirc.asm gdfill.asm gdline.asm gdscan.asm gdxfer.asm ports.asm \
	tick.asm) qb/dos/m32/block.asm
QBRT_DOS_INC := -Isrc/qbrt/include $(addprefix -I$(QBRT)/qb/,dos/m32 . dev dev/dos/m32 dev/dos) -I$(QBRT)/os/x86-m32
# the runtime's asm names, as Watcom's pragmas give them, for clang's C
NAMES := $(B)/qbrt/names.h

CORE_C := $(shell find src/core src/lib -name '*.c' -not -path 'src/core/ui/*') $(wildcard src/plat/dos32/*.c) src/boot.c
UI_C := $(wildcard src/core/ui/*.c)
DOS_ASM := $(wildcard src/plat/dos32/*.asm)
ASM_OBJ := $(DOS_ASM:src/%.asm=$(B)/obj/%.obj) $(QBRT_DOS_ASM:%.asm=$(B)/qbrt/%.obj)
CORE_OBJ := $(CORE_C:src/%.c=$(B)/obj/%.obj) $(QBRT_DOS_C:%.c=$(B)/qbrt/%.obj) $(B)/obj/qbrt/qbrt.obj \
	$(B)/obj/qbrt/qbrt_dos.obj $(ASM_OBJ)
BASIC_OBJ := $(CORE_OBJ) $(B)/obj/app/basic32.obj
IDE_OBJ := $(CORE_OBJ) $(UI_C:src/%.c=$(B)/obj/%.obj) $(B)/obj/app/ide32.obj
DOS_EXE := $(B)/basic32.exe $(B)/ide32.exe $(B)/DOS32A.EXE $(B)/BASIC.HLP
# the release: the programs by their DOS names, with the games (src/plat/wasm/web/demos)
DOS_GAMES := src/plat/wasm/web/demos/GORILLA.BAS src/plat/wasm/web/demos/NIBBLES.BAS

all: $(DOS_EXE)

$(NAMES): $(QBRT)/qb/dev/dos/m32/device.h $(QBRT)/qb/dev/dos/m32/devsym.h
	@mkdir -p $(@D)
	sed -n 's/^#pragma aux \([a-z_0-9]*\) "\(B\$$[A-Z0-9]*\)"/#pragma redefine_extname \1 \2/p' $^ > $@
	echo '#pragma redefine_extname music_tick B$$MUSICTICK' >> $@
	echo 'void music_tick(void); /* gcc renames a declaration, not a first definition */' >> $@

$(B)/qbrt/%.obj: $(QBRT)/%.c $(NAMES)
	@mkdir -p $(@D)
	$(call compile,$(subst -std=c23,-std=gnu11,$(CFLAGS)) -w -D__cdecl= -include $(NAMES) $(QBRT_DOS_INC))
$(B)/qbrt/%.obj: $(QBRT)/%.asm | $(JWASM)
	@mkdir -p $(@D)
	$(JWASM) -q -coff -I$(QBRT)/qb/dev/dos/m32 -Fo$@ $<
$(B)/obj/qbrt/%.obj: src/qbrt/%.c $(NAMES)
	@mkdir -p $(@D)
	$(call compile,$(CFLAGS) -D__cdecl= -include $(NAMES) $(subst -I,-isystem ,$(QBRT_DOS_INC)))
$(B)/obj/core/ui/%.obj: src/core/ui/%.c
	@mkdir -p $(@D)
	$(call compile,$(CFLAGS) -DAC=$(AC))
$(B)/obj/%.obj: src/%.c
	@mkdir -p $(@D)
	$(call compile,$(CFLAGS))
$(B)/obj/%.obj: src/%.asm | $(JWASM)
	@mkdir -p $(@D)
	$(JWASM) -q -zf0 -Fo$@ $<

$(B)/basic32.exe: $(BASIC_OBJ) tools/doslink.py | $(JWASM) $(JWLINK)
	$(PY) tools/doslink.py $@ $(BASIC_OBJ)
$(B)/ide32.exe: $(IDE_OBJ) tools/doslink.py | $(JWASM) $(JWLINK)
	$(PY) tools/doslink.py $@ $(IDE_OBJ)
$(B)/DOS32A.EXE: $(DOS32A)/dos32a.exe tools/dos32a_quiet.py
	$(PY) tools/dos32a_quiet.py $< $@
$(B)/BASIC.HLP: src/basic.hlp
	cp $< $@
dos-zip: $(DOS_EXE) $(DOS_GAMES)
	mkdir -p $(B)/release
	cp $(B)/ide32.exe $(B)/release/IDE32.EXE
	cp $(B)/basic32.exe $(B)/release/BASIC32.EXE
	cp $(B)/DOS32A.EXE $(B)/BASIC.HLP $(DOS_GAMES) $(B)/release/
	cd $(B)/release && zip -q -X ../QuickrBasic-dos.zip IDE32.EXE BASIC32.EXE DOS32A.EXE BASIC.HLP GORILLA.BAS NIBBLES.BAS

-include $(shell find $(B) -name '*.d' 2>/dev/null)

# ---- The tests ----

TOOLS_TEST := tools/cfmt_test.py tools/gas2coff_test.py tools/conformance_test.py
test: $(DOS_EXE) | $(DOSBOX_BIN)
	sh tests/gen/dfa.sh
	sh tests/gen/messages.sh
	$(PY) tools/dosrun_test.py
	$(PY) tools/libcalls_test.py
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/regress_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/conformance.py
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/fold_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/cost_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/gfx_test.py
	IDE_EXE=$(B)/ide32.exe $(PY) tools/ide_test.py

host-test: build/host/basic build/host/unit_tests build/host/golden_dump build/hostperf/basic
	for t in $(TOOLS_TEST); do $(PY) $$t || exit 1; done
	build/host/unit_tests
	sh tests/host/longpath.sh build/host/basic
	sh tests/host/dosname.sh build/host/basic
	sh tests/host/check.sh build/host/basic
	$(PY) tools/golden.py
	BASIC_EXE=build/host/basic $(PY) tools/regress_test.py build/host/basic
	BASIC_EXE=build/host/basic $(PY) tools/conformance.py
	$(PY) tools/fold_test.py build/host/basic
	$(PY) tools/pterm_test.py build/host/basic
	$(PY) tools/bench_test.py

# ---- The host build: the same core on this machine (plat/host/pfpu.c: the x87's on x86-64, C's elsewhere) ----

SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
HOST_CC ?= $(CLANG)$(if $(SDK), -isysroot $(SDK))
HOST_CFLAGS := -std=c23 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Wno-unused-parameter -Werror=undef -DARENA_AT_ZERO=0 -DPFIL_PATH=4096 -DEXEC_COUNT \
	-Isrc/plat/host/include -Isrc -Isrc/core
HOST_C := $(shell find src/core -name '*.c' -not -path 'src/core/ui/*') src/core/ui/tree.c src/core/ui/indent.c \
	$(wildcard src/plat/host/*.c) src/plat/synth/synth.c src/boot.c
QBRT_HOST_C := $(QBRT_PORTABLE) $(addprefix qb/,posix/block.c posix/ad.c posix/mathops.c dev/headless/headless.c \
	dev/host/video.c dev/host/vgadata.c dev/host/tick.c) shared/posix/os.c
QBRT_HOST_INC := -Isrc/qbrt/include $(addprefix -I$(QBRT)/,qb/posix qb qb/dev qb/dev/headless qb/dev/host shared/posix)
# play.c's waits for the tick come to qbrt_idle: BREAK, and the tick in the tests
QBRT_IDLE := '-DQB_IDLE()=do { extern void qbrt_idle(void); qbrt_idle(); } while (0)'

# host_rules DIR,CFLAGS[,RTFLAGS]: the core and the runtime for this machine in DIR, its objects DIR_OBJ;
# RTFLAGS the runtime's own and its glue's (src/qbrt)
define host_rules
$1/obj/qbrt/%.o: src/qbrt/%.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $2 $3 $$(subst -I,-isystem ,$$(QBRT_HOST_INC)) $$(DEPFLAGS) -c $$< -o $$@
$1/obj/%.o: src/%.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $2 $$(APP_FLAGS) $$(DEPFLAGS) -c $$< -o $$@
$1/qbrt/%.o: $$(QBRT)/%.c
	@mkdir -p $$(@D)
	$$(HOST_CC) $(filter -O%,$2) $3 $$(VIEW_REAL) -std=gnu11 -w -D__cdecl= $$(QBRT_IDLE) $$(QBRT_HOST_INC) \
		$$(DEPFLAGS) -c $$< -o $$@
$1_OBJ := $$(HOST_C:src/%.c=$1/obj/%.o) $$(QBRT_HOST_C:%.c=$1/qbrt/%.o) $1/obj/qbrt/qbrt.o $1/obj/qbrt/qbrt_host.o
-include $$(shell find $1 -name '*.d' 2>/dev/null)
endef
$(eval $(call host_rules,build/host,$(HOST_CFLAGS)))
# tools/bench.py's: optimized, no op count; QB_INSNS reports the instructions it ran
$(eval $(call host_rules,build/hostperf,$(filter-out -O1 -g -DEXEC_COUNT,$(HOST_CFLAGS)) -O2))

host: build/host/basic
build/host/basic: $(build/host_OBJ) build/host/obj/app/basic32.o build/host/obj/plat/host/app/term.o
	$(HOST_CC) $^ -lm -o $@
build/hostperf/basic: $(build/hostperf_OBJ) build/hostperf/obj/app/basic32.o build/hostperf/obj/plat/host/app/insns.o
	$(HOST_CC) $^ -lm -o $@

# ---- The Mac's: the IDE in a window, build/mac/QuickrBasic.app, universal (arm64 and x86-64, macOS 13 on); mac-zip,
# the app zipped. SDL 3 is linked in, built by tools/deps.sh sdl. The display is plat/view's, as the browser's; wrap.h
# is ld's --wrap ----

SDL_DIR := build/deps/sdl3
SDL_LIB := $(SDL_DIR)/lib/libSDL3.a
$(SDL_LIB): tools/deps.sh
	sh tools/deps.sh sdl
SDL_LIBS = $(SDL_LIB) $(shell PKG_CONFIG_PATH=$(SDL_DIR)/lib/pkgconfig pkg-config --static --libs-only-other sdl3 2>/dev/null)
MAC := build/mac
MAC_ARCH := -arch arm64 -arch x86_64 -mmacosx-version-min=13.0
MAC_CFLAGS := $(filter-out -O1 -DEXEC_COUNT,$(HOST_CFLAGS)) -O2 -DAC=$(AC) $(MAC_ARCH)
$(eval $(call host_rules,$(MAC),$(MAC_CFLAGS),$(MAC_ARCH) -include src/plat/view/wrap.h -DQBRT_SCREEN=1))
$(MAC)/qbrt/qb/dev/host/video.o: VIEW_REAL := -DVIEW_REAL_TEXT
$(MAC)/qbrt/qb/dev/headless/headless.o: VIEW_REAL := -DVIEW_REAL_TONE
$(MAC)/obj/plat/sdl/%.o: APP_FLAGS := -isystem $(SDL_DIR)/include
$(patsubst src/%.c,$(MAC)/obj/%.o,$(wildcard src/plat/sdl/*.c src/plat/sdl/app/*.c)): | $(SDL_LIB)
$(MAC)/obj/app/ide32.o: APP_FLAGS := -Dmain=qb_main
MAC_OBJ := $($(MAC)_OBJ) $(patsubst src/%.c,$(MAC)/obj/%.o,$(filter-out src/core/ui/tree.c src/core/ui/indent.c,\
	$(wildcard src/core/ui/*.c)) $(wildcard src/plat/view/*.c) $(wildcard src/plat/sdl/*.c)) $(MAC)/obj/app/ide32.o

APP := $(MAC)/QuickrBasic.app
DEMOS := $(wildcard src/plat/wasm/web/demos/*.BAS)

mac: $(APP)/Contents/Info.plist
$(MAC)/QuickrBasic: $(MAC_OBJ) $(MAC)/obj/plat/sdl/app/main.o $(SDL_LIB)
	$(HOST_CC) $(MAC_ARCH) $(filter %.o,$^) $(SDL_LIBS) -lm -o $@
# the bundle, signed ad hoc (Apple silicon runs nothing unsigned)
$(APP)/Contents/Info.plist: src/plat/sdl/Info.plist $(MAC)/QuickrBasic src/basic.hlp $(DEMOS)
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources/demos
	cp $(MAC)/QuickrBasic $(APP)/Contents/MacOS/
	cp src/basic.hlp $(APP)/Contents/Resources/BASIC.HLP
	cp $(DEMOS) $(APP)/Contents/Resources/demos/
	cp $< $@
	codesign --force -s - $(APP)
mac-zip: mac
	cd $(MAC) && ditto -c -k --keepParent QuickrBasic.app QuickrBasic-mac.zip
# each architecture's half (x86-64's under Rosetta, where there is one)
mac-test: mac
	$(PY) tools/mac_test.py $(APP)/Contents/MacOS/QuickrBasic arm64
	if arch -x86_64 /usr/bin/true 2>/dev/null; then $(PY) tools/mac_test.py $(APP)/Contents/MacOS/QuickrBasic x86_64; fi

UNIT_OBJ := $(patsubst tests/unit/%.c,build/host/unit/%.o,$(wildcard tests/unit/*.c))
build/host/unit/%.o: tests/unit/%.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(DEPFLAGS) -c $< -o $@
build/host/unit_tests: $(build/host_OBJ) $(UNIT_OBJ)
	$(HOST_CC) $^ -lm -o $@
build/host/golden/dump.o: tests/golden/dump.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(DEPFLAGS) -c $< -o $@
build/host/golden_dump: $(build/host_OBJ) build/host/golden/dump.o
	$(HOST_CC) $^ -lm -o $@

# ---- WebAssembly: the host's platform where it is portable, src/plat/wasm for the rest. Stock clang and
# wasm-ld; longjmp is wasm's exceptions, the memory shared with the page's thread (src/plat/wasm/include/wasm.h) ----

# the music's tick, a second: the page's thread fires it (wasm.mjs's MUSIC_HZ)
WASM_MUSIC_HZ := 100
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld) wasm-ld)
WASM_TARGET := --target=wasm32-unknown-unknown -ffreestanding -mbulk-memory -mexception-handling -matomics \
	-mllvm -wasm-enable-sjlj
WASM_CFLAGS := $(WASM_TARGET) -std=c23 -O2 -g -Wall -Wextra -Wno-unused-parameter -Werror=undef -DARENA_AT_ZERO=0 \
	-DPFIL_PATH=1024 -DAC=$(AC) -isystem src/plat/wasm/include -isystem src/lib/include -Isrc -Isrc/core
# -MD, not -MMD: the shim's headers are -isystem, and -MMD leaves those out of the dependencies
WASM_DEPFLAGS = -MD -MP -MF $@.d -MT $@
WASM_C := $(filter-out src/plat/host/% src/plat/synth/%,$(HOST_C)) $(addprefix src/plat/host/,pclk.c pfil.c pfpu.c pkey.c pscr.c ports.c) $(wildcard src/plat/wasm/*.c) \
	$(wildcard src/plat/view/*.c)
WASM_QBRT_C := $(filter-out shared/posix/os.c,$(QBRT_HOST_C))
WASM_OBJ := $(WASM_C:src/%.c=build/wasm/obj/%.o) $(WASM_QBRT_C:%.c=build/wasm/qbrt/%.o) build/wasm/obj/qbrt/qbrt.o \
	build/wasm/obj/qbrt/qbrt_wasm.o
# the runtime's text screen is pscr's (src/plat/wasm/rtext.c), its tones the page's too (qbrt_wasm.c); frame and
# render_stack the page's display
WASM_WRAP := gd_set_mode dev_tone dev_voice $(addprefix dev_text_,size cursor move cursor_show get put write scroll)
WASM_LDFLAGS := --no-entry --export=__main_void --export=__wasm_call_ctors --export=__stack_pointer --export=host_tick_fire --import-memory \
	--shared-memory --initial-memory=33554432 --max-memory=1073741824 --stack-first -z stack-size=8388608 \
	$(addprefix --wrap=,$(WASM_WRAP))

build/wasm/obj/qbrt/%.o: src/qbrt/%.c
	@mkdir -p $(@D)
	$(CLANG) $(WASM_CFLAGS) $(subst -I,-isystem ,$(QBRT_HOST_INC)) $(WASM_DEPFLAGS) -c $< -o $@
build/wasm/obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CLANG) $(WASM_CFLAGS) $(WASM_DEPFLAGS) -c $< -o $@
build/wasm/qbrt/%.o: $(QBRT)/%.c
	@mkdir -p $(@D)
	$(CLANG) $(WASM_TARGET) -O2 -std=gnu11 -w -D__cdecl= $(QBRT_IDLE) -DTICKS_PER_MINUTE=$(WASM_MUSIC_HZ)*60000L \
		-isystem src/plat/wasm/include \
		-isystem src/lib/include $(QBRT_HOST_INC) $(WASM_DEPFLAGS) -c $< -o $@
-include $(shell find build/wasm -name '*.d' 2>/dev/null)

WASM_UI_OBJ := $(patsubst src/%.c,build/wasm/obj/%.o,$(filter-out src/core/ui/tree.c src/core/ui/indent.c,\
	$(wildcard src/core/ui/*.c)))

wasm: build/wasm/basic build/wasm/ide.wasm
build/wasm/basic.wasm: $(WASM_OBJ) build/wasm/obj/app/basic32.o
	$(WASM_LD) $(WASM_LDFLAGS) $^ -o $@
build/wasm/ide.wasm: $(WASM_OBJ) $(WASM_UI_OBJ) build/wasm/obj/app/ide32.o
	$(WASM_LD) $(WASM_LDFLAGS) $^ -o $@
build/wasm/basic: build/wasm/basic.wasm
	printf '#!/bin/sh\nexec node "$(CURDIR)/tools/wasm/run.mjs" "$(CURDIR)/$<" "$$@"\n' > $@
	chmod +x $@
# The IDE's page, for GitHub Pages or tools/wasm/serve.py: its scripts, the module, the help, the demos
WEB_SRC := $(wildcard src/plat/wasm/web/*.mjs src/plat/wasm/web/*.js src/plat/wasm/web/*.html src/plat/wasm/web/demos/*)
web: build/web/ide.wasm build/web/synth.wasm build/web/BASIC.HLP $(WEB_SRC:src/plat/wasm/web/%=build/web/%)
# the synthesizer alone, for the page's AudioWorklet (web/synth.mjs)
build/web/synth.wasm: src/plat/wasm/worklet/worklet.c src/plat/synth/synth.c src/plat/synth/synth.h
	@mkdir -p $(@D)
	$(CLANG) --target=wasm32-unknown-unknown -ffreestanding -nostdlib -mbulk-memory -std=c23 -O2 -Wall -Wextra -Isrc \
		-Wl,--no-entry -Wl,--export-dynamic $(filter %.c,$^) -o $@
build/web/ide.wasm: build/wasm/ide.wasm
	@mkdir -p $(@D)
	cp $< $@
build/web/BASIC.HLP: src/basic.hlp
	@mkdir -p $(@D)
	cp $< $@
build/web/%: src/plat/wasm/web/%
	@mkdir -p $(@D)
	cp $< $@

wasm-test: build/wasm/basic build/wasm/ide.wasm
	grep -q 'wasm/include/wasm.h' build/wasm/obj/plat/wasm/pterm.o.d # a changed WasmIrq rebuilds its users
	BASIC_EXE=$< $(PY) tools/regress_test.py $<
	node tools/wasm/frame_test.mjs build/wasm/ide.wasm
	node tools/wasm/play_test.mjs build/wasm/basic.wasm
	BASIC_EXE=$< $(PY) tools/conformance.py

# ---- LTO: the C as bitcode, one module optimized whole by tools/lto.py (LLVM's tools), linked with the asm's
# objects into build/lto. PGO=1: what a training run never calls is cold (tools/pgo.py, build/lto/*.hot) ----

bitcode = $(patsubst build/c/%.obj,build/lto/%.bc,$(filter-out $(ASM_OBJ),$1))
hot = $(if $(PGO),--hot build/lto/$1.hot)

lto: build/lto/basic32.exe build/lto/ide32.exe build/lto/DOS32A.EXE build/lto/BASIC.HLP
build/lto/qbrt/%.bc: $(QBRT)/%.c build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(subst -std=c23,-std=gnu11,$(CFLAGS)) -flto -w -D__cdecl= -include build/c/qbrt/names.h $(QBRT_DOS_INC) \
		$(DEPFLAGS) -c $< -o $@
build/lto/obj/qbrt/%.bc: src/qbrt/%.c build/c/qbrt/names.h
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto -D__cdecl= -include build/c/qbrt/names.h $(subst -I,-isystem ,$(QBRT_DOS_INC)) \
		$(DEPFLAGS) -c $< -o $@
build/lto/obj/core/ui/%.bc: src/core/ui/%.c
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto -DAC=$(AC) $(DEPFLAGS) -c $< -o $@
build/lto/obj/%.bc: src/%.c
	@mkdir -p $(@D)
	$(CLANG) $(CFLAGS) -flto $(DEPFLAGS) -c $< -o $@

build/lto/prof/basic32.obj: $(call bitcode,$(BASIC_OBJ)) $(ASM_OBJ)
build/lto/prof/ide32.obj: $(call bitcode,$(IDE_OBJ)) $(ASM_OBJ)
build/lto/prof/%.obj:
	@mkdir -p $(@D)
	$(PY) tools/lto.py $(CLANG) $@ --profiling $(filter %.bc,$^) -- $(ASM_OBJ)
build/lto/%.hot: build/lto/prof/%.obj $(ASM_OBJ) | $(DOSBOX_BIN)
	$(PY) tools/pgo.py $@ build/lto/prof/$*.exe $(if $(findstring ide,$*),ide) -- $< $(ASM_OBJ)
.SECONDARY: build/lto/prof/basic32.obj build/lto/prof/ide32.obj

build/lto/basic32.exe: $(call bitcode,$(BASIC_OBJ)) $(ASM_OBJ) $(if $(PGO),build/lto/basic32.hot) | $(JWLINK)
	$(PY) tools/lto.py $(CLANG) build/lto/basic32.obj $(call hot,basic32) $(filter %.bc,$^) -- $(ASM_OBJ)
	$(PY) tools/doslink.py $@ build/lto/basic32.obj $(ASM_OBJ)
	$(PY) tools/lto_test.py $@
build/lto/ide32.exe: $(call bitcode,$(IDE_OBJ)) $(ASM_OBJ) $(if $(PGO),build/lto/ide32.hot) | $(JWLINK)
	$(PY) tools/lto.py $(CLANG) build/lto/ide32.obj $(call hot,ide32) $(filter %.bc,$^) -- $(ASM_OBJ)
	$(PY) tools/doslink.py $@ build/lto/ide32.obj $(ASM_OBJ)
build/lto/DOS32A.EXE: $(DOS32A)/dos32a.exe
	$(PY) tools/dos32a_quiet.py $< $@
build/lto/BASIC.HLP: src/basic.hlp
	cp $< $@
-include $(shell find build/lto -name '*.d' 2>/dev/null)

# ---- The rest ----

format:
	git ls-files 'src/*.[ch]' 'tests/*.[ch]' | grep -v -e qbrt/llrm/ -e comp/dfa.h -e io/gen.h -e io/msg.h \
		| xargs $(PY) tools/cfmt.py

clean:
	rm -rf build/c build/gcc build/host build/hostperf build/lto build/wasm build/web
