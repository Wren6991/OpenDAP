TESTCASES := $(wildcard *.cpp)
TEST_EXCECS := $(addprefix build/,$(patsubst %.cpp,%,$(TESTCASES)))
TESTS_RUN := $(addprefix run.,$(patsubst %.cpp,%,$(TESTCASES)))

.PHONY: all clean
.SECONDARY:
all: $(TESTS_RUN)

build/%: %.cpp ../tb/tb.o
	mkdir -p build
	clang++ -O0 -std=c++14 -Wall -Wextra -I ../include -I $(shell yosys-config --datdir)/include $< ../tb/tb.o -o $@

run.%: build/%
	./$<

../tb/tb.o:
	make -C ../tb

clean:
	make -C ../tb clean
	rm -rf build
