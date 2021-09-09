#include "tb.h"
#include <cstdio>

int main() {
	tb t("read_dpidr.vcd");
	send_dormant_to_swd(t);
	swd_line_reset(t);
	uint32_t id;
	swd_status_t status = swd_read(t, DP, 0, id);

	if (status == OK) {
		printf("OK, DPIDR = %08x\n", id);
		return 0;
	}
	else {
		printf("Status: %d\n", status);
		return -1;
	}
}
