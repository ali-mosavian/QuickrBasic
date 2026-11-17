# The retired 32-bit asm the C was ported from: build/asm32/basic32.exe and ide32.exe, DOS/32A programs.
#
#   make         both
#   make test    the regressions, QB 4.5 conformance and the editor, as the C build runs them
#   make syms    syms.inc and ui/syms.inc rewritten from the modules (tools/syms.py)
#   make clean

ROOT := $(abspath ../..)
OUT := $(ROOT)/build/asm32
PY ?= python3
AC ?= 1                            # the editor's Tab completion; AC=0 leaves it out
JWASM ?= $(ROOT)/build/deps/bin/jwasm
JWLINK ?= $(ROOT)/build/deps/bin/jwlink
DOSBOX_BIN ?= $(ROOT)/build/deps/bin/dosbox-x
DOS32A ?= $(ROOT)/third_party/dos32a
export DOSBOX_BIN

MODS := main vm/exec vm/flow vm/calls vm/builtins vm/sys vm/bin vm/bc comp/lex comp/decl comp/block comp/ctl \
	comp/comp comp/expr comp/fold comp/link comp/list mem/far mem/alloc mem/tab mem/prog mem/sym mem/scope mem/str \
	mem/arr io/io io/errmsg io/diag io/srd io/repl io/chan io/fio vm/using io/plat float/flt float/ftr float/fpu \
	float/dbl vm/dbg vm/event vm/chain
UI := ui/scr ui/mouse ui/compl ui/text ui/edit ui/draw ui/menu ui/dlg ui/lmap ui/dbg ui/dpane ui/win ui/view \
	ui/theme ui/syntax ui/ui
INC := $(wildcard *.inc */*.inc)
# regions.obj last: the zeroed state runs from main.asm's g_bss_s to its g_bss_e
BASIC_OBJ := $(MODS:%=$(OUT)/basic32/%.obj) $(OUT)/basic32/regions.obj
IDE_OBJ := $(MODS:%=$(OUT)/ide32/%.obj) $(UI:%=$(OUT)/ide32/%.obj) $(OUT)/ide32/regions.obj
LINK = $(JWLINK) option quiet format os2 le option stub=$(DOS32A)/stub32a.exe file $(subst $(eval) ,$(comma),$1) name $@
comma := ,

.PHONY: all test syms clean
all: $(OUT)/basic32.exe $(OUT)/ide32.exe $(OUT)/DOS32A.EXE $(OUT)/BASIC.HLP

$(JWASM) $(JWLINK):
	sh $(ROOT)/tools/deps.sh build
$(DOSBOX_BIN):
	sh $(ROOT)/tools/deps.sh dosbox

# each module on its own, the shared header x86f.inc
$(OUT)/basic32/%.obj: %.asm $(INC) | $(JWASM)
	@mkdir -p $(@D)
	$(JWASM) -q -zf0 -Fl=$(@:.obj=.lst) -Fo$@ $<
$(OUT)/ide32/%.obj: %.asm $(INC) | $(JWASM)
	@mkdir -p $(@D)
	$(JWASM) -q -zf0 -DIDE=1 -DAC=$(AC) -Fl=$(@:.obj=.lst) -Fo$@ $<

$(OUT)/basic32.exe: $(BASIC_OBJ) | $(JWLINK)
	$(call LINK,$(BASIC_OBJ))
$(OUT)/ide32.exe: $(IDE_OBJ) | $(JWLINK)
	$(call LINK,$(IDE_OBJ))
$(OUT)/DOS32A.EXE: $(DOS32A)/dos32a.exe
	$(PY) $(ROOT)/tools/dos32a_quiet.py $< $@
$(OUT)/BASIC.HLP: ui/basic.hlp
	cp $< $@

test: all | $(DOSBOX_BIN)
	sh tools/syms_test.sh
	BASIC_EXE=$(OUT)/basic32.exe $(PY) $(ROOT)/tools/regress_test.py $(OUT)/basic32.exe
	BASIC_EXE=$(OUT)/basic32.exe $(PY) $(ROOT)/tools/fold_test.py $(OUT)/basic32.exe
	BASIC_EXE=$(OUT)/basic32.exe $(PY) $(ROOT)/tools/conformance.py
	IDE_EXE=$(OUT)/ide32.exe $(PY) $(ROOT)/tools/ide_test.py

syms:
	$(PY) tools/syms.py
	$(PY) tools/syms.py IDE

clean:
	rm -rf $(OUT)
