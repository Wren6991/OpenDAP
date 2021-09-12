TESTCASES := $(wildcard *.cpp)
TEST_EXCECS := $(addprefix build/,$(patsubst %.cpp,%,$(TESTCASES)))
TESTS_RUN := $(addprefix run.,$(patsubst %.cpp,%,$(TESTCASES)))

INCDIR := $(shell yosys-config --datdir)/include ../include ../../common/include

.PHONY: all clean
.SECONDARY:
all: $(TESTS_RUN)

build/%: %.cpp ../tb/tb.o ../../common/swd_util.cpp
	mkdir -p build
	clang++ -O3 -std=c++14 -Wall $(addprefix -I,$(INCDIR)) $< ../../common/swd_util.cpp ../tb/tb.o -o $@

run.%: build/%
	./$<

# Bit of a hack to trigger tb rebuild when verilog or testbench changes
../tb/tb.o: ../tb/tb.cpp $(shell listfiles ../tb/dap_integration.f)
	make -C ../tb

clean:
	make -C ../tb clean
	rm -rf build
