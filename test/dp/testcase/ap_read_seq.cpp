#include "tb.h"
#include <cstdio>

// Test intent: perform a sequence of pipelined AP reads, and check that each
// read data response aligns with the correct place in the sequence.

ap_read_response read_callback(uint16_t addr) {
	static uint32_t count = 123;
	return {
		.rdata = count++,
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	(void)swd_read(t, DP, DP_REG_DPIDR, id);

	// First read primes the pump, its return is not meaningful.
	uint32_t data;
	(void)swd_read(t, AP, 0, data);

	int n_pipelined_reads = 10;
	for (int i = 0; i < n_pipelined_reads; ++i) {
		(void)swd_read(t, AP, 0, data);
		if ((int)data != 123 + i) {
			printf("Bad data %d for sequence number %d\n", (int)data, i);
			return -1;
		}
	}
	(void)swd_read(t, DP, DP_REG_RDBUF, data);
	if (data != 123 + n_pipelined_reads) {
		printf("Bad data at end.\n");
		return -1;
	}
	return 0;
}
