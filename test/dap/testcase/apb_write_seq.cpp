#include "tb.h"
#include <cstdio>

// Test intent: Check an APB read error correctly sets STICKYERR, and we can
// then recover from the error and issue more transfers.

const uint32_t wdata_magic = 0x00c30000;
const uint32_t start_addr =  0x5a000000;

std::vector<uint64_t> write_history;

apb_write_response write_callback(uint32_t addr, uint32_t data) {
	write_history.push_back((uint64_t)addr << 32 | data);
	return {
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_apb_write_callback(write_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	const uint32_t CSW_ADDR_INC = 0x10u;
	(void)swd_write(t, AP, AP_REG_CSW, CSW_ADDR_INC);
	(void)swd_write(t, AP, AP_REG_TAR, start_addr);
	
	std::vector<uint64_t> expected_write_seq;

	const int n_writes = 10;
	for (int i = 0; i < n_writes; ++i) {
		status = swd_write(t, AP, AP_REG_DRW, wdata_magic + i);
		tb_assert(status ==OK, "Should get OK for non-waited non-errored write sequence.\n");
		// Need some space for the async bridge, because write strobe at end
		// of data phase is quite close to next response phase. Reads OTOH
		// are fine.
		idle_clocks(t, 10);
		expected_write_seq.push_back(wdata_magic + i | ((uint64_t)(start_addr + 4 * i) << 32));
	}

	idle_clocks(t, 50);
	tb_assert(
		write_history.size() >= expected_write_seq.size(),
		"Not enough write data received, expected %lu, got %lu\n",
		expected_write_seq.size(), write_history.size()
	);
	tb_assert(
		write_history.size() <= expected_write_seq.size(),
		"Too much write data received, expected %lu, got %lu\n",
		expected_write_seq.size(), write_history.size()
	);
	for (size_t i = 0; i < expected_write_seq.size(); ++i){
		tb_assert(
			write_history[i] == expected_write_seq[i],
			"Bad data item %lu, expected %012lx, got %012lx\n",
			i, expected_write_seq[i], write_history[i]
		);
	}

	uint32_t data;
	(void)swd_read(t, AP, AP_REG_TAR, data);
	status = swd_read(t, DP, DP_REG_RDBUF, data); 
	// TAR points to the *next* transfer address.
	tb_assert(status == OK && data == start_addr + 4 * n_writes, "Bad final TAR value %08x\n", data);

	return 0;
}





