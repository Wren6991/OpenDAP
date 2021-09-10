#include "tb.h"

// Test intent: make sure invalid DLCR.TURNROUND causes protocol error and
// immediate lockout.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);
	uint32_t dpidr = 0;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, dpidr);

	// First check that we can write 0 to DLCR.TURNROUND.
	uint32_t dlcr = 0;
	status = swd_write(t, DP, DP_REG_SELECT, DP_BANK_DLCR);
	status = swd_write(t, DP, DP_REG_DLCR, 0);
	status = swd_read(t, DP, DP_REG_DLCR, dlcr);
	// Only set bit should be the weird RES1 at position 6.
	tb_assert(status == OK && dlcr == 0x40, "Bad DLCR readback\n");

	status = swd_write(t, DP, DP_REG_DLCR, 0x100);
	// The actual failing write should still give an OK, because it's the
	// write data that killed us.
	tb_assert(status == OK, "Bad DLCR write should still succeed.\n");

	// Immediately afterward the DP should drop off the bus, because we gave
	// an unsupported TURNROUND.
	status = swd_read(t, DP, DP_REG_DPIDR, dpidr);
	tb_assert(status == DISCONNECTED, "Should be locked out by bad DLCR write.\n");

	// Line reset should bring it back.
	swd_line_reset(t);
	status = swd_read(t, DP, DP_REG_DPIDR, dpidr);
	tb_assert(status == OK && dpidr == DPIDR_EXPECTED, "Should get good DPIDR readback after reset\n");

	return 0;
}
