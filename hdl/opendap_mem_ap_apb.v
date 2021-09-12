// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// APB3 Mem-AP implemementation

`default_nettype none

module opendap_mem_ap_apb #(
	// Bring your own JEP106 code
	parameter [10:0] IDR_DESIGNER       = 11'h7ff,
	parameter [3:0]  IDR_REVISION       = 4'h0,

	// Base of debug registers or ROM table
	parameter [31:0] BASE               = 32'h0000_0000,

	// Minimum of 10 (A[9:0]). 12 is common, for 4kB pages.
	parameter        TAR_INCREMENT_BITS = 12,

	parameter        W_ADDR             = 32, // do not modify
	parameter        W_DATA             = 32  // do not modify
) (
	input  wire              swclk,
	input  wire              rst_n_por,

	input  wire              clk_dst,
	input  wire              rst_n_dst,

	// DP-AP bus
	input  wire [5:0]        dpacc_addr,
	input  wire [W_DATA-1:0] dpacc_wdata,

	input  wire              dpacc_wen,
	input  wire              dpacc_ren,
	input  wire              dpacc_abort,

	output reg  [W_DATA-1:0] dpacc_rdata,
	output wire              dpacc_rdy,
	output wire              dpacc_err,

	// Downstream bus
	output wire              dst_psel,
	output wire              dst_penable,
	output wire              dst_pwrite,
	output wire [W_ADDR-1:0] dst_paddr,
	output wire [W_DATA-1:0] dst_pwdata,
	input  wire [W_DATA-1:0] dst_prdata,
	input  wire              dst_pready,
	input  wire              dst_pslverr
);

// ----------------------------------------------------------------------------
// AP logic

// Registers for extensions we don't implement (like T0TR from the tagged
// memory extension), are simply RES0, so not listed explicitly.

localparam REG_CSW  = 6'h00;
localparam REG_TAR  = 6'h01;
localparam REG_DRW  = 6'h03;
localparam REG_BD0  = 6'h04;
localparam REG_BD1  = 6'h05;
localparam REG_BD2  = 6'h06;
localparam REG_BD3  = 6'h07;
localparam REG_CFG  = 6'h3d;
localparam REG_BASE = 6'h3e;
localparam REG_IDR  = 6'h3f;


reg csw_tr_in_prog; // FIXME drive this
reg csw_addr_inc;

always @ (posedge swclk or negedge rst_n_por) begin
	if (!rst_n_por) begin
		csw_addr_inc <= 1'b0;
	end else if (dpacc_wen && dpacc_addr == REG_CSW) begin
		csw_addr_inc <= dpacc_wdata[4];
	end
end

reg  [31:0]       tar;

always @ (posedge swclk or negedge rst_n_por) begin
	if (!rst_n_por) begin
		tar <= {W_ADDR{1'b0}};
	end else if (dpacc_wen && dpacc_addr == REG_TAR) begin
		tar <= {dpacc_wdata[W_ADDR-1:2], 2'b00};
	end else if ((dpacc_wen || dpacc_ren) && csw_addr_inc && dpacc_addr == REG_DRW) begin
		// Note only DRW memory accesses increment, not BDx.
		tar <= {
			tar[W_ADDR-1:TAR_INCREMENT_BITS],
			tar[TAR_INCREMENT_BITS-1:2] + 1'b1, // self-determined size due to concat
			2'b00
		};
	end
end

reg [5:0] dpacc_addr_prev;
always @ (posedge swclk or negedge rst_n_por) begin
	if (!rst_n_por) begin
		dpacc_addr_prev <= 6'h0;
	end else if (dpacc_ren) begin
		dpacc_addr_prev <= dpacc_addr;
	end
end

// Register file read mux

wire [W_DATA-1:0] bridge_prdata;

always @ (*) begin
	case (dpacc_addr_prev)

	REG_CSW: dpacc_rdata = {
		1'b0,           // DbgSwEnable, unimplemented
		7'h0,           // Prot, unused for APB3 hence unimplemented
		1'b0,           // SDeviceEn, unimplemented
		7'h0,           // RES0
		4'h0,           // Type, unimplemented
		4'h0,           // Mode=Basic (no barriers), RO as we only have one mode
		csw_tr_in_prog,
		1'b1,           // DeviceEn=1 always
		1'b0,           // MSB of AddrInc is tied low as APB3 AP doesn't support packed transfers
		csw_addr_inc,
		1'b0,           // RES0
		3'h2            // Size=2, only 32-bit transfers supported
	};

	REG_TAR: dpacc_rdata = tar;

	REG_DRW: dpacc_rdata = bridge_prdata;

	REG_BD0: dpacc_rdata = bridge_prdata;

	REG_BD1: dpacc_rdata = bridge_prdata;

	REG_BD2: dpacc_rdata = bridge_prdata;

	REG_BD3: dpacc_rdata = bridge_prdata;

	REG_CFG: dpacc_rdata = {
		29'h0,          // RES0
		1'b0,           // LD=0, no large data
		1'b0,           // LA=0, no long address (why are addresses long and data large)
		1'b0            // BE=0, we support only the *correct* endianness
	};

	REG_BASE: dpacc_rdata = BASE;

	REG_IDR: dpacc_rdata = {
		IDR_REVISION,
		IDR_DESIGNER,
		4'h8,           // CLASS   = Mem-AP
		5'h0,           // RES0
		4'h0,           // VARIANT = 0
		4'h2            // TYPE    = APB2/APB3
	};

	endcase
end

// ----------------------------------------------------------------------------
// Non-optional async bridge
// (clock crossing and downstream protocol handling)

// Taking advantage of the bridge starting transfers immediately off of the
// setup phase, and not requiring a valid access phase.
wire              start_transfer;
wire              bridge_psel;
wire              bridge_penable = 1'b0;

wire              bridge_pwrite  = dpacc_wen;
wire [W_ADDR-1:0] bridge_paddr;
wire [W_DATA-1:0] bridge_pwdata  = dpacc_wdata;
wire              bridge_pready;
wire              bridge_pslverr;

opendap_apb_async_bridge #(
	.W_ADDR        (W_ADDR),
	.W_DATA        (W_DATA),
	.N_SYNC_STAGES (2)
) async_bridge (
	.clk_src     (swclk),
	.rst_n_src   (rst_n_por),

	.clk_dst     (clk_dst),
	.rst_n_dst   (rst_n_dst),

	.src_psel    (bridge_psel),
	.src_penable (bridge_penable),
	.src_pwrite  (bridge_pwrite),
	.src_paddr   (bridge_paddr),
	.src_pwdata  (bridge_pwdata),
	.src_prdata  (bridge_prdata),
	.src_pready  (bridge_pready),
	.src_pslverr (bridge_pslverr),

	.dst_psel    (dst_psel),
	.dst_penable (dst_penable),
	.dst_pwrite  (dst_pwrite),
	.dst_paddr   (dst_paddr),
	.dst_pwdata  (dst_pwdata),
	.dst_prdata  (dst_prdata),
	.dst_pready  (dst_pready),
	.dst_pslverr (dst_pslverr)
);

// Send bus transfers to bridge

assign bridge_paddr = {
	tar[W_ADDR-1:4],
	dpacc_addr == REG_DRW ? tar[3:2] : dpacc_addr[1:0],
	2'b00
};

wire dpacc_is_mem = dpacc_addr == REG_DRW || (dpacc_addr & 6'h3c) == REG_BD0;
assign bridge_psel = (dpacc_wen || dpacc_ren) && dpacc_is_mem;

// TODO abort
assign dpacc_rdy = bridge_pready;

reg error_vld;

always @ (posedge swclk or negedge rst_n_por) begin
	if (!rst_n_por) begin
		error_vld <= 1'b0;
	end else begin
		error_vld <= bridge_psel || !bridge_pready;
	end
end


assign dpacc_err = bridge_pslverr && error_vld;

endmodule

