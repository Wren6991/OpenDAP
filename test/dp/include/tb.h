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

// Constants

static const uint32_t DPIDR_EXPECTED = 0xdeadbeefu;
static const uint32_t TARGETID_EXPECTED = 0xbaadf00du; 

enum ap_dp_t {
	DP = 0,
	AP = 1
};

enum swd_status_t {
	OK           = 1,
	WAIT         = 2,
	FAULT        = 4,
	DISCONNECTED = 7
};

// Convenience functions

void put_bits(tb &t, const uint8_t *tx, int n_bits);
void get_bits(tb &t, uint8_t *rx, int n_bits);
void hiz_clocks(tb &t, int n_bits);
void idle_clocks(tb &t, int n_bits);

uint8_t swd_header(ap_dp_t ap_ndp, bool read_nwrite, uint8_t addr);

void send_dormant_to_swd(tb &t);
void swd_line_reset(tb &t);
void swd_targetsel(tb &t, uint32_t id);

swd_status_t swd_read(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t &data);
swd_status_t swd_write(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t data);
swd_status_t swd_read_orun(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t &data);
swd_status_t swd_write_orun(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t data);
