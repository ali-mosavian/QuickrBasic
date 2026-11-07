# make [TARGET] [FP_BITS=16|24|32] [FPE=n FPM=n]: the float is num_f$(FP_BITS).s;
# for 16, FPE exponent and FPM fraction bits (default 5 and 10, IEEE half).
FP_BITS ?= 24
FPE ?= 5
FPM ?= 10
ifeq ($(filter $(FP_BITS),16 24 32),)
$(error FP_BITS: 16, 24 or 32)
endif
export FP_BITS FPE FPM

PY := python3

.PHONY: test fpcheck mathbench size repl style clean

test:
	$(PY) tools/simtest.py
	sh test/tools/size_stale.sh

fpcheck:
	$(PY) tools/fpcheck.py

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
