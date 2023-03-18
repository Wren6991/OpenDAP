#include "tb.h"
#include <cstdio>
#include <vector>

// Test intent: check that a sequence of AP writes appear at the AP interface
// in correct order, with correct addresses and correct data.

std::vector<uint64_t> write_history;

ap_write_response write_callback(uint16_t addr, uint32_t data) {
	write_history.push_back((uint64_t)addr << 32 | data);
	return {
		.delay_cycles = 0,
		.err = false
	};
}

int main() {
	tb t("waves.vcd");
	t.set_ap_write_callback(write_callback);

	swd_status_t status = swd_prepare_dp_for_ap_access(t);
	tb_assert(status == OK, "Failed to connect to DP\n");

	const uint32_t magic = 0xabcd1234;
	std::vector<uint64_t> expected_write_seq;
	for (unsigned apsel = 0; apsel < 256; apsel = apsel ? apsel << 1 : 1) {
		for (unsigned bank = 0; bank < 16; ++bank) {
			status = swd_write(t, DP, DP_REG_SELECT, apsel << 24 | bank << 4);
			tb_assert(status == OK, "SELECT failed\n");
			for (unsigned reg = 0; reg < 4; ++reg) {
				uint16_t addr_expected = apsel << 6 | bank << 2 | reg;
				status = swd_write(t, AP, reg, magic - addr_expected);
				expected_write_seq.push_back(magic - addr_expected | (uint64_t)addr_expected << 32);
				tb_assert(status == OK, "write failed, APSEL=%u, APBANKSEL=%u, A[3:2]=%u\n", apsel, bank, reg);
			}
		}
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
