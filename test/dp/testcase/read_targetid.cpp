#include "tb.h"

// Test intent: check TARGETID register has expected value. This also
// exercises SELECT.DPBANKSEL writes.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// DPIDR read to transition to active state
	uint32_t data = 0;
	(void)swd_read(t, DP, 0, data);

	// Set DPBANKSEL to 2. Note our addresses are just A[3:2], not A[3:0].
	data = 0x2;
	(void)swd_write(t, DP, 2, data);

	swd_status_t status = swd_read(t, DP, 1, data);

	return status == OK && data == TARGETID_EXPECTED ? 0 : -1;
}
