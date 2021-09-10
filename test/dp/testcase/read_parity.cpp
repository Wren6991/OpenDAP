#include "tb.h"
#include <cstdio>

// Test intent: check that read parity bit is set correctly for some AP reads
// of random-like data.

static const int n_data = 20;
static const uint32_t random_data[n_data] = {
	0x91211e6du,
	0x42f8265du,
	0x35bd7f5du,
	0x5d5e63dfu,
	0xa0ff86d8u,
	0x0f11d233u,
	0xd4ced650u,
	0x7d8ae15fu,
	0xf6b22c66u,
	0x078d7694u,
	0x334870b8u,
	0x5abcac27u,
	0xa2d17176u,
	0xe23fa911u,
	0x7e2611d9u,
	0x06a631bbu,
	0x3c4a794cu,
	0x1977ad68u,
	0xaa513352u,
	0xeaec14afu
};

ap_read_response read_callback(uint16_t addr) {
	static int count = 0;
	return {
		.rdata = random_data[count++ % n_data],
		.delay_cycles = 0,
		.err = false
	};
}

bool even_parity(uint32_t data) {
	bool accum = false;
	for (int i = 0; i < 32; ++i)
		accum ^= (data >> i) & 0x1;
	return accum;
}

// Bits 31:0 of return are data. Bit 32 is parity.
uint64_t swd_read_with_parity(tb &t, uint8_t header) {
	put_bits(t, &header, 8);
	hiz_clocks(t, 4);
	uint8_t rx;
	uint64_t accum;
	for (int i = 0; i < 33; ++i) {
		get_bits(t, &rx, 1);
		accum = (accum >> 1) | ((uint64_t)rx << 63);
	}
	hiz_clocks(t, 1);
	return accum >> 31;
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t discard;
	(void)swd_read(t, DP, DP_REG_DPIDR, discard);
	(void)swd_read(t, AP, 0, discard);

	for (int i = 0; i < n_data; ++i) {
		uint64_t data_plus_parity = swd_read_with_parity(t, swd_header(AP, 1, 0));
		bool parity = data_plus_parity >> 32;
		uint32_t data = data_plus_parity & 0xffffffffu;
		tb_assert(even_parity(data) == parity, "Mismatch for RX data %08x with parity %d\n", data, (int)parity);
	}

	return 0;
}
