#include "tb.h"
#include <cstdio>

// Test intent: check a RESEND after a read that is not READBUF or AP read
// causes a protocol error.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);
	status = swd_read(t, DP, DP_REG_RESEND, id);
	if (status != DISCONNECTED) {
		printf("Should get immediate protocol error on bad RESEND\n");
		return -1;
	}

	return 0;
}
