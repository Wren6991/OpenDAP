TOP  := dap_integration
DOTF := dap_integration.f
SRCS := $(shell listfiles $(DOTF))

INCDIR := $(shell yosys-config --datdir)/include ../include ../../common/include

.PHONY: clean tb all

all: tb.o

SYNTH_CMD += read_verilog -I ../../../hdl $(shell listfiles $(DOTF));
SYNTH_CMD += write_cxxrtl dut.cpp

dut.cpp: $(SRCS)
	yosys -p "$(SYNTH_CMD)" 2>&1 > cxxrtl.log

clean::
	rm -f dut.cpp cxxrtl.log tb.o

tb.o: dut.cpp tb.cpp
	clang++ -O3 -std=c++14 -Wall $(addprefix -D,$(CDEFINES)) $(addprefix -I,$(INCDIR)) -c tb.cpp -o tb.o
