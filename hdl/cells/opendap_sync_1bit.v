// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// A 2FF synchronizer to mitigate metastabilities. This is a baseline
// implementation -- you should replace it with cells specific to your
// FPGA/process

`ifndef OPENDAP_REG_KEEP_ATTRIBUTE
`define OPENDAP_REG_KEEP_ATTRIBUTE (* keep = 1'b1 *)
`endif

module opendap_sync_1bit #(
	parameter N_STAGES = 2 // Should be >=2
) (
	input wire clk,
	input wire rst_n,
	input wire i,
	output wire o
);

`OPENDAP_REG_KEEP_ATTRIBUTE reg [N_STAGES-1:0] sync_flops;

always @ (posedge clk or negedge rst_n)
	if (!rst_n)
		sync_flops <= {N_STAGES{1'b0}};
	else
		sync_flops <= {sync_flops[N_STAGES-2:0], i};

assign o = sync_flops[N_STAGES-1];

endmodule
