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
	// Pullup on bus, so return 1 if pin tristated.
	return static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdo__en.get<bool>() ?
		static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_swdo.get<bool>() : true;
}

void tb::set_instid(uint8_t instid) {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->p_instid.set<uint8_t>(instid);
}

void tb::step() {
	static_cast<cxxrtl_design::p_opendap__sw__dp*>(dut)->step();
	vcd.sample(vcd_sample++);
	waves_fd << vcd.buffer;
	vcd.buffer.clear();
}

// Convenience functions

void put_bits(tb &t, const uint8_t *tx, int n_bits) {
	uint8_t shifter;
	for (int i = 0; i < n_bits; ++i) {
		if (i % 8 == 0)
			shifter = tx[i / 8];
		else
			shifter >>= 1;
		t.set_swdi(shifter & 1u);
		t.step();
		t.set_swclk(1);
		t.step();
		t.set_swclk(0);
	}
}

#include <cstdio>

void get_bits(tb &t, uint8_t *rx, int n_bits) {
	uint8_t shifter;
	for (int i = 0; i < n_bits; ++i) {
		t.step();
		bool sample = t.get_swdo();
		t.set_swdi(sample);
		t.set_swclk(1);
		t.step();
		t.set_swclk(0);

		shifter = (shifter >> 1) | (sample << 7);
		if (i % 8 == 7)
			rx[i / 8] = shifter;
	}
	if (n_bits % 8 != 0) {
		rx[n_bits / 8] = shifter >> (8 - n_bits % 8);
	}
}

void hiz_clocks(tb &t, int n_bits) {
	for (int i = 0; i < n_bits; ++i) {
		t.set_swdi(t.get_swdo());
		t.step();
		t.set_swclk(1);
		t.step();
		t.set_swclk(0);
	}
}

void idle_clocks(tb &t, int n_bits) {
	t.set_swdi(0);
	for (int i = 0; i < n_bits; ++i) {
		t.step();
		t.set_swclk(1);
		t.step();
		t.set_swclk(0);
	}
}

static const uint8_t seq_dormant_to_swd[] = {
	// Resync the LFSR (which is 7 bits, can't produce 8 1s)
	0xff,
	// A 0-bit, then 127 bits of LFSR output
	0x92, 0xf3, 0x09, 0x62,
	0x95, 0x2d, 0x85, 0x86,
	0xe9, 0xaf, 0xdd, 0xe3,
	0xa2, 0x0e, 0xbc, 0x19,
	// Four zero-bits, 8 bits of select sequence
	0xa0, 0x01
	// Total 148 bits.
};

void send_dormant_to_swd(tb &t) {
	put_bits(t, seq_dormant_to_swd, 148);
}

static const uint8_t seq_line_reset[7] = {
	// 50 1s, at least 2 0s.
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x03
};

void swd_line_reset(tb &t) {
	put_bits(t, seq_line_reset, 52);
}


uint8_t swd_header(ap_dp_t ap_ndp, bool read_nwrite, uint8_t addr) {
	addr &= 0x3;
	return
		1u << 0 |                                         // start
		(unsigned)ap_ndp << 1 |                           // APnDP
		(unsigned)read_nwrite << 2 |                      // RnW
		addr << 3 |                                       // A[3:2]
		((addr >> 1) ^ (addr & 1) ^ (unsigned)read_nwrite ^ (unsigned)ap_ndp) << 5 | // parity
		0u << 6 |                                         // stop
		1u << 7;   
}

void swd_targetsel(tb &t, uint32_t id) {
	uint8_t header = swd_header(DP, 0, 3);
	put_bits(t, &header, 8);
	// No response to TARGETSEL.
	hiz_clocks(t, 5);
	uint8_t txbuf[4];
	for (int i = 0; i < 4; ++i)
		txbuf[i] = (id >> i * 8) & 0xff;
	put_bits(t, txbuf, 32);
	// Parity
	txbuf[0] = 0;
	for (int i = 0; i < 32; ++i)
		txbuf[0] ^= (id >> i) & 0x1;
	put_bits(t, txbuf, 1);
}


swd_status_t swd_read(tb &t, ap_dp_t ap_ndp, uint8_t addr, uint32_t &data) {
	uint8_t header = swd_header(ap_ndp, 1, addr);
	put_bits(t, &header, 8);
	hiz_clocks(t, 1);
	uint8_t status;
	get_bits(t, &status, 3);
	if (status != OK) {
		hiz_clocks(t, 1);
		data = 0;
		return (swd_status_t)status;
	}
	uint8_t rxbuf[4];
	get_bits(t, rxbuf, 32);
	data = 0;
	for (int i = 0; i < 4; ++i)
		data = (data >> 8) | ((uint32_t)rxbuf[i] << 24);
	// Just discard parity bit -- have a separate test for that.
	get_bits(t, rxbuf, 1);
	// Turnaround for next packet header
	hiz_clocks(t, 1);
	return (swd_status_t)status;
}

swd_status_t swd_write(tb &t, ap_dp_t ap_ndp, uint8_t addr, uint32_t data) {
	uint8_t header = swd_header(ap_ndp, 0, addr);
	put_bits(t, &header, 8);
	hiz_clocks(t, 1);
	uint8_t status;
	get_bits(t, &status, 3);
	if (status != OK) {
		hiz_clocks(t, 1);
		data = 0;
		return (swd_status_t)status;
	}
	hiz_clocks(t, 1);
	uint8_t txbuf[4];
	for (int i = 0; i < 4; ++i)
		txbuf[i] = (data >> i * 8) & 0xff;
	put_bits(t, txbuf, 32);
	// Parity
	txbuf[0] = 0;
	for (int i = 0; i < 32; ++i)
		txbuf[0] ^= (data >> i) & 0x1;
	put_bits(t, txbuf, 1);
	return (swd_status_t)status;
}
