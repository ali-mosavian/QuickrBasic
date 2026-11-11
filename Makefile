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

.PHONY: x86 ide test gate fpcheck fpall mathbench size repl style clean

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

LLRM_BIN ?= /Users/alim/work/personal/llrm-qbmac/target/release

x86:                            # the DOS .COM, build/x86/basic.com
	mkdir -p build/x86
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -Fl=../../build/x86/main.lst -Fo ../../build/x86/main.obj main.asm
	cd src/x86 && $(LLRM_BIN)/jwasm -q -zf0 -DCSOFF=$$(python3 ../../tools/x86csoff.py ../../build/x86/main.lst) \
		-Fo ../../build/x86/main.obj main.asm   # again, the data at the code's end
	$(LLRM_BIN)/jwlink option quiet format dos com file build/x86/main.obj name build/x86/raw.com
	tail -c +257 build/x86/raw.com > build/x86/basic.com   # jwlink keeps the 100h bytes before org

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
