#include "tb.h"

#include <fstream>
#include <cstdint>

#include "dut.cpp"
#include <backends/cxxrtl/cxxrtl_vcd.h>

tb::tb(std::string vcdfile) {
	// Raw pointer... CXXRTL doesn't give us the type declaration wihout also
	// giving us non-inlined implementation, and I'm not very good at C++, so
	// we do this shit
	cxxrtl_design::p_opendap__sw__dp *dp = new cxxrtl_design::p_opendap__sw__dp;
	dut = dp;
 
	waves_fd.open(vcdfile);
	cxxrtl::debug_items all_debug_items;
	dp->debug_info(all_debug_items);
	vcd.timescale(1, "us");
	vcd.add(all_debug_items);
	vcd_sample = 0;

	dp->p_rst__n.set<bool>(false);
	dp->step();
	dp->p_rst__n.set<bool>(true);
	dp->p_ap__rdy.set<bool>(true);
	dp->step();

	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	vcd.buffer.clear();
}

void tb::set_swclk(bool swclk) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swclk.set<bool>(swclk);
}

void tb::set_swdi(bool swdi) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdi.set<bool>(swdi);
}

bool tb::get_swdo() {
	return static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdo.get<bool>();
}

void tb::step() {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->step();
	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	vcd.buffer.clear();
}

void put_bits(tb &t, const uint8_t *tx, int n_bits) {
	uint8_t shifter;
	for (int i = 0; i < n_bits; ++i) {
		if (i % 8 == 0)
			shifter = tx[i / 8];
		else
			shifter >>= 1;
		t.set_swclk(0);
		t.set_swdi(shifter & 1u);
		t.step();
		t.set_swclk(1);
		t.step();
		t.set_swclk(0);
	}
}
