#pragma once

#include <string>
#include <cstdint>
#include <fstream>
#include <backends/cxxrtl/cxxrtl.h>
#include <backends/cxxrtl/cxxrtl_vcd.h>

struct ap_read_response {
	uint32_t rdata;
	int delay_cycles;
	bool err;
};

struct ap_write_response {
	int delay_cycles;
	bool err;
};

typedef ap_read_response (*ap_read_callback)(uint16_t addr);

typedef ap_write_response (*ap_write_callback)(uint16_t addr, uint32_t data);

class tb {
public:
	tb(std::string vcdfile);
	void set_ap_read_callback(ap_read_callback cb);
	void set_ap_write_callback(ap_write_callback cb);

	void set_swclk(bool swclk);
	void set_swdi(bool swdi);
	bool get_swdo();
	void set_instid(uint8_t instid);
	void step();
private:
	int vcd_sample;
	bool swclk_prev;
	ap_read_callback read_callback;
	ap_read_response last_read_response;
	ap_write_callback write_callback;
	ap_write_response last_write_response;
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

const int DP_REG_DPIDR = 0;
const int DP_REG_ABORT = 0;
const int DP_REG_CTRL_STAT = 1;
const int DP_REG_DLCR = 1;
const int DP_REG_TARGETID = 1;
const int DP_REG_DLPIDR = 1;
const int DP_REG_EVENTSTAT = 1;
const int DP_REG_RESEND = 2;
const int DP_REG_SELECT = 2;
const int DP_REG_RDBUF = 3;
const int DP_REG_TARGETSEL = 3;

const int DP_BANK_CTRL_STAT = 0;
const int DP_BANK_DLCR = 1;
const int DP_BANK_TARGETID = 2;
const int DP_BANK_DLPIDR = 3;
const int DP_BANK_EVENTSTAT = 4;

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
