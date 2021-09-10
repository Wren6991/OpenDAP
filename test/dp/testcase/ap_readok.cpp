#include "tb.h"
#include <cstdio>

// Test intent: check that CTRL/STAT.READOK is set/cleared in line with
// APACC/RDBUFF read responses.

bool ap_gives_err = false;
ap_read_response read_callback(uint16_t addr) {
	return {
		.rdata = 0xcafef00du,
		.delay_cycles = 300,
		.err = ap_gives_err
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	swd_status_t status = swd_read(t, DP, DP_REG_DPIDR, id);

	uint32_t data;
	status = swd_read(t, AP, 0, data);
	tb_assert(status == OK, "Priming read should give OK\n");

	// Check WAIT and FAULT for APACC

	const uint32_t READOK_MASK = 0x40u;
	idle_clocks(t, 500);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "Must get OK to CTRL/STAT read.\n");
	tb_assert(data & READOK_MASK, "Priming AP read should have set READOK\n");

	status = swd_read(t, AP, 0, data);
	tb_assert(status == OK, "Second priming read should give OK\n");
	status = swd_read(t, AP, 0, data);
	tb_assert(status == WAIT, "AP read during stall should WAIT\n");

	idle_clocks(t, 500);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT read after WAIT should give OK\n");
	tb_assert(!(data & READOK_MASK), "READOK should be cleared by AP read WAIT\n");

	idle_clocks(t, 500);
	ap_gives_err = true;
	status = swd_read(t, AP, 0, data);
	tb_assert(status == OK, "Third priming AP read should give OK\n");
	idle_clocks(t, 500);
	status = swd_read(t, AP, 0, data);
	tb_assert(status == FAULT, "AP read following error completion should give FAULT\n");
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(!(data & READOK_MASK), "READOK should be cleared on FAULT\n");


	// Finally check that RDBUFF WAIT also clears READOK.

	ap_gives_err = false;
	(void)swd_write(t, DP, DP_REG_ABORT, 0x4);
	status = swd_read(t, AP, 0, data);
	tb_assert(status == OK, "AP read should return to OK after ABORT\n");

	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT read should not WAIT even when AP stalled\n");
	tb_assert(data & READOK_MASK, "READOK should still be set if AP is stalled but no WAIT has been issued\n");

	status = swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(status == WAIT, "RDBUF should WAIT on stall.\n");
	idle_clocks(t, 500);
	status = swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(status == OK, "CTRL/STAT should still be readable when WAIT clears\n");
	tb_assert(!(data & READOK_MASK), "READOK should be cleared by RDBUF WAIT\n");

	return 0;
}
