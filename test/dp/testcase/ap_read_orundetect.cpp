#include "tb.h"
#include <cstdio>

// Test intent: check that WAIT->FAULT sequence with skipped data phases is
// generated on stalled AP read with ORUNDETECT set.

ap_read_response read_callback(uint16_t addr) {
	static uint32_t count;
	return {
		.rdata = 0xcafef00du + count,
		.delay_cycles = 500,
		.err = false
	};
}

const uint32_t MASK_ORUNDETECT = 0x1;
const uint32_t MASK_STICKYORUN = 0x2;
const uint32_t MASK_ORUNERRCLR = 0x10;

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	(void)swd_write(t, DP, DP_REG_CTRL_STAT, MASK_ORUNDETECT);
	uint32_t data;
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(data & MASK_ORUNDETECT, "Failed to set CTRL/STAT.ORUNDETECT.\n");


	status = swd_read_orun(t, AP, 0, data);
	tb_assert(status == OK, "Priming read should give OK\n");

	// Because our host code is ignoring response and always driving 33 hi-Z
	// clocks on read, and we have a pullup on the bus, the DP will see a
	// protocol error if it is not also skipping through the address phase.
	// At that point we would get DISCONNECTED responses instead of WAIT/FAULT.
	status = swd_read_orun(t, AP, 0, data);
	tb_assert(status == WAIT, "First read on stall should WAIT\n");
	status = swd_read_orun(t, AP, 0, data);
	tb_assert(status == FAULT, "Second read on stall should FAULT\n");
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT read during stall + overrun should still give OK\n");

	idle_clocks(t, 500);
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "Should be able to read CTRL/STAT after stall clears\n");
	tb_assert(data & MASK_STICKYORUN, "STICKYORUN should be set.\n");

	swd_write_orun(t, DP, DP_REG_ABORT, MASK_ORUNERRCLR);
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "Should be able to read CTRL/STAT after ABORT\n");
	tb_assert(!(data & MASK_STICKYORUN), "STICKYORUN should be cleared by ABORT.\n");

	return 0;
}
