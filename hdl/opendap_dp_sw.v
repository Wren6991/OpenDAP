// ----------------------------------------------------------------------------
// Part of the OpenDAP project (c) Luke Wren 2021
// SPDX-License-Identifier CC0-1.0
// ----------------------------------------------------------------------------

 // DP SW implementation.

 module opendap_dp_sw #(
	parameter DPIDR = 32'hdeadbeef
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

opendap_dormant_monitor dormant_monitor (
	.swclk         (swclk),
	.rst_n         (rst_n),
	.swdi_reg      (swdi_reg),
	.exit_dormant  (exit_dormant),
	.enter_dormant (enter_dormant),
	.line_reset    (line_reset)
);


reg [6:0] header_sreg;
reg       header_sreg_en;

reg [31:0] data_sreg;
reg        data_sreg_en;

wire header_ok        = ~^header_sreg[4:0] && header_sreg[6:5] == 2'b10;
wire header_addr      = header_sreg[3:2];
wire header_r_nw      = header_sreg[1];
wire header_ap_ndp    = header_sreg[0];

reg ctrl_stat_cdbgpwrupreq;
reg ctrl_stat_csyspwrupreq;
reg ctrl_stat_orundetect;
reg ctrl_stat_readok;
reg ctrl_stat_stickyerr;
reg ctrl_stat_stickyorun;
reg ctrl_stat_wdataerr;

wire any_sticky_errors = ctrl_stat_stickyorun || ctrl_stat_stickyorun || ctrl_stat_wdataerr;

localparam W_LINK_STATE    = 2;
localparam LINK_DORMANT    = 2'd0;
localparam LINK_RESET      = 2'd1;
localparam LINK_ACTIVE     = 2'd2
localparam LINK_LOCKEDOUT  = 2'd3;

localparam W_PHASE_STATE         = 4;
localparam PHASE_IDLE            = ;
localparam PHASE_HEADER          = ;
localparam PHASE_TURN_TO_ACK     = ;
localparam PHASE_ACK_WAIT        = ;
localparam PHASE_ACK_FAULT       = ;
localparam PHASE_TURN_TO_IDLE    = ;
localparam PHASE_ACK_OK          = ;
localparam PHASE_TARGETSEL_ACK   = ;
localparam PHASE_TARGETSEL_DATA  = ;
localparam PHASE_TURN_TO_WDATA   = ;
localparam PHASE_WDATA           = ;
localparam PHASE_RDATA           = ;
localparam PHASE_TURN_FROM_RDATA = ;
localparam PHASE_SKIP_DATA       = ;

reg [W_LINK_STATE-1:0]  link_state, link_state_nxt;
reg [W_PHASE_STATE-1:0] phase,      phase_nxt;
reg [5:0]               bit_ctr,    bit_ctr_nxt;

reg swdo_next;
reg swdo_en_next;

always @ (*) begin
	link_state_nxt = link_state;
	phase_nxt = phase;
	bit_ctr_nxt = bit_ctr - |bit_ctr;

	header_sreg_en = 1'b0;
	data_sreg_en = 1'b0; 

	swdo_next = 1'b0;
	swdo_en_next = 1'b0;

	if (link_state == LINK_RESET || link_state == LINK_ACTIVE) case (phase)
		PHASE_IDLE: begin
			if (swdi_reg) begin
				phase_nxt = PHASE_HEADER;
				bit_ctr_nxt = 6'd6;
			end
		end
		PHASE_HEADER: begin
			if (~|bit_ctr) begin
				phase_nxt = PHASE_TURN_TO_ACK;
			end
		end
		PHASE_TURN_TO_ACK: begin
			if (!header_ok) begin
				link_state_nxt = LINK_LOCKEDOUT;
			end else if (!header_ap_ndp && !header_r_nw && header_addr == 2'b11) begin
				phase_nxt = PHASE_TARGETSEL_ACK;
			end else begin
				swdo_en_next = 1'b1;
				bit_ctr_nxt = 6'd2;
				if (any_sticky_errors) begin
					phase_nxt = PHASE_ACK_FAULT;
				end else if (!ap_rdy) begin
					phase_nxt = PHASE_ACK_WAIT;
				end else begin
					phase_nxt = PHASE_ACK_OK;
					swdo_next = 1'b1;
				end
			end
		end
		PHASE_ACK_FAULT: begin
			if (|bit_ctr) begin
				swdo_en_next = 1'b1;
				swdo_next = bit_ctr == 6'd1;
			end else if (ctrl_stat_orundetect) begin
				phase_nxt = PHASE_SKIP_DATA;
			end else begin
				phase_nxt = PHASE_TURN_TO_IDLE;
			end
		end
		PHASE_ACK_WAIT: begin
			if (|bit_ctr) begin
				swdo_en_next = 1'b1;
				swdo_next = bit_ctr == 6'd2;
			end else if (ctrl_stat_orundetect) begin
				phase_nxt = PHASE_SKIP_DATA;
				bit_ctr_nxt = 6'd32;
			end else begin
				phase_nxt = PHASE_TURN_TO_IDLE;
			end
		end
		PHASE_TURN_TO_IDLE: begin
			phase_nxt = PHASE_IDLE;
		end
		PHASE_ACK_OK: begin
			if (|bit_ctr) begin
				swdo_en_next = 1'b1;
			end else if (header_r_nw) begin
				swdo_en_next = 1'b1;
				swdo_next = data_sreg[0];
				phase_nxt = PHASE_RDATA;
				bit_ctr_nxt = 6'd31;
			end else begin
				phase_nxt = PH
			end
		end

	if (link_state == LINK_DORMANT && exit_dormant)
		link_state_nxt = LINK_RESET;
	if (link_state == LINK_LOCKEDOUT && line_reset)
		link_state_nxt = LINK_RESET;
	if (enter_dormant)
		link_state_nxt = LINK_DORMANT;

	if (link_state != LINK_RESET && link_state != LINK_ACTIVE)
		phase_nxt = PHASE_IDLE;


end
