#include "tb.h"
#include <cstdio>

// Test intent: ensure we get deselected when writing bad TARGETSEL value.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	swd_targetsel(t, (TARGETID_EXPECTED & 0x0fffffffu) | 0x80000000u);
	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);
	tb_assert(status == DISCONNECTED, "Should get no response after bad TARGETSEL\n");
	return 0;
}
