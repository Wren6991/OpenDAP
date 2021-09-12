#include "tb.h"

#include <fstream>
#include <cstdint>

#include "dut.cpp"
#include <backends/cxxrtl/cxxrtl_vcd.h>

tb::tb(std::string vcdfile) {
	cxxrtl_design::p_dap__integration *dap = new cxxrtl_design::p_dap__integration;
	dut = dap;
 
	waves_fd.open(vcdfile);
	cxxrtl::debug_items all_debug_items;
	dap->debug_info(all_debug_items);
	vcd.timescale(1, "us");
	vcd.add(all_debug_items);
	vcd_sample = 0;

	dap->p_rst__n.set<bool>(false);
	dap->step();
	dap->p_rst__n.set<bool>(true);
	dap->p_dst__pready.set<bool>(true);
	dap->step();

	swclk_prev = false;
	read_callback = NULL;
	write_callback = NULL;
	last_read_response.delay_cycles = 0;
	last_write_response.delay_cycles = 0;

	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	vcd.buffer.clear();
}

void tb::set_apb_read_callback(apb_read_callback cb) {
	read_callback = cb;
}

void tb::set_apb_write_callback(apb_write_callback cb) {
	write_callback = cb;
}

void tb::set_swclk(bool swclk) {
	static_cast<cxxrtl_design::p_dap__integration*>(dut)->p_swclk.set<bool>(swclk);
}

void tb::set_swdi(bool swdi) {
	static_cast<cxxrtl_design::p_dap__integration*>(dut)->p_swdi.set<bool>(swdi);
}

bool tb::get_swdo() {
	// Pullup on bus, so return 1 if pin tristated.
	return static_cast<cxxrtl_design::p_dap__integration*>(dut)->p_swdo__en.get<bool>() ?
		static_cast<cxxrtl_design::p_dap__integration*>(dut)->p_swdo.get<bool>() : true;
}

void tb::set_instid(uint8_t instid) {
	static_cast<cxxrtl_design::p_dap__integration*>(dut)->p_instid.set<uint8_t>(instid);
}

void tb::step() {
	cxxrtl_design::p_dap__integration *dp = static_cast<cxxrtl_design::p_dap__integration*>(dut);

	// Respond only to setup phase, then assume that access phase happens.
	// Less state to track.
	bool apb_start = dp->p_dst__psel.get<bool>() && !dp->p_dst__penable.get<bool>();
	uint32_t paddr = dp->p_dst__paddr.get<uint32_t>();
	bool pwrite = dp->p_dst__pwrite.get<bool>();
	uint32_t pwdata = dp->p_dst__pwdata.get<uint32_t>();

	dp->step();
	dp->step();
	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	waves_fd.flush();
	vcd.buffer.clear();

	// Field APB accesses using testcase callbacks if available, and provide
	// bus responses with correct timing based on callback results.
	if (!swclk_prev && dp->p_swclk.get<bool>()) {
		if (last_read_response.delay_cycles > 0) {
			--last_read_response.delay_cycles;
			if (last_read_response.delay_cycles == 0) {
				dp->p_dst__prdata.set<uint32_t>(last_read_response.rdata);
				dp->p_dst__pslverr.set<bool>(last_read_response.err);
				dp->p_dst__pready.set<bool>(1);
			}
		}
		if (last_write_response.delay_cycles > 0) {
			--last_write_response.delay_cycles;
			if (last_write_response.delay_cycles == 0) {
				dp->p_dst__pslverr.set<bool>(last_write_response.err);
				dp->p_dst__pready.set<bool>(1);
			}
		}
		if (apb_start && !pwrite && read_callback) {
			last_read_response = read_callback(paddr);
			if (last_read_response.delay_cycles == 0) {
				dp->p_dst__prdata.set<uint32_t>(last_read_response.rdata);
				dp->p_dst__pslverr.set<bool>(last_read_response.err);
			}
			else {
				dp->p_dst__pready.set<bool>(0);
			}
		}
		else if (apb_start && pwrite && write_callback) {
			last_write_response = write_callback(paddr, pwdata);
			if (last_write_response.delay_cycles == 0) {
				dp->p_dst__pslverr.set<bool>(last_write_response.err);
			}
			else {
				dp->p_dst__pready.set<bool>(0);
			}
		}
	}
	swclk_prev = dp->p_swclk.get<bool>();
}
