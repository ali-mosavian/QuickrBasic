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

.PHONY: test fpcheck fpall mathbench size repl style clean

test:
	$(PY) tools/simtest.py
	sh test/tools/size_stale.sh
	sh test/tools/routsize_fp.sh
	sh test/tools/prof_branch.sh
	sh test/tools/f8tab_stale.sh

fpcheck:
	$(PY) tools/fpcheck.py

fpall:                          # every float, exhaustive for f8 (2/5 overflows INT(-3.06))
	FP_BITS=8 FPE=5 FPM=2 EXH=1 $(PY) tools/fpcheck.py
	FP_BITS=8 FPE=2 FPM=5 EXH=1 $(PY) tools/fpcheck.py
	FP_BITS=8 FPE=4 FPM=3 EXH=1 $(PY) tools/fpcheck.py
	FP_BITS=16 FPE=5 FPM=10 $(PY) tools/fpcheck.py
	FP_BITS=16 FPE=7 FPM=8 $(PY) tools/fpcheck.py
	FP_BITS=24 $(PY) tools/fpcheck.py
	FP_BITS=32 $(PY) tools/fpcheck.py
	FP_BITS=16 FAST_SQR=1 $(PY) tools/fpcheck.py sqr
	FP_BITS=24 FAST_SQR=1 $(PY) tools/fpcheck.py sqr
	FP_BITS=32 FAST_SQR=1 $(PY) tools/fpcheck.py sqr

mathbench:
	$(PY) tools/mathbench.py

size:
	$(PY) tools/size.py -m

repl:
	tools/repl.sh

style:
	$(PY) tools/check_style.py

clean:
	rm -rf build
