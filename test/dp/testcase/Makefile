all:
	make -C ../tb
	clang++ -O3 -std=c++14 -I ../include -I $(shell yosys-config --datdir)/include read_dpidr.cpp ../tb/tb.o -o read_dpidr
