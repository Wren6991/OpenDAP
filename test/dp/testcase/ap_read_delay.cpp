#include "tb.h"
#include <cstdio>

// Test intent: check that repeated WAIT responses are generated on stalled AP
// read with no ORUNDETECT.

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

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	uint32_t data;
	status = swd_read(t, AP, 0, data);
	tb_assert(status == OK, "Priming read should give OK\n");
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == WAIT, "First stalled AP read attempt should give WAIT\n");

	// We have ORUNDETECT off, so no sticky errors. Just repeated WAIT.
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == WAIT, "Second stalled AP read attempt should give WAIT\n");

	idle_clocks(t, 100);

	status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == OK, "Should have unblocked by now\n");
	tb_assert(data == 0xcafef00du, "Bad data\n");

	return 0;
}
