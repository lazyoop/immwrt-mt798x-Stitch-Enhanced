/*
 * ar8327.h: AR8216 switch driver
 *
 * Copyright (C) 2009 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __AR8327_H
#define __AR8327_H

#define AR8327_NUM_PORTS	7
#define AR8327_NUM_LEDS		15
#define AR8327_PORTS_ALL	0x7f
#define AR8327_NUM_LED_CTRL_REGS	4

#define AR8327_REG_MASK				0x000

#define AR8327_REG_PAD0_MODE			0x004
#define AR8327_REG_PAD5_MODE			0x008
#define AR8327_REG_PAD6_MODE			0x00c
#define   AR8327_PAD_MAC_MII_RXCLK_SEL		BIT(0)
#define   AR8327_PAD_MAC_MII_TXCLK_SEL		BIT(1)
#define   AR8327_PAD_MAC_MII_EN			BIT(2)
#define   AR8327_PAD_MAC_GMII_RXCLK_SEL		BIT(4)
#define   AR8327_PAD_MAC_GMII_TXCLK_SEL		BIT(5)
#define   AR8327_PAD_MAC_GMII_EN		BIT(6)
#define   AR8327_PAD_SGMII_EN			BIT(7)
#define   AR8327_PAD_PHY_MII_RXCLK_SEL		BIT(8)
#define   AR8327_PAD_PHY_MII_TXCLK_SEL		BIT(9)
#define   AR8327_PAD_PHY_MII_EN			BIT(10)
#define   AR8327_PAD_PHY_GMII_PIPE_RXCLK_SEL	BIT(11)
#define   AR8327_PAD_PHY_GMII_RXCLK_SEL		BIT(12)
#define   AR8327_PAD_PHY_GMII_TXCLK_SEL		BIT(13)
#define   AR8327_PAD_PHY_GMII_EN		BIT(14)
#define   AR8327_PAD_PHYX_GMII_EN		BIT(16)
#define   AR8327_PAD_PHYX_RGMII_EN		BIT(17)
#define   AR8327_PAD_PHYX_MII_EN		BIT(18)
#define   AR8327_PAD_SGMII_DELAY_EN		BIT(19)
#define   AR8327_PAD_RGMII_RXCLK_DELAY_SEL	BITS(20, 2)
#define   AR8327_PAD_RGMII_RXCLK_DELAY_SEL_S	20
#define   AR8327_PAD_RGMII_TXCLK_DELAY_SEL	BITS(22, 2)
#define   AR8327_PAD_RGMII_TXCLK_DELAY_SEL_S	22
#define   AR8327_PAD_RGMII_RXCLK_DELAY_EN	BIT(24)
#define   AR8327_PAD_RGMII_TXCLK_DELAY_EN	BIT(25)
#define   AR8327_PAD_RGMII_EN			BIT(26)

#define AR8327_REG_POWER_ON_STRIP		0x010
#define   AR8327_POWER_ON_STRIP_POWER_ON_SEL	BIT(31)
#define   AR8327_POWER_ON_STRIP_LED_OPEN_EN	BIT(24)
#define   AR8327_POWER_ON_STRIP_SERDES_AEN	BIT(7)

#define AR8327_REG_INT_STATUS0			0x020
#define   AR8327_INT0_VT_DONE			BIT(20)

#define AR8327_REG_INT_STATUS1			0x024
#define AR8327_REG_INT_MASK0			0x028
#define AR8327_REG_INT_MASK1			0x02c

#define AR8327_REG_MODULE_EN			0x030
#define   AR8327_MODULE_EN_MIB			BIT(0)

#define AR8327_REG_MIB_FUNC			0x034
#define   AR8327_MIB_CPU_KEEP			BIT(20)

#define AR8327_REG_SERVICE_TAG			0x048
#define AR8327_REG_LED_CTRL(_i)			(0x050 + (_i) * 4)
#define AR8327_REG_LED_CTRL0			0x050
#define AR8327_REG_LED_CTRL1			0x054
#define AR8327_REG_LED_CTRL2			0x058
#define AR8327_REG_LED_CTRL3			0x05c
#define AR8327_REG_MAC_ADDR0			0x060
#define AR8327_REG_MAC_ADDR1			0x064

#define AR8327_REG_MAX_FRAME_SIZE		0x078
#define   AR8327_MAX_FRAME_SIZE_MTU		BITS(0, 14)

#define AR8327_REG_PORT_STATUS(_i)		(0x07c + (_i) * 4)

#define AR8327_REG_HEADER_CTRL			0x098
#define AR8327_REG_PORT_HEADER(_i)		(0x09c + (_i) * 4)

#define AR8327_REG_SGMII_CTRL			0x0e0
#define   AR8327_SGMII_CTRL_EN_PLL		BIT(1)
#define   AR8327_SGMII_CTRL_EN_RX		BIT(2)
#define   AR8327_SGMII_CTRL_EN_TX		BIT(3)

#define AR8327_REG_EEE_CTRL			0x100
#define   AR8327_EEE_CTRL_DISABLE_PHY(_i)	BIT(4 + (_i) * 2)

#define AR8327_REG_PORT_VLAN0(_i)		(0x420 + (_i) * 0x8)
#define   AR8327_PORT_VLAN0_DEF_SVID		BITS(0, 12)
#define   AR8327_PORT_VLAN0_DEF_SVID_S		0
#define   AR8327_PORT_VLAN0_DEF_CVID		BITS(16, 12)
#define   AR8327_PORT_VLAN0_DEF_CVID_S		16

#define AR8327_REG_PORT_VLAN1(_i)		(0x424 + (_i) * 0x8)
#define   AR8327_PORT_VLAN1_PORT_VLAN_PROP	BIT(6)
#define   AR8327_PORT_VLAN1_OUT_MODE		BITS(12, 2)
#define   AR8327_PORT_VLAN1_OUT_MODE_S		12
#define   AR8327_PORT_VLAN1_OUT_MODE_UNMOD	0
#define   AR8327_PORT_VLAN1_OUT_MODE_UNTAG	1
#define   AR8327_PORT_VLAN1_OUT_MODE_TAG	2
#define   AR8327_PORT_VLAN1_OUT_MODE_UNTOUCH	3

#define AR8327_REG_ATU_DATA0			0x600
#define AR8327_REG_ATU_DATA1			0x604
#define AR8327_REG_ATU_DATA2			0x608

#define AR8327_REG_ATU_FUNC			0x60c
#define   AR8327_ATU_FUNC_OP			BITS(0, 4)
#define   AR8327_ATU_FUNC_OP_NOOP		0x0
#define   AR8327_ATU_FUNC_OP_FLUSH		0x1
#define   AR8327_ATU_FUNC_OP_LOAD		0x2
#define   AR8327_ATU_FUNC_OP_PURGE		0x3
#define   AR8327_ATU_FUNC_OP_FLUSH_LOCKED	0x4
#define   AR8327_ATU_FUNC_OP_FLUSH_UNICAST	0x5
#define   AR8327_ATU_FUNC_OP_GET_NEXT		0x6
#define   AR8327_ATU_FUNC_OP_SEARCH_MAC		0x7
#define   AR8327_ATU_FUNC_OP_CHANGE_TRUNK	0x8
#define   AR8327_ATU_FUNC_BUSY			BIT(31)

#define AR8327_REG_VTU_FUNC0			0x0610
#define   AR8327_VTU_FUNC0_EG_MODE		BITS(4, 14)
#define   AR8327_VTU_FUNC0_EG_MODE_S(_i)	(4 + (_i) * 2)
#define   AR8327_VTU_FUNC0_EG_MODE_KEEP		0
#define   AR8327_VTU_FUNC0_EG_MODE_UNTAG	1
#define   AR8327_VTU_FUNC0_EG_MODE_TAG		2
#define   AR8327_VTU_FUNC0_EG_MODE_NOT		3
#define   AR8327_VTU_FUNC0_IVL			BIT(19)
#define   AR8327_VTU_FUNC0_VALID		BIT(20)

#define AR8327_REG_VTU_FUNC1			0x0614
#define   AR8327_VTU_FUNC1_OP			BITS(0, 3)
#define   AR8327_VTU_FUNC1_OP_NOOP		0
#define   AR8327_VTU_FUNC1_OP_FLUSH		1
#define   AR8327_VTU_FUNC1_OP_LOAD		2
#define   AR8327_VTU_FUNC1_OP_PURGE		3
#define   AR8327_VTU_FUNC1_OP_REMOVE_PORT	4
#define   AR8327_VTU_FUNC1_OP_GET_NEXT		5
#define   AR8327_VTU_FUNC1_OP_GET_ONE		6
#define   AR8327_VTU_FUNC1_FULL			BIT(4)
#define   AR8327_VTU_FUNC1_PORT			BIT(8, 4)
#define   AR8327_VTU_FUNC1_PORT_S		8
#define   AR8327_VTU_FUNC1_VID			BIT(16, 12)
#define   AR8327_VTU_FUNC1_VID_S		16
#define   AR8327_VTU_FUNC1_BUSY			BIT(31)

#define AR8327_REG_FWD_CTRL0			0x620
#define   AR8327_FWD_CTRL0_CPU_PORT_EN		BIT(10)
#define   AR8327_FWD_CTRL0_MIRROR_PORT		BITS(4, 4)
#define   AR8327_FWD_CTRL0_MIRROR_PORT_S	4

#define AR8327_REG_FWD_CTRL1			0x624
#define   AR8327_FWD_CTRL1_UC_FLOOD		BITS(0, 7)
#define   AR8327_FWD_CTRL1_UC_FLOOD_S		0
#define   AR8327_FWD_CTRL1_MC_FLOOD		BITS(8, 7)
#define   AR8327_FWD_CTRL1_MC_FLOOD_S		8
#define   AR8327_FWD_CTRL1_BC_FLOOD		BITS(16, 7)
#define   AR8327_FWD_CTRL1_BC_FLOOD_S		16
#define   AR8327_FWD_CTRL1_IGMP			BITS(24, 7)
#define   AR8327_FWD_CTRL1_IGMP_S		24

#define AR8327_REG_PORT_LOOKUP(_i)		(0x660 + (_i) * 0xc)
#define   AR8327_PORT_LOOKUP_MEMBER		BITS(0, 7)
#define   AR8327_PORT_LOOKUP_IN_MODE		BITS(8, 2)
#define   AR8327_PORT_LOOKUP_IN_MODE_S		8
#define   AR8327_PORT_LOOKUP_STATE		BITS(16, 3)
#define   AR8327_PORT_LOOKUP_STATE_S		16
#define   AR8327_PORT_LOOKUP_LEARN		BIT(20)
#define   AR8327_PORT_LOOKUP_ING_MIRROR_EN	BIT(25)

#define AR8327_REG_PORT_PRIO(_i)		(0x664 + (_i) * 0xc)

#define AR8327_REG_PORT_HOL_CTRL1(_i)		(0x974 + (_i) * 0x8)
#define   AR8327_PORT_HOL_CTRL1_EG_MIRROR_EN	BIT(16)

#define AR8337_PAD_MAC06_EXCHANGE_EN		BIT(31)

enum ar8327_led_pattern {
	AR8327_LED_PATTERN_OFF = 0,
	AR8327_LED_PATTERN_BLINK,
	AR8327_LED_PATTERN_ON,
	AR8327_LED_PATTERN_RULE,
};

struct ar8327_led_entry {
	unsigned reg;
	unsigned shift;
};

struct ar8327_led {
	struct led_classdev cdev;
	struct ar8xxx_priv *sw_priv;

	char *name;
	bool active_low;
	u8 led_num;
	enum ar8327_led_mode mode;

	struct mutex mutex;
	spinlock_t lock;
	struct work_struct led_work;
	bool enable_hw_mode;
	enum ar8327_led_pattern pattern;
};

struct ar8327_data {
	u32 port0_status;
	u32 port6_status;

	struct ar8327_led **leds;
	unsigned int num_leds;
};

#endif
