#pragma once

#include <string>
#include <cstdint>
#include <fstream>
#include <backends/cxxrtl/cxxrtl.h>
#include <backends/cxxrtl/cxxrtl_vcd.h>


// Instantiate new testbench, write your stimulus for the testcase.

class tb {
public:
	tb(std::string vcdfile);
	void set_swclk(bool swclk);
	void set_swdi(bool swdi);
	bool get_swdo();
	void step();
private:
	int vcd_sample;
	std::ofstream waves_fd;
	cxxrtl::vcd_writer vcd;
	cxxrtl::module *dut;
};

// Convenience functions:

void put_bits(tb &t, const uint8_t *tx, int n_bits);
void get_bits(tb &t, uint8_t *rx, int n_bits);
void send_dormant_to_swd(tb &t);

