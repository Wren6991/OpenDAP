TOP  := opendap_sw_dp
DOTF := ../../../hdl/opendap_sw_dp.f
SRCS := $(shell listfiles $(DOTF))

.PHONY: clean tb all

all: tb.o

SYNTH_CMD += read_verilog -I ../../../hdl $(shell listfiles $(DOTF));
# SYNTH_CMD += chparam -set DPIDR 32'hdeadbeef $(TOP);
SYNTH_CMD += write_cxxrtl dut.cpp

dut.cpp: $(SRCS)
	yosys -p "$(SYNTH_CMD)" 2>&1 > cxxrtl.log

clean::
	rm -f dut.cpp cxxrtl.log tb.o

tb.o: dut.cpp tb.cpp
	clang++ -O3 -std=c++14 -Wall $(addprefix -D,$(CDEFINES)) -I $(shell yosys-config --datdir)/include -I ../include -c tb.cpp -o tb.o
