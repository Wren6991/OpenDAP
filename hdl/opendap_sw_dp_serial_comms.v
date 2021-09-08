// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

// Serialise and deserialise data. Track link state, detect parity or serial
// protocol errors, provide OK/WAIT/FAULT responses. Perform parallel
// accesses on the core DP logic, which may be forwarded on to the AP.

 module opendap_sw_dp_serial_comms (
	input  wire        swclk,
	input  wire        rst_n,

	input  wire        swdi,
	output reg         swdo,
	output reg         swdo_en,

	output wire [1:0]  bus_addr,
	output wire [1:0]  bus_ap_ndp,
	output wire [31:0] bus_wdata,
	output reg         bus_wen,
	input  wire [31:0] bus_rdata,
	output reg         bus_ren,

	input  wire [31:0] targetid,

	output reg         dp_set_wdataerr,
	output reg         dp_set_stickyorun,
	input  wire        dp_orundetect,
	input  wire        dp_any_sticky_err,
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

wire header_ok     = ~^header_sreg[4:0] && !header_sreg[5] && swdi_reg;
wire header_addr   = header_sreg[3:2];
wire header_r_nw   = header_sreg[1];
wire header_ap_ndp = header_sreg[0];

assign bus_ap_ndp  = header_ap_ndp;
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
localparam PHASE_TURN_TO_ACK     = 4'd2;
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

	bus_wen = 1'b0;
	bus_ren = 1'b0;
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
				phase_nxt = PHASE_TURN_TO_ACK;
			end
		end

		PHASE_TURN_TO_ACK: begin
			// Note this is actually the cycle where the park bit appears in swdi_reg. The posedge
			// after we register the park bit is the edge where we assert the first ACK bit.
			if (!header_ok) begin
				link_state_nxt = LINK_LOCKEDOUT;
			end else if (!header_ap_ndp && !header_r_nw && header_addr == 2'b11) begin
				if (link_state == LINK_RESET) begin
					// Do not drive swdo_en -- TARGETSEL is supposed to be unacknowledged.
					phase_nxt = PHASE_TARGETSEL_ACK;
					bit_ctr_nxt = 6'd2;
				end else begin
					// Writes to TARGETSEL which don't immediately follow a line reset are
					// UNPREDICTABLE (ref B4.3.4). We allow multiple successful TARGETSELs when in
					// the reset state, but after leaving the reset state via DPIDR read, TARGETSEL
					// causes unconditional lockout/deselect.
					link_state_nxt = LINK_LOCKEDOUT;
				end
			end else if (link_state == LINK_RESET && !(!header_ap_ndp && header_r_nw && header_addr == 2'b00) begin
				// In reset state, anything but TARGETSEL write or DPIDR read causes immediate lockout
				link_state_nxt = LINK_LOCKEDOUT;
			end else begin
				// If we're in reset this must be DPIDR read, so go to active.
				link_state_nxt = LINK_ACTIVE;
				swdo_en_nxt = 1'b1;
				bit_ctr_nxt = 6'd2;
				if (dp_any_sticky_err) begin
					phase_nxt = PHASE_ACK_FAULT;
					dp_set_stickyorun = dp_orundetect;
				end else if (!ap_rdy) begin
					phase_nxt = PHASE_ACK_WAIT;
					dp_set_stickyorun = dp_orundetect;
				end else begin
					bus_ren = 1'b1;
					// Path from bus_ren -> dp_acc_protocol_err, careful to avoid loop.
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
			end
		end

		PHASE_RDATA: begin
			if (bit_ctr == 6'd0) begin
				phase_nxt = PHASE_TURN_FROM_RDATA;
			end else if (bit_ctr == 6'd1) begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = data_parity;
			end else begin
				swdo_en_nxt = 1'b1;
				swdo_nxt = data_sreg[0];
				data_parity_nxt = data_parity ^ data_sreg[0];
			end
		end

		PHASE_TURN_FROM_RDATA: begin
			phase_nxt = PHASE_IDLE;
		end

		PHASE_TURN_TO_WDATA: begin
			phase_nxt = PHASE_WDATA;
			bit_ctr_nxt = 6'd32;
		end

		PHASE_WDATA: begin
			if (~|bit_ctr) begin
				phase_nxt = PHASE_IDLE;
				if (swdi_reg != data_parity) begin
					dp_set_wdataerr = 1'b1;
				end else begin
					bus_wen = 1'b1;
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
			// state if TARGETID doesn't match data". A parity error is treated as a TARGETID
			// mismatch (B4.1.6) and also, confusingly, as a protocol error (B4.3.4), which we
			// luckily handle identically.
			if (~|bit_ctr) begin
				if (targetid != data_sreg || swdi_reg != data_parity) begin
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
	if (link_state == LINK_LOCKEDOUT && line_reset)
		link_state_nxt = LINK_RESET;
	if (enter_dormant)
		link_state_nxt = LINK_DORMANT;

	if (link_state != LINK_RESET && link_state != LINK_ACTIVE)
		phase_nxt = PHASE_IDLE;

end


endmodule
