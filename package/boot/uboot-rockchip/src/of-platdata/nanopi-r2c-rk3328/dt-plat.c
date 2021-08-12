/*
 * DO NOT MODIFY
 *
 * Declares the U_BOOT_DRIVER() records and platform data.
 * This was generated by dtoc from a .dtb (device tree binary) file.
 */

/* Allow use of U_BOOT_DRVINFO() in this file */
#define DT_PLAT_C

#include <common.h>
#include <dm.h>
#include <dt-structs.h>

/* Node /clock-controller@ff440000 index 0 */
static struct dtd_rockchip_rk3328_cru dtv_clock_controller_at_ff440000 = {
	.reg			= {0xff440000, 0x1000},
	.rockchip_grf		= 0x3a,
};
U_BOOT_DRVINFO(clock_controller_at_ff440000) = {
	.name		= "rockchip_rk3328_cru",
	.plat	= &dtv_clock_controller_at_ff440000,
	.plat_size	= sizeof(dtv_clock_controller_at_ff440000),
	.parent_idx	= -1,
};

/* Node /dmc index 1 */
static struct dtd_rockchip_rk3328_dmc dtv_dmc = {
	.reg			= {0xff400000, 0x1000, 0xff780000, 0x3000, 0xff100000, 0x1000, 0xff440000, 0x1000,
		0xff720000, 0x1000, 0xff798000, 0x1000},
	.rockchip_sdram_params	= {0x1, 0xa, 0x2, 0x1, 0x0, 0x0, 0x11, 0x0,
		0x11, 0x0, 0x0, 0x94291288, 0x0, 0x27, 0x462, 0x15,
		0x242, 0xff, 0x14d, 0x0, 0x1, 0x0, 0x0, 0x0,
		0x43049010, 0x64, 0x28003b, 0xd0, 0x20053, 0xd4, 0x220000, 0xd8,
		0x100, 0xdc, 0x40000, 0xe0, 0x0, 0xe4, 0x110000, 0xe8,
		0x420, 0xec, 0x400, 0xf4, 0xf011f, 0x100, 0x9060b06, 0x104,
		0x20209, 0x108, 0x505040a, 0x10c, 0x40400c, 0x110, 0x5030206, 0x114,
		0x3030202, 0x120, 0x3030b03, 0x124, 0x20208, 0x180, 0x1000040, 0x184,
		0x0, 0x190, 0x7030003, 0x198, 0x5001100, 0x1a0, 0xc0400003, 0x240,
		0x6000604, 0x244, 0x201, 0x250, 0xf00, 0x490, 0x1, 0xffffffff,
		0xffffffff, 0xffffffff, 0xffffffff, 0x4, 0xc, 0x28, 0xa, 0x2c,
		0x0, 0x30, 0x9, 0xffffffff, 0xffffffff, 0x77, 0x88, 0x79,
		0x79, 0x87, 0x97, 0x87, 0x78, 0x77, 0x78, 0x87,
		0x88, 0x87, 0x87, 0x77, 0x78, 0x78, 0x78, 0x78,
		0x78, 0x78, 0x78, 0x78, 0x78, 0x69, 0x9, 0x77,
		0x78, 0x77, 0x78, 0x77, 0x78, 0x77, 0x78, 0x77,
		0x79, 0x9, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
		0x78, 0x78, 0x78, 0x69, 0x9, 0x77, 0x78, 0x77,
		0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x79, 0x9,
		0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78,
		0x78, 0x69, 0x9, 0x77, 0x78, 0x77, 0x78, 0x77,
		0x78, 0x77, 0x78, 0x77, 0x79, 0x9, 0x78, 0x78,
		0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x78, 0x69,
		0x9, 0x77, 0x78, 0x77, 0x77, 0x77, 0x77, 0x77,
		0x77, 0x77, 0x79, 0x9},
};
U_BOOT_DRVINFO(dmc) = {
	.name		= "rockchip_rk3328_dmc",
	.plat	= &dtv_dmc,
	.plat_size	= sizeof(dtv_dmc),
	.parent_idx	= -1,
};

/* Node /mmc@ff500000 index 2 */
static struct dtd_rockchip_rk3288_dw_mshc dtv_mmc_at_ff500000 = {
	.bus_width		= 0x4,
	.cap_sd_highspeed	= true,
	.clocks			= {
			{0, {317}},
			{0, {33}},
			{0, {74}},
			{0, {78}},},
	.disable_wp		= true,
	.fifo_depth		= 0x100,
	.interrupts		= {0x0, 0xc, 0x4},
	.max_frequency		= 0x8f0d180,
	.pinctrl_0		= {0x47, 0x48, 0x49, 0x4a},
	.pinctrl_names		= "default",
	.reg			= {0xff500000, 0x4000},
	.sd_uhs_sdr104		= true,
	.sd_uhs_sdr12		= true,
	.sd_uhs_sdr25		= true,
	.sd_uhs_sdr50		= true,
	.u_boot_spl_fifo_mode	= true,
	.vmmc_supply		= 0x4b,
	.vqmmc_supply		= 0x1e,
};
U_BOOT_DRVINFO(mmc_at_ff500000) = {
	.name		= "rockchip_rk3288_dw_mshc",
	.plat	= &dtv_mmc_at_ff500000,
	.plat_size	= sizeof(dtv_mmc_at_ff500000),
	.parent_idx	= -1,
};

/* Node /serial@ff130000 index 3 */
static struct dtd_ns16550_serial dtv_serial_at_ff130000 = {
	.clock_frequency	= 0x16e3600,
	.clocks			= {
			{0, {40}},
			{0, {212}},},
	.dma_names		= {"tx", "rx"},
	.dmas			= {0x10, 0x6, 0x10, 0x7},
	.interrupts		= {0x0, 0x39, 0x4},
	.pinctrl_0		= 0x26,
	.pinctrl_names		= "default",
	.reg			= {0xff130000, 0x100},
	.reg_io_width		= 0x4,
	.reg_shift		= 0x2,
};
U_BOOT_DRVINFO(serial_at_ff130000) = {
	.name		= "ns16550_serial",
	.plat	= &dtv_serial_at_ff130000,
	.plat_size	= sizeof(dtv_serial_at_ff130000),
	.parent_idx	= -1,
};

/* Node /syscon@ff100000 index 4 */
static struct dtd_rockchip_rk3328_grf dtv_syscon_at_ff100000 = {
	.reg			= {0xff100000, 0x1000},
};
U_BOOT_DRVINFO(syscon_at_ff100000) = {
	.name		= "rockchip_rk3328_grf",
	.plat	= &dtv_syscon_at_ff100000,
	.plat_size	= sizeof(dtv_syscon_at_ff100000),
	.parent_idx	= -1,
};

