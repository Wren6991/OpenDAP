#include "tb.h"

// Test intent: check DLPIDR has the correct value, and responds to changes to instid.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// DPIDR read to transition to active state
	uint32_t data = 0;
	(void)swd_read(t, DP, DP_REG_DPIDR, data);

	data = DP_BANK_DLPIDR;
	(void)swd_write(t, DP, DP_REG_SELECT, data);
	(void)swd_read(t, DP, DP_REG_DLPIDR, data);
	tb_assert(data == 0x00000001, "Bad DLPIDR initial value\n");

	t.set_instid(0xf);
	(void)swd_read(t, DP, DP_REG_DLPIDR, data);
	tb_assert(data == 0xf0000001, "DLPIDR failed to respond to INSTID\n");
	return 0;
}
