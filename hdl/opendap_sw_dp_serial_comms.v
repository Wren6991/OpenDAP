// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// Serialise and deserialise data. Track link state, detect parity or serial
// protocol errors, provide OK/WAIT/FAULT responses. Perform parallel
// accesses on the core DP logic, which may be forwarded on to the AP.

`default_nettype none

 module opendap_sw_dp_serial_comms (
	input  wire        swclk,
	input  wire        rst_n,

	input  wire        swdi,
	output reg         swdo,
	output reg         swdo_en,

	output wire [1:0]  bus_addr,
	output wire        bus_r_nw,
	output wire        bus_ap_ndp,
	output wire [31:0] bus_wdata,
	output reg         bus_en,
	input  wire [31:0] bus_rdata,

	input  wire [31:0] targetsel_expected,

	output reg         dp_set_wdataerr,
	output reg         dp_set_stickyorun,
	input  wire        dp_orundetect,
	input  wire        dp_acc_fault,
	input  wire        dp_acc_protocol_err,
	input  wire        ap_rdy
);

reg swdi_reg;
always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		swdi_reg <= 1'b0;
	end else begin
		swdi_reg <= swdi;
	end
end

wire exit_dormant;
wire enter_dormant;
wire line_reset;

opendap_swd_dormant_monitor dormant_monitor (
	.swclk         (swclk),
	.rst_n         (rst_n),
	.swdi_reg      (swdi_reg),
	.exit_dormant  (exit_dormant),
	.enter_dormant (enter_dormant),
	.line_reset    (line_reset)
);


reg [5:0] header_sreg;
reg       header_sreg_en;

reg [31:0] data_sreg;
reg        data_sreg_en;

wire       header_ok     = ~^header_sreg[4:0] && !header_sreg[5] && swdi_reg;
wire [1:0] header_addr   = header_sreg[3:2];
wire       header_r_nw   = header_sreg[1];
wire       header_ap_ndp = header_sreg[0];

wire header_is_dpidr_read = !header_ap_ndp && header_r_nw && header_addr == 2'b00;
wire header_is_targetsel_write = !header_ap_ndp && !header_r_nw && header_addr == 2'b11;
wire header_is_resend = !header_ap_ndp && header_r_nw && header_addr == 2'b11;

assign bus_ap_ndp  = header_ap_ndp;
assign bus_r_nw    = header_r_nw;
assign bus_addr    = header_addr;
assign bus_wdata   = data_sreg;

localparam W_LINK_STATE          = 2;
localparam LINK_DORMANT          = 2'd0;
localparam LINK_RESET            = 2'd1;
localparam LINK_ACTIVE           = 2'd2;
localparam LINK_LOCKEDOUT        = 2'd3;

localparam W_PHASE_STATE         = 4;
localparam PHASE_IDLE            = 4'd0;
localparam PHASE_HEADER          = 4'd1;
localparam PHASE_PARK            = 4'd2;
localparam PHASE_ACK_WAIT        = 4'd3;
localparam PHASE_ACK_FAULT       = 4'd4;
localparam PHASE_ACK_OK          = 4'd5;
localparam PHASE_TURN_TO_IDLE    = 4'd6;
localparam PHASE_TURN_TO_WDATA   = 4'd7;
localparam PHASE_WDATA           = 4'd8;
localparam PHASE_RDATA           = 4'd9;
localparam PHASE_TURN_FROM_RDATA = 4'd10;
localparam PHASE_TARGETSEL_ACK   = 4'd11;
localparam PHASE_TARGETSEL_DATA  = 4'd12;

reg [W_LINK_STATE-1:0]  link_state,  link_state_nxt;
reg [W_PHASE_STATE-1:0] phase,       phase_nxt;
reg [5:0]               bit_ctr,     bit_ctr_nxt;
reg                     data_parity, data_parity_nxt;

reg                                  swdo_nxt;
reg                                  swdo_en_nxt;

always @ (*) begin
	link_state_nxt = link_state;
	phase_nxt = phase;
	bit_ctr_nxt = bit_ctr - |bit_ctr;
	data_parity_nxt = 1'b0;

	header_sreg_en = 1'b0;
	data_sreg_en = 1'b0;

	bus_en = 1'b0;
	dp_set_wdataerr = 1'b0;
	dp_set_stickyorun = 1'b0;

	swdo_nxt = 1'b0;
	swdo_en_nxt = 1'b0;

	if (link_state == LINK_RESET || link_state == LINK_ACTIVE) case (phase)

		PHASE_IDLE: begin
			if (swdi_reg) begin
				phase_nxt = PHASE_HEADER;
				bit_ctr_nxt = 6'd5;
			end
		end

		PHASE_HEADER: begin
			header_sreg_en = 1'b1;
			if (~|bit_ctr) begin
				phase_nxt = PHASE_PARK;
			end
		end

		PHASE_PARK: begin
			// This is the cycle where the park bit appears in swdi_reg. That means, in the outside
			// world, the park-to-ACK turnaround is ongoing. The posedge after we register the park
			// bit is the edge where we assert the first ACK bit.
			if (!header_ok) begin
				link_state_nxt = LINK_LOCKEDOUT;
				dp_set_stickyorun = dp_orundetect;
			end else if (header_is_targetsel_write) begin
				if (link_state == LINK_RESET) begin
					// Do not drive swdo_en -- TARGETSEL is supposed to be unacknowledged.
					phase_nxt = PHASE_TARGETSEL_ACK;
					bit_ctr_nxt = 6'd2;
				end else begin
					// Writes to TARGETSEL which don't immediately follow a line reset are
					// UNPREDICTABLE (ref B4.3.4). We allow multiple successful TARGETSELs when in
					// the reset state, but after leaving the reset state via DPIDR read, TARGETSEL
					// causes unconditional lockout.
					link_state_nxt = LINK_LOCKEDOUT;
				end
			end else if (link_state == LINK_RESET && !header_is_dpidr_read) begin
				// In reset state, anything but TARGETSEL write or DPIDR read causes immediate lockout
				link_state_nxt = LINK_LOCKEDOUT;
			end else begin
				// If we're in reset this must be DPIDR read, so go to active.
				link_state_nxt = LINK_ACTIVE;
				swdo_en_nxt = 1'b1;
				bit_ctr_nxt = 6'd2;
				if (dp_acc_fault) begin
					// This depends on the sticky error status, the packet header, *and* the value
					// of DPBANKSEL, so we rely on the DP to decode faults.
					phase_nxt = PHASE_ACK_FAULT;
					dp_set_stickyorun = dp_orundetect;
				end else if (!ap_rdy) begin
					phase_nxt = PHASE_ACK_WAIT;
					dp_set_stickyorun = dp_orundetect;
				end else begin
					bus_en = header_r_nw;
					if (dp_acc_protocol_err) begin
						link_state_nxt = LINK_LOCKEDOUT;
					end else begin
						phase_nxt = PHASE_ACK_OK;
						swdo_nxt = 1'b1;
					end
				end
			end
		end

		PHASE_ACK_FAULT: begin
			if (|bit_ctr) begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = bit_ctr == 6'd1;
			end else begin
				phase_nxt = PHASE_TURN_TO_IDLE;
				if (dp_orundetect)
					bit_ctr_nxt = 6'd32;
			end
		end

		PHASE_ACK_WAIT: begin
			if (|bit_ctr) begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = bit_ctr == 6'd2;
			end else begin
				phase_nxt = PHASE_TURN_TO_IDLE;
				if (dp_orundetect)
					bit_ctr_nxt = 6'd32;
			end
		end

		PHASE_TURN_TO_IDLE: begin
			if (~|bit_ctr)
				phase_nxt = PHASE_IDLE;
		end

		PHASE_ACK_OK: begin
			if (|bit_ctr) begin
				swdo_en_nxt = 1'b1;
			end else if (header_r_nw) begin
				phase_nxt = PHASE_RDATA;
				swdo_en_nxt = 1'b1;
				swdo_nxt = data_sreg[0];
				data_parity_nxt = data_parity ^ data_sreg[0];
				data_sreg_en = 1'b1;
				bit_ctr_nxt = 6'd32;
			end else begin
				phase_nxt = PHASE_TURN_TO_WDATA;
				bit_ctr_nxt = 1'b1;
			end
		end

		PHASE_RDATA: begin
			if (bit_ctr == 6'd0) begin
				phase_nxt = PHASE_TURN_FROM_RDATA;
				bit_ctr_nxt = 6'd1;
			end else if (bit_ctr == 6'd1) begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = data_parity;
			end else begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = data_sreg[0];
				data_parity_nxt = data_parity ^ data_sreg[0];
				data_sreg_en = 1'b1;
			end
		end

		PHASE_TURN_FROM_RDATA: begin
			// This lasts for two cycles, for similar reasons to TURN_TO_WDATA
			if (~|bit_ctr)
				phase_nxt = PHASE_IDLE;
		end

		PHASE_TURN_TO_WDATA: begin
			// This lasts for two cycles -- we swallowed one cycle by jumping the gun at the park
			// bit, to get the ACK out on time. Now we need to sync back up to the bits on swdi_reg.
			if (~|bit_ctr) begin
				phase_nxt = PHASE_WDATA;
				bit_ctr_nxt = 6'd32;
			end
		end

		PHASE_WDATA: begin
			if (~|bit_ctr) begin
				phase_nxt = PHASE_IDLE;
				if (swdi_reg != data_parity) begin
					dp_set_wdataerr = 1'b1;
				end else begin
					bus_en = 1'b1;
				end
			end else begin
				data_sreg_en = 1'b1;
				data_parity_nxt = data_parity ^ swdi_reg;
			end
		end

		PHASE_TARGETSEL_ACK: begin
			if (~|bit_ctr) begin
				phase_nxt = PHASE_TARGETSEL_DATA;
				bit_ctr_nxt = 6'd32;
			end

		end

		PHASE_TARGETSEL_DATA: begin
			// There are lots of words in the spec but TARGETSEL seems to really be "go to lockout
			// state if ID doesn't match". A parity error is treated as a ID mismatch (B4.1.6) and
			// also, confusingly, as a protocol error (B4.3.4), which we would handle identically.
			if (~|bit_ctr) begin
				if (targetsel_expected != data_sreg || swdi_reg != data_parity) begin
					link_state_nxt = LINK_LOCKEDOUT;
				end else begin
					phase_nxt = PHASE_IDLE;
				end
			end else begin
				data_sreg_en = 1'b1;
				data_parity_nxt = data_parity ^ swdi_reg;
			end

		end

	endcase

	if (link_state == LINK_DORMANT && exit_dormant)
		link_state_nxt = LINK_RESET;
	if (link_state == LINK_LOCKEDOUT && line_reset) begin
		link_state_nxt = LINK_RESET;
		dp_set_stickyorun = dp_orundetect;
	end
	if (enter_dormant)
		link_state_nxt = LINK_DORMANT;

	if (link_state != LINK_RESET && link_state != LINK_ACTIVE)
		phase_nxt = PHASE_IDLE;

end

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		link_state  <= LINK_DORMANT;
		phase       <= PHASE_IDLE;
		bit_ctr     <= 6'h0;
		data_parity <= 1'b0;
		swdo        <= 1'b0;
		swdo_en     <= 1'b0;
	end else begin
		link_state  <= link_state_nxt;
		phase       <= phase_nxt;
		bit_ctr     <= bit_ctr_nxt;
		data_parity <= data_parity_nxt;
		swdo        <= swdo_nxt;
		swdo_en     <= swdo_en_nxt;
	end
end

always @ (posedge swclk or negedge rst_n) begin
	if (!rst_n) begin
		data_sreg <= 32'h0;
		header_sreg <= 6'h0;
	end else begin
		// On reads we recirculate the data so that we can RESEND it.
		if (data_sreg_en)
			data_sreg <= {header_r_nw ? data_sreg[0] : swdi_reg, data_sreg[31:1]};
		if (bus_en && bus_r_nw && !header_is_resend)
			data_sreg <= bus_rdata;

		if (header_sreg_en)
			header_sreg <= {swdi_reg, header_sreg[5:1]};
	end
end

endmodule
