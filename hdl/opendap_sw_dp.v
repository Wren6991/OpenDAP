// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// SW-DP implementation. Supports DPv2, SWD version 2.

`default_nettype none

 module opendap_sw_dp #(
	parameter DPIDR     = 32'hdeadbeef,
	parameter TARGETID  = 32'hbaadf00d,
	parameter APSEL_MAX = 8'd0
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

	output wire [7:0]  ap_sel,
	output wire [5:0]  ap_addr,
	output wire [31:0] ap_wdata,
	output wire        ap_wen,
	input  wire [31:0] ap_rdata,
	output wire        ap_ren,
	input  wire        ap_rdy,
	input  wire        ap_err
);

wire        set_wdataerr;
wire        set_stickyorun;
wire        set_stickyerr = ap_rdy && ap_err;

wire [1:0]  hostacc_addr;
wire        hostacc_r_nw;
wire        hostacc_ap_ndp;
wire [31:0] hostacc_wdata;
wire        hostacc_en;
reg  [31:0] hostacc_rdata;
wire        hostacc_fault;

wire        hostacc_write = hostacc_en && !hostacc_fault && !hostacc_r_nw;
wire        hostacc_read  = hostacc_en && !hostacc_fault &&  hostacc_r_nw;
			/// TODO DAPABORT!

// ----------------------------------------------------------------------------
// DP register file

reg [7:0] select_apsel;
reg [3:0] select_apbanksel;
reg [3:0] select_dpbanksel;

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		select_apsel <= 8'h0;
		select_apbanksel <= 4'h0;
		select_dpbanksel <= 4'h0;
	end else if (hostacc_write && hostacc_addr == 2'b10) begin
		select_apsel <= hostacc_wdata[31:24];
		select_apbanksel <= hostacc_wdata[7:4];
		select_dpbanksel <= hostacc_wdata[3:0];
	end
end

reg ctrl_stat_csyspwrupreq;
reg ctrl_stat_cdbgpwrupreq;
reg ctrl_stat_orundetect;
reg ctrl_stat_readok;
reg ctrl_stat_stickyerr;
reg ctrl_stat_stickyorun;
reg ctrl_stat_wdataerr;

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		ctrl_stat_cdbgpwrupreq <= 1'b0;
		ctrl_stat_csyspwrupreq <= 1'b0;
		ctrl_stat_orundetect <= 1'b0;
		ctrl_stat_readok <= 1'b0;
		ctrl_stat_stickyerr <= 1'b0;
		ctrl_stat_stickyorun <= 1'b0;
		ctrl_stat_wdataerr <= 1'b0;
	end else begin
		// TODO READOK
		if (hostacc_write && hostacc_addr == 2'b00) begin
			// ABORT write
			ctrl_stat_stickyorun <= ctrl_stat_stickyorun && !hostacc_wdata[4];
			ctrl_stat_wdataerr <= ctrl_stat_wdataerr && !hostacc_wdata[3];
			ctrl_stat_stickyerr <= ctrl_stat_stickyerr && !hostacc_wdata[2];
		end
		if (hostacc_write && hostacc_addr == 2'b01 && select_dpbanksel == 4'h0) begin
			// CTRL/STAT write
			ctrl_stat_csyspwrupreq <= hostacc_wdata[30];
			ctrl_stat_cdbgpwrupreq <= hostacc_wdata[28];
			// CDBGRSTREQ is implemented RAZ/WI
			// We are MINDP; implement TRNCNT/MASKLANE/TRNMODE as RAZ/WI.
			ctrl_stat_orundetect <= hostacc_wdata[0];
		end
		if (set_wdataerr)
			ctrl_stat_wdataerr <= 1'b1;
		if (set_stickyorun)
			ctrl_stat_stickyorun <= 1'b1;
		if (set_stickyerr)
			ctrl_stat_stickyorun <= 1'b1;
	end
end

always @ (*) begin
	if (hostacc_ap_ndp) begin
		hostacc_rdata = ap_rdata;
	end else casez ({hostacc_addr, select_dpbanksel})
		6'h0z: hostacc_rdata = DPIDR;

		6'h10: hostacc_rdata = {
			csyspwrupack,
			ctrl_stat_csyspwrupreq,
			cdbgpwrupack,
			ctrl_stat_cdbgpwrupreq,
			2'b00,                  // CDBGRSTACK/CDBGRSTREQ
			2'b00,                  // RES0
			12'h0,                  // TRNCOUNT
			4'h0,                   // MASKLANE
			ctrl_stat_wdataerr,
			ctrl_stat_readok,
			ctrl_stat_stickyerr,
			1'b0,                   // STICKYCMP
			2'b00,                  // TRNMODE
			ctrl_stat_stickyorun,
			ctrl_stat_orundetect
		};

		6'h11: hostacc_rdata = 32'h0000_0040; // DLCR
		6'h12: hostacc_rdata = TARGETID;

		6'h13: hostacc_rdata = {              // DLPIDR
			instid, // TINSTANCE
			24'h0,  // RES0
			4'h1    // PROTVSN
		};

		6'h14: hostacc_rdata = {              // EVENTSTAT
			31'h0,  // RES0
			eventstat
		};

		6'h1z: hostacc_rdata = 32'h0000_0000; // UNPREDICTABLE
		6'h2z: hostacc_rdata = 32'h0000_0000; // RESEND is handled inside the serial comms.

		// Assumes AP rdata is stable (!). We may assume (B2.2.7) that the RDBUFF is
		// immediately preceded by the corresponding AP read.
		6'h3z: hostacc_rdata = ap_rdata;      // RDBUFF
	endcase
end

reg resend_possible;

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		resend_possible <= 1'b0;
	end else if (hostacc_en) begin
		// B2.2.8 says RESEND returns specifically the last AP read or RDBUFF
		// read, not just the last read in general. Since we are just
		// recirculating the shift register, we need to fail a RESEND if
		// any *other* type of read takes place since the last AP/RDBUFF
		// read, as well as if any write takes place.
		resend_possible <= hostacc_read && (hostacc_ap_ndp || hostacc_addr == 2'b11);
	end
end

wire hostacc_protocol_err =
	(hostacc_write && !hostacc_ap_ndp && {hostacc_addr, select_dpbanksel} == 6'h11 && // Bad TURNROUND
		hostacc_wdata[9:8] != 2'b00) ||
	(hostacc_read  && !hostacc_ap_ndp &&  hostacc_addr                    == 2'b10 && // Bad RESEND
		!resend_possible);

// ----------------------------------------------------------------------------
// Serial comms unit




wire any_sticky_errors = ctrl_stat_stickyorun || ctrl_stat_stickyorun || ctrl_stat_wdataerr;

// Access when a sticky flag is set causes a FAULT response, with the
// exception of very few DP registers. We are decoding this straight out of
// the serial comms' shift register, so must be combinatorial, and not a
// function of hostacc_en.

assign hostacc_fault = any_sticky_errors && !(!hostacc_ap_ndp && (
	( hostacc_r_nw && hostacc_addr == 2'b00                            ) || // DPIDR read
	( hostacc_r_nw && hostacc_addr == 2'b01 && select_dpbanksel == 4'h0) || // CTRL/STAT read
	(!hostacc_r_nw && hostacc_addr == 2'b00                            ) || // ABORT write
	(!hostacc_r_nw && hostacc_addr == 2'b11                            )    // TARGETSEL write
));

opendap_sw_dp_serial_comms serial_comms (
	.swclk               (swclk),
	.rst_n               (rst_n),

	.swdi                (swdi),
	.swdo                (swdo),
	.swdo_en             (swdo_en),

	.bus_addr            (hostacc_addr),
	.bus_r_nw            (hostacc_r_nw),
	.bus_ap_ndp          (hostacc_ap_ndp),
	.bus_wdata           (hostacc_wdata),
	.bus_rdata           (hostacc_rdata),
	.bus_en              (hostacc_en),

	.targetsel_expected  ({instid, TARGETID[27:0]}),
	.dp_set_wdataerr     (set_wdataerr),
	.dp_set_stickyorun   (set_stickyorun),
	.dp_orundetect       (ctrl_stat_orundetect),
	.dp_acc_fault        (hostacc_fault),
	.dp_acc_protocol_err (hostacc_protocol_err),
	.ap_rdy              (ap_rdy)
);

endmodule
