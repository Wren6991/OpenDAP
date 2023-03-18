#include "tb.h"
#include <cstdio>

// Test intent: check Mem-AP is accessible at APSEL 0 with the correct IDR value

int main() {
	tb t("waves.vcd");

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	uint32_t data;
	(void)swd_write(t, DP, DP_REG_SELECT, AP_BANK_IDR);
	(void)swd_read(t, AP, AP_REG_IDR, data);
	status = swd_read(t, DP, DP_REG_RDBUF, data);

	// REVISION = 0, DESIGNER = 7ff, CLASS = Mem-AP, TYPE = APB2/APB3
	tb_assert(status == OK && data == APIDR_EXPECTED, "Bad AP IDR read\n");
	return 0;
}
