#include "tb.h"
#include <cstdio>
#include <vector>

// Test intent: check that a sequence of stalling AP writes with ORUNDETECT
// set gives the correct OK -> WAIT -> FAULT sequence.

std::vector<uint64_t> write_history;

ap_write_response write_callback(uint16_t addr, uint32_t data) {
	write_history.push_back((uint64_t)addr << 32 | data);
	return {
		.delay_cycles = 500,
		.err = false
	};
}

const uint32_t MASK_ORUNDETECT = 0x1;
const uint32_t MASK_STICKYORUN = 0x2;
const uint32_t MASK_ORUNERRCLR = 0x10;

int main() {
	tb t("waves.vcd");
	t.set_ap_write_callback(write_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	(void)swd_write(t, DP, DP_REG_CTRL_STAT, MASK_ORUNDETECT);
	uint32_t data;
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(data & MASK_ORUNDETECT, "Failed to set CTRL/STAT.ORUNDETECT.\n");

	const uint32_t magic = 0xffff1234;
	std::vector<uint64_t> expected_write_seq;
	uint ctr = 0;

	status = swd_write_orun(t, AP, ctr & 0x3, magic + ctr);
	expected_write_seq.push_back((uint64_t)(ctr & 0x3) << 32 | magic + ctr);
	++ctr;
	tb_assert(status == OK, "First write should give OK\n");
	status = swd_write_orun(t, AP, ctr & 0x3, magic + ctr);
	++ctr;
	tb_assert(status == WAIT, "Second write should give WAIT\n");
	status = swd_write_orun(t, AP, ctr & 0x3, magic + ctr);
	++ctr;
	tb_assert(status == FAULT, "Third write should give WAIT\n");

	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT read during stall + overrun should still give OK\n");
	tb_assert(data & MASK_STICKYORUN, "STICKYORUN should have been set due to WAIT/FAULT\n");
	status = swd_write_orun(t, DP, DP_REG_ABORT, MASK_ORUNERRCLR);
	tb_assert(status == OK, "ABORT failed\n");
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK && !(data & MASK_STICKYORUN), "STICKYORUN clear failed\n");

	status = swd_write_orun(t, AP, ctr & 0x3, magic + ctr);
	++ctr;
	tb_assert(status == WAIT, "Response should return to WAIT after clearing STICKYORUN\n");

	// Do the ORUN clear thing again.
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT read during stall + overrun should still give OK\n");
	tb_assert(data & MASK_STICKYORUN, "STICKYORUN should have been set due to WAIT/FAULT\n");
	status = swd_write_orun(t, DP, DP_REG_ABORT, MASK_ORUNERRCLR);
	tb_assert(status == OK, "ABORT failed\n");
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK && !(data & MASK_STICKYORUN), "STICKYORUN clear failed\n");

	idle_clocks(t, 200);

	status = swd_write_orun(t, AP, ctr & 0x3, magic + ctr);
	expected_write_seq.push_back((uint64_t)(ctr & 0x3) << 32 | magic + ctr);
	++ctr;
	tb_assert(status == OK, "Write should give OK again after stall clears.\n");
	status = swd_read_orun(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK && !(data & MASK_STICKYORUN), "STICKYORUN should not have been set by non-WAIT stalled transfer\n");

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
