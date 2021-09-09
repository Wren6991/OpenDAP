#include "tb.h"
#include <cstdio>

// Test intent: hello world, make sure the DP actually syncs to the start of the packet.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	idle_clocks(t, 100);
	swd_line_reset(t);
	idle_clocks(t, 100);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);
	if (status == OK) {
		printf("OK, DPIDR = %08x\n", id);
	}

	return status == OK && id == DPIDR_EXPECTED ? 0 : -1;
}