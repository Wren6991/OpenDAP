#pragma once

class tb;

// Constants

static const uint32_t DPIDR_EXPECTED = 0xdeadbeefu;
static const uint32_t TARGETID_EXPECTED = 0xbaadf00du; 
// REVISION = 0, DESIGNER = 7ff, CLASS = Mem-AP, TYPE = APB2/APB3
static const uint32_t APIDR_EXPECTED = 0x0fff0002u;

enum ap_dp_t {
	DP = 0,
	AP = 1
};

enum swd_status_t {
	OK           = 1,
	WAIT         = 2,
	FAULT        = 4,
	DISCONNECTED = 7
};

static const int DP_REG_DPIDR      = 0;
static const int DP_REG_ABORT      = 0;
static const int DP_REG_CTRL_STAT  = 1;
static const int DP_REG_DLCR       = 1;
static const int DP_REG_TARGETID   = 1;
static const int DP_REG_DLPIDR     = 1;
static const int DP_REG_EVENTSTAT  = 1;
static const int DP_REG_RESEND     = 2;
static const int DP_REG_SELECT     = 2;
static const int DP_REG_RDBUF      = 3;
static const int DP_REG_TARGETSEL  = 3;

static const int DP_BANK_CTRL_STAT = 0;
static const int DP_BANK_DLCR      = 1;
static const int DP_BANK_TARGETID  = 2;
static const int DP_BANK_DLPIDR    = 3;
static const int DP_BANK_EVENTSTAT = 4;

static const uint32_t DP_CTRL_STAT_CSYSPWRUPACK = 1u << 31;
static const uint32_t DP_CTRL_STAT_CSYSPWRUPREQ = 1u << 30;
static const uint32_t DP_CTRL_STAT_CDBGPWRUPACK = 1u << 29;
static const uint32_t DP_CTRL_STAT_CDBGPWRUPREQ = 1u << 28;
static const uint32_t DP_CTRL_STAT_CDBGRSTACK   = 1u << 27;
static const uint32_t DP_CTRL_STAT_CDBGRSTREQ   = 1u << 26;
static const uint32_t DP_CTRL_STAT_WDATAERR     = 1u << 7;
static const uint32_t DP_CTRL_STAT_READOK       = 1u << 6;
static const uint32_t DP_CTRL_STAT_STICKYERR    = 1u << 5;
static const uint32_t DP_CTRL_STAT_STICKYCMP    = 1u << 4;
static const uint32_t DP_CTRL_STAT_STICKYORUN   = 1u << 1;
static const uint32_t DP_CTRL_STAT_ORUNDETECT   = 1u << 0;

static const int AP_REG_CSW   = 0;
static const int AP_REG_TAR   = 1;
static const int AP_REG_DRW   = 3;
static const int AP_REG_CFG   = 0;
static const int AP_REG_BASE  = 2;
static const int AP_REG_IDR   = 3;

static const int AP_BANK_CSW  = 0 << 4;
static const int AP_BANK_TAR  = 0 << 4;
static const int AP_BANK_DRW  = 0 << 4;
static const int AP_BANK_BDx  = 1 << 4;
static const int AP_BANK_CFG  = 0xf << 4;
static const int AP_BANK_BASE = 0xf << 4;
static const int AP_BANK_IDR  = 0xf << 4;

// Convenience functions

void put_bits(tb &t, const uint8_t *tx, int n_bits);
void get_bits(tb &t, uint8_t *rx, int n_bits);
void hiz_clocks(tb &t, int n_bits);
void idle_clocks(tb &t, int n_bits);

uint8_t swd_header(ap_dp_t ap_ndp, bool read_nwrite, uint8_t addr);

void send_dormant_to_swd(tb &t);
void swd_line_reset(tb &t);
void swd_targetsel(tb &t, uint32_t id);

swd_status_t swd_read(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t &data);
swd_status_t swd_write(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t data);
swd_status_t swd_read_orun(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t &data);
swd_status_t swd_write_orun(tb &t, ap_dp_t ap_dp, uint8_t addr, uint32_t data);

swd_status_t swd_prepare_dp_for_ap_access(tb &t);
