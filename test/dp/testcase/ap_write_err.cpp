#include "tb.h"
#include <cstdio>
#include <vector>

// Test intent: check that an AP write error sets the STICKYERR flag, and we
// can clear the flag and perform another AP write.

std::vector<uint64_t> write_history;

ap_write_response write_callback(uint16_t addr, uint32_t data) {
	write_history.push_back((uint64_t)addr << 32 | data);
	// Error response on the first transfer only.
	static int ctr = 0;
	return {
		.delay_cycles = 0,
		.err = ctr++ == 0
	};
}

static const uint32_t MASK_STICKYERR = 0x20;
static const uint32_t MASK_STKERRCLR = 0x04;

int main() {
	tb t("waves.vcd");
	t.set_ap_write_callback(write_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	const uint32_t magic = 0xabcd1234;
	std::vector<uint64_t> expected_write_seq;
	uint32_t ctr = 0;
	expected_write_seq.push_back(magic + ctr);
	status = swd_write(t, AP, 0, magic + ctr++);
	tb_assert(status == OK, "Erroring write should itself give OK\n");
	status = swd_write(t, AP, 0, magic + ctr++);
	tb_assert(status == FAULT, "Next access should immediately fault.\n");

	uint32_t data;
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "DP read should not be affected by sticky flags.\n");
	tb_assert(data & MASK_STICKYERR, "STICKYERR should be set.\n");
	(void)swd_write(t, DP, DP_REG_ABORT, MASK_STKERRCLR);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK && !(data & MASK_STICKYERR), "Failed to clear STICKYERR\n");

	for (int i = 0; i < 5; ++i) {
		expected_write_seq.push_back(magic + ctr);
		status = swd_write(t, AP, 0, magic + ctr++);
		tb_assert(status == OK, "Should be able to write after clearing STICKYERR\n");
	}

	idle_clocks(t, 5);
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

	return 0;
}
