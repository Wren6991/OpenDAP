#include "tb.h"

// Test intent:
//
// - Check WDATAERR is set by bad write data parity
//
// - Check it can be cleared by an ABORT write

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// DPIDR read to transition to active state
	uint32_t data = 0;
	(void)swd_read(t, DP, 0, data);


	// Write 0 to ABORT, with a 1 parity bit. (fails even parity check).
	uint8_t header = 0x81;
	put_bits(t, &header, 8);
	hiz_clocks(t, 5);
	idle_clocks(t, 32);
	put_bits(t, &header, 1);

	// We should not be locked out, but should now get FAULT on e.g. an AP
	// write, as a sticky flag is set.
	swd_status_t ack = swd_write(t, AP, 0, 0);

	if (ack != FAULT) {
		printf("Should get FAULT after write parity error.\n");
		return -1;
	}

	// Check the correct error flag is set in CTRL/STAT.
	uint32_t ctrl_stat = 0;
	ack = swd_read(t, DP, 1, ctrl_stat);
	if (ack != OK) {
		printf("CTRL/STAT read failed\n");
		return -1;
	}
	printf("CTRL/STAT %08x\n", ctrl_stat);
	if (!(ctrl_stat & 0x80)) {
		printf("WDATAERR not set.\n");
		return -1;
	}

	ack = swd_write(t, DP, 0, 0x8);
	ack = swd_read(t, DP, 1, ctrl_stat);
	printf("CTRL/STAT %08x\n", ctrl_stat);

	if (ack != OK || ctrl_stat & 0x80)
		return -1;

	return 0;
}
