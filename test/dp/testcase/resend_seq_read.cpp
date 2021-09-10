#include "tb.h"
#include <cstdio>

// Test intent: check that RESEND within a pipelined read sequence returns the
// expected data, and doesn't disturb later reads in the sequence or cause
// spurious AP access.

ap_read_response read_callback(uint16_t addr) {
	static uint32_t count = 0x12340000;
	return {
		.rdata = count++,
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_read_callback(read_callback);

	send_dormant_to_swd(t);
	swd_line_reset(t);

	uint32_t id;
	(void)swd_read(t, DP, DP_REG_DPIDR, id);

	// First read primes the pump, its return is not meaningful.
	uint32_t data;
	(void)swd_read(t, AP, 0, data);

	int n_pipelined_reads = 10;
	for (int i = 0; i < n_pipelined_reads; ++i) {
		(void)swd_read(t, AP, 0, data);
		tb_assert(data == 0x12340000 + 2 * i,
			"Bad first AP read %08x for sequence number %d\n", data, i);
		swd_status_t status = swd_read(t, DP, DP_REG_RESEND, data);
		tb_assert(status == OK && (int)data == 0x12340000 + 2 * i,
			"Bad RESEND for sequence number %d\n", i);
		(void)swd_read(t, AP, 0, data);
		tb_assert(data == 0x12340000 + 2 * i + 1,
			"Bad second AP read %08x for sequence number %d\n", data, i);
	}

	(void)swd_read(t, DP, DP_REG_RDBUF, data);
	tb_assert(data == 0x12340000 + 2 * n_pipelined_reads, "Bad data from RDBUF.\n");
	swd_status_t status = swd_read(t, DP, DP_REG_RESEND, data);
	tb_assert(status == OK && data == 0x12340000 + 2 * n_pipelined_reads,
		"Bad RESEND of RDBUF\n");
	status = swd_read(t, DP, DP_REG_RESEND, data);
	tb_assert(status == OK && data == 0x12340000 + 2 * n_pipelined_reads,
		"Repeated RESEND of same data should succeed\n");

	status = swd_read(t, DP, DP_REG_DPIDR, data);
	tb_assert(status == OK && data == DPIDR_EXPECTED, "Bad DPIDR read after RESEND\n");
	status = swd_read(t, DP, DP_REG_RESEND, data);
	tb_assert(status == DISCONNECTED, "RESEND after DPIDR should cause a protocol error\n");

	return 0;
}
