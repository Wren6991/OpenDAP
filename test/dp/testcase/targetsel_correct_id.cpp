#include "tb.h"
#include <cstdio>

// Test intent: ensure we do not get deselected when writing corrected TARGETSEL value.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	swd_targetsel(t, TARGETID_EXPECTED & 0x0fffffffu);
	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);
	return status == OK && id == DPIDR_EXPECTED ? 0 : -1;
}
