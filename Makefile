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

.PHONY: all test host host-test wasm wasm-test web mac mac-zip mac-test dos-zip lto format clean shaders
all:

# one run builds both: two would clone into the same directory at once under -j
$(DEPS)/jwasm: tools/deps.sh
	sh tools/deps.sh build
$(DEPS)/jwlink: $(DEPS)/jwasm
$(DEPS)/dosbox-x: tools/deps.sh
	sh tools/deps.sh dosbox
$(DEPS)/glslang: tools/deps.sh
	sh tools/deps.sh shaders
$(DEPS)/spirv-cross: $(DEPS)/glslang

# gfx's shader for each host's GPU, committed: run after an edit of src/plat/gpu/draw.*
shaders: $(DEPS)/spirv-cross
	python3 tools/shaders.py

# ---- The DOS build: clang for i386 (COFF), the start-up and OS layer by jwasm, linked by jwlink ----

# BASIC.HLP: help/*.md and status.txt, compiled (tools/mkhelp.py)
HELP := build/BASIC.HLP
$(HELP): tools/mkhelp.py $(shell find help -type f)
	@mkdir -p $(@D)
	$(PY) tools/mkhelp.py help -o $@

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
QBRT_PORTABLE := $(addprefix qb/,console.c cntext.c cngfx.c gfx.c gline.c gcirc.c gpaint.c gputget.c gdraw.c play.c music.c song.c \
	screen.c scrfn.c sleep.c sticks.c timer.c varptr.c error.c rtinit.c nhstutil.c nheap.c)
QBRT_DOS_C := $(QBRT_PORTABLE) $(addprefix qb/,dos/m32/ad.c posix/block.c dev/pixmem.c dev/dos/gfxdev.c \
	$(addprefix dev/dos/m32/,clock.c con.c key.c prn.c sb.c text.c tone.c))
QBRT_DOS_ASM := $(addprefix qb/dev/dos/m32/,bios.asm gdblit.asm gdcirc.asm gdfill.asm gdline.asm gdscan.asm gdxfer.asm ports.asm \
	sbisr.asm tick.asm) qb/dos/m32/block.asm
QBRT_DOS_INC := -Isrc/qbrt/include $(addprefix -I$(QBRT)/qb/,dos/m32 . dev dev/dos/m32 dev/dos) -I$(QBRT)/os/x86-m32
# the runtime's asm names, as Watcom's pragmas give them, for clang's C
NAMES := $(B)/qbrt/names.h

CORE_C := $(shell find src/core src/lib -name '*.c' -not -path 'src/core/ui/*') $(wildcard src/plat/dos32/*.c) \
	src/plat/synth/synth.c src/plat/audio/paud.c src/boot.c
UI_C := $(wildcard src/core/ui/*.c)
DOS_ASM := $(wildcard src/plat/dos32/*.asm)
ASM_OBJ := $(DOS_ASM:src/%.asm=$(B)/obj/%.obj) $(QBRT_DOS_ASM:%.asm=$(B)/qbrt/%.obj)
CORE_OBJ := $(CORE_C:src/%.c=$(B)/obj/%.obj) $(QBRT_DOS_C:%.c=$(B)/qbrt/%.obj) $(B)/obj/qbrt/qbrt.obj \
	$(B)/obj/qbrt/qbrt_dos.obj $(ASM_OBJ)
BASIC_OBJ := $(CORE_OBJ) $(B)/obj/app/basic32.obj
IDE_OBJ := $(CORE_OBJ) $(UI_C:src/%.c=$(B)/obj/%.obj) $(B)/obj/app/ide32.obj
DOS_EXE := $(B)/basic32.exe $(B)/ide32.exe $(B)/DOS32A.EXE $(B)/BASIC.HLP
# the release: the programs by their DOS names, with the games (src/plat/wasm/web/demos)
DOS_GAMES := $(addprefix src/plat/wasm/web/demos/,GORILLA.BAS NIBBLES.BAS MOTHLITE.BAS RUSTLINE.BAS CHARLIE.BAS CHARLIE.DAT)
# CHARLIE.BAS imports its package and the engine, the barkley and engine folders beside it
DOS_MODULES := $(shell find src/plat/wasm/web/demos/barkley src/plat/wasm/web/demos/engine -name '*.bas')

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
$(B)/BASIC.HLP: $(HELP)
	cp $< $@
dos-zip: $(DOS_EXE) $(DOS_GAMES) $(DOS_MODULES)
	mkdir -p $(B)/release
	cp $(B)/ide32.exe $(B)/release/IDE32.EXE
	cp $(B)/basic32.exe $(B)/release/BASIC32.EXE
	cp $(B)/DOS32A.EXE $(B)/BASIC.HLP $(DOS_GAMES) $(B)/release/
	rm -rf $(B)/release/barkley $(B)/release/engine
	cp -R src/plat/wasm/web/demos/barkley $(B)/release/barkley
	cp -R src/plat/wasm/web/demos/engine $(B)/release/engine
	cd $(B)/release && zip -q -X -r ../QuickrBasic-dos.zip IDE32.EXE BASIC32.EXE DOS32A.EXE BASIC.HLP GORILLA.BAS NIBBLES.BAS MOTHLITE.BAS RUSTLINE.BAS CHARLIE.BAS CHARLIE.DAT barkley engine

-include $(shell find $(B) -name '*.d' 2>/dev/null)

# ---- The tests ----

TOOLS_TEST := tools/cfmt_test.py tools/basic_src_test.py tools/libcalls_test.py tools/gas2coff_test.py tools/conformance_test.py tools/mkhelp_test.py tools/golden_test.py tools/qbx_test.py
test: $(DOS_EXE) | $(DOSBOX_BIN)
	sh tests/gen/dfa.sh
	sh tests/gen/messages.sh
	$(PY) tools/dosrun_test.py
	$(PY) tools/sb_test.py
	$(PY) tools/libcalls_test.py
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/regress_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/conformance.py
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/fold_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/cost_test.py $(B)/basic32.exe
	BASIC_EXE=$(B)/basic32.exe $(PY) tools/gfx_test.py
	IDE_EXE=$(B)/ide32.exe $(PY) tools/ide_test.py

# the native ABI's test extension (tests/ext): its glue, then a slice for each platform; native.sh packs them
build/ext/qxtest_qx.c: tests/ext/qxtest.bas tools/qbx.py
	@mkdir -p $(@D)
	$(PY) tools/qbx.py glue qxtest $(@D) $<
QXTEST := tests/ext/qxtest.c build/ext/qxtest_qx.c
QX_CFLAGS := -std=c23 -O2 -Wall -Wextra -ffreestanding -fno-math-errno -Isrc/qx -Ibuild/ext
build/host/ext/qxtest.so: $(QXTEST) src/qx/qx.h
	@mkdir -p $(@D)
	$(HOST_CC) $(QX_CFLAGS) $(HOST_SLICE) $(QXTEST) -o $@
# wasm's a side module, with the IDE's features; DOS's a DLL, made an image by qbx.py pack
WASM_FEATURES := atomics,bulk-memory,bulk-memory-opt,mutable-globals,sign-ext,call-indirect-overlong,nontrapping-fptoint,reference-types,multivalue
build/wasm/ext/qxtest.wasm: $(QXTEST) src/qx/qx.h
	@mkdir -p $(@D)
	$(CLANG) --target=wasm32-unknown-unknown -matomics -mbulk-memory -fPIC -fvisibility=hidden $(QX_CFLAGS) \
		-c tests/ext/qxtest.c -o $(@D)/qxtest.o
	$(CLANG) --target=wasm32-unknown-unknown -matomics -mbulk-memory -fPIC -fvisibility=hidden $(QX_CFLAGS) \
		-c build/ext/qxtest_qx.c -o $(@D)/qxtest_qx.o
	$(WASM_LD) -shared --experimental-pic --shared-memory --features=$(WASM_FEATURES) -o $@ $(@D)/qxtest.o $(@D)/qxtest_qx.o
LLD_LINK ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/lld-link) lld-link)
DOS_SLICE_CC = $(CLANG) --target=i386-pc-win32 -march=i386 -fno-asynchronous-unwind-tables $(QX_CFLAGS)
$(B)/ext/qxtest.dll: $(QXTEST) src/qx/qx.h
	@mkdir -p $(@D)
	$(DOS_SLICE_CC) -c tests/ext/qxtest.c -o $(@D)/qxtest.obj
	$(DOS_SLICE_CC) -c build/ext/qxtest_qx.c -o $(@D)/qxtest_qx.obj
	$(LLD_LINK) /dll /noentry /nodefaultlib /out:$@ $(@D)/qxtest.obj $(@D)/qxtest_qx.obj
# gfx (src/ext/gfx): its glue from its parts' interfaces, scene's to come with 3D. Its core is linked
# into the unit tests with tests/ext/gfx; the runtime's font and palette (vgadata.c) are already there
GFX_PARTS := $(addprefix src/ext/gfx/,display.bas surface.bas pen.bas draw.bas)
GFX_C := $(shell find src/ext/gfx -name '*.c')
GFX_INC := -Isrc/ext/gfx -Isrc
build/ext/gfx_qx.c: $(GFX_PARTS) tools/qbx.py
	@mkdir -p $(@D)
	$(PY) tools/qbx.py glue gfx $(@D) $(GFX_PARTS)
GFX_UNIT_OBJ := $(GFX_C:src/ext/gfx/%.c=build/host/gfx/%.o) build/host/gfx/gfx_qx.o \
	$(patsubst tests/ext/gfx/%.c,build/host/gfx/test/%.o,$(wildcard tests/ext/gfx/*.c))
build/host/gfx/%.o: src/ext/gfx/%.c | build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(QX_CFLAGS) -g $(GFX_INC) $(DEPFLAGS) -c $< -o $@
build/host/gfx/gfx_qx.o: build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(QX_CFLAGS) -g $(DEPFLAGS) -c $< -o $@
build/host/gfx/test/%.o: tests/ext/gfx/%.c | build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) -Isrc/qx -Ibuild/ext $(GFX_INC) -Itests/unit $(DEPFLAGS) -c $< -o $@

# gfx.qbx for this machine, in lib/ beside basic: its slice the core, its glue and its own font; it
# needs the host's minor 1
GFX_SLICE_C := $(GFX_C) build/ext/gfx_qx.c $(QBRT)/qb/dev/host/vgadata.c
build/host/ext/gfx.so: $(GFX_SLICE_C) $(shell find src/ext/gfx -name '*.h') src/qx/qx.h
	@mkdir -p $(@D)
	$(HOST_CC) $(QX_CFLAGS) $(HOST_SLICE) $(GFX_INC) $(GFX_SLICE_C) -o $@
build/host/lib/gfx.qbx: $(GFX_PARTS) build/host/ext/gfx.so tools/qbx.py
	@mkdir -p $(@D)
	$(PY) tools/qbx.py pack $@ --minor 1 $(GFX_PARTS) $(HOST_TAG)=build/host/ext/gfx.so

host-test: build/host/basic build/host/unit_tests build/host/golden_dump build/hostperf/basic build/host/ext/qxtest.so \
		build/host/lib/gfx.qbx
	for t in $(TOOLS_TEST); do $(PY) $$t || exit 1; done
	build/host/unit_tests
	sh tests/host/longpath.sh build/host/basic
	sh tests/host/dosname.sh build/host/basic
	sh tests/host/check.sh build/host/basic
	sh tests/host/import.sh build/host/basic
	PY=$(PY) sh tests/host/native.sh build/host/basic build/host/ext/qxtest.so $(HOST_TAG)
	sh tests/host/gfx.sh build/host/basic
	$(PY) tools/golden.py
	BASIC_EXE=build/host/basic $(PY) tools/regress_test.py build/host/basic
	BASIC_EXE=build/host/basic $(PY) tools/conformance.py
	BASIC_EXE=build/host/basic $(PY) tools/qr_test.py
	$(PY) tools/fold_test.py build/host/basic
	$(PY) tools/mothlite_test.py build/host/basic
	$(PY) tools/charlie_test.py build/host/basic
	$(PY) tools/mid2play_test.py build/host/basic
	$(PY) tools/pterm_test.py build/host/basic
	$(PY) tools/bench_test.py

# ---- The host build: the same core on this machine (plat/host/pfpu.c: the x87's on x86-64, C's elsewhere) ----

SDK := $(shell xcrun --show-sdk-path 2>/dev/null)
HOST_CC ?= $(CLANG)$(if $(SDK), -isysroot $(SDK))
HOST_CFLAGS := -std=c23 -D_DEFAULT_SOURCE -O1 -g -Wall -Wextra -Wno-unused-parameter -Werror=undef -DARENA_AT_ZERO=0 -DPFIL_PATH=4096 -DEXEC_COUNT \
	-Isrc/plat/host/include -Isrc -Isrc/core
HOST_C := $(shell find src/core -name '*.c' -not -path 'src/core/ui/*') src/core/ui/tree.c src/core/ui/indent.c \
	src/core/ui/helpfile.c src/core/ui/helpdoc.c \
	$(wildcard src/plat/host/*.c) src/plat/synth/synth.c src/plat/audio/paud.c src/boot.c
QBRT_HOST_C := $(QBRT_PORTABLE) $(addprefix qb/,posix/block.c posix/ad.c posix/mathops.c dev/headless/headless.c dev/pixmem.c \
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

# dlopen (plat/host/pext.c): in libc on the Mac, libdl on glibc before 2.34
HOST_OS := $(shell uname -s)
HOST_DL := $(if $(filter Linux,$(HOST_OS)),-ldl)
# this machine's slice in a .qbx, and how a slice is linked: the Mac's needs libSystem, though it uses none of it
HOST_TAG := $(if $(filter Darwin,$(HOST_OS)),macos,linux-x86_64)
# the Mac's slice universal, as the app is: its x86-64 half (Rosetta, an Intel Mac) loads it too
HOST_SLICE := -shared -fPIC -ffreestanding -fvisibility=hidden $(if $(filter Linux,$(HOST_OS)),-nostdlib) \
	$(if $(filter Darwin,$(HOST_OS)),-arch arm64 -arch x86_64 -mmacosx-version-min=13.0)

# Quickr's standard library, stdlib/*.bas, in lib/ beside each interpreter: where a module is looked for last
STDLIB := $(wildcard stdlib/*.bas)
build/%/lib/.stdlib: $(STDLIB)
	mkdir -p $(@D) && $(if $^,cp $^ $(@D) &&) touch $@

host: build/host/basic build/host/lib/gfx.qbx
build/host/basic: $(build/host_OBJ) build/host/obj/app/basic32.o build/host/obj/plat/host/app/term.o | build/host/lib/.stdlib
	$(HOST_CC) $^ -lm $(HOST_DL) -o $@
build/hostperf/basic: $(build/hostperf_OBJ) build/hostperf/obj/app/basic32.o build/hostperf/obj/plat/host/app/insns.o | build/hostperf/lib/.stdlib
	$(HOST_CC) $^ -lm $(HOST_DL) -o $@

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
DEMOS := $(shell find src/plat/wasm/web/demos -type f)

mac: $(APP)/Contents/Info.plist
$(MAC)/QuickrBasic: $(MAC_OBJ) $(MAC)/obj/plat/sdl/app/main.o $(SDL_LIB)
	$(HOST_CC) $(MAC_ARCH) $(filter %.o,$^) $(SDL_LIBS) -lm -o $@
# the bundle, signed ad hoc (Apple silicon runs nothing unsigned)
# Quickr's library in Resources/lib: psys_self puts the app there, and codesign takes only code in MacOS
GFX_DEMO := src/ext/gfx/demo/GFXDEMO.BAS
$(APP)/Contents/Info.plist: src/plat/sdl/Info.plist $(MAC)/QuickrBasic $(HELP) $(DEMOS) $(STDLIB) build/host/lib/gfx.qbx $(GFX_DEMO)
	rm -rf $(APP)/Contents/Resources/demos $(APP)/Contents/Resources/lib
	mkdir -p $(APP)/Contents/MacOS $(APP)/Contents/Resources/demos $(APP)/Contents/Resources/lib
	cp $(MAC)/QuickrBasic $(APP)/Contents/MacOS/
	cp build/host/lib/gfx.qbx $(APP)/Contents/Resources/lib/
	cp $(HELP) $(APP)/Contents/Resources/BASIC.HLP
	cp -R src/plat/wasm/web/demos/. $(GFX_DEMO) $(APP)/Contents/Resources/demos/
	cp $(STDLIB) $(APP)/Contents/Resources/lib/
	cp $< $@
	codesign --force -s - $(APP)
# gfx's first slice in the Mac's window: tests/ext/gfx/show.c draws CHARLIE's frame in the IDE's place,
# the core linked in beside the runtime's font (vgadata.c); gfx-shot runs it from here, QB_SHOT takes it
GFX_MAC_OBJ := $(GFX_C:src/ext/gfx/%.c=$(MAC)/gfx/%.o) $(MAC)/gfx/gfx_qx.o $(MAC)/gfx/test/charlie.o \
	$(MAC)/gfx/test/show.o
$(MAC)/gfx/%.o: src/ext/gfx/%.c | build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(MAC_ARCH) $(QX_CFLAGS) $(GFX_INC) $(DEPFLAGS) -c $< -o $@
$(MAC)/gfx/gfx_qx.o: build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(MAC_ARCH) $(QX_CFLAGS) $(DEPFLAGS) -c $< -o $@
$(MAC)/gfx/test/%.o: tests/ext/gfx/%.c | build/ext/gfx_qx.c
	@mkdir -p $(@D)
	$(HOST_CC) $(MAC_CFLAGS) -Isrc/qx -Ibuild/ext $(GFX_INC) $(DEPFLAGS) -c $< -o $@
$(MAC)/gfxshow: $(filter-out $(MAC)/obj/app/ide32.o,$(MAC_OBJ)) $(GFX_MAC_OBJ) $(MAC)/obj/plat/sdl/app/main.o $(SDL_LIB)
	$(HOST_CC) $(MAC_ARCH) $(filter %.o,$^) $(SDL_LIBS) -lm -o $@
gfx-shot: $(MAC)/gfxshow
	QB_SHOT=$(MAC)/charlie.ppm@30 $<

mac-zip: mac
	cd $(MAC) && ditto -c -k --keepParent QuickrBasic.app QuickrBasic-mac.zip
# each architecture's half (x86-64's under Rosetta, where there is one)
mac-test: mac $(MAC)/ext/qxtest.qbx
	$(PY) tools/mac_test.py $(APP)/Contents/MacOS/QuickrBasic arm64 $(MAC)/ext/qxtest.qbx
	if arch -x86_64 /usr/bin/true 2>/dev/null; then \
		$(PY) tools/mac_test.py $(APP)/Contents/MacOS/QuickrBasic x86_64 $(MAC)/ext/qxtest.qbx; fi
$(MAC)/ext/qxtest.qbx: tests/ext/qxtest.bas build/host/ext/qxtest.so tools/qbx.py
	@mkdir -p $(@D)
	$(PY) tools/qbx.py pack $@ $< macos=build/host/ext/qxtest.so

# The editor's pictures in docs/ide-design.md, taken from the Mac's window
feel-shots: mac
	$(PY) tools/diagrams/shots.py $(APP)/Contents/MacOS/QuickrBasic docs/img/feel

# The tutorial's pictures of the synthesizer, drawn from its own samples (docs/play-tutorial.md)
build/host/synthdump: tools/diagrams/synthdump.c src/plat/synth/synth.c src/plat/synth/synth.h
	$(HOST_CC) -std=c23 -O2 -Wall -Wextra -Isrc $(filter %.c,$^) -o $@
sound-diagrams: build/host/synthdump
	$(PY) tools/diagrams/sound.py $< docs/img/play

UNIT_OBJ := $(patsubst tests/unit/%.c,build/host/unit/%.o,$(wildcard tests/unit/*.c))
# the editor's, for tests/unit/editor_test.c
UNIT_UI_OBJ := $(patsubst src/%.c,build/host/obj/%.o,$(filter-out $(HOST_C),$(wildcard src/core/ui/*.c)))
$(UNIT_UI_OBJ): APP_FLAGS := -DAC=$(AC)
build/host/unit/%.o: tests/unit/%.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(DEPFLAGS) -c $< -o $@
build/host/test.hlp: tools/mkhelp.py $(wildcard tests/help/*)
	$(PY) tools/mkhelp.py tests/help -o $@
build/host/unit_tests: $(build/host_OBJ) $(UNIT_UI_OBJ) $(UNIT_OBJ) $(GFX_UNIT_OBJ) build/host/obj/lib/div64.o \
	build/host/test.hlp
	$(HOST_CC) $(filter-out %.hlp,$^) -lm $(HOST_DL) -o $@
build/host/golden/dump.o: tests/golden/dump.c
	@mkdir -p $(@D)
	$(HOST_CC) $(HOST_CFLAGS) $(DEPFLAGS) -c $< -o $@
build/host/golden_dump: $(build/host_OBJ) build/host/golden/dump.o
	$(HOST_CC) $^ -lm $(HOST_DL) -o $@

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
WASM_C := $(filter-out src/plat/host/% src/plat/synth/% src/plat/audio/%,$(HOST_C)) $(addprefix src/plat/host/,pclk.c pfil.c pfpu.c pgfx.c pkey.c pscr.c ports.c) $(wildcard src/plat/wasm/*.c) \
	$(wildcard src/plat/view/*.c)
WASM_QBRT_C := $(filter-out shared/posix/os.c,$(QBRT_HOST_C))
WASM_OBJ := $(WASM_C:src/%.c=build/wasm/obj/%.o) $(WASM_QBRT_C:%.c=build/wasm/qbrt/%.o) build/wasm/obj/qbrt/qbrt.o \
	build/wasm/obj/qbrt/qbrt_wasm.o
# the runtime's text screen is pscr's (src/plat/wasm/rtext.c), its tones the page's too (qbrt_wasm.c); frame and
# render_stack the page's display
WASM_WRAP := gd_set_mode dev_tone dev_voice $(addprefix dev_text_,size cursor move cursor_show get put write scroll pages_reset pages copy)
WASM_LDFLAGS := --no-entry --export=__main_void --export=__wasm_call_ctors --export=__stack_pointer --export=host_tick_fire --import-memory \
	--export-table --growable-table \
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
WEB_SRC := $(wildcard src/plat/wasm/web/*.mjs src/plat/wasm/web/*.js src/plat/wasm/web/*.html) \
	$(shell find src/plat/wasm/web/demos -type f)
web: build/web/ide.wasm build/web/synth.wasm build/web/BASIC.HLP build/web/demos/MODULES build/web/lib/MODULES \
	$(WEB_SRC:src/plat/wasm/web/%=build/web/%)
# Quickr's library, fetched to /lib beside C:\BASIC.EXE, where a module is looked for last; one name a line
build/web/lib/MODULES: $(STDLIB)
	@mkdir -p $(@D)
	cp $^ $(@D)
	cd stdlib && ls *.bas > $(CURDIR)/$@
# the demos' modules, one path a line: the page fetches them from here, as the folders cannot be listed
build/web/demos/MODULES: $(shell find src/plat/wasm/web/demos -mindepth 2 -name '*.bas')
	@mkdir -p $(@D)
	cd src/plat/wasm/web/demos && find . -mindepth 2 -name '*.bas' | sed 's|^\./||' | sort > $(CURDIR)/$@
# the synthesizer alone, for the page's AudioWorklet (web/synth.mjs)
build/web/synth.wasm: src/plat/wasm/worklet/worklet.c src/plat/synth/synth.c src/plat/synth/synth.h
	@mkdir -p $(@D)
	$(CLANG) --target=wasm32-unknown-unknown -ffreestanding -nostdlib -mbulk-memory -std=c23 -O2 -Wall -Wextra -Isrc \
		-Wl,--no-entry -Wl,--export-dynamic $(filter %.c,$^) -o $@
build/web/ide.wasm: build/wasm/ide.wasm
	@mkdir -p $(@D)
	cp $< $@
build/web/BASIC.HLP: $(HELP)
	@mkdir -p $(@D)
	cp $< $@
build/web/%: src/plat/wasm/web/%
	@mkdir -p $(@D)
	cp $< $@

wasm-test: build/wasm/basic build/wasm/ide.wasm build/wasm/ext/qxtest.wasm
	grep -q 'wasm/include/wasm.h' build/wasm/obj/plat/wasm/pterm.o.d # a changed WasmIrq rebuilds its users
	PY=$(PY) sh tests/host/native.sh $< build/wasm/ext/qxtest.wasm wasm32 audio
	BASIC_EXE=$< $(PY) tools/regress_test.py $<
	node tools/wasm/frame_test.mjs build/wasm/ide.wasm
	node tools/wasm/play_test.mjs build/wasm/basic.wasm
	node tools/wasm/scan_test.mjs build/wasm/basic.wasm
	node tools/wasm/upload_test.mjs
	node tools/wasm/retrace_test.mjs build/wasm/basic.wasm
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
build/lto/BASIC.HLP: $(HELP)
	cp $< $@
-include $(shell find build/lto -name '*.d' 2>/dev/null)

# ---- The rest ----

format:
	git ls-files 'src/*.[ch]' 'tests/*.[ch]' | grep -v -e qbrt/llrm/ -e comp/dfa.h -e io/gen.h -e io/msg.h \
		| xargs $(PY) tools/cfmt.py

clean:
	rm -rf build/c build/gcc build/host build/hostperf build/lto build/wasm build/web
