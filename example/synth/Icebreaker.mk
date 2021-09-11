CHIPNAME=fpga_icebreaker
DOTF=../fpga/fpga_icebreaker.f

DEVICE=up5k
PACKAGE=sg48

include $(SCRIPTS)/synth_ice40.mk

prog: bit
	iceprog $(CHIPNAME).bin
