#include "tb.h"
#include <cstdio>

// Test intent: make sure the DP ignores line resets in the dormant state
// (and is initially dormant)

int main() {
	tb t("waves.vcd");

	swd_line_reset(t);
	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);

	return status == DISCONNECTED ? 0 : -1;
}
