# The retired 16-bit asm before the 32-bit one (cbmbasic-bc 316224a): build/asm16/basic.com and
# basic-ide.com, DOS .COM programs.
#
#   make         both
#   make clean

ROOT := $(abspath ../..)
OUT := $(ROOT)/build/asm16
PY ?= python3
AC ?= 1                            # the editor's Tab completion; AC=0 leaves it out
JWASM ?= $(ROOT)/build/deps/bin/jwasm
JWLINK ?= $(ROOT)/build/deps/bin/jwlink
SRC := $(shell find . -name '*.asm' -o -name '*.inc')

.PHONY: all clean
all: $(OUT)/basic.com $(OUT)/basic-ide.com $(OUT)/BASIC.HLP

$(JWASM):
	sh $(ROOT)/tools/deps.sh build
$(JWLINK): $(JWASM)

# com DEFINES,DIR: main.asm assembled twice, the second time with the data placed after the code
# (tools/csoff.py reads the first listing); jwlink keeps the 100h bytes before org, which tail drops
define com
@mkdir -p $2
$(JWASM) -q -zf0 $1 -Fl=$2/main.lst -Fo$2/main.obj main.asm
$(JWASM) -q -zf0 $1 -DCSOFF=$$($(PY) tools/csoff.py $2/main.lst) -Fo$2/main.obj main.asm
$(JWLINK) option quiet format dos com file $2/main.obj name $2/raw.com
tail -c +257 $2/raw.com > $@
endef

$(OUT)/basic.com: $(SRC) | $(JWASM) $(JWLINK)
	$(call com,,$(OUT)/basic)
$(OUT)/basic-ide.com: $(SRC) | $(JWASM) $(JWLINK)
	$(call com,-DIDE=1 -DAC=$(AC),$(OUT)/ide)
$(OUT)/BASIC.HLP: ui/basic.hlp
	cp $< $@

clean:
	rm -rf $(OUT)
