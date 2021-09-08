// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

 module opendap_sw_dp #(
	parameter DPIDR = 32'hdeadbeef,
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

	output wire [7:0]  ap_sel,
	output wire [5:0]  ap_addr,
	output wire [31:0] ap_wdata,
	output wire        ap_wen,
	input  wire [31:0] ap_rdata,
	output wire        ap_ren,
	input  wire        ap_rdy
);

reg ctrl_stat_cdbgpwrupreq;
reg ctrl_stat_csyspwrupreq;
reg ctrl_stat_orundetect;
reg ctrl_stat_readok;
reg ctrl_stat_stickyerr;
reg ctrl_stat_stickyorun;
reg ctrl_stat_wdataerr;

wire any_sticky_errors = ctrl_stat_stickyorun || ctrl_stat_stickyorun || ctrl_stat_wdataerr;
