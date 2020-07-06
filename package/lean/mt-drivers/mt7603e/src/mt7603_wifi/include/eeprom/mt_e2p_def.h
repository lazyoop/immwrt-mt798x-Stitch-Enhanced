/*
 ***************************************************************************
 * MediaTek Inc. 
 *
 * All rights reserved. source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of MediaTek. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of MediaTek, Inc. is obtained.
 ***************************************************************************

	Module Name:
	mt_e2p_def.h
*/

#ifndef __MT_E2P_DEF_H__
#define __MT_E2P_DEF_H__


#define NIC_CONFIGURE_0 0x34
#define EXTERNAL_PA_MASK (0x3 << 8)
#define GET_PA_TYPE(p) (((p) & EXTERNAL_PA_MASK) >> 8)

#define NIC_CONFIGURE_0_TOP 0x35

#define NIC_CONFIGURE_1 0x36
#define INTERNAL_TX_ALC_EN (1 << 13)

#define NIC_CONFIGURE_1_TOP 0x37


#define XTAL_TRIM1 0x3A
#define XTAL_TRIM1_DIP_SELECTION (1 << 7)
#define XTAL_TRIM1_MASK (0x7F)

#define WIFI_RF_SETTING 0x48

#define G_BAND_20_40_BW_PWR_DELTA 0x50
#define G_BAND_20_40_BW_PWR_DELTA_MASK (0x3f)
#define G_BAND_20_40_BW_PWR_DELTA_SIGN (1 << 6)
#define G_BAND_20_40_BW_PWR_DELTA_EN (1 << 7)
#define A_BAND_20_40_BW_PWR_DELTA_MASK (0x3f << 8)
#define A_BAND_20_40_BW_PWR_DELTA_SIGN (1 << 14)
#define A_BAND_20_40_BW_PWR_DELTA_EN (1 << 15)

#define A_BAND_20_80_BW_PWR_DELTA 0x52
#define A_BAND_20_80_BW_PWR_DELTA_MASK (0x3f)
#define A_BAND_20_80_BW_PWR_DELTA_SIGN (1 << 6)
#define A_BAND_20_80_BW_PWR_DELTA_EN (1 << 7)
#define G_BAND_EXT_PA_SETTING_MASK (0x7f << 8)
#define G_BAND_EXT_PA_SETTING_EN (1 << 15)

#define A_BAND_20_80_BW_PWR_DELTA_ANALOG 0x53

#define THADC_ANALOG_PART 0x53
#define THADC_SLOP 0x54
#define THERMO_SLOPE_VARIATION_MASK (0x1f)
#define A_BAND_EXT_PA_SETTING 0x54
#define A_BAND_EXT_PA_SETTING_MASK (0x7f)
#define A_BAND_EXT_PA_SETTING_EN (1 << 7)
#define TEMP_SENSOR_CAL_MASK (0x7f << 8)
#define TEMP_SENSOR_CAL_EN (1 << 15)

#define TEMP_SENSOR_CAL 0x55
#define TEMPERATURE_SENSOR_CALIBRATION 0x55
#define THERMO_REF_ADC_VARIATION_MASK (0x7f)

#define TX0_G_BAND_TSSI_SLOPE 0x56
#define TX0_G_BAND_TSSI_SLOPE_MASK (0xff)
#define TX0_G_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define TX0_G_BAND_TSSI_SLOPE_TOP 0x57

#define TX0_G_BAND_TARGET_PWR 0x58
#define TX0_G_BAND_TARGET_PWR_MASK (0xff)
#define TX0_G_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define TX0_G_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define TX0_G_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define TX0_G_BAND_OFFSET_LOW 0x59

#define TX0_G_BAND_CHL_PWR_DELTA_MID 0x5A
#define TX0_G_BAND_CHL_PWR_DELTA_MID_MASK (0x3f)
#define TX0_G_BAND_CHL_PWR_DELTA_MID_SIGN (1 << 6)
#define TX0_G_BAND_CHL_PWR_DELTA_MID_EN (1 << 7)
#define TX0_G_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define TX0_G_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define TX0_G_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define TX0_G_BAND_OFFSET_HIGH 0x5B

#define TX1_G_BAND_TSSI_SLOPE 0x5C
#define TX1_G_BAND_TSSI_SLOPE_MASK (0xff)
#define TX1_G_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define TX1_G_BAND_TSSI_SLOPE_TOP 0x5D

#define TX1_G_BAND_TARGET_PWR 0x5E
#define TX1_G_BAND_TARGET_PWR_MASK (0xff)
#define TX1_G_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define TX1_G_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define TX1_G_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define TX1_G_BAND_CHL_PWR_DELATE_LOW 0x5F

#define TX1_G_BAND_CHL_PWR_DELTA_MID 0x60
#define TX1_G_BAND_CHL_PWR_DELTA_MID_MASK (0x3f)
#define TX1_G_BAND_CHL_PWR_DELTA_MID_SIGN (1 << 6)
#define TX1_G_BAND_CHL_PWR_DELTA_MID_EN (1 << 7)
#define TX1_G_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define TX1_G_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define TX1_G_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define TX1_G_BAND_CHL_PWR_DELTA_HIGH 0x61


#define GRP0_TX0_A_BAND_TSSI_SLOPE 0x62
#define GRP0_TX0_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP0_TX0_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP0_TX0_A_BAND_TARGET_PWR 0x64
#define GRP0_TX0_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_HI 0x66
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP0_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP1_TX0_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP1_TX0_A_BAND_TSSI_OFFSET 0x68
#define GRP1_TX0_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP1_TX0_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_LOW 0x6A
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP1_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define GRP2_TX0_A_BAND_TSSI_SLOPE 0x6C
#define GRP2_TX0_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP2_TX0_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP2_TX0_A_BAND_TARGET_PWR 0x6E
#define GRP2_TX0_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_HI 0x70
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP2_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP3_TX0_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP3_TX0_A_BAND_TSSI_OFFSET 0x72
#define GRP3_TX0_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP3_TX0_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_LOW 0x74
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP3_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define GRP4_TX0_A_BAND_TSSI_SLOPE 0x76
#define GRP4_TX0_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP4_TX0_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP4_TX0_A_BAND_TARGET_PWR 0x78
#define GRP4_TX0_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_HI 0x7A
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP4_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP5_TX0_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP5_TX0_A_BAND_TSSI_OFFSET 0x7C
#define GRP5_TX0_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP5_TX0_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_LOW 0X7E
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP5_TX0_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define GRP0_TX1_A_BAND_TSSI_SLOPE 0x80
#define GRP0_TX1_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP0_TX1_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP0_TX1_A_BAND_TARGET_PWR 0x82
#define GRP0_TX1_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_HI 0x84
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP0_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP1_TX1_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP1_TX1_A_BAND_TSSI_OFFSET 0x86
#define GRP1_TX1_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP1_TX1_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_LOW 0x88
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP1_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define GRP2_TX1_A_BAND_TSSI_SLOPE 0x8A
#define GRP2_TX1_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP2_TX1_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP2_TX1_A_BAND_TARGET_PWR 0x8C
#define GRP2_TX1_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_HI 0x8E
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP2_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP3_TX1_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP3_TX1_A_BAND_TSSI_OFFSET 0x90
#define GRP3_TX1_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP3_TX1_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_LOW 0x92
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP3_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define GRP4_TX1_A_BAND_TSSI_SLOPE 0x94
#define GRP4_TX1_A_BAND_TSSI_SLOPE_MASK (0xff)
#define GRP4_TX1_A_BAND_TSSI_OFFSET_MASK (0xff << 8)

#define GRP4_TX1_A_BAND_TARGET_PWR 0x96
#define GRP4_TX1_A_BAND_TARGET_PWR_MASK (0xff)
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f << 8)
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 14)
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 15)

#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_HI 0x98
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f)
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 6)
#define GRP4_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 7)
#define GRP5_TX1_A_BAND_TSSI_SLOPE_MASK (0xff << 8)

#define GRP5_TX1_A_BAND_TSSI_OFFSET 0x9A
#define GRP5_TX1_A_BAND_TSSI_OFFSET_MASK (0xff)
#define GRP5_TX1_A_BAND_TARGET_PWR_MASK (0xff << 8)

#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_LOW 0x9C
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_LOW_MASK (0x3f)
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_LOW_SIGN (1 << 6)
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_LOW_EN (1 << 7)
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_HI_MASK (0x3f << 8)
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_HI_SIGN (1 << 14)
#define GRP5_TX1_A_BAND_CHL_PWR_DELTA_HI_EN (1 << 15)

#define G_BAND_BANDEDGE_PWR_BACK_OFF 0x9E
#define G_BAND_BANDEDGE_PWR_BACK_OFF_MASK (0x7f)
#define G_BAND_BANDEDGE_PWR_BACK_OFF_EN (1 << 7)
#define XTAL_TRIM2_MASK (0x7f << 8) 
#define XTAL_TRIM2_DIP_SELECTION (1 << 15)

#define TX_PWR_CCK_1_2M 0xA0
#define TX_PWR_CCK_1_2M_MASK (0x3f)
#define TX_PWR_CCK_1_2M_SIGN (1 << 6)
#define TX_PWR_CCK_1_2M_EN (1 << 7)
#define TX_PWR_CCK_5_11M_MASK (0x3f << 8)
#define TX_PWR_CCK_5_11M_SIGN (1 << 14)
#define TX_PWR_CCK_5_11M_EN (1 << 15)

#define TX_PWR_CCK_5_11M 0xA1

#define TX_PWR_G_BAND_OFDM_6_9M 0xA2
#define TX_PWR_G_BAND_OFDM_6_9M_MASK (0x3f)
#define TX_PWR_G_BAND_OFDM_6_9M_SIGN (1 << 6)
#define TX_PWR_G_BAND_OFDM_6_9M_EN (1 << 7)
#define TX_PWR_G_BAND_OFDM_12_18M_MASK (0x3f << 8)
#define TX_PWR_G_BAND_OFDM_12_18M_SIGN (1 << 14)
#define TX_PWR_G_BAND_OFDM_12_18M_EN (1 << 15)

#define TX_PWR_OFDM_12_18M 0xA3

#define TX_PWR_G_BAND_OFDM_24_36M 0xA4
#define TX_PWR_G_BAND_OFDM_24_36M_MASK (0x3f)
#define TX_PWR_G_BAND_OFDM_24_36M_SIGN (1 << 6)
#define TX_PWR_G_BAND_OFDM_24_36M_EN (1 << 7)
#define TX_PWR_G_BAND_OFDM_48M_MASK (0x3f << 8)
#define TX_PWR_G_BAND_OFDM_48M_SIGN (1 << 14)
#define TX_PWR_G_BAND_OFDM_48M_EN (1 << 15)


#define TX_PWR_G_BNAD_OFDM_48 0xA5

#ifdef MT_MAC
#define TX_PWR_G_BAND_OFDM_54M 0xA6
#define TX_PWR_G_BAND_OFDM_54M_MASK (0x3f)
#define TX_PWR_G_BAND_OFDM_54M_SIGN (1 << 6)
#define TX_PWR_G_BAND_OFDM_54M_EN (1 << 7)
#define TX_PWR_HT_BPSK_MCS_0_8_MASK (0x3f << 8)
#define TX_PWR_HT_BPSK_MCS_0_8_SIGN (1 << 14)
#define TX_PWR_HT_BPSK_MCS_0_8_EN (1 << 15)

#define TX_PWR_HT_BPSK_MCS_0_8 0xA7

#define TX_PWR_HT_BPSK_MCS_32 0xA8
#define TX_PWR_HT_BPSK_MCS_32_MASK (0x3f)
#define TX_PWR_HT_BPSK_MCS_32_SIGN (1 << 6)
#define TX_PWR_HT_BPSK_MCS_32_EN (1 << 7)
#define TX_PWR_HT_QPSK_MCS_1_2_9_10_MASK (0x3f << 8)
#define TX_PWR_HT_QPSK_MCS_1_2_9_10_SIGN (1 << 14)
#define TX_PWR_HT_QPSK_MCS_1_2_9_10_EN (1 << 15)

#define TX_PWR_HT_QPSK_MCS_1_2_9_10 0xA9


#define TX_PWR_HT_16QAM_MCS_3_4_11_12 0xAA
#define TX_PWR_HT_16QAM_MCS_3_4_11_12_MASK (0x3f)
#define TX_PWR_HT_16QAM_MCS_3_4_11_12_SIGN (1 << 6)
#define TX_PWR_HT_16QAM_MCS_3_4_11_12_EN (1 << 7)
#define TX_PWR_HT_64QAM_MCS_5_13_MASK (0x3f << 8)
#define TX_PWR_HT_64QAM_MCS_5_13_SIGN (1 << 14)
#define TX_PWR_HT_64QAM_MCS_5_13_EN (1 << 15)

#define TX_PWR_HT_64QAM_MCS_5_13 0xAB


#define TX_PWR_HT_64QAM_MCS_6_14 0xAC
#define TX_PWR_HT_64QAM_MCS_6_14_MASK (0x3f)
#define TX_PWR_HT_64QAM_MCS_6_14_SIGN (1 << 6)
#define TX_PWR_HT_64QAM_MCS_6_14_EN (1 << 7)
#define TX_PWR_HT_64QAM_MCS_7_15_MASK (0x3f << 8)
#define TX_PWR_HT_64QAM_MCS_7_15_SIGN (1 << 14)
#define TX_PWR_HT_64QAM_MCS_7_15_EN (1 << 15)

#define TX_PWR_HT_64QAM_MCS_7_15 0xAD

#endif



#define CONFIG_G_BAND_CHL 0xB0
#define CONFIG_G_BAND_CHL_GRP1_MASK (0xff)
#define CONFIG_G_BAND_CHL_GRP2_MASK (0xff << 8)

#define TX_PWR_A_BAND_OFDM_6_9M 0xB2
#define TX_PWR_A_BAND_OFDM_6_9M_MASK (0x3f)
#define TX_PWR_A_BAND_OFDM_6_9M_SIGN (1 << 6)
#define TX_PWR_A_BAND_OFDM_6_9M_EN (1 << 7)
#define TX_PWR_A_BAND_OFDM_12_18M_MASK (0x3f << 8)
#define TX_PWR_A_BAND_OFDM_12_18M_SIGN (1 << 14)
#define TX_PWR_A_BAND_OFDM_12_18M_EN (1 << 15)

#define TX_PWR_A_BAND_OFDM_24_36M 0xB4
#define TX_PWR_A_BAND_OFDM_24_36M_MASK (0x3f)
#define TX_PWR_A_BAND_OFDM_24_36M_SIGN (1 << 6)
#define TX_PWR_A_BAND_OFDM_24_36M_EN (1 << 7)
#define TX_PWR_A_BAND_OFDM_48_54M_MASK (0x3f << 8)
#define TX_PWR_A_BAND_OFDM_48_54M_SIGN (1 << 14)
#define TX_PWR_A_BAND_OFDM_48_54M_EN (1 << 15)

#define CONFIG1_A_BAND_CHL 0xB6
#define CONFIG1_A_BAND_CHL_GRP1_MASK 0xff
#define CONFIG1_A_BAND_CHL_GRP2_MASK (0xff << 8)

#define CONFIG2_A_BAND_CHL 0xB8
#define CONFIG2_A_BAND_CHL_GRP1_MASK (0xff)
#define CONFIG2_A_BAND_CHL_GRP2_MASK (0xff << 8)

#define ELAN_RX_MODE_GAIN 	0xC0
#define ELAN_RX_MODE_NF		0xC1
#define ELAN_RX_MODE_P1DB	0xC2
#define ELAN_BYPASS_MODE_GAIN	0xC3
#define ELAN_BYPASS_MODE_NF		0xC4
#define ELAN_BYPASS_MODE_P1DB	0xC5

#define STEP_NUM_NEG_7	0xC6
#define STEP_NUM_NEG_6	0xC7
#define STEP_NUM_NEG_5	0xC8
#define STEP_NUM_NEG_4	0xC9
#define STEP_NUM_NEG_3	0xCA
#define STEP_NUM_NEG_2	0xCB
#define STEP_NUM_NEG_1	0xCC
#define STEP_NUM_NEG_0	0xCD

#define REF_STEP_24G 0xCE
#define REF_TEMP_24G 0xCF

#define STEP_NUM_PLUS_1 0xD0
#define STEP_NUM_PLUS_2 0xD1
#define STEP_NUM_PLUS_3 0xD2
#define STEP_NUM_PLUS_4 0xD3
#define STEP_NUM_PLUS_5 0xD4
#define STEP_NUM_PLUS_6 0xD5
#define STEP_NUM_PLUS_7 0xD6

#define CP_FT_VERSION 0xF0
#define XTAL_CALIB_FREQ_OFFSET 0xF4
#define XTAL_TRIM_2_COMP	0xF5
#define XTAL_TRIM_3_COMP 0xF6
#define WF_RCAL 0xF7
#define PCIE_RECAL 0xF8

#define THERMAL_COMPENSATION_OFFSET 0xF8



#endif
