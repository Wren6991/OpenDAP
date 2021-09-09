#include "tb.h"
#include <cstdio>

// Test intent: hello world

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);
	if (status == OK) {
		printf("OK, DPIDR = %08x\n", id);
	}

	return status == OK && id == DPIDR_EXPECTED ? 0 : -1;
}
