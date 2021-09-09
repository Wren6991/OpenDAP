#include "tb.h"
#include <cstdio>

// Test intent: Check that an AP read error sets STICKYERR and generates a
// FAULT response, and that STICKYERR can then be cleared, and the AP can be
// re-accessed succesfully.

ap_read_response read_callback(uint16_t addr) {
	static uint32_t count = 0;
	if (count == 0) {
		return {
			.rdata = count++ + 123,
			.delay_cycles = 100,
			.err = true
		};
	}
	else {
		return {
			.rdata = count++ + 123,
			.delay_cycles = 0,
			.err = false
		};
	}
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	(void)swd_read(t, DP, DP_REG_DPIDR, id);

	swd_status_t status;
	uint32_t data;
	status = swd_read(t, AP, 0, data);
	if (status != OK) {
		printf("Priming read should give OK\n");
		return -1;
	}
	for (int i = 0; i < 2; ++i) {
		status = swd_read(t, AP, 0, data);
		if (status != WAIT) {
			printf("AP access should be stalled.\n");
			return 1;
		}
	}

	idle_clocks(t, 100);
	status = swd_read(t, AP, 0, data);
	if (status != FAULT) {
		printf("Failed AP access should give FAULT.\n");
		return -1;
	}

	status = swd_write(t, DP, DP_REG_SELECT, DP_BANK_CTRL_STAT);
	if (status != OK) {
		printf("Should never get FAULT on SELECT\n");
		return -1;
	}
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	if (status != OK) {
		printf("Should never get FAULT on CTRL/STAT read\n");
		return -1;
	}
	if (!(data & 0x20)) {
		printf("STICKYERR should be set.\n");
		return -1;
	}
	if (data & 0x92) {
		printf("No other sticky flags should be set.\n");
		return -1;
	}

	(void)swd_write(t, DP, DP_REG_ABORT, 0x4);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	if (status != OK || data & 0x20) {
		printf("STICKYERR should be cleared by ABORT\n");
		return -1;
	}

	(void)swd_read(t, AP, 0, data);
	status = swd_read(t, DP, DP_REG_RDBUF, data);

	// The reads return a count sequence from 123 (including the first failing
	// write). +1 confirms the AP didn't see any reads between the faulting
	// one and this one.
	if (status != OK || data != 123 + 1) {
		printf("Bad final readback\n");
		return -1;
	}
	return 0;
}