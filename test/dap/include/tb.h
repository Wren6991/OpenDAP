#pragma once

#include <string>
#include <cstdint>
#include <fstream>
#include <backends/cxxrtl/cxxrtl.h>
#include <backends/cxxrtl/cxxrtl_vcd.h>

#include "swd_util.h"

struct apb_read_response {
	uint32_t rdata;
	int delay_cycles;
	bool err;
};

struct apb_write_response {
	int delay_cycles;
	bool err;
};

typedef apb_read_response (*apb_read_callback)(uint32_t addr);

typedef apb_write_response (*apb_write_callback)(uint32_t addr, uint32_t data);

class tb {
public:
	tb(std::string vcdfile);
	void set_apb_read_callback(apb_read_callback cb);
	void set_apb_write_callback(apb_write_callback cb);

	void set_swclk(bool swclk);
	void set_swdi(bool swdi);
	bool get_swdo();
	void set_instid(uint8_t instid);
	void step();
private:
	int vcd_sample;
	bool swclk_prev;
	apb_read_callback read_callback;
	apb_read_response last_read_response;
	apb_write_callback write_callback;
	apb_write_response last_write_response;
	std::ofstream waves_fd;
	cxxrtl::vcd_writer vcd;
	cxxrtl::module *dut;
};

#define tb_assert(cond, ...) if (!(cond)) {printf(__VA_ARGS__); exit(-1);}
