#include "tb.h"
#include <cstdio>

// Test intent:

// - Ensure we get deselected when writing the correct TARGETSEL value but
//   with a parity error.
//
// - Reconnect and check that WDATAERR was not set.

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint8_t header = 0x99;
	put_bits(t, &header, 8);
	hiz_clocks(t, 5);
	uint8_t targetsel[] = {
		TARGETID_EXPECTED & 0xff,
		TARGETID_EXPECTED >> 8 & 0xff,
		TARGETID_EXPECTED >> 16 & 0xff,
		TARGETID_EXPECTED >> 24 & 0x0f
	};
	put_bits(t, targetsel, 32);
	uint8_t parity = 0;
	for (int i = 0; i < 28; ++i)
		parity ^= (TARGETID_EXPECTED >> 1) & 0x1;
	// Flip the parity bit only.
	parity = !parity;
	put_bits(t, &parity, 1);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);
	tb_assert(status == DISCONNECTED, "DPIDR read should fail after bad TARGETSEL\n");

	swd_line_reset(t);
	swd_targetsel(t, TARGETID_EXPECTED & 0x0fffffffu);
	status = swd_read(t, DP, 0, id);
	tb_assert(status == OK && id == DPIDR_EXPECTED, "Couldn't reconnect after bad TARGETSEL\n");

	uint32_t stat;
	status = swd_read(t, DP, 1, stat);
	tb_assert(status == OK && !(stat & 0x80), "Didn't see expected STAT value.\n");

	return 0;

}
