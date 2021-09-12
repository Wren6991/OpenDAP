#include "tb.h"
#include <cstdio>

// Test intent: Perform a pipelined sequence of incrementing APB reads. Make
// sure APB gets the correct addresses, and we get the correct data.

const uint32_t rdata_magic = 0x1234;
const uint32_t start_addr =  0x5a000000;

apb_read_response read_callback(uint32_t addr) {
	return {
		.rdata = rdata_magic + addr,
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_apb_read_callback(read_callback);

	uint32_t id;
	send_dormant_to_swd(t);
	swd_line_reset(t);
	(void)swd_read(t, DP, DP_REG_DPIDR, id);

	const uint32_t CSW_ADDR_INC = 0x10u;
	(void)swd_write(t, AP, AP_REG_CSW, CSW_ADDR_INC);
	(void)swd_write(t, AP, AP_REG_TAR, start_addr);

	uint32_t data;
	(void)swd_read(t, AP, AP_REG_TAR, data);
	swd_status_t status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == OK && data == start_addr, "Bad readback of start address\n");

	// Priming AP read, kicks off first transfer. Data is not meaningful.
	status = swd_read(t, AP, AP_REG_DRW, data);
	tb_assert(status == OK, "Should get OK on priming read\n");

	const int n_pipelined_reads = 10;
	for (int i = 0; i < n_pipelined_reads; ++i) {
		status = swd_read(t, AP, AP_REG_DRW, data);
		tb_assert(status == OK, "Should get OK on all reads with no downstream stalls/errors\n");
		tb_assert(data == rdata_magic + start_addr + i * 4, "Bad data item %i: %08x\n", i, data);
	}
	status = swd_read(t, AP, AP_REG_DRW, data);
	tb_assert(status == OK && data == rdata_magic + start_addr + 4 * n_pipelined_reads, "Bad readback of trailing data\n");

	// Final address should be +4 for the fact that n+1 reads took place (n of
	// them overlapped and 1 not), and +4 again for the fact that TAR always
	// points to the *next* transfer address.
	(void)swd_read(t, AP, AP_REG_TAR, data);
	status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == OK && data == start_addr + 4 * (2 + n_pipelined_reads), "Bad ending address\n");
	return 0;
}





