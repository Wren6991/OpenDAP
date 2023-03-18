// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2023
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// IO registers for SWDIO in/out/oen pad signals. All paths between the SWDIO
// pad and the SW-DP pass through these registers 

// This is a baseline implementation -- you should replace it with cells
// specific to your FPGA/process, or replace it with passthrough wires so
// that you can instantiate a registered IO cell at a higher hierarchy
// level.

`ifndef OPENDAP_REG_KEEP_ATTRIBUTE
`define OPENDAP_REG_KEEP_ATTRIBUTE (* keep = 1'b1 *)
`endif

`default_nettype none

module opendap_swdio_flops (
	input  wire clk,
	input  wire rst_n,

	// DP-facing signals
	input  wire dp_swdo_next,
	input  wire dp_swdo_en_next,
	output wire dp_swdi_prev,

	// Pad-facing signals
	output wire pad_swdo,
	output wire pad_swdo_en,
	input  wire pad_swdi
);

`OPENDAP_REG_KEEP_ATTRIBUTE reg swdi_reg;
`OPENDAP_REG_KEEP_ATTRIBUTE reg swdo_reg;
`OPENDAP_REG_KEEP_ATTRIBUTE reg swdo_en_reg;

always @ (posedge clk or negedge rst_n) begin
	if (!rst_n) begin
		swdi_reg <= 1'b0;
		swdo_reg <= 1'b0;
		swdo_en_reg <= 1'b0;
	end else begin
		swdi_reg <= pad_swdi;
		swdo_reg <= dp_swdo_next;
		swdo_en_reg <= dp_swdo_en_next;
	end
end

assign pad_swdo = swdo_reg;
assign pad_swdo_en = swdo_en_reg;
assign dp_swdi_prev = swdi_reg;

endmodule

`ifndef YOSYS
`default_nettype wire
`endif
