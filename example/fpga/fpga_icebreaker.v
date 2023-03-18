// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// Example DP instantiation top-level on iCEBreaker FPGA board.

`default_nettype none

module fpga_icebreaker #(
	parameter        DPIDR              = 32'hdeadbeef,
	parameter        TARGETID           = 32'hbaadf00d,

	parameter [10:0] IDR_DESIGNER       = 11'h7ff,
	parameter [3:0]  IDR_REVISION       = 4'h0,
	parameter [31:0] BASE               = 32'h0000_0000,
	parameter        TAR_INCREMENT_BITS = 12
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
wire cdbgrstreq;
wire cdbgrstack = cdbgrstreq;

// Note eventstat is an active-low event signal, so tied high if unused.
wire [3:0]  instid = 4'h0;
wire        eventstat = 1'b1;

wire [7:0]  ap_sel;
wire [5:0]  ap_addr;
wire [31:0] ap_wdata;
wire        ap_wen;
wire        ap_ren;
wire        ap_abort;
wire [31:0] ap_rdata;
wire        ap_rdy;
wire        ap_err;

opendap_sw_dp #(
	.DPIDR    (DPIDR),
	.TARGETID (TARGETID)
) dp (
	.swclk        (swclk),
	.rst_n        (rst_n_por),

	.swdi         (swdi),
	.swdo         (swdo),
	.swdo_en      (swdo_en),

	.cdbgpwrupreq (cdbgpwrupreq),
	.cdbgpwrupack (cdbgpwrupack),
	.csyspwrupreq (csyspwrupreq),
	.csyspwrupack (csyspwrupack),
	.cdbgrstreq   (cdbgrstreq),
	.cdbgrstack   (cdbgrstack),

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
// assign led = !cdbgpwrupreq;

// ----------------------------------------------------------------------------
// Access port instantiation:

// This is in general asynchronous, but in the interest of giving it SOME clock:
wire        clk_dst = swclk;
wire        rst_n_dst = rst_n_por;

wire        ap0_wen = ap_wen && ap_sel == 8'h00;
wire        ap0_ren = ap_ren && ap_sel == 8'h00;
wire [31:0] ap0_rdata;

reg read_selected_ap0;
always @ (posedge swclk or negedge rst_n_por) begin
	if (!rst_n_por) begin
		read_selected_ap0 <= 1'b0;
	end else if (ap_ren) begin
		read_selected_ap0 <= ap_sel == 8'h00;
	end
end

assign led = !read_selected_ap0;

assign ap_rdata = read_selected_ap0 ? ap0_rdata : 32'h0;

wire        dst_psel;
wire        dst_penable;
wire        dst_pwrite;
wire [31:0] dst_paddr;
wire [31:0] dst_pwdata;
wire [31:0] dst_prdata;
wire        dst_pready;
wire        dst_pslverr;

opendap_mem_ap_apb #(
	.IDR_DESIGNER       (IDR_DESIGNER),
	.IDR_REVISION       (IDR_REVISION),
	.BASE               (BASE),
	.TAR_INCREMENT_BITS (TAR_INCREMENT_BITS)
) ap (
	.swclk       (swclk),
	.rst_n_por   (rst_n_por),

	.clk_dst     (clk_dst),
	.rst_n_dst   (rst_n_dst),

	.dpacc_addr  (ap_addr),
	.dpacc_wdata (ap_wdata),
	.dpacc_wen   (ap0_wen),
	.dpacc_ren   (ap0_ren),
	.dpacc_abort (ap_abort),
	.dpacc_rdata (ap0_rdata),
	.dpacc_rdy   (ap_rdy),
	.dpacc_err   (ap_err),

	.dst_psel    (dst_psel),
	.dst_penable (dst_penable),
	.dst_pwrite  (dst_pwrite),
	.dst_paddr   (dst_paddr),
	.dst_pwdata  (dst_pwdata),
	.dst_prdata  (dst_prdata),
	.dst_pready  (dst_pready),
	.dst_pslverr (dst_pslverr)
);

// ----------------------------------------------------------------------------
// Dummy APB slave

assign dst_prdata = dst_paddr;
assign dst_pready = 1'b1;
assign dst_pslverr = 1'b0;

endmodule
