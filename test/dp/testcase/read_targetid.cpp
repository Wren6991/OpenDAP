#include "tb.h"

// Test intent: check TARGETID register has expected value. This also
// exercises SELECT.DPBANKSEL writes.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// DPIDR read to transition to active state
	uint32_t data = 0;
	(void)swd_read(t, DP, DP_REG_DPIDR, data);

	// Set DPBANKSEL.
	data = DP_BANK_TARGETID;
	(void)swd_write(t, DP, DP_REG_SELECT, data);

	swd_status_t status = swd_read(t, DP, DP_REG_TARGETID, data);

	return status == OK && data == TARGETID_EXPECTED ? 0 : -1;
}
