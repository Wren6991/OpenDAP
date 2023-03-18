#include "tb.h"
#include <cstdio>

// Test intent: check that AP register address and APSEL make it to the AP
// interface, and read data makes it back.

ap_read_response read_callback(uint16_t addr) {
	// Note ADDR is {APSEL, APBANKSEL, A[3:2]}, so 14 bits total.
	return {
		.rdata = addr,
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	for (unsigned  apsel = 0; apsel < 256; apsel = apsel ? apsel << 1 : 1) {
		for (unsigned bank = 0; bank < 16; ++bank) {
			for (unsigned reg = 0; reg < 4; ++reg) {
				uint32_t data;
				(void)swd_write(t, DP, DP_REG_SELECT, apsel << 24 | bank << 4);
				(void)swd_read(t, AP, reg, data);
				status = swd_read(t, DP, DP_REG_RDBUF, data);
				tb_assert(status == OK && data == (reg | bank << 2 | apsel << 6),
					"Bad response, got %08x for APSEL=%u APBANKSEL=%u A[3:2]=%u\n",
					data, apsel, bank, reg
				);
			}
		}
	}
	return 0;
}
