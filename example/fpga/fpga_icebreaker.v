// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// Example DP instantiation top-level on iCEBreaker FPGA board.

module fpga_icebreaker #(
	parameter DPIDR    = 32'hdeadbeef,
	parameter TARGETID = 32'hbaadf00d
) (
	input  wire swclk,
	inout  wire swdio,

	// LED is connected to CDBGPWRUPREQ:
	output wire led
);

// ----------------------------------------------------------------------------
// Power-on reset

// The DAP's PoR reset is synchronous, but it is acceptable to synchronise
// it -- the synchroniser will elapse during the host's initial mandatory 8
// clocks with SWDIO high before Dormant-to-SWD is sent.

wire rst_n_por;

fpga_reset #(
	.SHIFT (2),
	.COUNT (0)
) rst_por_u (
	.clk         (swclk),
	.force_rst_n (1'b1),
	.rst_n       (rst_n_por)
);

// ----------------------------------------------------------------------------
// Debug Port instantiation

wire        swdi;
wire        swdo;
wire        swdo_en;

wire        cdbgpwrupreq;
wire        cdbgpwrupack = cdbgpwrupreq;
wire        csyspwrupreq;
wire        csyspwrupack = csyspwrupreq;

// Note eventstat is an active-low event signal, so tied high if unused.
wire [3:0]  instid = 4'h0;
wire        eventstat = 1'b1;

wire [7:0]  ap_sel;
wire [5:0]  ap_addr;
wire [31:0] ap_wdata;

wire        ap_wen;
wire        ap_ren;
wire        ap_abort;

wire [31:0] ap_rdata = 32'h12345678;
wire        ap_rdy = 1'b1;
wire        ap_err = 1'b0;;

opendap_sw_dp #(
	.DPIDR    (DPIDR),
	.TARGETID (TARGETID)
) inst_opendap_sw_dp (
	.swclk        (swclk),
	.rst_n        (rst_n_por),

	.swdi         (swdi),
	.swdo         (swdo),
	.swdo_en      (swdo_en),

	.cdbgpwrupreq (cdbgpwrupreq),
	.cdbgpwrupack (cdbgpwrupack),
	.csyspwrupreq (csyspwrupreq),
	.csyspwrupack (csyspwrupack),

	.instid       (instid),
	.eventstat    (eventstat),

	.ap_sel       (ap_sel),
	.ap_addr      (ap_addr),
	.ap_wdata     (ap_wdata),
	.ap_wen       (ap_wen),
	.ap_ren       (ap_ren),
	.ap_abort     (ap_abort),
	.ap_rdata     (ap_rdata),
	.ap_rdy       (ap_rdy),
	.ap_err       (ap_err)
);

// Mandatory pullup on SWDIO, so need to instantiate a pad:

SB_IO #(
	.PIN_TYPE (6'b10_10_01),
	//            |  |  |
	//            |  |  \----- Unregistered input
	//            |  \-------- Unregistered output
	//            \----------- Unregistered output enable
	.PULLUP  (1'b1)
) input_buffer (
	.D_OUT_0       (swdo),
	.D_IN_0        (swdi),
	.OUTPUT_ENABLE (swdo_en),
	.PACKAGE_PIN   (swdio)
);

// Active-low LED
assign led = !cdbgpwrupreq;

endmodule
