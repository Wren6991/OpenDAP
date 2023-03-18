#include "tb.h"

// Test intent: check CDBGPWRUPREQ/CSYSPWRUPREQ are read/writable, and are
// reflected in their respective ACKs (note we tie REQ -> ACK in the tb) Note
// this test may not run on real hardware due to unrealistic assumptions
// about how quickly the ACKs come back.

#define MASK_ALL_REQ (DP_CTRL_STAT_CDBGPWRUPREQ | DP_CTRL_STAT_CSYSPWRUPREQ)
#define MASK_ALL_ACK (DP_CTRL_STAT_CDBGPWRUPACK | DP_CTRL_STAT_CSYSPWRUPACK)
#define MASK_ALL_REQ_ACK (MASK_ALL_REQ | MASK_ALL_ACK) 

int main() {
	tb t("waves.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);

	// DPIDR read to transition to active state
	uint32_t data = 0;
	(void)swd_read(t, DP, DP_REG_DPIDR, data);

	data = DP_BANK_CTRL_STAT;
	(void)swd_write(t, DP, DP_REG_SELECT, data);
	(void)swd_write(t, DP, DP_REG_CTRL_STAT, 0);
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert((data & MASK_ALL_REQ) == 0, "Failed to clear REQs\n");
	// Note this may require some delay if run on real hardware:
	tb_assert((data & MASK_ALL_ACK) == 0, "ACKs failed to clear\n");

	(void)swd_write(t, DP, DP_REG_CTRL_STAT, DP_CTRL_STAT_CDBGPWRUPREQ);
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert(data & DP_CTRL_STAT_CDBGPWRUPREQ, "Failed to set CDBGPWRUPREQ\n");
	tb_assert(data & DP_CTRL_STAT_CDBGPWRUPACK, "No immediate CDBGPWRUPACK\n");
	tb_assert(!(data & DP_CTRL_STAT_CSYSPWRUPACK), "SYSPWRUPACK set unexpectedly\n");

	(void)swd_write(t, DP, DP_REG_CTRL_STAT, MASK_ALL_REQ);
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	// Note setting only SYSPWRREQ is not required to behave predictably
	tb_assert((data & MASK_ALL_REQ) == MASK_ALL_REQ, "Failed to set both REQs\n");
	tb_assert((data & MASK_ALL_ACK) == MASK_ALL_ACK, "Failed to immediately get both ACKs\n");

	(void)swd_write(t, DP, DP_REG_CTRL_STAT, MASK_ALL_REQ | DP_CTRL_STAT_CDBGRSTREQ);
	(void)swd_read(t, DP, DP_REG_CTRL_STAT, data);
	tb_assert((data & (DP_CTRL_STAT_CDBGRSTREQ | DP_CTRL_STAT_CDBGRSTACK)) ==
		(DP_CTRL_STAT_CDBGRSTREQ | DP_CTRL_STAT_CDBGRSTACK), "Failed to set RSTREQ/ACK");
	return 0;
}
