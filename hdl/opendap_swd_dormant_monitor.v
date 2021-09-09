// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// Watch for switches between the Dormant and SWD link states.
// Ref: IHI0031F, "B5.3 Dormant Operation"

module opendap_swd_dormant_monitor (
	input  wire swclk,
	input  wire rst_n,

	input  wire swdi_reg,

	output reg  exit_dormant,
	output reg  enter_dormant,
	output wire line_reset
);

// Magic numbers from the spec
localparam [7:0]  SELECT_D2S = 8'b01011000;
localparam [15:0] SELECT_S2D = 16'b00111101_11000111;
localparam [6:0]  LFSR_INIT  = 7'b1001001;
localparam [6:0]  LFSR_TAPS  = 7'b1001011;

// The "activation select" sequence is a zero bit followed by 127 cycles from
// this magic LFSR. We resync the LFSR whenever we see a sequence mismatch.

reg  [6:0] lfsr;
wire       lfsr_resync;

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		lfsr <= LFSR_INIT;
	end else if (lfsr_resync) begin
		lfsr <= LFSR_INIT;
	end else begin
		lfsr <= {^(lfsr & LFSR_TAPS), lfsr[6:1]};
	end
end

localparam W_STATE = 3;
localparam S_D2S_START_BIT = 3'd0;
localparam S_D2S_ALERT     = 3'd1;
localparam S_D2S_POSTALERT = 3'd2;
localparam S_D2S_SELECT    = 3'd3;
localparam S_S2D_RESET     = 3'd4;
localparam S_S2D_SELECT    = 3'd5;

reg [6:0]         bit_ctr, bit_ctr_nxt;
reg [5:0]         rst_ctr, rst_ctr_nxt;
reg [W_STATE-1:0] state,   state_nxt;

always @ (*) begin
	state_nxt = state;
	bit_ctr_nxt = bit_ctr - |bit_ctr;
	rst_ctr_nxt = swdi_reg ? rst_ctr - |rst_ctr : 6'd50;

	enter_dormant = 1'b0;
	exit_dormant = 1'b0;
	line_reset = 1'b0;
	lfsr_resync = 1'b1;

	case (state)
	S_D2S_START_BIT: begin
		bit_ctr_nxt = 7'd126;
		if (!swdi_reg) begin
			state_nxt = S_D2S_ALERT;
		end
	end
	S_D2S_ALERT: begin
		if (swdi_reg == lfsr[0]) begin
			lfsr_resync = 1'b0;
			if (~|bit_ctr) begin
				bit_ctr_nxt = 7'd3;
				state_nxt = S_D2S_POSTALERT;
			end
		end else begin
			bit_ctr_nxt = 7'd126;
			state_nxt = swdi_reg ? S_D2S_START_BIT : S_D2S_ALERT;
		end
	end
	S_D2S_POSTALERT: begin
		// Ignore 4 cycles whilst host drives SWDIO low.
		if (~|bit_ctr) begin
			bit_ctr_nxt = 7'd7;
			state_nxt = S_D2S_SELECT;
		end
	end
	S_D2S_SELECT: begin
		if (swdi_reg == SELECT_D2S[bit_ctr]) begin
			if (~|bit_ctr) begin
				exit_dormant = 1'b1;
				state_nxt = S_S2D_RESET;
				rst_ctr_nxt = 7'd49;
			end
		end else begin
			bit_ctr_nxt = 7'd126;
			state_nxt = swdi_reg ? S_D2S_START_BIT : S_D2S_ALERT;
		end
	end
	S_S2D_RESET: begin
		if (~|rst_ctr_nxt)
			state_nxt = S_S2D_SELECT;
	end
	S_S2D_SELECT: begin
		if (swdi_reg == SELECT_S2D[bit_ctr]) begin
			if (~|bit_ctr) begin
				enter_dormant = 1'b1;
				state_nxt = S_D2S_START_BIT;
			end
		end else begin
			if (~|rst_ctr && !swdi_reg) begin
				bit_ctr_nxt = 7'd14;
			end else if (~|rst_ctr_nxt) begin
				bit_ctr_nxt = 7'd15;
			end else begin
				state_nxt = S_S2D_RESET;
			end
		end
	end
	default: begin
		state_nxt = S_D2S_START_BIT;
	end
	endcase
end

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		state <= S_D2S_START_BIT;
		bit_ctr <= 7'd0;
		rst_ctr <= 6'd0;
	end else begin
		state <= state_nxt;
		bit_ctr <= bit_ctr_nxt;
		rst_ctr <= rst_ctr_nxt;
	end
end

assign line_reset = ~|rst_ctr_nxt;

endmodule
