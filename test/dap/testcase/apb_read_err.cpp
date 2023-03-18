#include "tb.h"
#include <cstdio>

// Test intent: Check an APB read error correctly sets STICKYERR, and we can
// then recover from the error and issue more transfers.

const uint32_t rdata_magic = 0x1234;
const uint32_t start_addr =  0x5a000000;

apb_read_response read_callback(uint32_t addr) {
	static int count = 0;
	return {
		.rdata = rdata_magic + addr,
		.delay_cycles = 0,
		.err = count++ == 0
	};
}

int main() {
	tb t("waves.vcd");
	t.set_apb_read_callback(read_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	const uint32_t CSW_ADDR_INC = 0x10u;
	(void)swd_write(t, AP, AP_REG_CSW, CSW_ADDR_INC);
	(void)swd_write(t, AP, AP_REG_TAR, start_addr);

	// Priming AP read, kicks off first transfer. Data is not meaningful.
	uint32_t data;
	status = swd_read(t, AP, AP_REG_DRW, data);
	tb_assert(status == OK, "Should get OK on priming read\n");

	// Note this is also an AP read, not RDBUFF. Normally this would cause a
	// TAR increment, but in this case it doesn't because the DP squashes the
	// access with a FAULT response. We'll check the address.
	status = swd_read(t, AP, AP_REG_DRW, data);
	tb_assert(status == FAULT, "Should get FAULT on retiring read\n");

	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "Should never get FAULT on CTRL/STAT read\n");
	tb_assert(data & 0x20, "STICKYERR should be set.\n");
	tb_assert(!(data & 0x92), "No other sticky flags should be set.\n");

	(void)swd_write(t, DP, DP_REG_ABORT, 0x4);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK && !(data & 0x20), "STICKYERR should be cleared by ABORT\n");

	// All better, we can read again.
	status = swd_read(t, AP, AP_REG_DRW, data);
	tb_assert(status == OK, "Should get OK on read after ABORT\n");
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	// Only one prior transfer that wasn't squashed by the DP, so we are
	// reading through a once-incremented TAR.
	tb_assert(status == OK && data == start_addr + rdata_magic + 4, "Bad readback %08x\n", data);
	(void)swd_read(t, AP, AP_REG_TAR, data);
	status = swd_read(t, DP, DP_REG_RDBUF, data); 
	// TAR points to the *next* transfer address.
	tb_assert(status == OK && data == start_addr + 8, "Bad final TAR value %08x\n", data);

	return 0;
}





