// Integrate SW-DP and APB3 Mem-AP for testing. Actual testbench logic is all C++.

module dap_integration #(
	parameter        DPIDR              = 32'hdeadbeef,
	parameter        TARGETID           = 32'hbaadf00d,

	parameter [10:0] IDR_DESIGNER       = 11'h7ff,
	parameter [3:0]  IDR_REVISION       = 4'h0,
	parameter [31:0] BASE               = 32'h0000_0000,
	parameter        TAR_INCREMENT_BITS = 12

) (

	input  wire        swclk,
	input  wire        rst_n,

	input  wire        swdi,
	output reg         swdo,
	output reg         swdo_en,

	output wire        cdbgpwrupreq,
	input  wire        cdbgpwrupack,
	output wire        csyspwrupreq,
	input  wire        csyspwrupack,

	input  wire [3:0]  instid,
	input  wire        eventstat,

	output wire        dst_psel,
	output wire        dst_penable,
	output wire        dst_pwrite,
	output wire [31:0] dst_paddr,
	output wire [31:0] dst_pwdata,
	input  wire [31:0] dst_prdata,
	input  wire        dst_pready,
	input  wire        dst_pslverr
);

wire cdbgpwrupreq;
wire cdbgpwrupack = cdbgpwrupreq;
wire csyspwrupreq;
wire csyspwrupack = csyspwrupreq;
wire cdbgrstreq;
wire cdbgrstack = cdbgrstreq;

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
	.rst_n        (rst_n),

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

opendap_mem_ap_apb #(
	.IDR_DESIGNER       (IDR_DESIGNER),
	.IDR_REVISION       (IDR_REVISION),
	.BASE               (BASE),
	.TAR_INCREMENT_BITS (TAR_INCREMENT_BITS)
) ap (
	.swclk       (swclk),
	.rst_n_por   (rst_n),

	// For simplicity, tie the clocks together. This CDC logic has already
	// been tested with JTAG on a RISC-V FPGA platform.
	.clk_dst     (swclk),
	.rst_n_dst   (rst_n),

	.dpacc_addr  (ap_addr),
	.dpacc_wdata (ap_wdata),
	.dpacc_wen   (ap_wen && ap_sel == 8'h00),
	.dpacc_ren   (ap_ren && ap_sel == 8'h00),
	.dpacc_abort (ap_abort),
	.dpacc_rdata (ap_rdata),
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

endmodule
