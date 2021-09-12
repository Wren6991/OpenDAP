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

	swclk_prev = false;
	read_callback = NULL;
	write_callback = NULL;
	last_read_response.delay_cycles = 0;
	last_write_response.delay_cycles = 0;

	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	vcd.buffer.clear();
}

void tb::set_ap_read_callback(ap_read_callback cb) {
	read_callback = cb;
}

void tb::set_ap_write_callback(ap_write_callback cb) {
	write_callback = cb;
}

void tb::set_swclk(bool swclk) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swclk.set<bool>(swclk);
}

void tb::set_swdi(bool swdi) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdi.set<bool>(swdi);
}

bool tb::get_swdo() {
	// Pullup on bus, so return 1 if pin tristated.
	return static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdo__en.get<bool>() ?
		static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdo.get<bool>() : true;
}

void tb::set_instid(uint8_t instid) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_instid.set<uint8_t>(instid);
}

void tb::step() {
	cxxrtl_design::p_opendap__sw__dp *dp = static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut);

	uint16_t ap_addr = dp->p_ap__addr.get<uint16_t>() | dp->p_ap__sel.get<uint16_t>() << 6;
	bool ap_wen = dp->p_ap__wen.get<bool>();
	bool ap_ren = dp->p_ap__ren.get<bool>();
	uint32_t ap_wdata = dp->p_ap__wdata.get<uint32_t>();

	dp->step();
	dp->step();
	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	waves_fd.flush();
	vcd.buffer.clear();

	// Field AP accesses using testcase callbacks if available, and provide AP
	// bus responses with correct timing based on callback results.
	if (!swclk_prev && dp->p_swclk.get<bool>()) {
		dp->p_ap__err.set<bool>(0);
		if (last_read_response.delay_cycles > 0) {
			--last_read_response.delay_cycles;
			if (last_read_response.delay_cycles == 0) {
				dp->p_ap__rdata.set<uint32_t>(last_read_response.rdata);
				dp->p_ap__err.set<bool>(last_read_response.err);
				dp->p_ap__rdy.set<bool>(1);
			}
		}
		if (last_write_response.delay_cycles > 0) {
			--last_write_response.delay_cycles;
			if (last_write_response.delay_cycles == 0) {
				dp->p_ap__err.set<bool>(last_write_response.err);
				dp->p_ap__rdy.set<bool>(1);
			}
		}
		if (ap_ren && read_callback) {
			last_read_response = read_callback(ap_addr);
			if (last_read_response.delay_cycles == 0) {
				dp->p_ap__rdata.set<uint32_t>(last_read_response.rdata);
				dp->p_ap__err.set<bool>(last_read_response.err);
			}
			else {
				dp->p_ap__rdy.set<bool>(0);
			}
		}
		else if (ap_wen && write_callback) {
			last_write_response = write_callback(ap_addr, ap_wdata);
			if (last_write_response.delay_cycles == 0) {
				dp->p_ap__err.set<bool>(last_write_response.err);
			}
			else {
				dp->p_ap__rdy.set<bool>(0);
			}
		}
	}
	swclk_prev = dp->p_swclk.get<bool>();
}
