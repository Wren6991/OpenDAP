#include "tb.h"

const uint8_t selection_alert[16] = {
	0x92, 0xf3, 0x09, 0x62,
	0x95, 0x2d, 0x85, 0x86,
	0xe9, 0xaf, 0xdd, 0xe3,
	0xa2, 0x0e, 0xbc, 0x19
};

const uint8_t dpidr_read[6] = {
	0xa5, 0x00, 0x00, 0x00, 0x00, 0x00
};

int main() {
	tb t("read_dpidr.vcd");
	uint8_t c = 0xff;
	put_bits(t, &c, 8);
	put_bits(t, selection_alert, 128);
	c = 0;
	put_bits(t, &c, 4);
	c = 0x1a;
	put_bits(t, &c, 8);
	c = 0;
	put_bits(t, &c, 8);
	c = 0xff;
	put_bits(t, &c, 8);
	put_bits(t, &c, 8);
	put_bits(t, &c, 8);
	put_bits(t, &c, 8);
	put_bits(t, &c, 8);
	put_bits(t, &c, 8);
	c = 0x03;
	put_bits(t, &c, 8);

	// Now in reset state.
	put_bits(t, dpidr_read, 48);

	return 0;
}
