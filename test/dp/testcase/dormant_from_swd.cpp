#include "tb.h"
#include <cstdio>

// Test intent: make sure we can return to dormant after going to SWD state.

const uint8_t swd_to_dormant[9] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // at least 50 clocks high (this is 56)
	0xbc, 0xe3                                // magic select sequence (starting with 2 clocks low, completing a line reset)
};

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);
	if (status != OK || id != DPIDR_EXPECTED) {
		printf("Failed to go to SWD state\n");
		return -1;
	}

	put_bits(t, swd_to_dormant, 72);

	// The DP should now be unresponsive to line reset.
	for (int i = 0; i < 5; ++i) {
		swd_line_reset(t);
		status = swd_read(t, DP, DP_REG_DPIDR, id);
		if (status != DISCONNECTED) {
			printf("Failed to return to dormant state\n");
			return -1;
		}
	}

	// We can wake it up again with the magic sequence
	send_dormant_to_swd(t);
	swd_line_reset(t);
	status = swd_read(t, DP, DP_REG_DPIDR, id);
	if (status != OK || id != DPIDR_EXPECTED) {
		printf("Failed to re-enter SWD state after issuing SWD-to-Dormant\n");
		return -1;
	}
	return 0;
}
