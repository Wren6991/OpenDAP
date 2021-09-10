#include "tb.h"
#include <cstdio>

// Test intent: check the instid signal is checked accurately against the
// TARGETSEL value.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);

	for (int i = 0; i < 16; ++i) {
		t.set_instid(i);
		swd_line_reset(t);
		swd_targetsel(t, (TARGETID_EXPECTED & 0x0fffffffu) | (uint32_t)i << 28);
		uint32_t id;
		swd_status_t status = swd_read(t, DP, 0, id);
		tb_assert(status == OK && id == DPIDR_EXPECTED, "Failed to select with instid %d\n", i);
	}

	swd_line_reset(t);
	swd_targetsel(t, TARGETID_EXPECTED & 0x0fffffffu);
	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);
	tb_assert(status == DISCONNECTED, "Failed to fail\n");
	return 0;
}
