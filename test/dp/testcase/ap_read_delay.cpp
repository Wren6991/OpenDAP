#include "tb.h"
#include <cstdio>

// Test intent: check that AP register address and APSEL make it to the AP
// interface, and read data makes it back.

ap_read_response read_callback(uint16_t addr) {
	return {
		.rdata = 0xcafef00du,
		.delay_cycles = 100,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);

	uint32_t data;
	status = swd_read(t, AP, 0, data);
	if (status != OK) {
		printf("Priming read should give OK\n");
		return -1;
	}
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	if (status != WAIT) {
		printf("First stalled AP read attempt should give WAIT\n");
		return -1;
	}
	// We have ORUNDETECT off, so no sticky errors. Just repeated WAIT.
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	if (status != WAIT) {
		printf("Second stalled AP read attempt should give WAIT\n");
		return -1;
	}
	idle_clocks(t, 100);

	status = swd_read(t, DP, DP_REG_RDBUF, data);
	if (status != OK) {
		printf("Should have unblocked by now\n");
		return -1;
	}
	if (data != 0xcafef00du) {
		printf("Bad data\n");
		return -1;
	}

	return 0;
}
