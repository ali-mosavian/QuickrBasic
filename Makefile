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

.PHONY: test gate fpcheck fpall mathbench size repl style clean

test:
	$(PY) tools/harness.py test
	sh test/tools/size_stale.sh
	sh test/tools/routsize_fp.sh
	sh test/tools/prof_branch.sh
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

style:
	$(PY) tools/asmstyle.py

clean:
	rm -rf build
