#include "tb.h"

// Test intent: ensure an invalid access in the reset state causes a protocol error.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// Anything but DPIDR read or TARGETSEL write is a protocol error.
	// This is an ABORT.
	uint32_t data = 0;
	swd_status_t status = swd_write(t, DP, 0, data);

	// DP should immediately drop off the bus, we see three ones due to the parking.
	return status == DISCONNECTED ? 0 : -1;
}
