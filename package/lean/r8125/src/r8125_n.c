/*
################################################################################
#
# r8125 is the Linux device driver released for Realtek 2.5Gigabit Ethernet
# controllers with PCI-Express interface.
#
# Copyright(c) 2019 Realtek Semiconductor Corp. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#
# Author:
# Realtek NIC software team <nicfae@realtek.com>
# No. 2, Innovation Road II, Hsinchu Science Park, Hsinchu 300, Taiwan
#
################################################################################
*/

/************************************************************************************
 *  This product is covered by one or more of the following patents:
 *  US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

/*
 * This driver is modified from r8169.c in Linux kernel 2.6.18
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <linux/crc32.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/ip.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>
#endif
#include <linux/tcp.h>
#include <linux/init.h>
#include <linux/rtnetlink.h>
#include <linux/completion.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <linux/pci-aspm.h>
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
#include <linux/prefetch.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define dev_printk(A,B,fmt,args...) printk(A fmt,##args)
#else
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
#include <linux/mdio.h>
#endif

#include <asm/io.h>
#include <asm/irq.h>

#include "r8125.h"
#include "rtl_eeprom.h"
#include "rtltool.h"

#ifdef ENABLE_R8125_PROCFS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

/* Maximum number of multicast addresses to filter (vs. Rx-all-multicast).
   The RTL chips use a 64 element hash table based on the Ethernet CRC. */
static const int multicast_filter_limit = 32;

#define _R(NAME,MAC,RCR,MASK, JumFrameSz) \
    { .name = NAME, .mcfg = MAC, .RCR_Cfg = RCR, .RxConfigMask = MASK, .jumbo_frame_sz = JumFrameSz }

static const struct {
        const char *name;
        u8 mcfg;
        u32 RCR_Cfg;
        u32 RxConfigMask;   /* Clears the bits supported by this chip */
        u32 jumbo_frame_sz;
} rtl_chip_info[] = {
        _R("RTL8125",
        CFG_METHOD_2,
        BIT_30 | BIT_22 | BIT_23 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("RTL8125",
        CFG_METHOD_3,
        BIT_30 | BIT_22 | BIT_23 | (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_9k),

        _R("Unknown",
        CFG_METHOD_DEFAULT,
        (RX_DMA_BURST << RxCfgDMAShift),
        0xff7e5880,
        Jumbo_Frame_1k)
};
#undef _R

#ifndef PCI_VENDOR_ID_DLINK
#define PCI_VENDOR_ID_DLINK 0x1186
#endif

static struct pci_device_id rtl8125_pci_tbl[] = {
        { PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x8125), },
        { PCI_DEVICE(PCI_VENDOR_ID_REALTEK, 0x3000), },
        {0,},
};

MODULE_DEVICE_TABLE(pci, rtl8125_pci_tbl);

static int rx_copybreak = 0;
static int use_dac = 1;
static int timer_count = 0x2600;

static struct {
        u32 msg_enable;
} debug = { -1 };

static unsigned int speed_mode = SPEED_2500;
static unsigned int duplex_mode = DUPLEX_FULL;
static unsigned int autoneg_mode = AUTONEG_ENABLE;
static unsigned int advertising_mode =  ADVERTISED_10baseT_Half |
                                        ADVERTISED_10baseT_Full |
                                        ADVERTISED_100baseT_Half |
                                        ADVERTISED_100baseT_Full |
                                        ADVERTISED_1000baseT_Half |
                                        ADVERTISED_1000baseT_Full |
                                        ADVERTISED_2500baseX_Full;
#ifdef CONFIG_ASPM
static int aspm = 1;
#else
static int aspm = 0;
#endif
#ifdef ENABLE_S5WOL
static int s5wol = 1;
#else
static int s5wol = 0;
#endif
#ifdef ENABLE_S5_KEEP_CURR_MAC
static int s5_keep_curr_mac = 1;
#else
static int s5_keep_curr_mac = 0;
#endif
#ifdef ENABLE_EEE
static int eee_enable = 1;
#else
static int eee_enable = 0;
#endif
#ifdef CONFIG_SOC_LAN
static ulong hwoptimize = HW_PATCH_SOC_LAN;
#else
static ulong hwoptimize = 0;
#endif
#ifdef ENABLE_S0_MAGIC_PACKET
static int s0_magic_packet = 1;
#else
static int s0_magic_packet = 0;
#endif

MODULE_AUTHOR("Realtek and the Linux r8125 crew <netdev@vger.kernel.org>");
MODULE_DESCRIPTION("RealTek RTL8125 2.5Gigabit Ethernet driver");

module_param(speed_mode, uint, 0);
MODULE_PARM_DESC(speed_mode, "force phy operation. Deprecated by ethtool (8).");

module_param(duplex_mode, uint, 0);
MODULE_PARM_DESC(duplex_mode, "force phy operation. Deprecated by ethtool (8).");

module_param(autoneg_mode, uint, 0);
MODULE_PARM_DESC(autoneg_mode, "force phy operation. Deprecated by ethtool (8).");

module_param(advertising_mode, uint, 0);
MODULE_PARM_DESC(advertising_mode, "force phy operation. Deprecated by ethtool (8).");

module_param(aspm, int, 0);
MODULE_PARM_DESC(aspm, "Enable ASPM.");

module_param(s5wol, int, 0);
MODULE_PARM_DESC(s5wol, "Enable Shutdown Wake On Lan.");

module_param(s5_keep_curr_mac, int, 0);
MODULE_PARM_DESC(s5_keep_curr_mac, "Enable Shutdown Keep Current MAC Address.");

module_param(rx_copybreak, int, 0);
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");

module_param(use_dac, int, 0);
MODULE_PARM_DESC(use_dac, "Enable PCI DAC. Unsafe on 32 bit PCI slot.");

module_param(timer_count, int, 0);
MODULE_PARM_DESC(timer_count, "Timer Interrupt Interval.");

module_param(eee_enable, int, 0);
MODULE_PARM_DESC(eee_enable, "Enable Energy Efficient Ethernet.");

module_param(hwoptimize, ulong, 0);
MODULE_PARM_DESC(hwoptimize, "Enable HW optimization function.");

module_param(s0_magic_packet, int, 0);
MODULE_PARM_DESC(s0_magic_packet, "Enable S0 Magic Packet.");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
module_param_named(debug, debug.msg_enable, int, 0);
MODULE_PARM_DESC(debug, "Debug verbosity level (0=none, ..., 16=all)");
#endif//LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

MODULE_LICENSE("GPL");

MODULE_VERSION(RTL8125_VERSION);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void rtl8125_esd_timer(unsigned long __opaque);
#else
static void rtl8125_esd_timer(struct timer_list *t);
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
static void rtl8125_link_timer(unsigned long __opaque);
#else
static void rtl8125_link_timer(struct timer_list *t);
#endif
static void rtl8125_tx_clear(struct rtl8125_private *tp);
static void rtl8125_rx_clear(struct rtl8125_private *tp);

static int rtl8125_open(struct net_device *dev);
static int rtl8125_start_xmit(struct sk_buff *skb, struct net_device *dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8125_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
#else
static irqreturn_t rtl8125_interrupt(int irq, void *dev_instance);
#endif
static int rtl8125_init_ring(struct net_device *dev);
static void rtl8125_hw_config(struct net_device *dev);
static void rtl8125_hw_start(struct net_device *dev);
static int rtl8125_close(struct net_device *dev);
static void rtl8125_set_rx_mode(struct net_device *dev);
static void rtl8125_tx_timeout(struct net_device *dev);
static struct net_device_stats *rtl8125_get_stats(struct net_device *dev);
static int rtl8125_rx_interrupt(struct net_device *, struct rtl8125_private *, void __iomem *, napi_budget);
static int rtl8125_change_mtu(struct net_device *dev, int new_mtu);
static void rtl8125_down(struct net_device *dev);

static int rtl8125_set_mac_address(struct net_device *dev, void *p);
static void rtl8125_rar_set(struct rtl8125_private *tp, uint8_t *addr);
static void rtl8125_desc_addr_fill(struct rtl8125_private *);
static void rtl8125_tx_desc_init(struct rtl8125_private *tp);
static void rtl8125_rx_desc_init(struct rtl8125_private *tp);

static void rtl8125_hw_reset(struct net_device *dev);

static void rtl8125_phy_power_up(struct net_device *dev);
static void rtl8125_phy_power_down(struct net_device *dev);
static int rtl8125_set_speed(struct net_device *dev, u8 autoneg, u32 speed, u8 duplex, u32 adv);
static bool rtl8125_set_phy_mcu_patch_request(struct rtl8125_private *tp);
static bool rtl8125_clear_phy_mcu_patch_request(struct rtl8125_private *tp);

#ifdef CONFIG_R8125_NAPI
static int rtl8125_poll(napi_ptr napi, napi_budget budget);
#endif

#if ((LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0) && \
     LINUX_VERSION_CODE >= KERNEL_VERSION(4,6,00)))
void ethtool_convert_legacy_u32_to_link_mode(unsigned long *dst,
                u32 legacy_u32)
{
        bitmap_zero(dst, __ETHTOOL_LINK_MODE_MASK_NBITS);
        dst[0] = legacy_u32;
}

bool ethtool_convert_link_mode_to_legacy_u32(u32 *legacy_u32,
                const unsigned long *src)
{
        bool retval = true;

        /* TODO: following test will soon always be true */
        if (__ETHTOOL_LINK_MODE_MASK_NBITS > 32) {
                __ETHTOOL_DECLARE_LINK_MODE_MASK(ext);

                bitmap_zero(ext, __ETHTOOL_LINK_MODE_MASK_NBITS);
                bitmap_fill(ext, 32);
                bitmap_complement(ext, ext, __ETHTOOL_LINK_MODE_MASK_NBITS);
                if (bitmap_intersects(ext, src,
                                      __ETHTOOL_LINK_MODE_MASK_NBITS)) {
                        /* src mask goes beyond bit 31 */
                        retval = false;
                }
        }
        *legacy_u32 = src[0];
        return retval;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)

#ifndef LPA_1000FULL
#define LPA_1000FULL            0x0800
#endif

#ifndef LPA_1000HALF
#define LPA_1000HALF            0x0400
#endif

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0)
static inline void eth_hw_addr_random(struct net_device *dev)
{
        random_ether_addr(dev->dev_addr);
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#undef ethtool_ops
#define ethtool_ops _kc_ethtool_ops

struct _kc_ethtool_ops {
        int  (*get_settings)(struct net_device *, struct ethtool_cmd *);
        int  (*set_settings)(struct net_device *, struct ethtool_cmd *);
        void (*get_drvinfo)(struct net_device *, struct ethtool_drvinfo *);
        int  (*get_regs_len)(struct net_device *);
        void (*get_regs)(struct net_device *, struct ethtool_regs *, void *);
        void (*get_wol)(struct net_device *, struct ethtool_wolinfo *);
        int  (*set_wol)(struct net_device *, struct ethtool_wolinfo *);
        u32  (*get_msglevel)(struct net_device *);
        void (*set_msglevel)(struct net_device *, u32);
        int  (*nway_reset)(struct net_device *);
        u32  (*get_link)(struct net_device *);
        int  (*get_eeprom_len)(struct net_device *);
        int  (*get_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
        int  (*set_eeprom)(struct net_device *, struct ethtool_eeprom *, u8 *);
        int  (*get_coalesce)(struct net_device *, struct ethtool_coalesce *);
        int  (*set_coalesce)(struct net_device *, struct ethtool_coalesce *);
        void (*get_ringparam)(struct net_device *, struct ethtool_ringparam *);
        int  (*set_ringparam)(struct net_device *, struct ethtool_ringparam *);
        void (*get_pauseparam)(struct net_device *,
                               struct ethtool_pauseparam*);
        int  (*set_pauseparam)(struct net_device *,
                               struct ethtool_pauseparam*);
        u32  (*get_rx_csum)(struct net_device *);
        int  (*set_rx_csum)(struct net_device *, u32);
        u32  (*get_tx_csum)(struct net_device *);
        int  (*set_tx_csum)(struct net_device *, u32);
        u32  (*get_sg)(struct net_device *);
        int  (*set_sg)(struct net_device *, u32);
        u32  (*get_tso)(struct net_device *);
        int  (*set_tso)(struct net_device *, u32);
        int  (*self_test_count)(struct net_device *);
        void (*self_test)(struct net_device *, struct ethtool_test *, u64 *);
        void (*get_strings)(struct net_device *, u32 stringset, u8 *);
        int  (*phys_id)(struct net_device *, u32);
        int  (*get_stats_count)(struct net_device *);
        void (*get_ethtool_stats)(struct net_device *, struct ethtool_stats *,
                                  u64 *);
} *ethtool_ops = NULL;

#undef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev, ops) (ethtool_ops = (ops))

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)
#ifndef SET_ETHTOOL_OPS
#define SET_ETHTOOL_OPS(netdev,ops) \
         ( (netdev)->ethtool_ops = (ops) )
#endif //SET_ETHTOOL_OPS
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,16,0)

//#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)
#ifndef netif_msg_init
#define netif_msg_init _kc_netif_msg_init
/* copied from linux kernel 2.6.20 include/linux/netdevice.h */
static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
        /* use default */
        if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
                return default_msg_enable_bits;
        if (debug_value == 0)   /* no output */
                return 0;
        /* set low N bits */
        return (1 << debug_value) - 1;
}

#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,5)

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)
static inline void eth_copy_and_sum (struct sk_buff *dest,
                                     const unsigned char *src,
                                     int len, int base)
{
        memcpy (dest->data, src, len);
}
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,22)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 /include/linux/time.h */
/* Parameters used to convert the timespec values: */
#define MSEC_PER_SEC    1000L

/* copied from linux kernel 2.6.20 /include/linux/jiffies.h */
/*
 * Change timeval to jiffies, trying to avoid the
 * most obvious overflows..
 *
 * And some not so obvious.
 *
 * Note that we don't want to return MAX_LONG, because
 * for various timeout reasons we often end up having
 * to wait "jiffies+1" in order to guarantee that we wait
 * at _least_ "jiffies" - so "jiffies+1" had better still
 * be positive.
 */
#define MAX_JIFFY_OFFSET ((~0UL >> 1)-1)

/*
 * Convert jiffies to milliseconds and back.
 *
 * Avoid unnecessary multiplications/divisions in the
 * two most common HZ cases:
 */
static inline unsigned int _kc_jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
        return (j * MSEC_PER_SEC) / HZ;
#endif
}

static inline unsigned long _kc_msecs_to_jiffies(const unsigned int m)
{
        if (m > _kc_jiffies_to_msecs(MAX_JIFFY_OFFSET))
                return MAX_JIFFY_OFFSET;
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
        return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
        return m * (HZ / MSEC_PER_SEC);
#else
        return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)

/* copied from linux kernel 2.6.12.6 /include/linux/pm.h */
typedef int __bitwise pci_power_t;

/* copied from linux kernel 2.6.12.6 /include/linux/pci.h */
typedef u32 __bitwise pm_message_t;

#define PCI_D0  ((pci_power_t __force) 0)
#define PCI_D1  ((pci_power_t __force) 1)
#define PCI_D2  ((pci_power_t __force) 2)
#define PCI_D3hot   ((pci_power_t __force) 3)
#define PCI_D3cold  ((pci_power_t __force) 4)
#define PCI_POWER_ERROR ((pci_power_t __force) -1)

/* copied from linux kernel 2.6.12.6 /drivers/pci/pci.c */
/**
 * pci_choose_state - Choose the power state of a PCI device
 * @dev: PCI device to be suspended
 * @state: target sleep state for the whole system. This is the value
 *  that is passed to suspend() function.
 *
 * Returns PCI power state suitable for given device and given system
 * message.
 */

pci_power_t pci_choose_state(struct pci_dev *dev, pm_message_t state)
{
        if (!pci_find_capability(dev, PCI_CAP_ID_PM))
                return PCI_D0;

        switch (state) {
        case 0:
                return PCI_D0;
        case 3:
                return PCI_D3hot;
        default:
                printk("They asked me for state %d\n", state);
//      BUG();
        }
        return PCI_D0;
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
/**
 * msleep_interruptible - sleep waiting for waitqueue interruptions
 * @msecs: Time in milliseconds to sleep for
 */
#define msleep_interruptible _kc_msleep_interruptible
unsigned long _kc_msleep_interruptible(unsigned int msecs)
{
        unsigned long timeout = _kc_msecs_to_jiffies(msecs);

        while (timeout && !signal_pending(current)) {
                set_current_state(TASK_INTERRUPTIBLE);
                timeout = schedule_timeout(timeout);
        }
        return _kc_jiffies_to_msecs(timeout);
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)
/* copied from linux kernel 2.6.20 include/linux/sched.h */
#ifndef __sched
#define __sched     __attribute__((__section__(".sched.text")))
#endif

/* copied from linux kernel 2.6.20 kernel/timer.c */
signed long __sched schedule_timeout_uninterruptible(signed long timeout)
{
        __set_current_state(TASK_UNINTERRUPTIBLE);
        return schedule_timeout(timeout);
}

/* copied from linux kernel 2.6.20 include/linux/mii.h */
#undef if_mii
#define if_mii _kc_if_mii
static inline struct mii_ioctl_data *if_mii(struct ifreq *rq)
{
        return (struct mii_ioctl_data *) &rq->ifr_ifru;
}
#endif  //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,7)

struct rtl8125_counters {
        u64 tx_packets;
        u64 rx_packets;
        u64 tx_errors;
        u32 rx_errors;
        u16 rx_missed;
        u16 align_errors;
        u32 tx_one_collision;
        u32 tx_multi_collision;
        u64 rx_unicast;
        u64 rx_broadcast;
        u32 rx_multicast;
        u16 tx_aborted;
        u16 tx_underun;
};

#ifdef ENABLE_R8125_PROCFS
/****************************************************************************
*   -----------------------------PROCFS STUFF-------------------------
*****************************************************************************
*/

static struct proc_dir_entry *rtl8125_proc;
static int proc_init_num = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
static int proc_get_driver_variable(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        seq_puts(m, "\nDump Driver Variable\n");

        spin_lock_irqsave(&tp->lock, flags);
        seq_puts(m, "Variable\tValue\n----------\t-----\n");
        seq_printf(m, "MODULENAME\t%s\n", MODULENAME);
        seq_printf(m, "driver version\t%s\n", RTL8125_VERSION);
        seq_printf(m, "chipset\t%d\n", tp->chipset);
        seq_printf(m, "chipset_name\t%s\n", rtl_chip_info[tp->chipset].name);
        seq_printf(m, "mtu\t%d\n", dev->mtu);
        seq_printf(m, "NUM_RX_DESC\t0x%x\n", NUM_RX_DESC);
        seq_printf(m, "cur_rx\t0x%x\n", tp->cur_rx);
        seq_printf(m, "dirty_rx\t0x%x\n", tp->dirty_rx);
        seq_printf(m, "NUM_TX_DESC\t0x%x\n", NUM_TX_DESC);
        seq_printf(m, "cur_tx\t0x%x\n", tp->cur_tx);
        seq_printf(m, "dirty_tx\t0x%x\n", tp->dirty_tx);
        seq_printf(m, "rx_buf_sz\t0x%x\n", tp->rx_buf_sz);
        seq_printf(m, "esd_flag\t0x%x\n", tp->esd_flag);
        seq_printf(m, "pci_cfg_is_read\t0x%x\n", tp->pci_cfg_is_read);
        seq_printf(m, "rtl8125_rx_config\t0x%x\n", tp->rtl8125_rx_config);
        seq_printf(m, "cp_cmd\t0x%x\n", tp->cp_cmd);
        seq_printf(m, "intr_mask\t0x%x\n", tp->intr_mask);
        seq_printf(m, "timer_intr_mask\t0x%x\n", tp->timer_intr_mask);
        seq_printf(m, "wol_enabled\t0x%x\n", tp->wol_enabled);
        seq_printf(m, "wol_opts\t0x%x\n", tp->wol_opts);
        seq_printf(m, "efuse_ver\t0x%x\n", tp->efuse_ver);
        seq_printf(m, "eeprom_type\t0x%x\n", tp->eeprom_type);
        seq_printf(m, "autoneg\t0x%x\n", tp->autoneg);
        seq_printf(m, "duplex\t0x%x\n", tp->duplex);
        seq_printf(m, "speed\t%d\n", tp->speed);
        seq_printf(m, "advertising\t0x%x\n", tp->advertising);
        seq_printf(m, "eeprom_len\t0x%x\n", tp->eeprom_len);
        seq_printf(m, "cur_page\t0x%x\n", tp->cur_page);
        seq_printf(m, "bios_setting\t0x%x\n", tp->bios_setting);
        seq_printf(m, "features\t0x%x\n", tp->features);
        seq_printf(m, "org_pci_offset_99\t0x%x\n", tp->org_pci_offset_99);
        seq_printf(m, "org_pci_offset_180\t0x%x\n", tp->org_pci_offset_180);
        seq_printf(m, "issue_offset_99_event\t0x%x\n", tp->issue_offset_99_event);
        seq_printf(m, "org_pci_offset_80\t0x%x\n", tp->org_pci_offset_80);
        seq_printf(m, "org_pci_offset_81\t0x%x\n", tp->org_pci_offset_81);
        seq_printf(m, "use_timer_interrrupt\t0x%x\n", tp->use_timer_interrrupt);
        seq_printf(m, "HwIcVerUnknown\t0x%x\n", tp->HwIcVerUnknown);
        seq_printf(m, "NotWrRamCodeToMicroP\t0x%x\n", tp->NotWrRamCodeToMicroP);
        seq_printf(m, "NotWrMcuPatchCode\t0x%x\n", tp->NotWrMcuPatchCode);
        seq_printf(m, "HwHasWrRamCodeToMicroP\t0x%x\n", tp->HwHasWrRamCodeToMicroP);
        seq_printf(m, "sw_ram_code_ver\t0x%x\n", tp->sw_ram_code_ver);
        seq_printf(m, "hw_ram_code_ver\t0x%x\n", tp->hw_ram_code_ver);
        seq_printf(m, "rtk_enable_diag\t0x%x\n", tp->rtk_enable_diag);
        seq_printf(m, "ShortPacketSwChecksum\t0x%x\n", tp->ShortPacketSwChecksum);
        seq_printf(m, "UseSwPaddingShortPkt\t0x%x\n", tp->UseSwPaddingShortPkt);
        seq_printf(m, "RequireAdcBiasPatch\t0x%x\n", tp->RequireAdcBiasPatch);
        seq_printf(m, "AdcBiasPatchIoffset\t0x%x\n", tp->AdcBiasPatchIoffset);
        seq_printf(m, "RequireAdjustUpsTxLinkPulseTiming\t0x%x\n", tp->RequireAdjustUpsTxLinkPulseTiming);
        seq_printf(m, "SwrCnt1msIni\t0x%x\n", tp->SwrCnt1msIni);
        seq_printf(m, "HwSuppNowIsOobVer\t0x%x\n", tp->HwSuppNowIsOobVer);
        seq_printf(m, "HwFiberModeVer\t0x%x\n", tp->HwFiberModeVer);
        seq_printf(m, "HwFiberStat\t0x%x\n", tp->HwFiberStat);
        seq_printf(m, "HwSwitchMdiToFiber\t0x%x\n", tp->HwSwitchMdiToFiber);
        seq_printf(m, "NicCustLedValue\t0x%x\n", tp->NicCustLedValue);
        seq_printf(m, "RequiredSecLanDonglePatch\t0x%x\n", tp->RequiredSecLanDonglePatch);
        seq_printf(m, "HwSuppDashVer\t0x%x\n", tp->HwSuppDashVer);
        seq_printf(m, "DASH\t0x%x\n", tp->DASH);
        seq_printf(m, "dash_printer_enabled\t0x%x\n", tp->dash_printer_enabled);
        seq_printf(m, "HwSuppKCPOffloadVer\t0x%x\n", tp->HwSuppKCPOffloadVer);
        seq_printf(m, "speed_mode\t0x%x\n", speed_mode);
        seq_printf(m, "duplex_mode\t0x%x\n", duplex_mode);
        seq_printf(m, "autoneg_mode\t0x%x\n", autoneg_mode);
        seq_printf(m, "advertising_mode\t0x%x\n", advertising_mode);
        seq_printf(m, "aspm\t0x%x\n", aspm);
        seq_printf(m, "s5wol\t0x%x\n", s5wol);
        seq_printf(m, "s5_keep_curr_mac\t0x%x\n", s5_keep_curr_mac);
        seq_printf(m, "eee_enable\t0x%x\n", eee_enable);
        seq_printf(m, "hwoptimize\t0x%lx\n", hwoptimize);
        seq_printf(m, "proc_init_num\t0x%x\n", proc_init_num);
        seq_printf(m, "s0_magic_packet\t0x%x\n", s0_magic_packet);
        seq_printf(m, "HwSuppMagicPktVer\t0x%x\n", tp->HwSuppMagicPktVer);
        seq_printf(m, "HwSuppCheckPhyDisableModeVer\t0x%x\n", tp->HwSuppCheckPhyDisableModeVer);
        seq_printf(m, "HwPkgDet\t0x%x\n", tp->HwPkgDet);
        seq_printf(m, "HwSuppGigaForceMode\t0x%x\n", tp->HwSuppGigaForceMode);
        seq_printf(m, "random_mac\t0x%x\n", tp->random_mac);
        seq_printf(m, "org_mac_addr\t%pM\n", tp->org_mac_addr);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        seq_printf(m, "perm_addr\t%pM\n", dev->perm_addr);
#endif
        seq_printf(m, "dev_addr\t%pM\n", dev->dev_addr);
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_tally_counter(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8125_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;

        seq_puts(m, "\nDump Tally Counter\n");

        //ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters) {
                seq_puts(m, "\nDump Tally Counter Fail\n");
                return 0;
        }

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_puts(m, "Statistics\tValue\n----------\t-----\n");
        seq_printf(m, "tx_packets\t%lld\n", le64_to_cpu(counters->tx_packets));
        seq_printf(m, "rx_packets\t%lld\n", le64_to_cpu(counters->rx_packets));
        seq_printf(m, "tx_errors\t%lld\n", le64_to_cpu(counters->tx_errors));
        seq_printf(m, "rx_missed\t%lld\n", le64_to_cpu(counters->rx_missed));
        seq_printf(m, "align_errors\t%lld\n", le64_to_cpu(counters->align_errors));
        seq_printf(m, "tx_one_collision\t%lld\n", le64_to_cpu(counters->tx_one_collision));
        seq_printf(m, "tx_multi_collision\t%lld\n", le64_to_cpu(counters->tx_multi_collision));
        seq_printf(m, "rx_unicast\t%lld\n", le64_to_cpu(counters->rx_unicast));
        seq_printf(m, "rx_broadcast\t%lld\n", le64_to_cpu(counters->rx_broadcast));
        seq_printf(m, "rx_multicast\t%lld\n", le64_to_cpu(counters->rx_multicast));
        seq_printf(m, "tx_aborted\t%lld\n", le64_to_cpu(counters->tx_aborted));
        seq_printf(m, "tx_underun\t%lld\n", le64_to_cpu(counters->tx_underun));

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_registers(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8125_MAC_REGS_SIZE;
        u8 byte_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        seq_puts(m, "\nDump MAC Registers\n");
        seq_puts(m, "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 16 && n < max; i++, n++) {
                        byte_rd = readb(ioaddr + n);
                        seq_printf(m, "%02x ", byte_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_pcie_phy(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8125_EPHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        seq_puts(m, "\nDump PCIE PHY\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8125_ephy_read(ioaddr, n);
                        seq_printf(m, "%04x ", word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_eth_phy(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8125_PHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        seq_puts(m, "\nDump Ethernet PHY\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        seq_puts(m, "\n####################page 0##################\n ");
        rtl8125_mdio_write(tp, 0x1f, 0x0000);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8125_mdio_read(tp, n);
                        seq_printf(m, "%04x ", word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_extended_registers(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8125_ERI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        seq_puts(m, "\nDump Extended Registers\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%02x:\t", n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        dword_rd = rtl8125_eri_read(ioaddr, n, 4, ERIAR_ExGMAC);
                        seq_printf(m, "%08x ", dword_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}

static int proc_get_pci_registers(struct seq_file *m, void *v)
{
        struct net_device *dev = m->private;
        int i, n, max = R8125_PCI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        seq_puts(m, "\nDump PCI Registers\n");
        seq_puts(m, "\nOffset\tValue\n------\t-----\n ");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                seq_printf(m, "\n0x%03x:\t", n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
                        seq_printf(m, "%08x ", dword_rd);
                }
        }

        n = 0x110;
        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
        seq_printf(m, "\n0x%03x:\t%08x ", n, dword_rd);
        n = 0x70c;
        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
        seq_printf(m, "\n0x%03x:\t%08x ", n, dword_rd);

        spin_unlock_irqrestore(&tp->lock, flags);

        seq_putc(m, '\n');
        return 0;
}
#else

static int proc_get_driver_variable(char *page, char **start,
                                    off_t offset, int count,
                                    int *eof, void *data)
{
        struct net_device *dev = data;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Driver Driver\n");

        spin_lock_irqsave(&tp->lock, flags);
        len += snprintf(page + len, count - len,
                        "Variable\tValue\n----------\t-----\n");

        len += snprintf(page + len, count - len,
                        "MODULENAME\t%s\n"
                        "driver version\t%s\n"
                        "chipset\t%d\n"
                        "chipset_name\t%s\n"
                        "mtu\t%d\n"
                        "NUM_RX_DESC\t0x%x\n"
                        "cur_rx\t0x%x\n"
                        "dirty_rx\t0x%x\n"
                        "NUM_TX_DESC\t0x%x\n"
                        "cur_tx\t0x%x\n"
                        "dirty_tx\t0x%x\n"
                        "rx_buf_sz\t0x%x\n"
                        "esd_flag\t0x%x\n"
                        "pci_cfg_is_read\t0x%x\n"
                        "rtl8125_rx_config\t0x%x\n"
                        "cp_cmd\t0x%x\n"
                        "intr_mask\t0x%x\n"
                        "timer_intr_mask\t0x%x\n"
                        "wol_enabled\t0x%x\n"
                        "wol_opts\t0x%x\n"
                        "efuse_ver\t0x%x\n"
                        "eeprom_type\t0x%x\n"
                        "autoneg\t0x%x\n"
                        "duplex\t0x%x\n"
                        "speed\t%d\n"
                        "advertising\t0x%x\n"
                        "eeprom_len\t0x%x\n"
                        "cur_page\t0x%x\n"
                        "bios_setting\t0x%x\n"
                        "features\t0x%x\n"
                        "org_pci_offset_99\t0x%x\n"
                        "org_pci_offset_180\t0x%x\n"
                        "issue_offset_99_event\t0x%x\n"
                        "org_pci_offset_80\t0x%x\n"
                        "org_pci_offset_81\t0x%x\n"
                        "use_timer_interrrupt\t0x%x\n"
                        "HwIcVerUnknown\t0x%x\n"
                        "NotWrRamCodeToMicroP\t0x%x\n"
                        "NotWrMcuPatchCode\t0x%x\n"
                        "HwHasWrRamCodeToMicroP\t0x%x\n"
                        "sw_ram_code_ver\t0x%x\n"
                        "hw_ram_code_ver\t0x%x\n"
                        "rtk_enable_diag\t0x%x\n"
                        "ShortPacketSwChecksum\t0x%x\n"
                        "UseSwPaddingShortPkt\t0x%x\n"
                        "RequireAdcBiasPatch\t0x%x\n"
                        "AdcBiasPatchIoffset\t0x%x\n"
                        "RequireAdjustUpsTxLinkPulseTiming\t0x%x\n"
                        "SwrCnt1msIni\t0x%x\n"
                        "HwSuppNowIsOobVer\t0x%x\n"
                        "HwFiberModeVer\t0x%x\n"
                        "HwFiberStat\t0x%x\n"
                        "HwSwitchMdiToFiber\t0x%x\n"
                        "NicCustLedValue\t0x%x\n"
                        "RequiredSecLanDonglePatch\t0x%x\n"
                        "HwSuppDashVer\t0x%x\n"
                        "DASH\t0x%x\n"
                        "dash_printer_enabled\t0x%x\n"
                        "HwSuppKCPOffloadVer\t0x%x\n"
                        "speed_mode\t0x%x\n"
                        "duplex_mode\t0x%x\n"
                        "autoneg_mode\t0x%x\n"
                        "advertising_mode\t0x%x\n"
                        "aspm\t0x%x\n"
                        "s5wol\t0x%x\n"
                        "s5_keep_curr_mac\t0x%x\n"
                        "eee_enable\t0x%x\n"
                        "hwoptimize\t0x%lx\n"
                        "proc_init_num\t0x%x\n"
                        "s0_magic_packet\t0x%x\n"
                        "HwSuppMagicPktVer\t0x%x\n"
                        "HwSuppCheckPhyDisableModeVer\t0x%x\n"
                        "HwPkgDet\t0x%x\n"
                        "HwSuppGigaForceMode\t0x%x\n"
                        "random_mac\t0x%x\n"
                        "org_mac_addr\t%pM\n"
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
                        "perm_addr\t%pM\n"
#endif
                        "dev_addr\t%pM\n",
                        MODULENAME,
                        RTL8125_VERSION,
                        tp->chipset,
                        rtl_chip_info[tp->chipset].name,
                        dev->mtu,
                        NUM_RX_DESC,
                        tp->cur_rx,
                        tp->dirty_rx,
                        NUM_TX_DESC,
                        tp->cur_tx,
                        tp->dirty_tx,
                        tp->rx_buf_sz,
                        tp->esd_flag,
                        tp->pci_cfg_is_read,
                        tp->rtl8125_rx_config,
                        tp->cp_cmd,
                        tp->intr_mask,
                        tp->timer_intr_mask,
                        tp->wol_enabled,
                        tp->wol_opts,
                        tp->efuse_ver,
                        tp->eeprom_type,
                        tp->autoneg,
                        tp->duplex,
                        tp->speed,
                        tp->advertising,
                        tp->eeprom_len,
                        tp->cur_page,
                        tp->bios_setting,
                        tp->features,
                        tp->org_pci_offset_99,
                        tp->org_pci_offset_180,
                        tp->issue_offset_99_event,
                        tp->org_pci_offset_80,
                        tp->org_pci_offset_81,
                        tp->use_timer_interrrupt,
                        tp->HwIcVerUnknown,
                        tp->NotWrRamCodeToMicroP,
                        tp->NotWrMcuPatchCode,
                        tp->HwHasWrRamCodeToMicroP,
                        tp->sw_ram_code_ver,
                        tp->hw_ram_code_ver,
                        tp->rtk_enable_diag,
                        tp->ShortPacketSwChecksum,
                        tp->UseSwPaddingShortPkt,
                        tp->RequireAdcBiasPatch,
                        tp->AdcBiasPatchIoffset,
                        tp->RequireAdjustUpsTxLinkPulseTiming,
                        tp->SwrCnt1msIni,
                        tp->HwSuppNowIsOobVer,
                        tp->HwFiberModeVer,
                        tp->HwFiberStat,
                        tp->HwSwitchMdiToFiber,
                        tp->NicCustLedValue,
                        tp->RequiredSecLanDonglePatch,
                        tp->HwSuppDashVer,
                        tp->DASH,
                        tp->dash_printer_enabled,
                        tp->HwSuppKCPOffloadVer,
                        speed_mode,
                        duplex_mode,
                        autoneg_mode,
                        advertising_mode,
                        aspm,
                        s5wol,
                        s5_keep_curr_mac,
                        eee_enable,
                        hwoptimize,
                        proc_init_num,
                        s0_magic_packet,
                        tp->HwSuppMagicPktVer,
                        tp->HwSuppCheckPhyDisableModeVer,
                        tp->HwPkgDet,
                        tp->HwSuppGigaForceMode,
                        tp->random_mac,
                        tp->org_mac_addr,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
                        dev->perm_addr,
#endif
                        dev->dev_addr
                       );
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_tally_counter(char *page, char **start,
                                  off_t offset, int count,
                                  int *eof, void *data)
{
        struct net_device *dev = data;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8125_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Tally Counter\n");

        //ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters) {
                len += snprintf(page + len, count - len,
                                "\nDump Tally Counter Fail\n");
                goto out;
        }

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len,
                        "Statistics\tValue\n----------\t-----\n");

        len += snprintf(page + len, count - len,
                        "tx_packets\t%lld\n"
                        "rx_packets\t%lld\n"
                        "tx_errors\t%lld\n"
                        "rx_missed\t%lld\n"
                        "align_errors\t%lld\n"
                        "tx_one_collision\t%lld\n"
                        "tx_multi_collision\t%lld\n"
                        "rx_unicast\t%lld\n"
                        "rx_broadcast\t%lld\n"
                        "rx_multicast\t%lld\n"
                        "tx_aborted\t%lld\n"
                        "tx_underun\t%lld\n",
                        le64_to_cpu(counters->tx_packets),
                        le64_to_cpu(counters->rx_packets),
                        le64_to_cpu(counters->tx_errors),
                        le64_to_cpu(counters->rx_missed),
                        le64_to_cpu(counters->align_errors),
                        le64_to_cpu(counters->tx_one_collision),
                        le64_to_cpu(counters->tx_multi_collision),
                        le64_to_cpu(counters->rx_unicast),
                        le64_to_cpu(counters->rx_broadcast),
                        le64_to_cpu(counters->rx_multicast),
                        le64_to_cpu(counters->tx_aborted),
                        le64_to_cpu(counters->tx_underun)
                       );

        len += snprintf(page + len, count - len, "\n");
out:
        *eof = 1;
        return len;
}

static int proc_get_registers(char *page, char **start,
                              off_t offset, int count,
                              int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8125_MAC_REGS_SIZE;
        u8 byte_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump MAC Registers\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 16 && n < max; i++, n++) {
                        byte_rd = readb(ioaddr + n);
                        len += snprintf(page + len, count - len,
                                        "%02x ",
                                        byte_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_pcie_phy(char *page, char **start,
                             off_t offset, int count,
                             int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8125_EPHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump PCIE PHY\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8125_ephy_read(ioaddr, n);
                        len += snprintf(page + len, count - len,
                                        "%04x ",
                                        word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_eth_phy(char *page, char **start,
                            off_t offset, int count,
                            int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8125_PHY_REGS_SIZE/2;
        u16 word_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Ethernet PHY\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        len += snprintf(page + len, count - len,
                        "\n####################page 0##################\n");
        rtl8125_mdio_write(tp, 0x1f, 0x0000);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 8 && n < max; i++, n++) {
                        word_rd = rtl8125_mdio_read(tp, n);
                        len += snprintf(page + len, count - len,
                                        "%04x ",
                                        word_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}

static int proc_get_extended_registers(char *page, char **start,
                                       off_t offset, int count,
                                       int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8125_ERI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump Extended Registers\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%02x:\t",
                                n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        dword_rd = rtl8125_eri_read(ioaddr, n, 4, ERIAR_ExGMAC);
                        len += snprintf(page + len, count - len,
                                        "%08x ",
                                        dword_rd);
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");
out:
        *eof = 1;
        return len;
}

static int proc_get_pci_registers(char *page, char **start,
                                  off_t offset, int count,
                                  int *eof, void *data)
{
        struct net_device *dev = data;
        int i, n, max = R8125_PCI_REGS_SIZE;
        u32 dword_rd;
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;
        int len = 0;

        len += snprintf(page + len, count - len,
                        "\nDump PCI Registers\n"
                        "Offset\tValue\n------\t-----\n");

        spin_lock_irqsave(&tp->lock, flags);
        for (n = 0; n < max;) {
                len += snprintf(page + len, count - len,
                                "\n0x%03x:\t",
                                n);

                for (i = 0; i < 4 && n < max; i++, n+=4) {
                        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
                        len += snprintf(page + len, count - len,
                                        "%08x ",
                                        dword_rd);
                }
        }

        n = 0x110;
        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
        len += snprintf(page + len, count - len,
                        "\n0x%03x:\t%08x ",
                        n,
                        dword_rd);
        n = 0x70c;
        pci_read_config_dword(tp->pci_dev, n, &dword_rd);
        len += snprintf(page + len, count - len,
                        "\n0x%03x:\t%08x ",
                        n,
                        dword_rd);
        spin_unlock_irqrestore(&tp->lock, flags);

        len += snprintf(page + len, count - len, "\n");

        *eof = 1;
        return len;
}
#endif
static void rtl8125_proc_module_init(void)
{
        //create /proc/net/r8125
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
        rtl8125_proc = proc_mkdir(MODULENAME, init_net.proc_net);
#else
        rtl8125_proc = proc_mkdir(MODULENAME, proc_net);
#endif
        if (!rtl8125_proc)
                dprintk("cannot create %s proc entry \n", MODULENAME);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
/*
 * seq_file wrappers for procfile show routines.
 */
static int rtl8125_proc_open(struct inode *inode, struct file *file)
{
        struct net_device *dev = proc_get_parent_data(inode);
        int (*show)(struct seq_file *, void *) = PDE_DATA(inode);

        return single_open(file, show, dev);
}

static const struct file_operations rtl8125_proc_fops = {
        .open           = rtl8125_proc_open,
        .read           = seq_read,
        .llseek         = seq_lseek,
        .release        = single_release,
};
#endif

/*
 * Table of proc files we need to create.
 */
struct rtl8125_proc_file {
        char name[12];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
        int (*show)(struct seq_file *, void *);
#else
        int (*show)(char *, char **, off_t, int, int *, void *);
#endif
};

static const struct rtl8125_proc_file rtl8125_proc_files[] = {
        { "driver_var", &proc_get_driver_variable },
        { "tally", &proc_get_tally_counter },
        { "registers", &proc_get_registers },
        { "pcie_phy", &proc_get_pcie_phy },
        { "eth_phy", &proc_get_eth_phy },
        { "ext_regs", &proc_get_extended_registers },
        { "pci_regs", &proc_get_pci_registers },
        { "" }
};

static void rtl8125_proc_init(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        const struct rtl8125_proc_file *f;
        struct proc_dir_entry *dir;

        if (rtl8125_proc && !tp->proc_dir) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                dir = proc_mkdir_data(dev->name, 0, rtl8125_proc, dev);
                if (!dir) {
                        printk("Unable to initialize /proc/net/%s/%s\n",
                               MODULENAME, dev->name);
                        return;
                }

                tp->proc_dir = dir;
                proc_init_num++;

                for (f = rtl8125_proc_files; f->name[0]; f++) {
                        if (!proc_create_data(f->name, S_IFREG | S_IRUGO, dir,
                                              &rtl8125_proc_fops, f->show)) {
                                printk("Unable to initialize "
                                       "/proc/net/%s/%s/%s\n",
                                       MODULENAME, dev->name, f->name);
                                return;
                        }
                }
#else
                dir = proc_mkdir(dev->name, rtl8125_proc);
                if (!dir) {
                        printk("Unable to initialize /proc/net/%s/%s\n",
                               MODULENAME, dev->name);
                        return;
                }

                tp->proc_dir = dir;
                proc_init_num++;

                for (f = rtl8125_proc_files; f->name[0]; f++) {
                        if (!create_proc_read_entry(f->name, S_IFREG | S_IRUGO,
                                                    dir, f->show, dev)) {
                                printk("Unable to initialize "
                                       "/proc/net/%s/%s/%s\n",
                                       MODULENAME, dev->name, f->name);
                                return;
                        }
                }
#endif
        }
}

static void rtl8125_proc_remove(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->proc_dir) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                remove_proc_subtree(dev->name, rtl8125_proc);
                proc_init_num--;

#else
                const struct rtl8125_proc_file *f;
                struct rtl8125_private *tp = netdev_priv(dev);

                for (f = rtl8125_proc_files; f->name[0]; f++)
                        remove_proc_entry(f->name, tp->proc_dir);

                remove_proc_entry(dev->name, rtl8125_proc);
                proc_init_num--;
#endif
                tp->proc_dir = NULL;
        }
}

#endif //ENABLE_R8125_PROCFS

static inline u16 map_phy_ocp_addr(u16 PageNum, u8 RegNum)
{
        u16 OcpPageNum = 0;
        u8 OcpRegNum = 0;
        u16 OcpPhyAddress = 0;

        if ( PageNum == 0 ) {
                OcpPageNum = OCP_STD_PHY_BASE_PAGE + ( RegNum / 8 );
                OcpRegNum = 0x10 + ( RegNum % 8 );
        } else {
                OcpPageNum = PageNum;
                OcpRegNum = RegNum;
        }

        OcpPageNum <<= 4;

        if ( OcpRegNum < 16 ) {
                OcpPhyAddress = 0;
        } else {
                OcpRegNum -= 16;
                OcpRegNum <<= 1;

                OcpPhyAddress = OcpPageNum + OcpRegNum;
        }


        return OcpPhyAddress;
}

static void mdio_real_write_phy_ocp(struct rtl8125_private *tp,
                                    u16 RegAddr,
                                    u16 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        int i;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(RegAddr % 2);
#endif
        data32 = RegAddr/2;
        data32 <<= OCPR_Addr_Reg_shift;
        data32 |= OCPR_Write | value;

        RTL_W32(PHYOCP, data32);
        for (i = 0; i < 100; i++) {
                udelay(1);

                if (!(RTL_R32(PHYOCP) & OCPR_Flag))
                        break;
        }
}

static void mdio_direct_write_phy_ocp(struct rtl8125_private *tp,
                                      u16 RegAddr,
                                      u16 value)
{
        if (tp->rtk_enable_diag) return;

        mdio_real_write_phy_ocp(tp, RegAddr, value);
}

static void rtl8125_mdio_write_phy_ocp(struct rtl8125_private *tp,
                                       u16 PageNum,
                                       u32 RegAddr,
                                       u32 value)
{
        u16 ocp_addr;

        ocp_addr = map_phy_ocp_addr(PageNum, RegAddr);

        mdio_direct_write_phy_ocp(tp, ocp_addr, value);
}

static void mdio_real_write(struct rtl8125_private *tp,
                            u32 RegAddr,
                            u32 value)
{
        if (RegAddr == 0x1F) {
                tp->cur_page = value;
                return;
        }
        rtl8125_mdio_write_phy_ocp(tp, tp->cur_page, RegAddr, value);
}

void rtl8125_mdio_write(struct rtl8125_private *tp,
                        u32 RegAddr,
                        u32 value)
{
        if (tp->rtk_enable_diag) return;

        mdio_real_write(tp, RegAddr, value);
}

void rtl8125_mdio_prot_write(struct rtl8125_private *tp,
                             u32 RegAddr,
                             u32 value)
{
        mdio_real_write(tp, RegAddr, value);
}

void rtl8125_mdio_prot_write_phy_ocp(struct rtl8125_private *tp,
                                     u32 RegAddr,
                                     u32 value)
{
        mdio_real_write_phy_ocp(tp, RegAddr, value);
}

static u32 mdio_real_read_phy_ocp(struct rtl8125_private *tp,
                                  u16 RegAddr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        int i, value = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(RegAddr % 2);
#endif
        data32 = RegAddr/2;
        data32 <<= OCPR_Addr_Reg_shift;

        RTL_W32(PHYOCP, data32);
        for (i = 0; i < 100; i++) {
                udelay(1);

                if (RTL_R32(PHYOCP) & OCPR_Flag)
                        break;
        }
        value = RTL_R32(PHYOCP) & OCPDR_Data_Mask;

        return value;
}

static u32 mdio_direct_read_phy_ocp(struct rtl8125_private *tp,
                                    u16 RegAddr)
{
        if (tp->rtk_enable_diag) return 0xffffffff;

        return mdio_real_read_phy_ocp(tp, RegAddr);
}

static u32 rtl8125_mdio_read_phy_ocp(struct rtl8125_private *tp,
                                     u16 PageNum,
                                     u32 RegAddr)
{
        u16 ocp_addr;

        ocp_addr = map_phy_ocp_addr(PageNum, RegAddr);

        return mdio_direct_read_phy_ocp(tp, ocp_addr);
}

static u32 mdio_real_read(struct rtl8125_private *tp,
                          u32 RegAddr)
{
        return rtl8125_mdio_read_phy_ocp(tp, tp->cur_page, RegAddr);
}

u32 rtl8125_mdio_read(struct rtl8125_private *tp,
                      u32 RegAddr)
{
        if (tp->rtk_enable_diag) return 0xffffffff;

        return mdio_real_read(tp, RegAddr);
}

u32 rtl8125_mdio_prot_read(struct rtl8125_private *tp,
                           u32 RegAddr)
{
        return mdio_real_read(tp, RegAddr);
}

u32 rtl8125_mdio_prot_read_phy_ocp(struct rtl8125_private *tp,
                                   u32 RegAddr)
{
        return mdio_real_read_phy_ocp(tp, RegAddr);
}

static void ClearAndSetEthPhyBit(struct rtl8125_private *tp, u8  addr, u16 clearmask, u16 setmask)
{
        u16 PhyRegValue;

        PhyRegValue = rtl8125_mdio_read(tp, addr);
        PhyRegValue &= ~clearmask;
        PhyRegValue |= setmask;
        rtl8125_mdio_write(tp, addr, PhyRegValue);
}

void rtl8125_clear_eth_phy_bit(struct rtl8125_private *tp, u8 addr, u16 mask)
{
        ClearAndSetEthPhyBit(tp,
                             addr,
                             mask,
                             0
                            );
}

void rtl8125_set_eth_phy_bit(struct rtl8125_private *tp,  u8  addr, u16  mask)
{
        ClearAndSetEthPhyBit(tp,
                             addr,
                             0,
                             mask
                            );
}

static void ClearAndSetEthPhyOcpBit(struct rtl8125_private *tp, u16 addr, u16 clearmask, u16 setmask)
{
        u16 PhyRegValue;

        PhyRegValue = mdio_direct_read_phy_ocp(tp, addr);
        PhyRegValue &= ~clearmask;
        PhyRegValue |= setmask;
        mdio_direct_write_phy_ocp(tp, addr, PhyRegValue);
}

void ClearEthPhyOcpBit(struct rtl8125_private *tp, u16 addr, u16 mask)
{
        ClearAndSetEthPhyOcpBit(tp,
                                addr,
                                mask,
                                0
                               );
}

void SetEthPhyOcpBit(struct rtl8125_private *tp,  u16 addr, u16 mask)
{
        ClearAndSetEthPhyOcpBit(tp,
                                addr,
                                0,
                                mask
                               );
}

void rtl8125_mac_ocp_write(struct rtl8125_private *tp, u16 reg_addr, u16 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(reg_addr % 2);
#endif

        data32 = reg_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;
        data32 += value;
        data32 |= OCPR_Write;

        RTL_W32(MACOCP, data32);
}

u16 rtl8125_mac_ocp_read(struct rtl8125_private *tp, u16 reg_addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 data32;
        u16 data16 = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(reg_addr % 2);
#endif

        data32 = reg_addr/2;
        data32 <<= OCPR_Addr_Reg_shift;

        RTL_W32(MACOCP, data32);
        data16 = (u16)RTL_R32(MACOCP);

        return data16;
}

void
ClearAndSetMcuAccessRegBit(
        struct rtl8125_private *tp,
        u16   addr,
        u16   clearmask,
        u16   setmask
)
{
        u16 PhyRegValue;

        PhyRegValue = rtl8125_mac_ocp_read(tp, addr);
        PhyRegValue &= ~clearmask;
        PhyRegValue |= setmask;
        rtl8125_mac_ocp_write(tp, addr, PhyRegValue);
}

void
ClearMcuAccessRegBit(
        struct rtl8125_private *tp,
        u16   addr,
        u16   mask
)
{
        ClearAndSetMcuAccessRegBit(tp,
                                   addr,
                                   mask,
                                   0
                                  );
}

void
SetMcuAccessRegBit(
        struct rtl8125_private *tp,
        u16   addr,
        u16   mask
)
{
        ClearAndSetMcuAccessRegBit(tp,
                                   addr,
                                   0,
                                   mask
                                  );
}

u32 rtl8125_ocp_read_with_oob_base_address(struct rtl8125_private *tp, u16 addr, u8 len, const u32 base_address)
{
        void __iomem *ioaddr = tp->mmio_addr;

        return rtl8125_eri_read_with_oob_base_address(ioaddr, addr, len, ERIAR_OOB, base_address);
}

u32 rtl8125_ocp_read(struct rtl8125_private *tp, u16 addr, u8 len)
{
        u32 value = 0;

        if (HW_DASH_SUPPORT_TYPE_2(tp))
                value = rtl8125_ocp_read_with_oob_base_address(tp, addr, len, NO_BASE_ADDRESS);
        else if (HW_DASH_SUPPORT_TYPE_3(tp))
                value = rtl8125_ocp_read_with_oob_base_address(tp, addr, len, RTL8168FP_OOBMAC_BASE);

        return value;
}

u32 rtl8125_ocp_write_with_oob_base_address(struct rtl8125_private *tp, u16 addr, u8 len, u32 value, const u32 base_address)
{
        void __iomem *ioaddr = tp->mmio_addr;

        return rtl8125_eri_write_with_oob_base_address(ioaddr, addr, len, value, ERIAR_OOB, base_address);
}

void rtl8125_ocp_write(struct rtl8125_private *tp, u16 addr, u8 len, u32 value)
{
        if (HW_DASH_SUPPORT_TYPE_2(tp))
                rtl8125_ocp_write_with_oob_base_address(tp, addr, len, value, NO_BASE_ADDRESS);
        else if (HW_DASH_SUPPORT_TYPE_3(tp))
                rtl8125_ocp_write_with_oob_base_address(tp, addr, len, value, RTL8168FP_OOBMAC_BASE);
}

void rtl8125_oob_mutex_lock(struct rtl8125_private *tp)
{
        u8 reg_16, reg_a0;
        u32 wait_cnt_0, wait_Cnt_1;
        u16 ocp_reg_mutex_ib;
        u16 ocp_reg_mutex_oob;
        u16 ocp_reg_mutex_prio;

        if (!tp->DASH) return;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                ocp_reg_mutex_oob = 0x110;
                ocp_reg_mutex_ib = 0x114;
                ocp_reg_mutex_prio = 0x11C;
                break;
        }

        rtl8125_ocp_write(tp, ocp_reg_mutex_ib, 1, BIT_0);
        reg_16 = rtl8125_ocp_read(tp, ocp_reg_mutex_oob, 1);
        wait_cnt_0 = 0;
        while(reg_16) {
                reg_a0 = rtl8125_ocp_read(tp, ocp_reg_mutex_prio, 1);
                if (reg_a0) {
                        rtl8125_ocp_write(tp, ocp_reg_mutex_ib, 1, 0x00);
                        reg_a0 = rtl8125_ocp_read(tp, ocp_reg_mutex_prio, 1);
                        wait_Cnt_1 = 0;
                        while(reg_a0) {
                                reg_a0 = rtl8125_ocp_read(tp, ocp_reg_mutex_prio, 1);

                                wait_Cnt_1++;

                                if (wait_Cnt_1 > 2000)
                                        break;
                        };
                        rtl8125_ocp_write(tp, ocp_reg_mutex_ib, 1, BIT_0);

                }
                reg_16 = rtl8125_ocp_read(tp, ocp_reg_mutex_oob, 1);

                wait_cnt_0++;

                if (wait_cnt_0 > 2000)
                        break;
        };
}

void rtl8125_oob_mutex_unlock(struct rtl8125_private *tp)
{
        u16 ocp_reg_mutex_ib;
        u16 ocp_reg_mutex_oob;
        u16 ocp_reg_mutex_prio;

        if (!tp->DASH) return;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                ocp_reg_mutex_oob = 0x110;
                ocp_reg_mutex_ib = 0x114;
                ocp_reg_mutex_prio = 0x11C;
                break;
        }

        rtl8125_ocp_write(tp, ocp_reg_mutex_prio, 1, BIT_0);
        rtl8125_ocp_write(tp, ocp_reg_mutex_ib, 1, 0x00);
}

void rtl8125_oob_notify(struct rtl8125_private *tp, u8 cmd)
{
        void __iomem *ioaddr = tp->mmio_addr;

        rtl8125_eri_write(ioaddr, 0xE8, 1, cmd, ERIAR_ExGMAC);

        rtl8125_ocp_write(tp, 0x30, 1, 0x01);
}

static int rtl8125_check_dash(struct rtl8125_private *tp)
{
        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                if (rtl8125_ocp_read(tp, 0x128, 1) & BIT_0)
                        return 1;
        }

        return 0;
}

void rtl8125_dash2_disable_tx(struct rtl8125_private *tp)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                u16 WaitCnt;
                u8 TmpUchar;

                //Disable oob Tx
                RTL_CMAC_W8(CMAC_IBCR2, RTL_CMAC_R8(CMAC_IBCR2) & ~( BIT_0 ));
                WaitCnt = 0;

                //wait oob tx disable
                do {
                        TmpUchar = RTL_CMAC_R8(CMAC_IBISR0);

                        if ( TmpUchar & ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE ) {
                                break;
                        }

                        udelay( 50 );
                        WaitCnt++;
                } while(WaitCnt < 2000);

                //Clear ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE
                RTL_CMAC_W8(CMAC_IBISR0, RTL_CMAC_R8(CMAC_IBISR0) | ISRIMR_DASH_TYPE2_TX_DISABLE_IDLE);
        }
}

void rtl8125_dash2_enable_tx(struct rtl8125_private *tp)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                RTL_CMAC_W8(CMAC_IBCR2, RTL_CMAC_R8(CMAC_IBCR2) | BIT_0);
        }
}

void rtl8125_dash2_disable_rx(struct rtl8125_private *tp)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                RTL_CMAC_W8(CMAC_IBCR0, RTL_CMAC_R8(CMAC_IBCR0) & ~( BIT_0 ));
        }
}

void rtl8125_dash2_enable_rx(struct rtl8125_private *tp)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                RTL_CMAC_W8(CMAC_IBCR0, RTL_CMAC_R8(CMAC_IBCR0) | BIT_0);
        }
}

static void rtl8125_dash2_disable_txrx(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                rtl8125_dash2_disable_tx( tp );
                rtl8125_dash2_disable_rx( tp );
        }
}

static void rtl8125_driver_start(struct rtl8125_private *tp)
{
        if (!tp->DASH)
                return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                int timeout;
                u32 tmp_value;

                rtl8125_ocp_write(tp, 0x180, 1, OOB_CMD_DRIVER_START);
                tmp_value = rtl8125_ocp_read(tp, 0x30, 1);
                tmp_value |= BIT_0;
                rtl8125_ocp_write(tp, 0x30, 1, tmp_value);

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if (rtl8125_ocp_read(tp, 0x124, 1) & BIT_0)
                                break;
                }
        }
}

static void rtl8125_driver_stop(struct rtl8125_private *tp)
{
        if (!tp->DASH)
                return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                struct net_device *dev = tp->dev;
                int timeout;
                u32 tmp_value;

                rtl8125_dash2_disable_txrx(dev);

                rtl8125_ocp_write(tp, 0x180, 1, OOB_CMD_DRIVER_STOP);
                tmp_value = rtl8125_ocp_read(tp, 0x30, 1);
                tmp_value |= BIT_0;
                rtl8125_ocp_write(tp, 0x30, 1, tmp_value);

                for (timeout = 0; timeout < 10; timeout++) {
                        mdelay(10);
                        if (!(rtl8125_ocp_read(tp, 0x124, 1) & BIT_0))
                                break;
                }
        }
}

void rtl8125_ephy_write(void __iomem *ioaddr, int RegAddr, int value)
{
        int i;

        RTL_W32(EPHYAR,
                EPHYAR_Write |
                (RegAddr & EPHYAR_Reg_Mask_v2) << EPHYAR_Reg_shift |
                (value & EPHYAR_Data_Mask));

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8125 has completed EPHY write */
                if (!(RTL_R32(EPHYAR) & EPHYAR_Flag))
                        break;
        }

        udelay(20);
}

u16 rtl8125_ephy_read(void __iomem *ioaddr, int RegAddr)
{
        int i;
        u16 value = 0xffff;

        RTL_W32(EPHYAR,
                EPHYAR_Read | (RegAddr & EPHYAR_Reg_Mask_v2) << EPHYAR_Reg_shift);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8125 has completed EPHY read */
                if (RTL_R32(EPHYAR) & EPHYAR_Flag) {
                        value = (u16) (RTL_R32(EPHYAR) & EPHYAR_Data_Mask);
                        break;
                }
        }

        udelay(20);

        return value;
}

/*
static void ClearAndSetPCIePhyBit(struct rtl8125_private *tp, u8 addr, u16 clearmask, u16 setmask)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u16 EphyValue;

        EphyValue = rtl8125_ephy_read( ioaddr, addr );
        EphyValue &= ~clearmask;
        EphyValue |= setmask;
        rtl8125_ephy_write( ioaddr, addr, EphyValue);
}

static void ClearPCIePhyBit(struct rtl8125_private *tp, u8 addr, u16 mask)
{
        ClearAndSetPCIePhyBit( tp,
                               addr,
                               mask,
                               0
                             );
}

static void SetPCIePhyBit( struct rtl8125_private *tp, u8 addr, u16 mask)
{
        ClearAndSetPCIePhyBit( tp,
                               addr,
                               0,
                               mask
                             );
}
*/

static u32
rtl8125_csi_other_fun_read(struct rtl8125_private *tp,
                           u8 multi_fun_sel_bit,
                           u32 addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 cmd;
        int i;
        u32 value = 0;

        cmd = CSIAR_Read | CSIAR_ByteEn << CSIAR_ByteEn_shift | (addr & CSIAR_Addr_Mask);

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                multi_fun_sel_bit = 0;

        if (multi_fun_sel_bit > 7)
                return 0xffffffff;

        cmd |= multi_fun_sel_bit << 16;

        RTL_W32(CSIAR, cmd);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8125 has completed CSI read */
                if (RTL_R32(CSIAR) & CSIAR_Flag) {
                        value = (u32)RTL_R32(CSIDR);
                        break;
                }
        }

        udelay(20);

        return value;
}

static void
rtl8125_csi_other_fun_write(struct rtl8125_private *tp,
                            u8 multi_fun_sel_bit,
                            u32 addr,
                            u32 value)
{
        void __iomem *ioaddr = tp->mmio_addr;
        u32 cmd;
        int i;

        RTL_W32(CSIDR, value);
        cmd = CSIAR_Write | CSIAR_ByteEn << CSIAR_ByteEn_shift | (addr & CSIAR_Addr_Mask);
        if (tp->mcfg == CFG_METHOD_DEFAULT)
                multi_fun_sel_bit = 0;

        if ( multi_fun_sel_bit > 7 )
                return;

        cmd |= multi_fun_sel_bit << 16;

        RTL_W32(CSIAR, cmd);

        for (i = 0; i < 10; i++) {
                udelay(100);

                /* Check if the RTL8125 has completed CSI write */
                if (!(RTL_R32(CSIAR) & CSIAR_Flag))
                        break;
        }

        udelay(20);
}

static u32
rtl8125_csi_read(struct rtl8125_private *tp,
                 u32 addr)
{
        u8 multi_fun_sel_bit;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                multi_fun_sel_bit = 0;
        else
                multi_fun_sel_bit = 1;

        return rtl8125_csi_other_fun_read(tp, multi_fun_sel_bit, addr);
}

static void
rtl8125_csi_write(struct rtl8125_private *tp,
                  u32 addr,
                  u32 value)
{
        u8 multi_fun_sel_bit;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                multi_fun_sel_bit = 0;
        else
                multi_fun_sel_bit = 1;

        rtl8125_csi_other_fun_write(tp, multi_fun_sel_bit, addr, value);
}

static u8
rtl8125_csi_fun0_read_byte(struct rtl8125_private *tp,
                           u32 addr)
{
        u8 RetVal = 0;

        if (tp->mcfg == CFG_METHOD_DEFAULT) {
                struct pci_dev *pdev = tp->pci_dev;

                pci_read_config_byte(pdev, addr, &RetVal);
        } else {
                u32 TmpUlong;
                u16 RegAlignAddr;
                u8 ShiftByte;

                RegAlignAddr = addr & ~(0x3);
                ShiftByte = addr & (0x3);
                TmpUlong = rtl8125_csi_other_fun_read(tp, 0, addr);
                TmpUlong >>= (8*ShiftByte);
                RetVal = (u8)TmpUlong;
        }

        udelay(20);

        return RetVal;
}

static void
rtl8125_csi_fun0_write_byte(struct rtl8125_private *tp,
                            u32 addr,
                            u8 value)
{
        if (tp->mcfg == CFG_METHOD_DEFAULT) {
                struct pci_dev *pdev = tp->pci_dev;

                pci_write_config_byte(pdev, addr, value);
        } else {
                u32 TmpUlong;
                u16 RegAlignAddr;
                u8 ShiftByte;

                RegAlignAddr = addr & ~(0x3);
                ShiftByte = addr & (0x3);
                TmpUlong = rtl8125_csi_other_fun_read(tp, 0, RegAlignAddr);
                TmpUlong &= ~(0xFF << (8*ShiftByte));
                TmpUlong |= (value << (8*ShiftByte));
                rtl8125_csi_other_fun_write( tp, 0, RegAlignAddr, TmpUlong );
        }

        udelay(20);
}

u32 rtl8125_eri_read_with_oob_base_address(void __iomem *ioaddr, int addr, int len, int type, const u32 base_address)
{
        int i, val_shift, shift = 0;
        u32 value1 = 0, value2 = 0, mask;
        u32 eri_cmd;
        const u32 transformed_base_address = ((base_address & 0x00FFF000) << 6) | (base_address & 0x000FFF);

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % ERIAR_Addr_Align;
                addr = addr & ~0x3;

                eri_cmd = ERIAR_Read |
                          transformed_base_address |
                          type << ERIAR_Type_shift |
                          ERIAR_ByteEn << ERIAR_ByteEn_shift |
                          (addr & 0x0FFF);
                if (addr & 0xF000) {
                        u32 tmp;

                        tmp = addr & 0xF000;
                        tmp >>= 12;
                        eri_cmd |= (tmp << 20) & 0x00F00000;
                }

                RTL_W32(ERIAR, eri_cmd);

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8125 has completed ERI read */
                        if (RTL_R32(ERIAR) & ERIAR_Flag)
                                break;
                }

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = RTL_R32(ERIDR) & mask;
                value2 |= (value1 >> val_shift * 8) << shift * 8;

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return value2;
}

u32 rtl8125_eri_read(void __iomem *ioaddr, int addr, int len, int type)
{
        return rtl8125_eri_read_with_oob_base_address(ioaddr, addr, len, type, 0);
}

int rtl8125_eri_write_with_oob_base_address(void __iomem *ioaddr, int addr, int len, u32 value, int type, const u32 base_address)
{
        int i, val_shift, shift = 0;
        u32 value1 = 0, mask;
        u32 eri_cmd;
        const u32 transformed_base_address = ((base_address & 0x00FFF000) << 6) | (base_address & 0x000FFF);

        if (len > 4 || len <= 0)
                return -1;

        while (len > 0) {
                val_shift = addr % ERIAR_Addr_Align;
                addr = addr & ~0x3;

                if (len == 1)       mask = (0xFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 2)  mask = (0xFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else if (len == 3)  mask = (0xFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;
                else            mask = (0xFFFFFFFF << (val_shift * 8)) & 0xFFFFFFFF;

                value1 = rtl8125_eri_read_with_oob_base_address(ioaddr, addr, 4, type, base_address) & ~mask;
                value1 |= ((value << val_shift * 8) >> shift * 8);

                RTL_W32(ERIDR, value1);

                eri_cmd = ERIAR_Write |
                          transformed_base_address |
                          type << ERIAR_Type_shift |
                          ERIAR_ByteEn << ERIAR_ByteEn_shift |
                          (addr & 0x0FFF);
                if (addr & 0xF000) {
                        u32 tmp;

                        tmp = addr & 0xF000;
                        tmp >>= 12;
                        eri_cmd |= (tmp << 20) & 0x00F00000;
                }

                RTL_W32(ERIAR, eri_cmd);

                for (i = 0; i < 10; i++) {
                        udelay(100);

                        /* Check if the RTL8125 has completed ERI write */
                        if (!(RTL_R32(ERIAR) & ERIAR_Flag))
                                break;
                }

                if (len <= 4 - val_shift) {
                        len = 0;
                } else {
                        len -= (4 - val_shift);
                        shift = 4 - val_shift;
                        addr += 4;
                }
        }

        udelay(20);

        return 0;
}

int rtl8125_eri_write(void __iomem *ioaddr, int addr, int len, u32 value, int type)
{
        return rtl8125_eri_write_with_oob_base_address(ioaddr, addr, len, value, type, NO_BASE_ADDRESS);
}

static void
rtl8125_enable_rxdvgate(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_3);
                mdelay(2);
                break;
        }
}

static void
rtl8125_disable_rxdvgate(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(0xF2, RTL_R8(0xF2) & ~BIT_3);
                mdelay(2);
                break;
        }
}

static u8
rtl8125_is_gpio_low(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u8 gpio_low = FALSE;

        switch (tp->HwSuppCheckPhyDisableModeVer) {
        case 3:
                if (!(rtl8125_mac_ocp_read(tp, 0xDC04) & BIT_13))
                        gpio_low = TRUE;
                break;
        }

        if (gpio_low)
                dprintk("gpio is low.\n");

        return gpio_low;
}

static u8
rtl8125_is_phy_disable_mode_enabled(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 phy_disable_mode_enabled = FALSE;

        switch (tp->HwSuppCheckPhyDisableModeVer) {
        case 3:
                if (RTL_R8(0xF2) & BIT_5)
                        phy_disable_mode_enabled = TRUE;
                break;
        }

        if (phy_disable_mode_enabled)
                dprintk("phy disable mode enabled.\n");

        return phy_disable_mode_enabled;
}

static u8
rtl8125_is_in_phy_disable_mode(struct net_device *dev)
{
        u8 in_phy_disable_mode = FALSE;

        if (rtl8125_is_phy_disable_mode_enabled(dev) && rtl8125_is_gpio_low(dev))
                in_phy_disable_mode = TRUE;

        if (in_phy_disable_mode)
                dprintk("Hardware is in phy disable mode.\n");

        return in_phy_disable_mode;
}

static void
rtl8125_enable_phy_disable_mode(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->HwSuppCheckPhyDisableModeVer) {
        case 3:
                RTL_W8(0xF2, RTL_R8(0xF2) | BIT_5);
                break;
        }

        dprintk("enable phy disable mode.\n");
}

static void
rtl8125_disable_phy_disable_mode(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->HwSuppCheckPhyDisableModeVer) {
        case 3:
                RTL_W8(0xF2, RTL_R8(0xF2) & ~BIT_5);
                break;
        }

        mdelay(1);

        dprintk("disable phy disable mode.\n");
}

void
rtl8125_wait_txrx_fifo_empty(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                for (i = 0; i < 10; i++) {
                        udelay(100);
                        if ((RTL_R8(MCUCmd_reg) & (Txfifo_empty | Rxfifo_empty)) == (Txfifo_empty | Rxfifo_empty))
                                break;

                }
                break;
        }
}

#ifdef ENABLE_DASH_SUPPORT

inline void
rtl8125_enable_dash2_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                RTL_CMAC_W8(CMAC_IBIMR0, ( ISRIMR_DASH_TYPE2_ROK | ISRIMR_DASH_TYPE2_TOK | ISRIMR_DASH_TYPE2_TDU | ISRIMR_DASH_TYPE2_RDU | ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE ));
        }
}

static inline void
rtl8125_disable_dash2_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        if (!tp->DASH) return;

        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                RTL_CMAC_W8(CMAC_IBIMR0, 0);
        }
}
#endif

static inline void
rtl8125_enable_hw_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        RTL_W32(IMR0_8125, tp->intr_mask);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                rtl8125_enable_dash2_interrupt(tp, ioaddr);
#endif
}

static inline void
rtl8125_disable_hw_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        RTL_W32(IMR0_8125, 0x0000);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                rtl8125_disable_dash2_interrupt(tp, ioaddr);
#endif
}


static inline void
rtl8125_switch_to_hw_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        RTL_W32(TIMER_INT0_8125, 0x0000);

        rtl8125_enable_hw_interrupt(tp, ioaddr);
}

static inline void
rtl8125_switch_to_timer_interrupt(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        if (tp->use_timer_interrrupt) {
                RTL_W32(TIMER_INT0_8125, timer_count);
                RTL_W32(TCTR0_8125, timer_count);
                RTL_W32(IMR0_8125, tp->timer_intr_mask);

#ifdef ENABLE_DASH_SUPPORT
                if (tp->DASH)
                        rtl8125_enable_dash2_interrupt(tp, ioaddr);
#endif
        } else {
                rtl8125_switch_to_hw_interrupt(tp, ioaddr);
        }
}

static void
rtl8125_irq_mask_and_ack(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        rtl8125_disable_hw_interrupt(tp, ioaddr);
#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH) {
                if (tp->dash_printer_enabled) {
                        RTL_W16(IntrStatus, RTL_R16(IntrStatus) &
                                ~(ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET));
                } else {
                        if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                                RTL_CMAC_W8(CMAC_IBISR0, RTL_CMAC_R8(CMAC_IBISR0));
                        }
                }
        } else {
                RTL_W16(IntrStatus, RTL_R16(IntrStatus));
        }
#else
        RTL_W16(IntrStatus, RTL_R16(IntrStatus));
#endif
}

static void
rtl8125_nic_reset(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        rtl8125_enable_rxdvgate(dev);

        rtl8125_wait_txrx_fifo_empty(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                mdelay(2);
                break;
        }

        /* Soft reset the chip. */
        RTL_W8(ChipCmd, CmdReset);

        /* Check that the chip has finished the reset. */
        for (i = 100; i > 0; i--) {
                udelay(100);
                if ((RTL_R8(ChipCmd) & CmdReset) == 0)
                        break;
        }
}

static void
rtl8125_hw_clear_timer_int(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W32(TIMER_INT0_8125, 0x0000);
                RTL_W32(TIMER_INT1_8125, 0x0000);
                RTL_W32(TIMER_INT2_8125, 0x0000);
                RTL_W32(TIMER_INT3_8125, 0x0000);
                break;
        }
}

static void
rtl8125_hw_clear_int_miti(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                //IntMITI_0-IntMITI_31
                for (i=0xA00; i<0xB00; i+=4) {
                        RTL_W32(i, 0x0000);
                }
                break;
        }
}

static void
rtl8125_hw_reset(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        /* Disable interrupts */
        rtl8125_irq_mask_and_ack(tp, ioaddr);

        rtl8125_hw_clear_timer_int(dev);

        rtl8125_nic_reset(dev);
}

static unsigned int
rtl8125_xmii_reset_pending(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned int retval;

        rtl8125_mdio_write(tp, 0x1f, 0x0000);
        retval = rtl8125_mdio_read(tp, MII_BMCR) & BMCR_RESET;

        return retval;
}

static unsigned int
rtl8125_xmii_link_ok(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned int retval;

        retval = (RTL_R16(PHYstatus) & LinkStatus) ? 1 : 0;

        return retval;
}

static void
rtl8125_xmii_reset_enable(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int i, val = 0;

        if (rtl8125_is_in_phy_disable_mode(dev)) {
                return;
        }

        rtl8125_mdio_write(tp, 0x1f, 0x0000);
        rtl8125_mdio_write(tp, MII_ADVERTISE, rtl8125_mdio_read(tp, MII_ADVERTISE) &
                           ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
                             ADVERTISE_100HALF | ADVERTISE_100FULL));
        rtl8125_mdio_write(tp, MII_CTRL1000, rtl8125_mdio_read(tp, MII_CTRL1000) &
                           ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL));
        mdio_direct_write_phy_ocp(tp, 0xA5D4, mdio_direct_read_phy_ocp(tp, 0xA5D4) & ~(RTK_ADVERTISE_2500FULL));
        rtl8125_mdio_write(tp, MII_BMCR, BMCR_RESET | BMCR_ANENABLE);

        for (i = 0; i < 2500; i++) {
                val = rtl8125_mdio_read(tp, MII_BMCR) & BMCR_RESET;

                if (!val) {
                        return;
                }

                mdelay(1);
        }

        if (netif_msg_link(tp))
                printk(KERN_ERR "%s: PHY reset failed.\n", dev->name);
}

void rtl8125_init_ring_indexes(struct rtl8125_private *tp)
{
        tp->dirty_tx = 0;
        tp->dirty_rx = 0;
        tp->cur_tx = 0;
        tp->cur_rx = 0;
}

static void
rtl8125_issue_offset_99_event(struct rtl8125_private *tp)
{
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xE09A,  rtl8125_mac_ocp_read(tp, 0xE09A) | BIT_0);
                break;
        }
}

#ifdef ENABLE_DASH_SUPPORT
static void
NICChkTypeEnableDashInterrupt(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->DASH) {
                //
                // even disconnected, enable 3 dash interrupt mask bits for in-band/out-band communication
                //
                if (HW_DASH_SUPPORT_TYPE_2(tp) || HW_DASH_SUPPORT_TYPE_3(tp)) {
                        rtl8125_enable_dash2_interrupt(tp, ioaddr);
                        RTL_W16(IntrMask, (ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET));
                }
        }
}
#endif

static int rtl8125_enable_eee_plus(struct rtl8125_private *tp)
{
        int ret;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xE080, rtl8125_mac_ocp_read(tp, 0xE080)|BIT_1);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEEPlus\n");
                ret = -EOPNOTSUPP;
                break;
        }

        return ret;
}

static int rtl8125_disable_eee_plus(struct rtl8125_private *tp)
{
        int ret;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xE080, rtl8125_mac_ocp_read(tp, 0xE080)&~BIT_1);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEEPlus\n");
                ret = -EOPNOTSUPP;
                break;
        }

        return ret;
}

static void
rtl8125_check_link_status(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int link_status_on;

        link_status_on = tp->link_ok(dev);

        if (netif_carrier_ok(dev) != link_status_on) {
                if (link_status_on) {
                        rtl8125_hw_config(dev);

                        if ((tp->mcfg == CFG_METHOD_2) &&
                            netif_running(dev)) {
                                if (RTL_R16(PHYstatus)&FullDup)
                                        RTL_W32(TxConfig, (RTL_R32(TxConfig) | (BIT_24 | BIT_25)) & ~BIT_19);
                                else
                                        RTL_W32(TxConfig, (RTL_R32(TxConfig) | BIT_25) & ~(BIT_19 | BIT_24));
                        }

                        if ((tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3) &&
                            (RTL_R8(PHYstatus) & _10bps))
                                rtl8125_enable_eee_plus(tp);

                        rtl8125_hw_start(dev);

                        netif_carrier_on(dev);

                        netif_wake_queue(dev);

                        rtl8125_mdio_write(tp, 0x1F, 0x0000);
                        tp->phy_reg_anlpar = rtl8125_mdio_read(tp, MII_LPA);

                        if (netif_msg_ifup(tp))
                                printk(KERN_INFO PFX "%s: link up\n", dev->name);
                } else {
                        if (netif_msg_ifdown(tp))
                                printk(KERN_INFO PFX "%s: link down\n", dev->name);

                        tp->phy_reg_anlpar = 0;

                        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3)
                                rtl8125_disable_eee_plus(tp);

                        netif_stop_queue(dev);

                        netif_carrier_off(dev);

                        rtl8125_hw_reset(dev);

                        rtl8125_tx_clear(tp);

                        rtl8125_rx_clear(tp);

                        rtl8125_init_ring(dev);

                        rtl8125_set_speed(dev, tp->autoneg, tp->speed, tp->duplex, tp->advertising);

#ifdef ENABLE_DASH_SUPPORT
                        if (tp->DASH) {
                                NICChkTypeEnableDashInterrupt(tp);
                        }
#endif
                }
        }
}

static void
rtl8125_link_option(u8 *aut,
                    u32 *spd,
                    u8 *dup,
                    u32 *adv)
{
        if ((*spd != SPEED_2500) && (*spd != SPEED_1000) &&
            (*spd != SPEED_100) && (*spd != SPEED_10))
                *spd = SPEED_2500;

        if ((*dup != DUPLEX_FULL) && (*dup != DUPLEX_HALF))
                *dup = DUPLEX_FULL;

        if ((*aut != AUTONEG_ENABLE) && (*aut != AUTONEG_DISABLE))
                *aut = AUTONEG_ENABLE;

        *adv &= (ADVERTISED_10baseT_Half |
                 ADVERTISED_10baseT_Full |
                 ADVERTISED_100baseT_Half |
                 ADVERTISED_100baseT_Full |
                 ADVERTISED_1000baseT_Half |
                 ADVERTISED_1000baseT_Full |
                 ADVERTISED_2500baseX_Full);
        if (*adv == 0)
                *adv = (ADVERTISED_10baseT_Half |
                        ADVERTISED_10baseT_Full |
                        ADVERTISED_100baseT_Half |
                        ADVERTISED_100baseT_Full |
                        ADVERTISED_1000baseT_Half |
                        ADVERTISED_1000baseT_Full |
                        ADVERTISED_2500baseX_Full);
}

/*
static void
rtl8125_enable_ocp_phy_power_saving(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u16 val;

        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3) {
                val = mdio_direct_read_phy_ocp(tp, 0xC416);
                if (val != 0x0050) {
                        rtl8125_set_phy_mcu_patch_request(tp);
                        mdio_direct_write_phy_ocp(tp, 0xC416, 0x0000);
                        mdio_direct_write_phy_ocp(tp, 0xC416, 0x0050);
                        rtl8125_clear_phy_mcu_patch_request(tp);
                }
        }
}
*/

static void
rtl8125_disable_ocp_phy_power_saving(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u16 val;

        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3) {
                val = mdio_direct_read_phy_ocp(tp, 0xC416);
                if (val != 0x0500) {
                        rtl8125_set_phy_mcu_patch_request(tp);
                        mdio_direct_write_phy_ocp(tp, 0xC416, 0x0000);
                        mdio_direct_write_phy_ocp(tp, 0xC416, 0x0500);
                        rtl8125_clear_phy_mcu_patch_request(tp);
                }
        }
}

static void
rtl8125_wait_ll_share_fifo_ready(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;

        for (i = 0; i < 10; i++) {
                udelay(100);
                if (RTL_R16(0xD2) & BIT_9)
                        break;
        }
}

static void
rtl8125_disable_pci_offset_99(struct rtl8125_private *tp)
{
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xE032,  rtl8125_mac_ocp_read(tp, 0xE032) & ~(BIT_0 | BIT_1));
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_csi_fun0_write_byte(tp, 0x99, 0x00);
                break;
        }
}

static void
rtl8125_enable_pci_offset_99(struct rtl8125_private *tp)
{
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_csi_fun0_write_byte(tp, 0x99, tp->org_pci_offset_99);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE032);
                csi_tmp &= ~(BIT_0 | BIT_1);
                if (!(tp->org_pci_offset_99 & (BIT_5 | BIT_6)))
                        csi_tmp |= BIT_1;
                if (!(tp->org_pci_offset_99 & BIT_2))
                        csi_tmp |= BIT_0;
                rtl8125_mac_ocp_write(tp, 0xE032, csi_tmp);
                break;
        }
}

static void
rtl8125_init_pci_offset_99(struct rtl8125_private *tp)
{
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xCDD0, 0x9003);
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE034);
                csi_tmp |= (BIT_15|BIT_14);
                rtl8125_mac_ocp_write(tp, 0xE034, csi_tmp);
                rtl8125_mac_ocp_write(tp, 0xCDD8, 0x9003);
                rtl8125_mac_ocp_write(tp, 0xCDDA, 0x9003);
                rtl8125_mac_ocp_write(tp, 0xCDDC, 0x9003);
                rtl8125_mac_ocp_write(tp, 0xCDD2, 0x883C);
                rtl8125_mac_ocp_write(tp, 0xCDD4, 0x8C12);
                rtl8125_mac_ocp_write(tp, 0xCDD6, 0x9003);
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE032);
                csi_tmp |= (BIT_14);
                rtl8125_mac_ocp_write(tp, 0xE032, csi_tmp);
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE0A2);
                csi_tmp |= (BIT_0);
                rtl8125_mac_ocp_write(tp, 0xE0A2, csi_tmp);
                break;
        }

        rtl8125_enable_pci_offset_99(tp);
}

static void
rtl8125_disable_pci_offset_180(struct rtl8125_private *tp)
{
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE032);
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE092);
                csi_tmp &= 0xFF00;
                rtl8125_mac_ocp_write(tp, 0xE092, csi_tmp);
                break;
        }
}

static void
rtl8125_enable_pci_offset_180(struct rtl8125_private *tp)
{
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE094);
                csi_tmp &= 0x00FF;
                rtl8125_mac_ocp_write(tp, 0xE094, csi_tmp);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE032);
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xE092);
                csi_tmp &= 0xFF00;
                csi_tmp |= BIT_2;
                rtl8125_mac_ocp_write(tp, 0xE092, csi_tmp);
                break;
        }

        rtl8125_mac_ocp_write(tp, 0xE094, 0x0000);
}

static void
rtl8125_init_pci_offset_180(struct rtl8125_private *tp)
{
        if (tp->org_pci_offset_180 & (BIT_0|BIT_1))
                rtl8125_enable_pci_offset_180(tp);
        else
                rtl8125_disable_pci_offset_180(tp);
}

static void
rtl8125_set_pci_99_180_exit_driver_para(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_issue_offset_99_event(tp);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_disable_pci_offset_99(tp);
                break;
        }
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_disable_pci_offset_180(tp);
                break;
        }
}

static void
rtl8125_enable_cfg9346_write(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W8(Cfg9346, RTL_R8(Cfg9346) | Cfg9346_Unlock);
}

static void
rtl8125_disable_cfg9346_write(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W8(Cfg9346, RTL_R8(Cfg9346) & ~Cfg9346_Unlock);
}

static void
rtl8125_hw_d3_para(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W16(RxMaxSize, RX_BUF_SIZE);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                rtl8125_enable_cfg9346_write(tp);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                rtl8125_disable_cfg9346_write(tp);
                break;
        }

#ifdef ENABLE_REALWOW_SUPPORT
        rtl8125_set_realwow_d3_para(dev);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xEA18, 0x0064);
                break;
        }

        rtl8125_set_pci_99_180_exit_driver_para(dev);

        /*disable ocp phy power saving*/
        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3)
                rtl8125_disable_ocp_phy_power_saving(dev);

        rtl8125_disable_rxdvgate(dev);
}

static void
rtl8125_enable_magic_packet(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        switch (tp->HwSuppMagicPktVer) {
        case WAKEUP_MAGIC_PACKET_V3:
                rtl8125_mac_ocp_write(tp, 0xC0B6, rtl8125_mac_ocp_read(tp, 0xC0B6) | BIT_0);
                break;
        }
}
static void
rtl8125_disable_magic_packet(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        switch (tp->HwSuppMagicPktVer) {
        case WAKEUP_MAGIC_PACKET_V3:
                rtl8125_mac_ocp_write(tp, 0xC0B6, rtl8125_mac_ocp_read(tp, 0xC0B6) & ~BIT_0);
                break;
        }
}

#define WAKE_ANY (WAKE_PHY | WAKE_MAGIC | WAKE_UCAST | WAKE_BCAST | WAKE_MCAST)

static void
rtl8125_get_hw_wol(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 options;
        u32 csi_tmp;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        tp->wol_opts = 0;
        options = RTL_R8(Config1);
        if (!(options & PMEnable))
                goto out_unlock;

        options = RTL_R8(Config3);
        if (options & LinkUp)
                tp->wol_opts |= WAKE_PHY;

        switch (tp->HwSuppMagicPktVer) {
        case WAKEUP_MAGIC_PACKET_V3:
                csi_tmp = rtl8125_mac_ocp_read(tp, 0xC0B6);
                if (csi_tmp & BIT_0)
                        tp->wol_opts |= WAKE_MAGIC;
                break;
        }

        options = RTL_R8(Config5);
        if (options & UWF)
                tp->wol_opts |= WAKE_UCAST;
        if (options & BWF)
                tp->wol_opts |= WAKE_BCAST;
        if (options & MWF)
                tp->wol_opts |= WAKE_MCAST;

out_unlock:
        tp->wol_enabled = (tp->wol_opts) ? WOL_ENABLED : WOL_DISABLED;

        spin_unlock_irqrestore(&tp->lock, flags);
}

static void
rtl8125_set_hw_wol(struct net_device *dev, u32 wolopts)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i,tmp;
        static struct {
                u32 opt;
                u16 reg;
                u8  mask;
        } cfg[] = {
                { WAKE_PHY,   Config3, LinkUp },
                { WAKE_UCAST, Config5, UWF },
                { WAKE_BCAST, Config5, BWF },
                { WAKE_MCAST, Config5, MWF },
                { WAKE_ANY,   Config5, LanWake },
                { WAKE_MAGIC, Config3, MagicPacket },
        };

        switch (tp->HwSuppMagicPktVer) {
        case WAKEUP_MAGIC_PACKET_V3:
                tmp = ARRAY_SIZE(cfg) - 1;

                if (wolopts & WAKE_MAGIC)
                        rtl8125_enable_magic_packet(dev);
                else
                        rtl8125_disable_magic_packet(dev);
                break;
        }

        rtl8125_enable_cfg9346_write(tp);

        for (i = 0; i < tmp; i++) {
                u8 options = RTL_R8(cfg[i].reg) & ~cfg[i].mask;
                if (wolopts & cfg[i].opt)
                        options |= cfg[i].mask;
                RTL_W8(cfg[i].reg, options);
        }

        rtl8125_disable_cfg9346_write(tp);
}

static void
rtl8125_phy_restart_nway(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (rtl8125_is_in_phy_disable_mode(dev)) return;

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        rtl8125_mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_ANRESTART);
}

static void
rtl8125_phy_setup_force_mode(struct net_device *dev, u32 speed, u8 duplex)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u16 bmcr_true_force = 0;

        if (rtl8125_is_in_phy_disable_mode(dev)) return;

        if ((speed == SPEED_10) && (duplex == DUPLEX_HALF)) {
                bmcr_true_force = BMCR_SPEED10;
        } else if ((speed == SPEED_10) && (duplex == DUPLEX_FULL)) {
                bmcr_true_force = BMCR_SPEED10 | BMCR_FULLDPLX;
        } else if ((speed == SPEED_100) && (duplex == DUPLEX_HALF)) {
                bmcr_true_force = BMCR_SPEED100;
        } else if ((speed == SPEED_100) && (duplex == DUPLEX_FULL)) {
                bmcr_true_force = BMCR_SPEED100 | BMCR_FULLDPLX;
        } else if ((speed == SPEED_1000) && (duplex == DUPLEX_FULL) &&
                   tp->HwSuppGigaForceMode) {
                bmcr_true_force = BMCR_SPEED1000 | BMCR_FULLDPLX;
        } else {
                netif_err(tp, drv, dev, "Failed to set phy force mode!\n");
                return;
        }

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        rtl8125_mdio_write(tp, MII_BMCR, bmcr_true_force);
}

static void
rtl8125_powerdown_pll(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        if (tp->wol_enabled == WOL_ENABLED || tp->DASH || tp->EnableKCPOffload) {
                int auto_nego;
                int giga_ctrl;
                u16 anlpar;

                rtl8125_set_hw_wol(dev, tp->wol_opts);

                if (tp->mcfg == CFG_METHOD_2 ||
                    tp->mcfg == CFG_METHOD_3) {
                        rtl8125_enable_cfg9346_write(tp);
                        RTL_W8(Config2, RTL_R8(Config2) | PMSTS_En);
                        rtl8125_disable_cfg9346_write(tp);
                }

                rtl8125_mdio_write(tp, 0x1F, 0x0000);
                auto_nego = rtl8125_mdio_read(tp, MII_ADVERTISE);
                auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL
                               | ADVERTISE_100HALF | ADVERTISE_100FULL);

                if (netif_running(dev))
                        anlpar = tp->phy_reg_anlpar;
                else
                        anlpar = rtl8125_mdio_read(tp, MII_LPA);

#ifdef CONFIG_DOWN_SPEED_100
                auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);
#else
                if (anlpar & (LPA_10HALF | LPA_10FULL))
                        auto_nego |= (ADVERTISE_10HALF | ADVERTISE_10FULL);
                else
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);
#endif

                if (tp->DASH)
                        auto_nego |= (ADVERTISE_100FULL | ADVERTISE_100HALF | ADVERTISE_10HALF | ADVERTISE_10FULL);

                giga_ctrl = rtl8125_mdio_read(tp, MII_CTRL1000) & ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
                rtl8125_mdio_write(tp, MII_ADVERTISE, auto_nego);
                rtl8125_mdio_write(tp, MII_CTRL1000, giga_ctrl);
                if (tp->mcfg == CFG_METHOD_2 ||
                    tp->mcfg == CFG_METHOD_3) {
                        int ctrl_2500;

                        ctrl_2500 = mdio_direct_read_phy_ocp(tp, 0xA5D4);
                        ctrl_2500 &= ~(RTK_ADVERTISE_2500FULL);
                        mdio_direct_write_phy_ocp(tp, 0xA5D4, ctrl_2500);
                }
                rtl8125_phy_restart_nway(dev);

                RTL_W32(RxConfig, RTL_R32(RxConfig) | AcceptBroadcast | AcceptMulticast | AcceptMyPhys);

                return;
        }

        if (tp->DASH)
                return;

        rtl8125_phy_power_down(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(PMCH, RTL_R8(PMCH) & ~BIT_7);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(0xF2, RTL_R8(0xF2) & ~BIT_6);
                break;
        }
}

static void rtl8125_powerup_pll(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(PMCH, RTL_R8(PMCH) | BIT_7 | BIT_6);
                break;
        }

        rtl8125_phy_power_up(dev);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static void
rtl8125_get_wol(struct net_device *dev,
                struct ethtool_wolinfo *wol)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u8 options;
        unsigned long flags;

        wol->wolopts = 0;

        if (tp->mcfg == CFG_METHOD_DEFAULT) {
                wol->supported = 0;
                return;
        } else {
                wol->supported = WAKE_ANY;
        }

        spin_lock_irqsave(&tp->lock, flags);

        options = RTL_R8(Config1);
        if (!(options & PMEnable))
                goto out_unlock;

        wol->wolopts = tp->wol_opts;

out_unlock:
        spin_unlock_irqrestore(&tp->lock, flags);
}

static int
rtl8125_set_wol(struct net_device *dev,
                struct ethtool_wolinfo *wol)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

        tp->wol_opts = wol->wolopts;

        tp->wol_enabled = (tp->wol_opts) ? WOL_ENABLED : WOL_DISABLED;

        spin_unlock_irqrestore(&tp->lock, flags);

        device_set_wakeup_enable(&tp->pci_dev->dev, wol->wolopts);

        return 0;
}

static void
rtl8125_get_drvinfo(struct net_device *dev,
                    struct ethtool_drvinfo *info)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        strcpy(info->driver, MODULENAME);
        strcpy(info->version, RTL8125_VERSION);
        strcpy(info->bus_info, pci_name(tp->pci_dev));
        info->regdump_len = R8125_REGS_DUMP_SIZE;
        info->eedump_len = tp->eeprom_len;
}

static int
rtl8125_get_regs_len(struct net_device *dev)
{
        return R8125_REGS_DUMP_SIZE;
}
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)

static int
rtl8125_set_speed_xmii(struct net_device *dev,
                       u8 autoneg,
                       u32 speed,
                       u8 duplex,
                       u32 adv)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int auto_nego = 0;
        int giga_ctrl = 0;
        int ctrl_2500 = 0;
        int rc = -EINVAL;

        //Disable Giga Lite
        ClearEthPhyOcpBit(tp, 0xA428, BIT_9);
        ClearEthPhyOcpBit(tp, 0xA5EA, BIT_0);

        if (speed != SPEED_2500 &&
            (speed != SPEED_1000) &&
            (speed != SPEED_100) &&
            (speed != SPEED_10)) {
                speed = SPEED_2500;
                duplex = DUPLEX_FULL;
        }

        giga_ctrl = rtl8125_mdio_read(tp, MII_CTRL1000);
        giga_ctrl &= ~(ADVERTISE_1000HALF | ADVERTISE_1000FULL);
        ctrl_2500 = mdio_direct_read_phy_ocp(tp, 0xA5D4);
        ctrl_2500 &= ~(RTK_ADVERTISE_2500FULL);

        if (autoneg == AUTONEG_ENABLE) {
                /*n-way force*/
                auto_nego = rtl8125_mdio_read(tp, MII_ADVERTISE);
                auto_nego &= ~(ADVERTISE_10HALF | ADVERTISE_10FULL |
                               ADVERTISE_100HALF | ADVERTISE_100FULL |
                               ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

                if (adv & ADVERTISED_10baseT_Half)
                        auto_nego |= ADVERTISE_10HALF;
                if (adv & ADVERTISED_10baseT_Full)
                        auto_nego |= ADVERTISE_10FULL;
                if (adv & ADVERTISED_100baseT_Half)
                        auto_nego |= ADVERTISE_100HALF;
                if (adv & ADVERTISED_100baseT_Full)
                        auto_nego |= ADVERTISE_100FULL;
                if (adv & ADVERTISED_1000baseT_Half)
                        giga_ctrl |= ADVERTISE_1000HALF;
                if (adv & ADVERTISED_1000baseT_Full)
                        giga_ctrl |= ADVERTISE_1000FULL;
                if (adv & ADVERTISED_2500baseX_Full)
                        ctrl_2500 |= RTK_ADVERTISE_2500FULL;

                //flow control
                if (dev->mtu <= ETH_DATA_LEN)
                        auto_nego |= ADVERTISE_PAUSE_CAP|ADVERTISE_PAUSE_ASYM;

                tp->phy_auto_nego_reg = auto_nego;
                tp->phy_1000_ctrl_reg = giga_ctrl;

                tp->phy_2500_ctrl_reg = ctrl_2500;

                rtl8125_mdio_write(tp, 0x1f, 0x0000);
                rtl8125_mdio_write(tp, MII_ADVERTISE, auto_nego);
                rtl8125_mdio_write(tp, MII_CTRL1000, giga_ctrl);
                mdio_direct_write_phy_ocp(tp, 0xA5D4, ctrl_2500);
                rtl8125_phy_restart_nway(dev);
                mdelay(20);
        } else {
                /*true force*/
                if (speed == SPEED_10 || speed == SPEED_100 ||
                    (speed == SPEED_1000 && duplex == DUPLEX_FULL &&
                     tp->HwSuppGigaForceMode)) {
                        rtl8125_phy_setup_force_mode(dev, speed, duplex);
                } else
                        goto out;
        }

        tp->autoneg = autoneg;
        tp->speed = speed;
        tp->duplex = duplex;
        tp->advertising = adv;

        rc = 0;
out:
        return rc;
}

static int
rtl8125_set_speed(struct net_device *dev,
                  u8 autoneg,
                  u32 speed,
                  u8 duplex,
                  u32 adv)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int ret;

        ret = tp->set_speed(dev, autoneg, speed, duplex, adv);

        return ret;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static int
rtl8125_set_settings(struct net_device *dev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
                     struct ethtool_cmd *cmd
#else
                     const struct ethtool_link_ksettings *cmd
#endif
                    )
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int ret;
        unsigned long flags;
        u8 autoneg;
        u32 speed;
        u8 duplex;
        u32 supported, advertising;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
        autoneg = cmd->autoneg;
        speed = cmd->speed;
        duplex = cmd->duplex;
        supported = cmd->supported;
        advertising = cmd->advertising;
#else
        const struct ethtool_link_settings *base = &cmd->base;
        autoneg = base->autoneg;
        speed = base->speed;
        duplex = base->duplex;
        ethtool_convert_link_mode_to_legacy_u32(&supported,
                                                cmd->link_modes.supported);
        ethtool_convert_link_mode_to_legacy_u32(&advertising,
                                                cmd->link_modes.advertising);
#endif
        if (advertising & ~supported)
                return -EINVAL;

        spin_lock_irqsave(&tp->lock, flags);
        ret = rtl8125_set_speed(dev, autoneg, speed, duplex, advertising);
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
static u32
rtl8125_get_tx_csum(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u32 ret;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        ret = ((dev->features & NETIF_F_IP_CSUM) != 0);
#else
        ret = ((dev->features & (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)) != 0);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

static u32
rtl8125_get_rx_csum(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u32 ret;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        ret = tp->cp_cmd & RxChkSum;
        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

static int
rtl8125_set_tx_csum(struct net_device *dev,
                    u32 data)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        if (data)
                dev->features |= NETIF_F_IP_CSUM;
        else
                dev->features &= ~NETIF_F_IP_CSUM;
#else
        if (data)
                dev->features |= (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
        else
                dev->features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

static int
rtl8125_set_rx_csum(struct net_device *dev,
                    u32 data)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

        spin_lock_irqsave(&tp->lock, flags);

        if (data)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        RTL_W16(CPlusCmd, tp->cp_cmd);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)

#ifdef CONFIG_R8125_VLAN

static inline u32
rtl8125_tx_vlan_tag(struct rtl8125_private *tp,
                    struct sk_buff *skb)
{
        u32 tag;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        tag = (tp->vlgrp && vlan_tx_tag_present(skb)) ?
              TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
        tag = (vlan_tx_tag_present(skb)) ?
              TxVlanTag | swab16(vlan_tx_tag_get(skb)) : 0x00;
#else
        tag = (skb_vlan_tag_present(skb)) ?
              TxVlanTag | swab16(skb_vlan_tag_get(skb)) : 0x00;
#endif

        return tag;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)

static void
rtl8125_vlan_rx_register(struct net_device *dev,
                         struct vlan_group *grp)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        tp->vlgrp = grp;
        if (tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3) {
                if (tp->vlgrp) {
                        tp->rtl8125_rx_config |= (BIT_22 | BIT_23);
                        RTL_W32(RxConfig, RTL_R32(RxConfig) | (BIT_22 | BIT_23))
                } else {
                        tp->rtl8125_rx_config &= ~(BIT_22 | BIT_23);
                        RTL_W32(RxConfig, RTL_R32(RxConfig) & ~(BIT_22 | BIT_23))
                }
        }
        spin_unlock_irqrestore(&tp->lock, flags);
}

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
static void
rtl8125_vlan_rx_kill_vid(struct net_device *dev,
                         unsigned short vid)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        if (tp->vlgrp)
                tp->vlgrp->vlan_devices[vid] = NULL;
#else
        vlan_group_set_device(tp->vlgrp, vid, NULL);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
        spin_unlock_irqrestore(&tp->lock, flags);
}
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)

static int
rtl8125_rx_vlan_skb(struct rtl8125_private *tp,
                    struct RxDesc *desc,
                    struct sk_buff *skb)
{
        u32 opts2 = le32_to_cpu(desc->opts2);
        int ret = -1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        if (tp->vlgrp && (opts2 & RxVlanTag)) {
                rtl8125_rx_hwaccel_skb(skb, tp->vlgrp,
                                       swab16(opts2 & 0xffff));
                ret = 0;
        }
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, swab16(opts2 & 0xffff));
#else
        if (opts2 & RxVlanTag)
                __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), swab16(opts2 & 0xffff));
#endif

        desc->opts2 = 0;
        return ret;
}

#else /* !CONFIG_R8125_VLAN */

static inline u32
rtl8125_tx_vlan_tag(struct rtl8125_private *tp,
                    struct sk_buff *skb)
{
        return 0;
}

static int
rtl8125_rx_vlan_skb(struct rtl8125_private *tp,
                    struct RxDesc *desc,
                    struct sk_buff *skb)
{
        return -1;
}

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)

static netdev_features_t rtl8125_fix_features(struct net_device *dev,
                netdev_features_t features)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        if (dev->mtu > MSS_MAX)
                features &= ~NETIF_F_ALL_TSO;
        if (dev->mtu > ETH_DATA_LEN) {
                features &= ~NETIF_F_ALL_TSO;
                features &= ~NETIF_F_ALL_CSUM;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        return features;
}

static int rtl8125_hw_set_features(struct net_device *dev,
                                   netdev_features_t features)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 rx_config;

        rx_config = RTL_R32(RxConfig);
        if (features & NETIF_F_RXALL)
                rx_config |= (AcceptErr | AcceptRunt);
        else
                rx_config &= ~(AcceptErr | AcceptRunt);

        if (dev->features & NETIF_F_HW_VLAN_RX)
                rx_config |= (BIT_22 | BIT_23);
        else
                rx_config &= ~(BIT_22 | BIT_23);

        RTL_W32(RxConfig, rx_config);

        if (features & NETIF_F_RXCSUM)
                tp->cp_cmd |= RxChkSum;
        else
                tp->cp_cmd &= ~RxChkSum;

        RTL_W16(CPlusCmd, tp->cp_cmd);
        RTL_R16(CPlusCmd);

        return 0;
}

static int rtl8125_set_features(struct net_device *dev,
                                netdev_features_t features)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        features &= NETIF_F_RXALL | NETIF_F_RXCSUM | NETIF_F_HW_VLAN_RX;

        spin_lock_irqsave(&tp->lock, flags);
        if (features ^ dev->features)
                rtl8125_hw_set_features(dev, features);
        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

#endif

static void rtl8125_gset_xmii(struct net_device *dev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
                              struct ethtool_cmd *cmd
#else
                              struct ethtool_link_ksettings *cmd
#endif
                             )
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u16 status;
        u8 autoneg, duplex;
        u32 speed = 0;
        u16 bmcr;
        u32 supported, advertising;
        unsigned long flags;

        supported = SUPPORTED_10baseT_Half |
                    SUPPORTED_10baseT_Full |
                    SUPPORTED_100baseT_Half |
                    SUPPORTED_100baseT_Full |
                    SUPPORTED_1000baseT_Full |
                    SUPPORTED_2500baseX_Full |
                    SUPPORTED_Autoneg |
                    SUPPORTED_TP |
                    SUPPORTED_Pause	|
                    SUPPORTED_Asym_Pause;

        advertising = ADVERTISED_TP;

        spin_lock_irqsave(&tp->lock, flags);
        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        bmcr = rtl8125_mdio_read(tp, MII_BMCR);
        spin_unlock_irqrestore(&tp->lock, flags);

        if (bmcr & BMCR_ANENABLE) {
                advertising |= ADVERTISED_Autoneg;
                autoneg = AUTONEG_ENABLE;

                if (tp->phy_auto_nego_reg & ADVERTISE_10HALF)
                        advertising |= ADVERTISED_10baseT_Half;
                if (tp->phy_auto_nego_reg & ADVERTISE_10FULL)
                        advertising |= ADVERTISED_10baseT_Full;
                if (tp->phy_auto_nego_reg & ADVERTISE_100HALF)
                        advertising |= ADVERTISED_100baseT_Half;
                if (tp->phy_auto_nego_reg & ADVERTISE_100FULL)
                        advertising |= ADVERTISED_100baseT_Full;
                if (tp->phy_1000_ctrl_reg & ADVERTISE_1000FULL)
                        advertising |= ADVERTISED_1000baseT_Full;
                if (tp->phy_2500_ctrl_reg & RTK_ADVERTISE_2500FULL)
                        advertising |= ADVERTISED_2500baseX_Full;
        } else {
                autoneg = AUTONEG_DISABLE;
        }

        status = RTL_R16(PHYstatus);

        if (status & LinkStatus) {
                /*link on*/
                if (status & _2500bpsF)
                        speed = SPEED_2500;
                else if (status & _1000bpsF)
                        speed = SPEED_1000;
                else if (status & _100bps)
                        speed = SPEED_100;
                else if (status & _10bps)
                        speed = SPEED_10;

                if (status & TxFlowCtrl)
                        advertising |= ADVERTISED_Asym_Pause;

                if (status & RxFlowCtrl)
                        advertising |= ADVERTISED_Pause;

                duplex = ((status & (_1000bpsF | _2500bpsF)) || (status & FullDup)) ?
                         DUPLEX_FULL : DUPLEX_HALF;
        } else {
                /*link down*/
                speed = SPEED_UNKNOWN;
                duplex = DUPLEX_UNKNOWN;
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
        cmd->supported = supported;
        cmd->advertising = advertising;
        cmd->autoneg = autoneg;
        cmd->speed = speed;
        cmd->duplex = duplex;
        cmd->port = PORT_TP;
#else
        ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.supported,
                                                supported);
        ethtool_convert_legacy_u32_to_link_mode(cmd->link_modes.advertising,
                                                advertising);
        cmd->base.autoneg = autoneg;
        cmd->base.speed = speed;
        cmd->base.duplex = duplex;
        cmd->base.port = PORT_TP;
#endif
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static int
rtl8125_get_settings(struct net_device *dev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
                     struct ethtool_cmd *cmd
#else
                     struct ethtool_link_ksettings *cmd
#endif
                    )
{
        struct rtl8125_private *tp = netdev_priv(dev);

        tp->get_settings(dev, cmd);

        return 0;
}

static void rtl8125_get_regs(struct net_device *dev, struct ethtool_regs *regs,
                             void *p)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        unsigned int i;
        u8 *data = p;
        unsigned long flags;

        if (regs->len < R8125_REGS_DUMP_SIZE)
                return /* -EINVAL */;

        memset(p, 0, regs->len);

        spin_lock_irqsave(&tp->lock, flags);
        for (i = 0; i < R8125_MAC_REGS_SIZE; i++)
                *data++ = readb(ioaddr + i);
        data = (u8*)p + 256;

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        for (i = 0; i < R8125_PHY_REGS_SIZE/2; i++) {
                *(u16*)data = rtl8125_mdio_read(tp, i);
                data += 2;
        }
        data = (u8*)p + 256 * 2;

        for (i = 0; i < R8125_EPHY_REGS_SIZE/2; i++) {
                *(u16*)data = rtl8125_ephy_read(ioaddr, i);
                data += 2;
        }
        data = (u8*)p + 256 * 3;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                for (i = 0; i < R8125_ERI_REGS_SIZE; i+=4) {
                        *(u32*)data = rtl8125_eri_read(ioaddr, i , 4, ERIAR_ExGMAC);
                        data += 4;
                }
                break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);
}

static u32
rtl8125_get_msglevel(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        return tp->msg_enable;
}

static void
rtl8125_set_msglevel(struct net_device *dev,
                     u32 value)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        tp->msg_enable = value;
}

static const char rtl8125_gstrings[][ETH_GSTRING_LEN] = {
        "tx_packets",
        "rx_packets",
        "tx_errors",
        "rx_errors",
        "rx_missed",
        "align_errors",
        "tx_single_collisions",
        "tx_multi_collisions",
        "unicast",
        "broadcast",
        "multicast",
        "tx_aborted",
        "tx_underrun",
};
#endif //#LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static int rtl8125_get_stats_count(struct net_device *dev)
{
        return ARRAY_SIZE(rtl8125_gstrings);
}
#endif //#LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
#else
static int rtl8125_get_sset_count(struct net_device *dev, int sset)
{
        switch (sset) {
        case ETH_SS_STATS:
                return ARRAY_SIZE(rtl8125_gstrings);
        default:
                return -EOPNOTSUPP;
        }
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static void
rtl8125_get_ethtool_stats(struct net_device *dev,
                          struct ethtool_stats *stats,
                          u64 *data)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct rtl8125_counters *counters;
        dma_addr_t paddr;
        u32 cmd;
        u32 WaitCnt;
        unsigned long flags;

        ASSERT_RTNL();

        counters = tp->tally_vaddr;
        paddr = tp->tally_paddr;
        if (!counters)
                return;

        spin_lock_irqsave(&tp->lock, flags);
        RTL_W32(CounterAddrHigh, (u64)paddr >> 32);
        cmd = (u64)paddr & DMA_BIT_MASK(32);
        RTL_W32(CounterAddrLow, cmd);
        RTL_W32(CounterAddrLow, cmd | CounterDump);

        WaitCnt = 0;
        while (RTL_R32(CounterAddrLow) & CounterDump) {
                udelay(10);

                WaitCnt++;
                if (WaitCnt > 20)
                        break;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        data[0] = le64_to_cpu(counters->tx_packets);
        data[1] = le64_to_cpu(counters->rx_packets);
        data[2] = le64_to_cpu(counters->tx_errors);
        data[3] = le32_to_cpu(counters->rx_errors);
        data[4] = le16_to_cpu(counters->rx_missed);
        data[5] = le16_to_cpu(counters->align_errors);
        data[6] = le32_to_cpu(counters->tx_one_collision);
        data[7] = le32_to_cpu(counters->tx_multi_collision);
        data[8] = le64_to_cpu(counters->rx_unicast);
        data[9] = le64_to_cpu(counters->rx_broadcast);
        data[10] = le32_to_cpu(counters->rx_multicast);
        data[11] = le16_to_cpu(counters->tx_aborted);
        data[12] = le16_to_cpu(counters->tx_underun);
}

static void
rtl8125_get_strings(struct net_device *dev,
                    u32 stringset,
                    u8 *data)
{
        switch (stringset) {
        case ETH_SS_STATS:
                memcpy(data, *rtl8125_gstrings, sizeof(rtl8125_gstrings));
                break;
        }
}
#endif //#LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)

static int rtl_get_eeprom_len(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        return tp->eeprom_len;
}

static int rtl_get_eeprom(struct net_device *dev, struct ethtool_eeprom *eeprom, u8 *buf)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int i,j,ret;
        int start_w, end_w;
        int VPD_addr, VPD_data;
        u32 *eeprom_buff;
        u16 tmp;

        if (tp->eeprom_type == EEPROM_TYPE_NONE) {
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Detect none EEPROM\n");
                return -EOPNOTSUPP;
        } else if (eeprom->len == 0 || (eeprom->offset+eeprom->len) > tp->eeprom_len) {
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Invalid parameter\n");
                return -EINVAL;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                VPD_addr = 0xD2;
                VPD_data = 0xD4;
                break;
        }

        start_w = eeprom->offset >> 2;
        end_w = (eeprom->offset + eeprom->len - 1) >> 2;

        eeprom_buff = kmalloc(sizeof(u32)*(end_w - start_w + 1), GFP_KERNEL);
        if (!eeprom_buff)
                return -ENOMEM;

        rtl8125_enable_cfg9346_write(tp);
        ret = -EFAULT;
        for (i=start_w; i<=end_w; i++) {
                pci_write_config_word(tp->pci_dev, VPD_addr, (u16)i*4);
                ret = -EFAULT;
                for (j = 0; j < 10; j++) {
                        udelay(400);
                        pci_read_config_word(tp->pci_dev, VPD_addr, &tmp);
                        if (tmp&0x8000) {
                                ret = 0;
                                break;
                        }
                }

                if (ret)
                        break;

                pci_read_config_dword(tp->pci_dev, VPD_data, &eeprom_buff[i-start_w]);
        }
        rtl8125_disable_cfg9346_write(tp);

        if (!ret)
                memcpy(buf, (u8 *)eeprom_buff + (eeprom->offset & 3), eeprom->len);

        kfree(eeprom_buff);

        return ret;
}

#undef ethtool_op_get_link
#define ethtool_op_get_link _kc_ethtool_op_get_link
static u32 _kc_ethtool_op_get_link(struct net_device *dev)
{
        return netif_carrier_ok(dev) ? 1 : 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
#undef ethtool_op_get_sg
#define ethtool_op_get_sg _kc_ethtool_op_get_sg
static u32 _kc_ethtool_op_get_sg(struct net_device *dev)
{
#ifdef NETIF_F_SG
        return (dev->features & NETIF_F_SG) != 0;
#else
        return 0;
#endif
}

#undef ethtool_op_set_sg
#define ethtool_op_set_sg _kc_ethtool_op_set_sg
static int _kc_ethtool_op_set_sg(struct net_device *dev, u32 data)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->mcfg == CFG_METHOD_DEFAULT)
                return -EOPNOTSUPP;

#ifdef NETIF_F_SG
        if (data)
                dev->features |= NETIF_F_SG;
        else
                dev->features &= ~NETIF_F_SG;
#endif

        return 0;
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
static int
rtl_ethtool_get_eee(struct net_device *net, struct ethtool_eee *eee)
{
        struct rtl8125_private *tp = netdev_priv(net);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 lp, adv, supported = 0;
        unsigned long flags;
        u16 val;

        switch (tp->mcfg) {
        default:
                return -EOPNOTSUPP;
        }

        if (unlikely(tp->rtk_enable_diag))
                return -EBUSY;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_mdio_write(tp, 0x1F, 0x0A5C);
        val = rtl8125_mdio_read(tp, 0x12);
        supported = mmd_eee_cap_to_ethtool_sup_t(val);

        rtl8125_mdio_write(tp, 0x1F, 0x0A5D);
        val = rtl8125_mdio_read(tp, 0x10);
        adv = mmd_eee_adv_to_ethtool_adv_t(val);

        val = rtl8125_mdio_read(tp, 0x11);
        lp = mmd_eee_adv_to_ethtool_adv_t(val);

        val = rtl8125_eri_read(ioaddr, 0x1B0, 2, ERIAR_ExGMAC);
        val &= BIT_1 | BIT_0;

        rtl8125_mdio_write(tp, 0x1F, 0x0000);

        spin_unlock_irqrestore(&tp->lock, flags);

        eee->eee_enabled = !!val;
        eee->eee_active = !!(supported & adv & lp);
        eee->supported = supported;
        eee->advertised = adv;
        eee->lp_advertised = lp;

        return 0;
}

static int
rtl_ethtool_set_eee(struct net_device *net, struct ethtool_eee *eee)
{
        struct rtl8125_private *tp = netdev_priv(net);
        unsigned long flags;
        u32 data;

        switch (tp->mcfg) {
        default:
                return -EOPNOTSUPP;
        }

        if (unlikely(tp->rtk_enable_diag))
                return -EBUSY;

        spin_lock_irqsave(&tp->lock, flags);

        if (eee->eee_enabled) {
                data = rtl8125_mac_ocp_read(tp, 0xE040);
                data &= ~(BIT_1 | BIT_0);
                rtl8125_mac_ocp_write(tp, 0xE040, data);
                ClearEthPhyOcpBit(tp, 0xA432, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA5D0, (BIT_2 | BIT_1));
                ClearEthPhyOcpBit(tp, 0xA428, BIT_7);
                ClearEthPhyOcpBit(tp, 0xA6D4, BIT_0);

                ClearEthPhyOcpBit(tp, 0xA6D8, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA428, BIT_7);
                ClearEthPhyOcpBit(tp, 0xA4A2, BIT_9);
        } else {
                data = rtl8125_mac_ocp_read(tp, 0xE040);
                data &= ~(BIT_1 | BIT_0);
                rtl8125_mac_ocp_write(tp, 0xE040, data);
                ClearEthPhyOcpBit(tp, 0xA432, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA5D0, (BIT_2 | BIT_1));
                ClearEthPhyOcpBit(tp, 0xA428, BIT_7);
                ClearEthPhyOcpBit(tp, 0xA6D4, BIT_0);

                ClearEthPhyOcpBit(tp, 0xA6D8, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA428, BIT_7);
                ClearEthPhyOcpBit(tp, 0xA4A2, BIT_9);
        }

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        data = rtl8125_mdio_read(tp, MII_BMCR);
        data |= BMCR_RESET;
        rtl8125_mdio_write(tp, MII_BMCR, data);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0) */

static int rtl_nway_reset(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;
        int ret, bmcr;

        if (unlikely(tp->rtk_enable_diag))
                return -EBUSY;

        spin_lock_irqsave(&tp->lock, flags);

        /* if autoneg is off, it's an error */
        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        bmcr = rtl8125_mdio_read(tp, MII_BMCR);

        if (bmcr & BMCR_ANENABLE) {
                bmcr |= BMCR_ANRESTART;
                rtl8125_mdio_write(tp, MII_BMCR, bmcr);
                ret = 0;
        } else {
                ret = -EINVAL;
        }

        spin_unlock_irqrestore(&tp->lock, flags);

        return ret;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
static const struct ethtool_ops rtl8125_ethtool_ops = {
        .get_drvinfo        = rtl8125_get_drvinfo,
        .get_regs_len       = rtl8125_get_regs_len,
        .get_link       = ethtool_op_get_link,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,6,0)
        .get_settings       = rtl8125_get_settings,
        .set_settings       = rtl8125_set_settings,
#else
        .get_link_ksettings       = rtl8125_get_settings,
        .set_link_ksettings       = rtl8125_set_settings,
#endif
        .get_msglevel       = rtl8125_get_msglevel,
        .set_msglevel       = rtl8125_set_msglevel,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,3,0)
        .get_rx_csum        = rtl8125_get_rx_csum,
        .set_rx_csum        = rtl8125_set_rx_csum,
        .get_tx_csum        = rtl8125_get_tx_csum,
        .set_tx_csum        = rtl8125_set_tx_csum,
        .get_sg         = ethtool_op_get_sg,
        .set_sg         = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
        .get_tso        = ethtool_op_get_tso,
        .set_tso        = ethtool_op_set_tso,
#endif
#endif
        .get_regs       = rtl8125_get_regs,
        .get_wol        = rtl8125_get_wol,
        .set_wol        = rtl8125_set_wol,
        .get_strings        = rtl8125_get_strings,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,33)
        .get_stats_count    = rtl8125_get_stats_count,
#else
        .get_sset_count     = rtl8125_get_sset_count,
#endif
        .get_ethtool_stats  = rtl8125_get_ethtool_stats,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#ifdef ETHTOOL_GPERMADDR
        .get_perm_addr      = ethtool_op_get_perm_addr,
#endif
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
        .get_eeprom     = rtl_get_eeprom,
        .get_eeprom_len     = rtl_get_eeprom_len,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
        .get_ts_info        = ethtool_op_get_ts_info,
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
        .get_eee = rtl_ethtool_get_eee,
        .set_eee = rtl_ethtool_set_eee,
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0) */
        .nway_reset = rtl_nway_reset,
};
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)

static int rtl8125_enable_eee(struct rtl8125_private *tp)
{
        int ret;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                SetMcuAccessRegBit(tp, 0xE040, (BIT_1|BIT_0));
                SetMcuAccessRegBit(tp, 0xEB62, (BIT_2|BIT_1));

                SetEthPhyOcpBit(tp, 0xA432, BIT_4);
                SetEthPhyOcpBit(tp, 0xA5D0, (BIT_2 | BIT_1));
                ClearEthPhyOcpBit(tp, 0xA6D4, BIT_0);

                ClearEthPhyOcpBit(tp, 0xA6D8, BIT_4);
                SetEthPhyOcpBit(tp, 0xA4A2, BIT_9);
                SetEthPhyOcpBit(tp, 0xA428, BIT_7);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEE\n");
                ret = -EOPNOTSUPP;
                break;
        }

        /*Advanced EEE*/
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_set_phy_mcu_patch_request(tp);
                SetMcuAccessRegBit(tp, 0xE052, BIT_0);
                ClearEthPhyOcpBit(tp, 0xA442, BIT_12 | BIT_13);
                ClearEthPhyOcpBit(tp, 0xA430, BIT_15);
                rtl8125_clear_phy_mcu_patch_request(tp);
                break;
        }

        return ret;
}

static int rtl8125_disable_eee(struct rtl8125_private *tp)
{
        int ret;

        ret = 0;
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                ClearMcuAccessRegBit(tp, 0xE040, (BIT_1|BIT_0));
                ClearMcuAccessRegBit(tp, 0xEB62, (BIT_2|BIT_1));

                ClearEthPhyOcpBit(tp, 0xA432, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA5D0, (BIT_2 | BIT_1));
                ClearEthPhyOcpBit(tp, 0xA6D4, BIT_0);

                ClearEthPhyOcpBit(tp, 0xA6D8, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA428, BIT_7);
                ClearEthPhyOcpBit(tp, 0xA4A2, BIT_9);
                break;

        default:
//      dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support EEE\n");
                ret = -EOPNOTSUPP;
                break;
        }

        /*Advanced EEE*/
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_set_phy_mcu_patch_request(tp);
                ClearMcuAccessRegBit(tp, 0xE052, BIT_0);
                ClearEthPhyOcpBit(tp, 0xA442, BIT_12 | BIT_13);
                ClearEthPhyOcpBit(tp, 0xA430, BIT_15);
                rtl8125_clear_phy_mcu_patch_request(tp);
                break;
        }

        return ret;
}

#if 0

static int rtl8125_enable_green_feature(struct rtl8125_private *tp)
{
        u16 gphy_val;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8011);
                SetEthPhyOcpBit(tp, 0xA438, BIT_15);
                rtl8125_mdio_write(tp, 0x00, 0x9200);
                break;
        default:
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support Green Feature\n");
                break;
        }

        return 0;
}

static int rtl8125_disable_green_feature(struct rtl8125_private *tp)
{
        u16 gphy_val;
        unsigned long flags;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8011);
                ClearEthPhyOcpBit(tp, 0xA438, BIT_15);
                rtl8125_mdio_write(tp, 0x00, 0x9200);
                break;
        default:
                dev_printk(KERN_DEBUG, &tp->pci_dev->dev, "Not Support Green Feature\n");
                break;
        }

        return 0;
}

#endif

static void rtl8125_get_mac_version(struct rtl8125_private *tp, void __iomem *ioaddr)
{
        u32 reg,val32;
        u32 ICVerID;

        val32 = RTL_R32(TxConfig);
        reg = val32 & 0x7c800000;
        ICVerID = val32 & 0x00700000;

        switch (reg) {
        case 0x60800000:
                if (ICVerID == 0x00000000) {
                        tp->mcfg = CFG_METHOD_2;
                } else if (ICVerID == 0x100000) {
                        tp->mcfg = CFG_METHOD_3;
                } else {
                        tp->mcfg = CFG_METHOD_3;
                        tp->HwIcVerUnknown = TRUE;
                }

                tp->efuse_ver = EFUSE_SUPPORT_V4;
                break;
        default:
                printk("unknown chip version (%x)\n",reg);
                tp->mcfg = CFG_METHOD_DEFAULT;
                tp->HwIcVerUnknown = TRUE;
                tp->efuse_ver = EFUSE_NOT_SUPPORT;
                break;
        }
}

static void
rtl8125_print_mac_version(struct rtl8125_private *tp)
{
        int i;
        for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
                if (tp->mcfg == rtl_chip_info[i].mcfg) {
                        dprintk("Realtek PCIe 2.5GbE Family Controller mcfg = %04d\n",
                                rtl_chip_info[i].mcfg);
                        return;
                }
        }

        dprintk("mac_version == Unknown\n");
}

static void
rtl8125_tally_counter_addr_fill(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->tally_paddr)
                return;

        RTL_W32(CounterAddrHigh, (u64)tp->tally_paddr >> 32);
        RTL_W32(CounterAddrLow, (u64)tp->tally_paddr & (DMA_BIT_MASK(32)));
}

static void
rtl8125_tally_counter_clear(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->tally_paddr)
                return;

        RTL_W32(CounterAddrHigh, (u64)tp->tally_paddr >> 32);
        RTL_W32(CounterAddrLow, ((u64)tp->tally_paddr & (DMA_BIT_MASK(32))) | CounterReset);
}

static void
rtl8125_clear_phy_ups_reg(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        ClearEthPhyOcpBit(tp, 0xA468, BIT_3 | BIT_1);
}

static int
rtl8125_is_ups_resume(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3)
                return (rtl8125_mac_ocp_read(tp, 0xD42C) & BIT_8);

        return 0;
}

static void
rtl8125_clear_ups_resume_bit(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3)
                rtl8125_mac_ocp_write(tp, 0xD408, rtl8125_mac_ocp_read(tp, 0xD408) & ~(BIT_8));
}

static void
rtl8125_wait_phy_ups_resume(struct net_device *dev, u16 PhyState)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u16 TmpPhyState;
        int i=0;

        if (tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3) {
                do {
                        TmpPhyState = mdio_direct_read_phy_ocp(tp, 0xA420);
                        TmpPhyState &= 0x7;
                        mdelay(1);
                        i++;
                } while ((i < 100) && (TmpPhyState != PhyState));
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18)
        WARN_ON_ONCE(i == 100);
#endif
}

void
rtl8125_enable_now_is_oob(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if ( tp->HwSuppNowIsOobVer == 1 ) {
                RTL_W8(MCUCmd_reg, RTL_R8(MCUCmd_reg) | Now_is_oob);
        }
}

void
rtl8125_disable_now_is_oob(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if ( tp->HwSuppNowIsOobVer == 1 ) {
                RTL_W8(MCUCmd_reg, RTL_R8(MCUCmd_reg) & ~Now_is_oob);
        }
}

static void
rtl8125_exit_oob(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u16 data16;

        RTL_W32(RxConfig, RTL_R32(RxConfig) & ~(AcceptErr | AcceptRunt | AcceptBroadcast | AcceptMulticast | AcceptMyPhys |  AcceptAllPhys));

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_dash2_disable_txrx(dev);
                break;
        }

        if (tp->DASH) {
                rtl8125_driver_stop(tp);
                rtl8125_driver_start(tp);
#ifdef ENABLE_DASH_SUPPORT
                DashHwInit(dev);
#endif
        }

#ifdef ENABLE_REALWOW_SUPPORT
        rtl8125_realwow_hw_init(dev);
#else
        //Disable realwow  function
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xC0BC, 0x00FF);
                break;
        }
#endif //ENABLE_REALWOW_SUPPORT

        rtl8125_nic_reset(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_disable_now_is_oob(tp);

                data16 = rtl8125_mac_ocp_read(tp, 0xE8DE) & ~BIT_14;
                rtl8125_mac_ocp_write(tp, 0xE8DE, data16);
                rtl8125_wait_ll_share_fifo_ready(dev);

                rtl8125_mac_ocp_write(tp, 0xC0AA, 0x07D0);
                rtl8125_mac_ocp_write(tp, 0xC0A6, 0x0150);
                rtl8125_mac_ocp_write(tp, 0xC01E, 0x5555);

                rtl8125_wait_ll_share_fifo_ready(dev);
                break;
        }

        //wait ups resume (phy state 2)
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                if (rtl8125_is_ups_resume(dev)) {
                        rtl8125_wait_phy_ups_resume(dev, 2);
                        rtl8125_clear_ups_resume_bit(dev);
                        rtl8125_clear_phy_ups_reg(dev);
                }
                break;
        };

        tp->phy_reg_anlpar = 0;
}

void
rtl8125_hw_disable_mac_mcu_bps(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_enable_cfg9346_write(tp);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                rtl8125_disable_cfg9346_write(tp);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xFC38, 0x0000);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xFC28, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC2A, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC2C, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC2E, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC30, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC32, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC34, 0x0000);
                rtl8125_mac_ocp_write(tp, 0xFC36, 0x0000);
                mdelay(3);
                rtl8125_mac_ocp_write(tp, 0xFC26, 0x0000);
                break;
        }
}

static void
rtl8125_set_mac_mcu_8125a_2(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_hw_disable_mac_mcu_bps(dev);

        rtl8125_mac_ocp_write(tp, 0xF800, 0xE008);
        rtl8125_mac_ocp_write(tp, 0xF802, 0xE01E);
        rtl8125_mac_ocp_write(tp, 0xF804, 0xE02E);
        rtl8125_mac_ocp_write(tp, 0xF806, 0xE054);
        rtl8125_mac_ocp_write(tp, 0xF808, 0xE057);
        rtl8125_mac_ocp_write(tp, 0xF80A, 0xE059);
        rtl8125_mac_ocp_write(tp, 0xF80C, 0xE05B);
        rtl8125_mac_ocp_write(tp, 0xF80E, 0xE05D);
        rtl8125_mac_ocp_write(tp, 0xF810, 0x9996);
        rtl8125_mac_ocp_write(tp, 0xF812, 0x49D1);
        rtl8125_mac_ocp_write(tp, 0xF814, 0xF005);
        rtl8125_mac_ocp_write(tp, 0xF816, 0x49D4);
        rtl8125_mac_ocp_write(tp, 0xF818, 0xF10A);
        rtl8125_mac_ocp_write(tp, 0xF81A, 0x49D8);
        rtl8125_mac_ocp_write(tp, 0xF81C, 0xF108);
        rtl8125_mac_ocp_write(tp, 0xF81E, 0xC00F);
        rtl8125_mac_ocp_write(tp, 0xF820, 0x7100);
        rtl8125_mac_ocp_write(tp, 0xF822, 0x209C);
        rtl8125_mac_ocp_write(tp, 0xF824, 0x249C);
        rtl8125_mac_ocp_write(tp, 0xF826, 0xC009);
        rtl8125_mac_ocp_write(tp, 0xF828, 0x9900);
        rtl8125_mac_ocp_write(tp, 0xF82A, 0xE004);
        rtl8125_mac_ocp_write(tp, 0xF82C, 0xC006);
        rtl8125_mac_ocp_write(tp, 0xF82E, 0x1900);
        rtl8125_mac_ocp_write(tp, 0xF830, 0x9900);
        rtl8125_mac_ocp_write(tp, 0xF832, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF834, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF836, 0x5A48);
        rtl8125_mac_ocp_write(tp, 0xF838, 0xE0C2);
        rtl8125_mac_ocp_write(tp, 0xF83A, 0x0004);
        rtl8125_mac_ocp_write(tp, 0xF83C, 0xE10A);
        rtl8125_mac_ocp_write(tp, 0xF83E, 0xC60F);
        rtl8125_mac_ocp_write(tp, 0xF840, 0x73C4);
        rtl8125_mac_ocp_write(tp, 0xF842, 0x49B3);
        rtl8125_mac_ocp_write(tp, 0xF844, 0xF106);
        rtl8125_mac_ocp_write(tp, 0xF846, 0x73C2);
        rtl8125_mac_ocp_write(tp, 0xF848, 0xC608);
        rtl8125_mac_ocp_write(tp, 0xF84A, 0xB406);
        rtl8125_mac_ocp_write(tp, 0xF84C, 0xC609);
        rtl8125_mac_ocp_write(tp, 0xF84E, 0xFF80);
        rtl8125_mac_ocp_write(tp, 0xF850, 0xC605);
        rtl8125_mac_ocp_write(tp, 0xF852, 0xB406);
        rtl8125_mac_ocp_write(tp, 0xF854, 0xC605);
        rtl8125_mac_ocp_write(tp, 0xF856, 0xFF80);
        rtl8125_mac_ocp_write(tp, 0xF858, 0x0544);
        rtl8125_mac_ocp_write(tp, 0xF85A, 0x0568);
        rtl8125_mac_ocp_write(tp, 0xF85C, 0xE906);
        rtl8125_mac_ocp_write(tp, 0xF85E, 0xCDE8);
        rtl8125_mac_ocp_write(tp, 0xF860, 0xC724);
        rtl8125_mac_ocp_write(tp, 0xF862, 0xC624);
        rtl8125_mac_ocp_write(tp, 0xF864, 0x9EE2);
        rtl8125_mac_ocp_write(tp, 0xF866, 0x1E01);
        rtl8125_mac_ocp_write(tp, 0xF868, 0x9EE0);
        rtl8125_mac_ocp_write(tp, 0xF86A, 0x76E0);
        rtl8125_mac_ocp_write(tp, 0xF86C, 0x49E0);
        rtl8125_mac_ocp_write(tp, 0xF86E, 0xF1FE);
        rtl8125_mac_ocp_write(tp, 0xF870, 0x76E6);
        rtl8125_mac_ocp_write(tp, 0xF872, 0x486D);
        rtl8125_mac_ocp_write(tp, 0xF874, 0x4868);
        rtl8125_mac_ocp_write(tp, 0xF876, 0x9EE4);
        rtl8125_mac_ocp_write(tp, 0xF878, 0x1E03);
        rtl8125_mac_ocp_write(tp, 0xF87A, 0x9EE0);
        rtl8125_mac_ocp_write(tp, 0xF87C, 0x76E0);
        rtl8125_mac_ocp_write(tp, 0xF87E, 0x49E0);
        rtl8125_mac_ocp_write(tp, 0xF880, 0xF1FE);
        rtl8125_mac_ocp_write(tp, 0xF882, 0xC615);
        rtl8125_mac_ocp_write(tp, 0xF884, 0x9EE2);
        rtl8125_mac_ocp_write(tp, 0xF886, 0x1E01);
        rtl8125_mac_ocp_write(tp, 0xF888, 0x9EE0);
        rtl8125_mac_ocp_write(tp, 0xF88A, 0x76E0);
        rtl8125_mac_ocp_write(tp, 0xF88C, 0x49E0);
        rtl8125_mac_ocp_write(tp, 0xF88E, 0xF1FE);
        rtl8125_mac_ocp_write(tp, 0xF890, 0x76E6);
        rtl8125_mac_ocp_write(tp, 0xF892, 0x486F);
        rtl8125_mac_ocp_write(tp, 0xF894, 0x9EE4);
        rtl8125_mac_ocp_write(tp, 0xF896, 0x1E03);
        rtl8125_mac_ocp_write(tp, 0xF898, 0x9EE0);
        rtl8125_mac_ocp_write(tp, 0xF89A, 0x76E0);
        rtl8125_mac_ocp_write(tp, 0xF89C, 0x49E0);
        rtl8125_mac_ocp_write(tp, 0xF89E, 0xF1FE);
        rtl8125_mac_ocp_write(tp, 0xF8A0, 0x7196);
        rtl8125_mac_ocp_write(tp, 0xF8A2, 0xC702);
        rtl8125_mac_ocp_write(tp, 0xF8A4, 0xBF00);
        rtl8125_mac_ocp_write(tp, 0xF8A6, 0x5A44);
        rtl8125_mac_ocp_write(tp, 0xF8A8, 0xEB0E);
        rtl8125_mac_ocp_write(tp, 0xF8AA, 0x0070);
        rtl8125_mac_ocp_write(tp, 0xF8AC, 0x00C3);
        rtl8125_mac_ocp_write(tp, 0xF8AE, 0x1BC0);
        rtl8125_mac_ocp_write(tp, 0xF8B0, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF8B2, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF8B4, 0x0E26);
        rtl8125_mac_ocp_write(tp, 0xF8B6, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF8B8, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF8BA, 0x0EBA);
        rtl8125_mac_ocp_write(tp, 0xF8BC, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF8BE, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF8C0, 0x0000);
        rtl8125_mac_ocp_write(tp, 0xF8C2, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF8C4, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF8C6, 0x0000);
        rtl8125_mac_ocp_write(tp, 0xF8C8, 0xC602);
        rtl8125_mac_ocp_write(tp, 0xF8CA, 0xBE00);
        rtl8125_mac_ocp_write(tp, 0xF8CC, 0x0000);

        rtl8125_mac_ocp_write(tp, 0xFC26, 0x8000);

        rtl8125_mac_ocp_write(tp, 0xFC2A, 0x0540);
        rtl8125_mac_ocp_write(tp, 0xFC2E, 0x0E24);
        rtl8125_mac_ocp_write(tp, 0xFC30, 0x0EB8);

        rtl8125_mac_ocp_write(tp, 0xFC48, 0x001A);
}

static void
rtl8125_hw_mac_mcu_config(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->NotWrMcuPatchCode == TRUE) return;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
                rtl8125_hw_disable_mac_mcu_bps(dev);
                break;
        case CFG_METHOD_3:
                rtl8125_set_mac_mcu_8125a_2(dev);
                break;
        }
}

static void
rtl8125_hw_init(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 csi_tmp;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_enable_cfg9346_write(tp);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                rtl8125_disable_cfg9346_write(tp);
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                break;
        }

        //Disable UPS
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xD40A, rtl8125_mac_ocp_read( tp, 0xD40A) & ~(BIT_4));
                break;
        }

        rtl8125_hw_mac_mcu_config(dev);

        /*disable ocp phy power saving*/
        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3)
                rtl8125_disable_ocp_phy_power_saving(dev);

        //Set PCIE uncorrectable error status mask pcie 0x108
        csi_tmp = rtl8125_csi_read(tp, 0x108);
        csi_tmp |= BIT_20;
        rtl8125_csi_write(tp, 0x108, csi_tmp);

        if (s0_magic_packet == 1)
                rtl8125_enable_magic_packet(dev);
}

static void
rtl8125_hw_ephy_config(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
                rtl8125_ephy_write(ioaddr, 0x01, 0xA812);
                rtl8125_ephy_write(ioaddr, 0x09, 0x520C);
                rtl8125_ephy_write(ioaddr, 0x04, 0xD000);
                rtl8125_ephy_write(ioaddr, 0x0D, 0xF702);
                rtl8125_ephy_write(ioaddr, 0x0A, 0x8653);
                rtl8125_ephy_write(ioaddr, 0x06, 0x001E);
                rtl8125_ephy_write(ioaddr, 0x08, 0x3595);
                rtl8125_ephy_write(ioaddr, 0x20, 0x9455);
                rtl8125_ephy_write(ioaddr, 0x21, 0x99FF);
                rtl8125_ephy_write(ioaddr, 0x02, 0x6046);
                rtl8125_ephy_write(ioaddr, 0x29, 0xFE00);
                rtl8125_ephy_write(ioaddr, 0x23, 0xAB62);

                rtl8125_ephy_write(ioaddr, 0x41, 0xA80C);
                rtl8125_ephy_write(ioaddr, 0x49, 0x520C);
                rtl8125_ephy_write(ioaddr, 0x44, 0xD000);
                rtl8125_ephy_write(ioaddr, 0x4D, 0xF702);
                rtl8125_ephy_write(ioaddr, 0x4A, 0x8653);
                rtl8125_ephy_write(ioaddr, 0x46, 0x001E);
                rtl8125_ephy_write(ioaddr, 0x48, 0x3595);
                rtl8125_ephy_write(ioaddr, 0x60, 0x9455);
                rtl8125_ephy_write(ioaddr, 0x61, 0x99FF);
                rtl8125_ephy_write(ioaddr, 0x42, 0x6046);
                rtl8125_ephy_write(ioaddr, 0x69, 0xFE00);
                rtl8125_ephy_write(ioaddr, 0x63, 0xAB62);
                break;
        case CFG_METHOD_3:
                rtl8125_ephy_write(ioaddr, 0x04, 0xD000);
                rtl8125_ephy_write(ioaddr, 0x0A, 0x8653);
                rtl8125_ephy_write(ioaddr, 0x23, 0xAB66);
                rtl8125_ephy_write(ioaddr, 0x20, 0x9455);
                rtl8125_ephy_write(ioaddr, 0x21, 0x99FF);
                rtl8125_ephy_write(ioaddr, 0x29, 0xFE04);

                rtl8125_ephy_write(ioaddr, 0x44, 0xD000);
                rtl8125_ephy_write(ioaddr, 0x4A, 0x8653);
                rtl8125_ephy_write(ioaddr, 0x63, 0xAB66);
                rtl8125_ephy_write(ioaddr, 0x60, 0x9455);
                rtl8125_ephy_write(ioaddr, 0x61, 0x99FF);
                rtl8125_ephy_write(ioaddr, 0x69, 0xFE04);
                break;
        }
}

static int
rtl8125_check_hw_phy_mcu_code_ver(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int ram_code_ver_match = 0;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x801E);
                tp->hw_ram_code_ver = mdio_direct_read_phy_ocp(tp, 0xA438);
                break;
        default:
                tp->hw_ram_code_ver = ~0;
                break;
        }

        if ( tp->hw_ram_code_ver == tp->sw_ram_code_ver) {
                ram_code_ver_match = 1;
                tp->HwHasWrRamCodeToMicroP = TRUE;
        }

        return ram_code_ver_match;
}

static void
rtl8125_write_hw_phy_mcu_code_ver(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x801E);
                mdio_direct_write_phy_ocp(tp, 0xA438, tp->sw_ram_code_ver);
                tp->hw_ram_code_ver = tp->sw_ram_code_ver;
                break;
        }
}

static void
rtl8125_acquire_phy_mcu_patch_key_lock(struct rtl8125_private *tp)
{
        u16 PatchKey;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
                PatchKey = 0x8600;
                break;
        case CFG_METHOD_3:
                PatchKey = 0x8601;
                break;
        default:
                return;
        }
        mdio_direct_write_phy_ocp(tp, 0xA436, 0x8024);
        mdio_direct_write_phy_ocp(tp, 0xA438, PatchKey);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xB82E);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0001);
}

static void
rtl8125_release_phy_mcu_patch_key_lock(struct rtl8125_private *tp)
{
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                ClearEthPhyOcpBit(tp, 0xB82E, BIT_0);
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8024);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                break;
        default:
                break;
        }
}

bool
rtl8125_set_phy_mcu_patch_request(struct rtl8125_private *tp)
{
        u16 gphy_val;
        u16 WaitCount = 0;
        int i;
        bool bSuccess = TRUE;

        SetEthPhyOcpBit(tp, 0xB820, BIT_4);

        i = 0;
        do {
                gphy_val = mdio_direct_read_phy_ocp(tp, 0xB800);
                gphy_val &= BIT_6;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != BIT_6 && i < 1000);

        if (gphy_val != BIT_6 && WaitCount == 1000) bSuccess = FALSE;

        return bSuccess;
}

bool
rtl8125_clear_phy_mcu_patch_request(struct rtl8125_private *tp)
{
        u16 gphy_val;
        u16 WaitCount = 0;
        int i;
        bool bSuccess = TRUE;

        ClearEthPhyOcpBit(tp, 0xB820, BIT_4);

        i = 0;
        do {
                gphy_val = mdio_direct_read_phy_ocp(tp, 0xB800);
                gphy_val &= BIT_6;
                udelay(50);
                udelay(50);
                i++;
        } while(gphy_val != BIT_6 && i < 1000);

        if (gphy_val != BIT_6 && WaitCount == 1000) bSuccess = FALSE;

        return bSuccess;
}

static void
rtl8125_real_set_phy_mcu_8125_2(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_acquire_phy_mcu_patch_key_lock(tp);


        SetEthPhyOcpBit(tp, 0xB820, BIT_7);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8013);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8021);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x802f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x803d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8042);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8051);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8051);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa088);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a50);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8008);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1a3);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x401a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd707);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40c2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60a6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f8b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a6c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8080);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd019);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1a2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x401a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd707);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40c4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60a6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f8b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a84);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8970);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c07);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0901);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcf09);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd705);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xceff);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf0a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1213);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8401);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8580);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1253);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd064);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd181);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4018);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc50f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd706);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2c59);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x804d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc60f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc605);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x10fd);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA026);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA024);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA022);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x10f4);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA020);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1252);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA006);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1206);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA004);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a78);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a60);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a4f);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA008);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3f00);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0010);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8066);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x807c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8089);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x808e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80b2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80c2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x62db);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x655c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd73e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x614a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0505);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0509);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x653c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd73e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x614a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0502);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0506);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x050a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd73e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x614a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0505);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0506);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x050c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd73e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x614a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0509);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x050a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x050c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0508);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0304);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd73e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x614a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0321);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0502);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0321);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0321);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0508);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0321);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0346);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8208);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x609d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa50f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x001a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x001a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x607d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00ab);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60fd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa50f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaa0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x017b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a05);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x017b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60fd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa50f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaa0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x01e0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a05);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x01e0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60fd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa50f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaa0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0231);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0503);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a05);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0231);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08E);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08C);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0221);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08A);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x01ce);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA088);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0169);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA086);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00a6);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA084);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x000d);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA082);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0308);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA080);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x029f);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA090);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x007f);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0020);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8017);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8029);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8054);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x805a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8064);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80a7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9430);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9480);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb408);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd120);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd057);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x064b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb80);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9906);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0567);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb94);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x82a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x800a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8406);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8dff);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0773);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb91);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4063);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd139);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd140);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07dc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa110);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa2a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4045);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa180);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x405d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa720);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0742);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07ec);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f74);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0742);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7fb6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x82a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07dc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x064b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07c0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5fa7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0481);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x94bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x870c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa00a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa280);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8220);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x078e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb92);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4063);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd140);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd150);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd703);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6121);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x61a2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6223);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf02f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d10);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf00f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d20);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf00a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d30);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf005);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d40);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa008);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4046);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x405d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa720);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0742);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07f7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f74);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0742);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7fb5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x800a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3ad4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0537);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x064b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8301);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x800a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x82a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa70c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9402);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x890c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x064b);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10E);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0642);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10C);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0686);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10A);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0788);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA108);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x047b);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA106);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x065c);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA104);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0769);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA102);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0565);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA100);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x06f9);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA110);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00ff);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb87c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8530);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb87e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf85);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3caf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8593);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf85);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9caf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x85a5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5afb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe083);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfb0c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x020d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x021b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x10bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86d7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbe0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x83fc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1b10);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xda02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xdd02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5afb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe083);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfd0c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x020d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x021b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x10bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86dd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86e0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbe0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x83fe);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1b10);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf2f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbd02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2cac);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0286);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x65af);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x212b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x022c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86b6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf21);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cd1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x03bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8710);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x870d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8719);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8716);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x871f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x871c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8728);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8725);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8707);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbad);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x281c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd100);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1302);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2202);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2b02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae1a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd101);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1302);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2202);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2b02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd101);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3402);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3102);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3d02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3a02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4302);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4c02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4902);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd100);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2e02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4602);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf87);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4f02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ab7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf35);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7ff8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfaef);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x69bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86e3);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86fb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86e6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86fe);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86e9);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86ec);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfbbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x025a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7bf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86ef);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0262);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7cbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86f2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0262);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7cbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86f5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0262);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7cbf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x86f8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0262);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7cef);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x96fe);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfc04);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf8fa);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xef69);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xef02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6273);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf202);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6273);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf502);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6273);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbf86);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf802);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6273);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xef96);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfefc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0420);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb540);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x53b5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4086);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb540);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb9b5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40c8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb03a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc8b0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbac8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb13a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc8b1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xba77);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbd26);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffbd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2677);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbd28);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffbd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbd26);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc8bd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2640);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbd28);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc8bd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x28bb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa430);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x98b0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1eba);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb01e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xdcb0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e98);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb09e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbab0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9edc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb09e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x98b1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1eba);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb11e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xdcb1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e98);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb19e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbab1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9edc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb19e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x11b0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e22);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb01e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x33b0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e11);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb09e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x22b0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9e33);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb09e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x11b1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e22);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb11e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x33b1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1e11);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb19e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x22b1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9e33);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb19e);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb85e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2f71);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb860);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x20d9);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb862);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x2109);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb864);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x34e7);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb878);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x000f);


        ClearEthPhyOcpBit(tp, 0xB820, BIT_7);


        rtl8125_release_phy_mcu_patch_key_lock(tp);
}

static void
rtl8125_set_phy_mcu_8125_2(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_set_phy_mcu_patch_request(tp);

        rtl8125_real_set_phy_mcu_8125_2(dev);

        rtl8125_clear_phy_mcu_patch_request(tp);
}

static void
rtl8125_real_set_phy_mcu_8125_3(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_acquire_phy_mcu_patch_key_lock(tp);


        SetEthPhyOcpBit(tp, 0xB820, BIT_7);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x808b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x808f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8093);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8097);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x809d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80a1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80aa);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x607b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf00e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x42da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf01e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x615b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1456);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14a4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f2e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf01c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1456);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14a4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f2e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf024);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1456);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14a4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f2e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf02c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1456);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14a4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x14bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f2e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf034);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd719);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4118);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac11);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa410);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4779);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1444);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf034);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd719);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4118);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac22);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa420);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4559);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1444);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf023);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd719);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4118);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac44);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa440);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4339);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1444);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd719);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4118);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac88);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa480);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xce00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4119);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xac0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1444);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf001);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1456);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd718);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5fac);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc48f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x141b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd504);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x121a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd0b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1bb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0898);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd0b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1bb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a0e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd064);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd18a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0b7e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x401c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd501);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa804);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8804);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x053b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd500);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa301);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0648);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc520);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa201);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x252d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1646);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd708);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4006);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1646);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0308);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA026);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0307);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA024);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1645);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA022);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0647);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA020);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x053a);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA006);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0b7c);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA004);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0a0c);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0896);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x11a1);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA008);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xff00);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0010);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8015);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x801a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xad02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x02d7);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00ed);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0509);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xc100);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x008f);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08E);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08C);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA08A);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA088);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA086);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA084);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA082);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x008d);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA080);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00eb);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA090);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0103);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA016);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0020);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA012);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8014);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8018);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8024);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8051);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8055);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8072);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x80dc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfffd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfffd);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8301);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x800a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x82a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa70c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x9402);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x890c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8840);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa380);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x066e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb91);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4063);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd139);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd140);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa110);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa2a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4085);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa180);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8280);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x405d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa720);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0743);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07f0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5f74);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0743);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7fb6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x82a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0c0f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x066e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd158);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd04d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x03d4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x94bc);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x870c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8380);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd10d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07c4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5fb4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa190);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa00a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa280);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa404);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa220);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd130);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07c4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5fb4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xbb80);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1c4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd074);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa301);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x604b);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa90c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0556);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xcb92);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4063);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd116);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd119);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd040);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd703);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x60a0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6241);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x63e2);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6583);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf054);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x611e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d10);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf02f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d50);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf02a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x611e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d20);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf021);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d60);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf01c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x611e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d30);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf013);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d70);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf00e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x611e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x40da);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d40);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf005);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d80);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x405d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa720);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd700);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x5ff4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa008);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd704);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x4046);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa002);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0743);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07fb);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd703);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7f6f);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7f4e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7f2d);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7f0c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x800a);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0cf0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0d00);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07e8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8010);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa740);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1000);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0743);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x7fb5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd701);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3ad4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0556);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8610);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x066e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd1f5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xd049);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x1800);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x01ec);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10E);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x01ea);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10C);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x06a9);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA10A);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x078a);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA108);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x03d2);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA106);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x067f);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA104);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0665);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA102);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA100);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xA110);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00fc);


        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb87c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8530);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb87e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf85);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x3caf);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8545);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf85);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x45af);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8545);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xee82);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xf900);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0103);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xaf03);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb7f8);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe0a6);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x00e1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa601);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xef01);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x58f0);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa080);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x37a1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8402);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae16);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa185);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x02ae);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x11a1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8702);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae0c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xa188);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x02ae);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x07a1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x8902);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae02);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xae1c);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe0b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x62e1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb463);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6901);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe4b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x62e5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb463);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe0b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x62e1);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb463);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x6901);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xe4b4);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x62e5);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xb463);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xfc04);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb85e);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x03b3);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb860);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb862);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb864);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0xffff);
        mdio_direct_write_phy_ocp(tp, 0xA436, 0xb878);
        mdio_direct_write_phy_ocp(tp, 0xA438, 0x0001);


        ClearEthPhyOcpBit(tp, 0xB820, BIT_7);


        rtl8125_release_phy_mcu_patch_key_lock(tp);
}

static void
rtl8125_set_phy_mcu_8125_3(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_set_phy_mcu_patch_request(tp);

        rtl8125_real_set_phy_mcu_8125_3(dev);

        rtl8125_clear_phy_mcu_patch_request(tp);
}

static void
rtl8125_init_hw_phy_mcu(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u8 require_disable_phy_disable_mode = FALSE;

        if (tp->NotWrRamCodeToMicroP == TRUE) return;
        if (rtl8125_check_hw_phy_mcu_code_ver(dev)) return;

        if (HW_SUPPORT_CHECK_PHY_DISABLE_MODE(tp) && rtl8125_is_in_phy_disable_mode(dev))
                require_disable_phy_disable_mode = TRUE;

        if (require_disable_phy_disable_mode)
                rtl8125_disable_phy_disable_mode(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
                rtl8125_set_phy_mcu_8125_2(dev);
                break;
        case CFG_METHOD_3:
                rtl8125_set_phy_mcu_8125_3(dev);
                break;
        }

        if (require_disable_phy_disable_mode)
                rtl8125_enable_phy_disable_mode(dev);

        rtl8125_write_hw_phy_mcu_code_ver(dev);

        rtl8125_mdio_write(tp,0x1F, 0x0000);

        tp->HwHasWrRamCodeToMicroP = TRUE;
}

static void
rtl8125_hw_phy_config(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        tp->phy_reset_enable(dev);

        if (HW_DASH_SUPPORT_TYPE_3(tp) && tp->HwPkgDet == 0x06) return;

        rtl8125_init_hw_phy_mcu(dev);

        if (tp->mcfg == CFG_METHOD_2) {
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD40,
                                        0x03FF,
                                        0x84
                                       );

                SetEthPhyOcpBit(tp, 0xAD4E, BIT_4);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD16,
                                        0x03FF,
                                        0x0006
                                       );
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD32,
                                        0x003F,
                                        0x0006
                                       );
                ClearEthPhyOcpBit(tp, 0xAC08, BIT_12);
                ClearEthPhyOcpBit(tp, 0xAC08, BIT_8);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAC8A,
                                        BIT_15|BIT_14|BIT_13|BIT_12,
                                        BIT_14|BIT_13|BIT_12
                                       );
                SetEthPhyOcpBit(tp, 0xAD18, BIT_10);
                SetEthPhyOcpBit(tp, 0xAD1A, 0x3FF);
                SetEthPhyOcpBit(tp, 0xAD1C, 0x3FF);

                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80EA);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0xC400
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80EB);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0x0700,
                                        0x0300
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80F8);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x1C00
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80F1);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x3000
                                       );

                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80FE);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0xA500
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8102);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x5000
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8105);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x3300
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8100);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x7000
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8104);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0xF000
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8106);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0x6500
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80DC);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xA438,
                                        0xFF00,
                                        0xED00
                                       );
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80DF);
                SetEthPhyOcpBit(tp, 0xA438, BIT_8);
                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80E1);
                ClearEthPhyOcpBit(tp, 0xA438, BIT_8);

                ClearAndSetEthPhyOcpBit(tp,
                                        0xBF06,
                                        0x003F,
                                        0x38
                                       );

                mdio_direct_write_phy_ocp(tp, 0xA436, 0x819F);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0xD0B6);

                mdio_direct_write_phy_ocp(tp, 0xBC34, 0x5555);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xBF0A,
                                        BIT_11|BIT_10|BIT_9,
                                        BIT_11|BIT_9
                                       );

                ClearEthPhyOcpBit(tp, 0xA5C0, BIT_10);

                SetEthPhyOcpBit(tp, 0xA442, BIT_11);

                //enable aldps
                //GPHY OCP 0xA430 bit[2] = 0x1 (en_aldps)
                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                SetEthPhyOcpBit(tp, 0xA430, BIT_2);
                        }
                }
        } else if (tp->mcfg == CFG_METHOD_3) {
                SetEthPhyOcpBit(tp, 0xAD4E, BIT_4);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD16,
                                        0x03FF,
                                        0x03FF
                                       );
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD32,
                                        0x003F,
                                        0x0006
                                       );
                ClearEthPhyOcpBit(tp, 0xAC08, BIT_12);
                ClearEthPhyOcpBit(tp, 0xAC08, BIT_8);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xACC0,
                                        BIT_1|BIT_0,
                                        BIT_1
                                       );
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD40,
                                        BIT_7|BIT_6|BIT_5,
                                        BIT_6
                                       );
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAD40,
                                        BIT_2|BIT_1|BIT_0,
                                        BIT_2
                                       );
                ClearEthPhyOcpBit(tp, 0xAC14, BIT_7);
                ClearEthPhyOcpBit(tp, 0xAC80, BIT_9|BIT_8);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAC5E,
                                        BIT_2|BIT_1|BIT_0,
                                        BIT_1
                                       );
                mdio_direct_write_phy_ocp(tp, 0xAD4C, 0x00A8);
                mdio_direct_write_phy_ocp(tp, 0xAC5C, 0x01FF);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xAC8A,
                                        BIT_7|BIT_6|BIT_5|BIT_4,
                                        BIT_5|BIT_4
                                       );

                mdio_direct_write_phy_ocp(tp, 0xB87C, 0x80A2);
                mdio_direct_write_phy_ocp(tp, 0xB87E, 0x0153);
                mdio_direct_write_phy_ocp(tp, 0xB87C, 0x809C);
                mdio_direct_write_phy_ocp(tp, 0xB87E, 0x0153);


                mdio_direct_write_phy_ocp(tp, 0xA436, 0x81B3);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0043);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00A7);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00D6);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00EC);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00F6);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00FB);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00FD);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00FF);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x00BB);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0058);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0029);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0013);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0009);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0004);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0002);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x0000);


                mdio_direct_write_phy_ocp(tp, 0xA436, 0x8257);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x020F);


                mdio_direct_write_phy_ocp(tp, 0xA436, 0x80EA);
                mdio_direct_write_phy_ocp(tp, 0xA438, 0x7843);


                rtl8125_set_phy_mcu_patch_request(tp);

                ClearEthPhyOcpBit(tp, 0xB896, BIT_0);
                ClearEthPhyOcpBit(tp, 0xB892, 0xFF00);

                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC091);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x6E12);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC092);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x1214);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC094);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x1516);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC096);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x171B);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC098);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x1B1C);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC09A);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x1F1F);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC09C);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x2021);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC09E);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x2224);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC0A0);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x2424);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC0A2);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x2424);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC0A4);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x2424);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC018);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x0AF2);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC01A);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x0D4A);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC01C);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x0F26);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC01E);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x118D);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC020);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x14F3);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC022);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x175A);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC024);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x19C0);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC026);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x1C26);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC089);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x6050);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC08A);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x5F6E);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC08C);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x6E6E);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC08E);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x6E6E);
                mdio_direct_write_phy_ocp(tp, 0xB88E, 0xC090);
                mdio_direct_write_phy_ocp(tp, 0xB890, 0x6E12);

                SetEthPhyOcpBit(tp, 0xB896, BIT_0);

                rtl8125_clear_phy_mcu_patch_request(tp);


                SetEthPhyOcpBit(tp, 0xD068, BIT_13);


                mdio_direct_write_phy_ocp(tp, 0xA436, 0x81A2);
                SetEthPhyOcpBit(tp, 0xA438, BIT_8);
                ClearAndSetEthPhyOcpBit(tp,
                                        0xB54C,
                                        0xFF00,
                                        0xDB00);


                ClearEthPhyOcpBit(tp, 0xA454, BIT_0);


                SetEthPhyOcpBit(tp, 0xA5D4, BIT_5);
                ClearEthPhyOcpBit(tp, 0xAD4E, BIT_4);
                ClearEthPhyOcpBit(tp, 0xA86A, BIT_0);


                SetEthPhyOcpBit(tp, 0xA442, BIT_11);

                //enable aldps
                //GPHY OCP 0xA430 bit[2] = 0x1 (en_aldps)
                if (aspm) {
                        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                                SetEthPhyOcpBit(tp, 0xA430, BIT_2);
                        }
                }
        }

        /*ocp phy power saving*/
        /*
        if (aspm) {
        if (tp->mcfg == CFG_METHOD_2 || tp->mcfg == CFG_METHOD_3)
                rtl8125_enable_ocp_phy_power_saving(dev);
        }
        */

        rtl8125_mdio_write(tp, 0x1F, 0x0000);

        if (tp->HwHasWrRamCodeToMicroP == TRUE) {
                if (eee_enable == 1)
                        rtl8125_enable_eee(tp);
                else
                        rtl8125_disable_eee(tp);
        }
}

static inline void rtl8125_delete_esd_timer(struct net_device *dev, struct timer_list *timer)
{
        del_timer_sync(timer);
}

static inline void rtl8125_request_esd_timer(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->esd_timer;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
        setup_timer(timer, rtl8125_esd_timer, (unsigned long)dev);
#else
        timer_setup(timer, rtl8125_esd_timer, 0);
#endif
        mod_timer(timer, jiffies + RTL8125_ESD_TIMEOUT);
}

static inline void rtl8125_delete_link_timer(struct net_device *dev, struct timer_list *timer)
{
        del_timer_sync(timer);
}

static inline void rtl8125_request_link_timer(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
        setup_timer(timer, rtl8125_link_timer, (unsigned long)dev);
#else
        timer_setup(timer, rtl8125_link_timer, 0);
#endif
        mod_timer(timer, jiffies + RTL8125_LINK_TIMEOUT);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
/*
 * Polling 'interrupt' - used by things like netconsole to send skbs
 * without having to re-enable interrupts. It's not called while
 * the interrupt routine is executing.
 */
static void
rtl8125_netpoll(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;

        disable_irq(pdev->irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        rtl8125_interrupt(pdev->irq, dev, NULL);
#else
        rtl8125_interrupt(pdev->irq, dev);
#endif
        enable_irq(pdev->irq);
}
#endif

static void
rtl8125_get_bios_setting(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->bios_setting = RTL_R32(TimeInt2);
                break;
        }
}

static void
rtl8125_set_bios_setting(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W32(TimeInt2, tp->bios_setting);
                break;
        }
}

static void
rtl8125_init_software_variable(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        void __iomem *ioaddr = tp->mmio_addr;

        rtl8125_get_bios_setting(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                //tp->HwSuppDashVer = 3;
                break;
        default:
                tp->HwSuppDashVer = 0;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwPkgDet = rtl8125_mac_ocp_read(tp, 0xDC00);
                tp->HwPkgDet = (tp->HwPkgDet >> 3) & 0x07;
                break;
        }

        if (HW_DASH_SUPPORT_TYPE_3(tp) && tp->HwPkgDet == 0x06)
                eee_enable = 0;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwSuppNowIsOobVer = 1;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwPcieSNOffset = 0x16C;
                break;
        }

#ifdef ENABLE_REALWOW_SUPPORT
        rtl8125_get_realwow_hw_version(dev);
#endif //ENABLE_REALWOW_SUPPORT

        if (HW_DASH_SUPPORT_DASH(tp) && rtl8125_check_dash(tp))
                tp->DASH = 1;
        else
                tp->DASH = 0;

        if (tp->DASH) {
                if (HW_DASH_SUPPORT_TYPE_3(tp)) {
                        u64 CmacMemPhysAddress;
                        void __iomem *cmac_ioaddr = NULL;

                        //map CMAC IO space
                        CmacMemPhysAddress = rtl8125_csi_other_fun_read(tp, 0, 0x18);
                        if (!(CmacMemPhysAddress & BIT_0)) {
                                if (CmacMemPhysAddress & BIT_2)
                                        CmacMemPhysAddress |=  (u64)rtl8125_csi_other_fun_read(tp, 0, 0x1C) << 32;

                                CmacMemPhysAddress &=  0xFFFFFFF0;
                                /* ioremap MMIO region */
                                cmac_ioaddr = ioremap(CmacMemPhysAddress, R8125_REGS_SIZE);
                        }

                        if (cmac_ioaddr == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                                if (netif_msg_probe(tp))
                                        dev_err(&pdev->dev, "cannot remap CMAC MMIO, aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        }

                        if (cmac_ioaddr == NULL) {
                                tp->DASH = 0;
                        } else {
                                tp->mapped_cmac_ioaddr = cmac_ioaddr;
                        }
                }
        }

        if	(HW_DASH_SUPPORT_TYPE_3(tp))
                tp->cmac_ioaddr = tp->mapped_cmac_ioaddr;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                tp->intr_mask = RxDescUnavail | TxOK | RxOK | SWInt;
                tp->timer_intr_mask = PCSTimeout;
                break;
        }

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH) {
                if (HW_DASH_SUPPORT_TYPE_3(tp)) {
                        tp->timer_intr_mask |= ( ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET);
                        tp->intr_mask |= ( ISRIMR_DASH_INTR_EN | ISRIMR_DASH_INTR_CMAC_RESET);
                }
        }
#endif
        if (aspm) {
                switch (tp->mcfg) {
                case CFG_METHOD_2:
                case CFG_METHOD_3:
                        tp->org_pci_offset_99 = rtl8125_csi_fun0_read_byte(tp, 0x99);
                        tp->org_pci_offset_99 &= ~(BIT_5|BIT_6);
                        break;
                }

                switch (tp->mcfg) {
                case CFG_METHOD_2:
                case CFG_METHOD_3:
                        tp->org_pci_offset_180 = rtl8125_csi_fun0_read_byte(tp, 0x214);
                        break;
                }
        }

        pci_read_config_byte(pdev, 0x80, &tp->org_pci_offset_80);
        pci_read_config_byte(pdev, 0x81, &tp->org_pci_offset_81);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
        default:
                tp->use_timer_interrrupt = TRUE;
                break;
        }

        if (timer_count == 0 || tp->mcfg == CFG_METHOD_DEFAULT)
                tp->use_timer_interrrupt = FALSE;

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwSuppMagicPktVer = WAKEUP_MAGIC_PACKET_V3;
                break;
        default:
                tp->HwSuppMagicPktVer = WAKEUP_MAGIC_PACKET_NOT_SUPPORT;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwSuppCheckPhyDisableModeVer = 3;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                tp->HwSuppGigaForceMode = TRUE;
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_2;
                break;
        case CFG_METHOD_3:
                tp->sw_ram_code_ver = NIC_RAMCODE_VERSION_CFG_METHOD_3;
                break;
        }

        if (tp->HwIcVerUnknown) {
                tp->NotWrRamCodeToMicroP = TRUE;
                tp->NotWrMcuPatchCode = TRUE;
        }

        tp->NicCustLedValue = RTL_R16(CustomLED);

        rtl8125_get_hw_wol(dev);

        rtl8125_link_option((u8*)&autoneg_mode, (u32*)&speed_mode, (u8*)&duplex_mode, (u32*)&advertising_mode);

        tp->autoneg = autoneg_mode;
        tp->speed = speed_mode;
        tp->duplex = duplex_mode;
        tp->advertising = advertising_mode;

        tp->max_jumbo_frame_size = rtl_chip_info[tp->chipset].jumbo_frame_sz;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
        /* MTU range: 60 - hw-specific max */
        dev->min_mtu = ETH_ZLEN;
        dev->max_mtu = tp->max_jumbo_frame_size;
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
}

static void
rtl8125_release_board(struct pci_dev *pdev,
                      struct net_device *dev,
                      void __iomem *ioaddr)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_set_bios_setting(dev);
        rtl8125_rar_set(tp, tp->org_mac_addr);
        tp->wol_enabled = WOL_DISABLED;

        if (!tp->DASH)
                rtl8125_phy_power_down(dev);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                FreeAllocatedDashShareMemory(dev);
#endif

        if (tp->mapped_cmac_ioaddr != NULL)
                iounmap(tp->mapped_cmac_ioaddr);

        iounmap(ioaddr);
        pci_release_regions(pdev);
        pci_disable_device(pdev);
        free_netdev(dev);
}

static int
rtl8125_get_mac_address(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        int i;
        u8 mac_addr[MAC_ADDR_LEN];

        for (i = 0; i < MAC_ADDR_LEN; i++)
                mac_addr[i] = RTL_R8(MAC0 + i);

        if(tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3) {
                *(u32*)&mac_addr[0] = RTL_R32(BACKUP_ADDR0_8125);
                *(u16*)&mac_addr[4] = RTL_R16(BACKUP_ADDR1_8125);
        }

        if (!is_valid_ether_addr(mac_addr)) {
                netif_err(tp, probe, dev, "Invalid ether addr %pM\n",
                          mac_addr);
                eth_hw_addr_random(dev);
                ether_addr_copy(mac_addr, dev->dev_addr);
                netif_info(tp, probe, dev, "Random ether addr %pM\n",
                           mac_addr);
                tp->random_mac = 1;
        }

        rtl8125_rar_set(tp, mac_addr);

        for (i = 0; i < MAC_ADDR_LEN; i++) {
                dev->dev_addr[i] = RTL_R8(MAC0 + i);
                tp->org_mac_addr[i] = dev->dev_addr[i]; /* keep the original MAC address */
        }
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
#endif
//  memcpy(dev->dev_addr, dev->dev_addr, dev->addr_len);

        return 0;
}

/**
 * rtl8125_set_mac_address - Change the Ethernet Address of the NIC
 * @dev: network interface device structure
 * @p:   pointer to an address structure
 *
 * Return 0 on success, negative on failure
 **/
static int
rtl8125_set_mac_address(struct net_device *dev,
                        void *p)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct sockaddr *addr = p;
        unsigned long flags;

        if (!is_valid_ether_addr(addr->sa_data))
                return -EADDRNOTAVAIL;

        spin_lock_irqsave(&tp->lock, flags);

        memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

        rtl8125_rar_set(tp, dev->dev_addr);

        spin_unlock_irqrestore(&tp->lock, flags);

        return 0;
}

/******************************************************************************
 * rtl8125_rar_set - Puts an ethernet address into a receive address register.
 *
 * tp - The private data structure for driver
 * addr - Address to put into receive address register
 *****************************************************************************/
void
rtl8125_rar_set(struct rtl8125_private *tp,
                uint8_t *addr)
{
        void __iomem *ioaddr = tp->mmio_addr;
        uint32_t rar_low = 0;
        uint32_t rar_high = 0;

        rar_low = ((uint32_t) addr[0] |
                   ((uint32_t) addr[1] << 8) |
                   ((uint32_t) addr[2] << 16) |
                   ((uint32_t) addr[3] << 24));

        rar_high = ((uint32_t) addr[4] |
                    ((uint32_t) addr[5] << 8));

        rtl8125_enable_cfg9346_write(tp);
        RTL_W32(MAC0, rar_low);
        RTL_W32(MAC4, rar_high);

        rtl8125_disable_cfg9346_write(tp);
}

#ifdef ETHTOOL_OPS_COMPAT
static int ethtool_get_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd = { ETHTOOL_GSET };
        int err;

        if (!ethtool_ops->get_settings)
                return -EOPNOTSUPP;

        err = ethtool_ops->get_settings(dev, &cmd);
        if (err < 0)
                return err;

        if (copy_to_user(useraddr, &cmd, sizeof(cmd)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_settings(struct net_device *dev, void *useraddr)
{
        struct ethtool_cmd cmd;

        if (!ethtool_ops->set_settings)
                return -EOPNOTSUPP;

        if (copy_from_user(&cmd, useraddr, sizeof(cmd)))
                return -EFAULT;

        return ethtool_ops->set_settings(dev, &cmd);
}

static int ethtool_get_drvinfo(struct net_device *dev, void *useraddr)
{
        struct ethtool_drvinfo info;
        struct ethtool_ops *ops = ethtool_ops;

        if (!ops->get_drvinfo)
                return -EOPNOTSUPP;

        memset(&info, 0, sizeof(info));
        info.cmd = ETHTOOL_GDRVINFO;
        ops->get_drvinfo(dev, &info);

        if (ops->self_test_count)
                info.testinfo_len = ops->self_test_count(dev);
        if (ops->get_stats_count)
                info.n_stats = ops->get_stats_count(dev);
        if (ops->get_regs_len)
                info.regdump_len = ops->get_regs_len(dev);
        if (ops->get_eeprom_len)
                info.eedump_len = ops->get_eeprom_len(dev);

        if (copy_to_user(useraddr, &info, sizeof(info)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_regs(struct net_device *dev, char *useraddr)
{
        struct ethtool_regs regs;
        struct ethtool_ops *ops = ethtool_ops;
        void *regbuf;
        int reglen, ret;

        if (!ops->get_regs || !ops->get_regs_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&regs, useraddr, sizeof(regs)))
                return -EFAULT;

        reglen = ops->get_regs_len(dev);
        if (regs.len > reglen)
                regs.len = reglen;

        regbuf = kmalloc(reglen, GFP_USER);
        if (!regbuf)
                return -ENOMEM;

        ops->get_regs(dev, &regs, regbuf);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &regs, sizeof(regs)))
                goto out;
        useraddr += offsetof(struct ethtool_regs, data);
        if (copy_to_user(useraddr, regbuf, reglen))
                goto out;
        ret = 0;

out:
        kfree(regbuf);
        return ret;
}

static int ethtool_get_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol = { ETHTOOL_GWOL };

        if (!ethtool_ops->get_wol)
                return -EOPNOTSUPP;

        ethtool_ops->get_wol(dev, &wol);

        if (copy_to_user(useraddr, &wol, sizeof(wol)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_wol(struct net_device *dev, char *useraddr)
{
        struct ethtool_wolinfo wol;

        if (!ethtool_ops->set_wol)
                return -EOPNOTSUPP;

        if (copy_from_user(&wol, useraddr, sizeof(wol)))
                return -EFAULT;

        return ethtool_ops->set_wol(dev, &wol);
}

static int ethtool_get_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GMSGLVL };

        if (!ethtool_ops->get_msglevel)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_msglevel(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_msglevel(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_msglevel)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_msglevel(dev, edata.data);
        return 0;
}

static int ethtool_nway_reset(struct net_device *dev)
{
        if (!ethtool_ops->nway_reset)
                return -EOPNOTSUPP;

        return ethtool_ops->nway_reset(dev);
}

static int ethtool_get_link(struct net_device *dev, void *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GLINK };

        if (!ethtool_ops->get_link)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_link(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_get_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->get_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        ret = -EFAULT;
        if (copy_to_user(useraddr, &eeprom, sizeof(eeprom)))
                goto out;
        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_set_eeprom(struct net_device *dev, void *useraddr)
{
        struct ethtool_eeprom eeprom;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->set_eeprom || !ops->get_eeprom_len)
                return -EOPNOTSUPP;

        if (copy_from_user(&eeprom, useraddr, sizeof(eeprom)))
                return -EFAULT;

        /* Check for wrap and zero */
        if (eeprom.offset + eeprom.len <= eeprom.offset)
                return -EINVAL;

        /* Check for exceeding total eeprom len */
        if (eeprom.offset + eeprom.len > ops->get_eeprom_len(dev))
                return -EINVAL;

        data = kmalloc(eeprom.len, GFP_USER);
        if (!data)
                return -ENOMEM;

        ret = -EFAULT;
        if (copy_from_user(data, useraddr + sizeof(eeprom), eeprom.len))
                goto out;

        ret = ops->set_eeprom(dev, &eeprom, data);
        if (ret)
                goto out;

        if (copy_to_user(useraddr + sizeof(eeprom), data, eeprom.len))
                ret = -EFAULT;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce = { ETHTOOL_GCOALESCE };

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        ethtool_ops->get_coalesce(dev, &coalesce);

        if (copy_to_user(useraddr, &coalesce, sizeof(coalesce)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_coalesce(struct net_device *dev, void *useraddr)
{
        struct ethtool_coalesce coalesce;

        if (!ethtool_ops->get_coalesce)
                return -EOPNOTSUPP;

        if (copy_from_user(&coalesce, useraddr, sizeof(coalesce)))
                return -EFAULT;

        return ethtool_ops->set_coalesce(dev, &coalesce);
}

static int ethtool_get_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam = { ETHTOOL_GRINGPARAM };

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_ringparam(dev, &ringparam);

        if (copy_to_user(useraddr, &ringparam, sizeof(ringparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_ringparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_ringparam ringparam;

        if (!ethtool_ops->get_ringparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&ringparam, useraddr, sizeof(ringparam)))
                return -EFAULT;

        return ethtool_ops->set_ringparam(dev, &ringparam);
}

static int ethtool_get_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam = { ETHTOOL_GPAUSEPARAM };

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        ethtool_ops->get_pauseparam(dev, &pauseparam);

        if (copy_to_user(useraddr, &pauseparam, sizeof(pauseparam)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_pauseparam(struct net_device *dev, void *useraddr)
{
        struct ethtool_pauseparam pauseparam;

        if (!ethtool_ops->get_pauseparam)
                return -EOPNOTSUPP;

        if (copy_from_user(&pauseparam, useraddr, sizeof(pauseparam)))
                return -EFAULT;

        return ethtool_ops->set_pauseparam(dev, &pauseparam);
}

static int ethtool_get_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GRXCSUM };

        if (!ethtool_ops->get_rx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_rx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_rx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_rx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        ethtool_ops->set_rx_csum(dev, edata.data);
        return 0;
}

static int ethtool_get_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTXCSUM };

        if (!ethtool_ops->get_tx_csum)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tx_csum(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tx_csum(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tx_csum)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tx_csum(dev, edata.data);
}

static int ethtool_get_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GSG };

        if (!ethtool_ops->get_sg)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_sg(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_sg(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_sg)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_sg(dev, edata.data);
}

static int ethtool_get_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata = { ETHTOOL_GTSO };

        if (!ethtool_ops->get_tso)
                return -EOPNOTSUPP;

        edata.data = ethtool_ops->get_tso(dev);

        if (copy_to_user(useraddr, &edata, sizeof(edata)))
                return -EFAULT;
        return 0;
}

static int ethtool_set_tso(struct net_device *dev, char *useraddr)
{
        struct ethtool_value edata;

        if (!ethtool_ops->set_tso)
                return -EOPNOTSUPP;

        if (copy_from_user(&edata, useraddr, sizeof(edata)))
                return -EFAULT;

        return ethtool_ops->set_tso(dev, edata.data);
}

static int ethtool_self_test(struct net_device *dev, char *useraddr)
{
        struct ethtool_test test;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->self_test || !ops->self_test_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&test, useraddr, sizeof(test)))
                return -EFAULT;

        test.len = ops->self_test_count(dev);
        data = kmalloc(test.len * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->self_test(dev, &test, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &test, sizeof(test)))
                goto out;
        useraddr += sizeof(test);
        if (copy_to_user(useraddr, data, test.len * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_get_strings(struct net_device *dev, void *useraddr)
{
        struct ethtool_gstrings gstrings;
        struct ethtool_ops *ops = ethtool_ops;
        u8 *data;
        int ret;

        if (!ops->get_strings)
                return -EOPNOTSUPP;

        if (copy_from_user(&gstrings, useraddr, sizeof(gstrings)))
                return -EFAULT;

        switch (gstrings.string_set) {
        case ETH_SS_TEST:
                if (!ops->self_test_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->self_test_count(dev);
                break;
        case ETH_SS_STATS:
                if (!ops->get_stats_count)
                        return -EOPNOTSUPP;
                gstrings.len = ops->get_stats_count(dev);
                break;
        default:
                return -EINVAL;
        }

        data = kmalloc(gstrings.len * ETH_GSTRING_LEN, GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_strings(dev, gstrings.string_set, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &gstrings, sizeof(gstrings)))
                goto out;
        useraddr += sizeof(gstrings);
        if (copy_to_user(useraddr, data, gstrings.len * ETH_GSTRING_LEN))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_phys_id(struct net_device *dev, void *useraddr)
{
        struct ethtool_value id;

        if (!ethtool_ops->phys_id)
                return -EOPNOTSUPP;

        if (copy_from_user(&id, useraddr, sizeof(id)))
                return -EFAULT;

        return ethtool_ops->phys_id(dev, id.data);
}

static int ethtool_get_stats(struct net_device *dev, void *useraddr)
{
        struct ethtool_stats stats;
        struct ethtool_ops *ops = ethtool_ops;
        u64 *data;
        int ret;

        if (!ops->get_ethtool_stats || !ops->get_stats_count)
                return -EOPNOTSUPP;

        if (copy_from_user(&stats, useraddr, sizeof(stats)))
                return -EFAULT;

        stats.n_stats = ops->get_stats_count(dev);
        data = kmalloc(stats.n_stats * sizeof(u64), GFP_USER);
        if (!data)
                return -ENOMEM;

        ops->get_ethtool_stats(dev, &stats, data);

        ret = -EFAULT;
        if (copy_to_user(useraddr, &stats, sizeof(stats)))
                goto out;
        useraddr += sizeof(stats);
        if (copy_to_user(useraddr, data, stats.n_stats * sizeof(u64)))
                goto out;
        ret = 0;

out:
        kfree(data);
        return ret;
}

static int ethtool_ioctl(struct ifreq *ifr)
{
        struct net_device *dev = __dev_get_by_name(ifr->ifr_name);
        void *useraddr = (void *) ifr->ifr_data;
        u32 ethcmd;

        /*
         * XXX: This can be pushed down into the ethtool_* handlers that
         * need it.  Keep existing behaviour for the moment.
         */
        if (!capable(CAP_NET_ADMIN))
                return -EPERM;

        if (!dev || !netif_device_present(dev))
                return -ENODEV;

        if (copy_from_user(&ethcmd, useraddr, sizeof (ethcmd)))
                return -EFAULT;

        switch (ethcmd) {
        case ETHTOOL_GSET:
                return ethtool_get_settings(dev, useraddr);
        case ETHTOOL_SSET:
                return ethtool_set_settings(dev, useraddr);
        case ETHTOOL_GDRVINFO:
                return ethtool_get_drvinfo(dev, useraddr);
        case ETHTOOL_GREGS:
                return ethtool_get_regs(dev, useraddr);
        case ETHTOOL_GWOL:
                return ethtool_get_wol(dev, useraddr);
        case ETHTOOL_SWOL:
                return ethtool_set_wol(dev, useraddr);
        case ETHTOOL_GMSGLVL:
                return ethtool_get_msglevel(dev, useraddr);
        case ETHTOOL_SMSGLVL:
                return ethtool_set_msglevel(dev, useraddr);
        case ETHTOOL_NWAY_RST:
                return ethtool_nway_reset(dev);
        case ETHTOOL_GLINK:
                return ethtool_get_link(dev, useraddr);
        case ETHTOOL_GEEPROM:
                return ethtool_get_eeprom(dev, useraddr);
        case ETHTOOL_SEEPROM:
                return ethtool_set_eeprom(dev, useraddr);
        case ETHTOOL_GCOALESCE:
                return ethtool_get_coalesce(dev, useraddr);
        case ETHTOOL_SCOALESCE:
                return ethtool_set_coalesce(dev, useraddr);
        case ETHTOOL_GRINGPARAM:
                return ethtool_get_ringparam(dev, useraddr);
        case ETHTOOL_SRINGPARAM:
                return ethtool_set_ringparam(dev, useraddr);
        case ETHTOOL_GPAUSEPARAM:
                return ethtool_get_pauseparam(dev, useraddr);
        case ETHTOOL_SPAUSEPARAM:
                return ethtool_set_pauseparam(dev, useraddr);
        case ETHTOOL_GRXCSUM:
                return ethtool_get_rx_csum(dev, useraddr);
        case ETHTOOL_SRXCSUM:
                return ethtool_set_rx_csum(dev, useraddr);
        case ETHTOOL_GTXCSUM:
                return ethtool_get_tx_csum(dev, useraddr);
        case ETHTOOL_STXCSUM:
                return ethtool_set_tx_csum(dev, useraddr);
        case ETHTOOL_GSG:
                return ethtool_get_sg(dev, useraddr);
        case ETHTOOL_SSG:
                return ethtool_set_sg(dev, useraddr);
        case ETHTOOL_GTSO:
                return ethtool_get_tso(dev, useraddr);
        case ETHTOOL_STSO:
                return ethtool_set_tso(dev, useraddr);
        case ETHTOOL_TEST:
                return ethtool_self_test(dev, useraddr);
        case ETHTOOL_GSTRINGS:
                return ethtool_get_strings(dev, useraddr);
        case ETHTOOL_PHYS_ID:
                return ethtool_phys_id(dev, useraddr);
        case ETHTOOL_GSTATS:
                return ethtool_get_stats(dev, useraddr);
        default:
                return -EOPNOTSUPP;
        }

        return -EOPNOTSUPP;
}
#endif //ETHTOOL_OPS_COMPAT

static int
rtl8125_do_ioctl(struct net_device *dev,
                 struct ifreq *ifr,
                 int cmd)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct mii_ioctl_data *data = if_mii(ifr);
        int ret;
        unsigned long flags;

        ret = 0;
        switch (cmd) {
        case SIOCGMIIPHY:
                data->phy_id = 32; /* Internal PHY */
                break;

        case SIOCGMIIREG:
                spin_lock_irqsave(&tp->lock, flags);
                rtl8125_mdio_write(tp, 0x1F, 0x0000);
                data->val_out = rtl8125_mdio_read(tp, data->reg_num);
                spin_unlock_irqrestore(&tp->lock, flags);
                break;

        case SIOCSMIIREG:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;
                spin_lock_irqsave(&tp->lock, flags);
                rtl8125_mdio_write(tp, 0x1F, 0x0000);
                rtl8125_mdio_write(tp, data->reg_num, data->val_in);
                spin_unlock_irqrestore(&tp->lock, flags);
                break;

#ifdef ETHTOOL_OPS_COMPAT
        case SIOCETHTOOL:
                ret = ethtool_ioctl(ifr);
                break;
#endif

#ifdef ENABLE_DASH_SUPPORT
        case SIOCDEVPRIVATE_RTLDASH:
                if (!netif_running(dev)) {
                        ret = -ENODEV;
                        break;
                }
                if (!capable(CAP_NET_ADMIN)) {
                        ret = -EPERM;
                        break;
                }

                ret = rtl8125_dash_ioctl(dev, ifr);
                break;
#endif

#ifdef ENABLE_REALWOW_SUPPORT
        case SIOCDEVPRIVATE_RTLREALWOW:
                if (!netif_running(dev)) {
                        ret = -ENODEV;
                        break;
                }

                ret = rtl8125_realwow_ioctl(dev, ifr);
                break;
#endif

        case SIOCRTLTOOL:
                ret = rtl8125_tool_ioctl(tp, ifr);
                break;

        default:
                ret = -EOPNOTSUPP;
                break;
        }

        return ret;
}

static void
rtl8125_phy_power_up(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        if (rtl8125_is_in_phy_disable_mode(dev)) {
                return;
        }

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        rtl8125_mdio_write(tp, MII_BMCR, BMCR_ANENABLE);

        //wait ups resume (phy state 3)
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_wait_phy_ups_resume(dev, 3);
                break;
        };
}

static void
rtl8125_phy_power_down(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_mdio_write(tp, 0x1F, 0x0000);
        rtl8125_mdio_write(tp, MII_BMCR, BMCR_ANENABLE | BMCR_PDOWN);
}

static int __devinit
rtl8125_init_board(struct pci_dev *pdev,
                   struct net_device **dev_out,
                   void __iomem **ioaddr_out)
{
        void __iomem *ioaddr;
        struct net_device *dev;
        struct rtl8125_private *tp;
        int rc = -ENOMEM, i, pm_cap;

        assert(ioaddr_out != NULL);

        /* dev zeroed in alloc_etherdev */
        dev = alloc_etherdev(sizeof (*tp));
        if (dev == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_drv(&debug))
                        dev_err(&pdev->dev, "unable to alloc new ethernet\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                goto err_out;
        }

        SET_MODULE_OWNER(dev);
        SET_NETDEV_DEV(dev, &pdev->dev);
        tp = netdev_priv(dev);
        tp->dev = dev;
        tp->msg_enable = netif_msg_init(debug.msg_enable, R8125_MSG_DEFAULT);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
        if (!aspm)
                pci_disable_link_state(pdev, PCIE_LINK_STATE_L0S | PCIE_LINK_STATE_L1 |
                                       PCIE_LINK_STATE_CLKPM);
#endif

        /* enable device (incl. PCI PM wakeup and hotplug setup) */
        rc = pci_enable_device(pdev);
        if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "enable failure\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                goto err_out_free_dev;
        }

        rc = pci_set_mwi(pdev);
        if (rc < 0)
                goto err_out_disable;

        /* save power state before pci_enable_device overwrites it */
        pm_cap = pci_find_capability(pdev, PCI_CAP_ID_PM);
        if (pm_cap) {
                u16 pwr_command;

                pci_read_config_word(pdev, pm_cap + PCI_PM_CTRL, &pwr_command);
        } else {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp)) {
                        dev_err(&pdev->dev, "PowerManagement capability not found.\n");
                }
#else
                printk("PowerManagement capability not found.\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

        }

        /* make sure PCI base addr 1 is MMIO */
        if (!(pci_resource_flags(pdev, 2) & IORESOURCE_MEM)) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "region #1 not an MMIO resource, aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi;
        }
        /* check for weird/broken PCI region reporting */
        if (pci_resource_len(pdev, 2) < R8125_REGS_SIZE) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "Invalid PCI region size(s), aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -ENODEV;
                goto err_out_mwi;
        }

        rc = pci_request_regions(pdev, MODULENAME);
        if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "could not request regions.\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                goto err_out_mwi;
        }

        if ((sizeof(dma_addr_t) > 4) &&
            use_dac &&
            !pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) &&
            !pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64))) {
                dev->features |= NETIF_F_HIGHDMA;
        } else {
                rc = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
                if (rc < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        if (netif_msg_probe(tp))
                                dev_err(&pdev->dev, "DMA configuration failed.\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        goto err_out_free_res;
                }
        }

        pci_set_master(pdev);

        /* ioremap MMIO region */
        ioaddr = ioremap(pci_resource_start(pdev, 2), pci_resource_len(pdev, 2));
        if (ioaddr == NULL) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_err(&pdev->dev, "cannot remap MMIO, aborting\n");
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                rc = -EIO;
                goto err_out_free_res;
        }

        /* Identify chip attached to board */
        rtl8125_get_mac_version(tp, ioaddr);

        rtl8125_print_mac_version(tp);

        for (i = ARRAY_SIZE(rtl_chip_info) - 1; i >= 0; i--) {
                if (tp->mcfg == rtl_chip_info[i].mcfg)
                        break;
        }

        if (i < 0) {
                /* Unknown chip: assume array element #0, original RTL-8125 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                if (netif_msg_probe(tp))
                        dev_printk(KERN_DEBUG, &pdev->dev, "unknown chip version, assuming %s\n", rtl_chip_info[0].name);
#else
                printk("Realtek unknown chip version, assuming %s\n", rtl_chip_info[0].name);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
                i++;
        }

        tp->chipset = i;

        *ioaddr_out = ioaddr;
        *dev_out = dev;
out:
        return rc;

err_out_free_res:
        pci_release_regions(pdev);

err_out_mwi:
        pci_clear_mwi(pdev);

err_out_disable:
        pci_disable_device(pdev);

err_out_free_dev:
        free_netdev(dev);
err_out:
        *ioaddr_out = NULL;
        *dev_out = NULL;
        goto out;
}

static void
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
rtl8125_esd_timer(unsigned long __opaque)
#else
rtl8125_esd_timer(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8125_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->esd_timer;
#else
        struct rtl8125_private *tp = from_timer(tp, t, esd_timer);
        struct net_device *dev = tp->dev;
        struct timer_list *timer = t;
#endif
        struct pci_dev *pdev = tp->pci_dev;
        unsigned long timeout = RTL8125_ESD_TIMEOUT;
        unsigned long flags;
        u8 cmd;
        u16 io_base_l;
        u16 mem_base_l;
        u16 mem_base_h;
        u8 ilr;
        u16 resv_0x1c_h;
        u16 resv_0x1c_l;
        u16 resv_0x20_l;
        u16 resv_0x20_h;
        u16 resv_0x24_l;
        u16 resv_0x24_h;
        u16 resv_0x2c_h;
        u16 resv_0x2c_l;
        u32 pci_sn_l;
        u32 pci_sn_h;

        spin_lock_irqsave(&tp->lock, flags);

        tp->esd_flag = 0;

        pci_read_config_byte(pdev, PCI_COMMAND, &cmd);
        if (cmd != tp->pci_cfg_space.cmd) {
                printk(KERN_ERR "%s: cmd = 0x%02x, should be 0x%02x \n.", dev->name, cmd, tp->pci_cfg_space.cmd);
                pci_write_config_byte(pdev, PCI_COMMAND, tp->pci_cfg_space.cmd);
                tp->esd_flag |= BIT_0;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &io_base_l);
        if (io_base_l != tp->pci_cfg_space.io_base_l) {
                printk(KERN_ERR "%s: io_base_l = 0x%04x, should be 0x%04x \n.", dev->name, io_base_l, tp->pci_cfg_space.io_base_l);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_0, tp->pci_cfg_space.io_base_l);
                tp->esd_flag |= BIT_1;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_2, &mem_base_l);
        if (mem_base_l != tp->pci_cfg_space.mem_base_l) {
                printk(KERN_ERR "%s: mem_base_l = 0x%04x, should be 0x%04x \n.", dev->name, mem_base_l, tp->pci_cfg_space.mem_base_l);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_2, tp->pci_cfg_space.mem_base_l);
                tp->esd_flag |= BIT_2;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, &mem_base_h);
        if (mem_base_h!= tp->pci_cfg_space.mem_base_h) {
                printk(KERN_ERR "%s: mem_base_h = 0x%04x, should be 0x%04x \n.", dev->name, mem_base_h, tp->pci_cfg_space.mem_base_h);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, tp->pci_cfg_space.mem_base_h);
                tp->esd_flag |= BIT_3;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_3, &resv_0x1c_l);
        if (resv_0x1c_l != tp->pci_cfg_space.resv_0x1c_l) {
                printk(KERN_ERR "%s: resv_0x1c_l = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x1c_l, tp->pci_cfg_space.resv_0x1c_l);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_3, tp->pci_cfg_space.resv_0x1c_l);
                tp->esd_flag |= BIT_4;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, &resv_0x1c_h);
        if (resv_0x1c_h != tp->pci_cfg_space.resv_0x1c_h) {
                printk(KERN_ERR "%s: resv_0x1c_h = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x1c_h, tp->pci_cfg_space.resv_0x1c_h);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, tp->pci_cfg_space.resv_0x1c_h);
                tp->esd_flag |= BIT_5;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &resv_0x20_l);
        if (resv_0x20_l != tp->pci_cfg_space.resv_0x20_l) {
                printk(KERN_ERR "%s: resv_0x20_l = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x20_l, tp->pci_cfg_space.resv_0x20_l);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4, tp->pci_cfg_space.resv_0x20_l);
                tp->esd_flag |= BIT_6;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &resv_0x20_h);
        if (resv_0x20_h != tp->pci_cfg_space.resv_0x20_h) {
                printk(KERN_ERR "%s: resv_0x20_h = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x20_h, tp->pci_cfg_space.resv_0x20_h);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, tp->pci_cfg_space.resv_0x20_h);
                tp->esd_flag |= BIT_7;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &resv_0x24_l);
        if (resv_0x24_l != tp->pci_cfg_space.resv_0x24_l) {
                printk(KERN_ERR "%s: resv_0x24_l = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x24_l, tp->pci_cfg_space.resv_0x24_l);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5, tp->pci_cfg_space.resv_0x24_l);
                tp->esd_flag |= BIT_8;
        }

        pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &resv_0x24_h);
        if (resv_0x24_h != tp->pci_cfg_space.resv_0x24_h) {
                printk(KERN_ERR "%s: resv_0x24_h = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x24_h, tp->pci_cfg_space.resv_0x24_h);
                pci_write_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, tp->pci_cfg_space.resv_0x24_h);
                tp->esd_flag |= BIT_9;
        }

        pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &ilr);
        if (ilr != tp->pci_cfg_space.ilr) {
                printk(KERN_ERR "%s: ilr = 0x%02x, should be 0x%02x \n.", dev->name, ilr, tp->pci_cfg_space.ilr);
                pci_write_config_byte(pdev, PCI_INTERRUPT_LINE, tp->pci_cfg_space.ilr);
                tp->esd_flag |= BIT_10;
        }

        pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &resv_0x2c_l);
        if (resv_0x2c_l != tp->pci_cfg_space.resv_0x2c_l) {
                printk(KERN_ERR "%s: resv_0x2c_l = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x2c_l, tp->pci_cfg_space.resv_0x2c_l);
                pci_write_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, tp->pci_cfg_space.resv_0x2c_l);
                tp->esd_flag |= BIT_11;
        }

        pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, &resv_0x2c_h);
        if (resv_0x2c_h != tp->pci_cfg_space.resv_0x2c_h) {
                printk(KERN_ERR "%s: resv_0x2c_h = 0x%04x, should be 0x%04x \n.", dev->name, resv_0x2c_h, tp->pci_cfg_space.resv_0x2c_h);
                pci_write_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, tp->pci_cfg_space.resv_0x2c_h);
                tp->esd_flag |= BIT_12;
        }

        if (tp->HwPcieSNOffset > 0) {
                pci_sn_l = rtl8125_csi_read(tp, tp->HwPcieSNOffset);
                if (pci_sn_l != tp->pci_cfg_space.pci_sn_l) {
                        printk(KERN_ERR "%s: pci_sn_l = 0x%08x, should be 0x%08x \n.", dev->name, pci_sn_l, tp->pci_cfg_space.pci_sn_l);
                        rtl8125_csi_write(tp, tp->HwPcieSNOffset, tp->pci_cfg_space.pci_sn_l);
                        tp->esd_flag |= BIT_13;
                }

                pci_sn_h = rtl8125_csi_read(tp, tp->HwPcieSNOffset + 4);
                if (pci_sn_h != tp->pci_cfg_space.pci_sn_h) {
                        printk(KERN_ERR "%s: pci_sn_h = 0x%08x, should be 0x%08x \n.", dev->name, pci_sn_h, tp->pci_cfg_space.pci_sn_h);
                        rtl8125_csi_write(tp, tp->HwPcieSNOffset + 4, tp->pci_cfg_space.pci_sn_h);
                        tp->esd_flag |= BIT_14;
                }
        }

        if (tp->esd_flag != 0) {
                printk(KERN_ERR "%s: esd_flag = 0x%04x\n.\n", dev->name, tp->esd_flag);
                netif_stop_queue(dev);
                netif_carrier_off(dev);
                rtl8125_hw_reset(dev);
                rtl8125_tx_clear(tp);
                rtl8125_rx_clear(tp);
                rtl8125_init_ring(dev);
                rtl8125_hw_init(dev);
                rtl8125_powerup_pll(dev);
                rtl8125_hw_ephy_config(dev);
                rtl8125_hw_phy_config(dev);
                rtl8125_hw_config(dev);
                rtl8125_set_speed(dev, tp->autoneg, tp->speed, tp->duplex, tp->advertising);
                tp->esd_flag = 0;
        }
        spin_unlock_irqrestore(&tp->lock, flags);

        mod_timer(timer, jiffies + timeout);
}

static void
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
rtl8125_link_timer(unsigned long __opaque)
#else
rtl8125_link_timer(struct timer_list *t)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
        struct net_device *dev = (struct net_device *)__opaque;
        struct rtl8125_private *tp = netdev_priv(dev);
        struct timer_list *timer = &tp->link_timer;
#else
        struct rtl8125_private *tp = from_timer(tp, t, link_timer);
        struct net_device *dev = tp->dev;
        struct timer_list *timer = t;
#endif
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        rtl8125_check_link_status(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        mod_timer(timer, jiffies + RTL8125_LINK_TIMEOUT);
}

/* Cfg9346_Unlock assumed. */
static unsigned rtl8125_try_msi(struct pci_dev *pdev, struct rtl8125_private *tp)
{
        unsigned msi = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
        if (pci_enable_msi(pdev))
                dev_info(&pdev->dev, "no MSI. Back to INTx.\n");
        else
                msi |= RTL_FEATURE_MSI;
#endif

        return msi;
}

static void rtl8125_disable_msi(struct pci_dev *pdev, struct rtl8125_private *tp)
{
        if (tp->features & RTL_FEATURE_MSI) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,13)
                pci_disable_msi(pdev);
#endif
                tp->features &= ~RTL_FEATURE_MSI;
        }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
static const struct net_device_ops rtl8125_netdev_ops = {
        .ndo_open       = rtl8125_open,
        .ndo_stop       = rtl8125_close,
        .ndo_get_stats      = rtl8125_get_stats,
        .ndo_start_xmit     = rtl8125_start_xmit,
        .ndo_tx_timeout     = rtl8125_tx_timeout,
        .ndo_change_mtu     = rtl8125_change_mtu,
        .ndo_set_mac_address    = rtl8125_set_mac_address,
        .ndo_do_ioctl       = rtl8125_do_ioctl,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,1,0)
        .ndo_set_multicast_list = rtl8125_set_rx_mode,
#else
        .ndo_set_rx_mode    = rtl8125_set_rx_mode,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
#ifdef CONFIG_R8125_VLAN
        .ndo_vlan_rx_register   = rtl8125_vlan_rx_register,
#endif
#else
        .ndo_fix_features   = rtl8125_fix_features,
        .ndo_set_features   = rtl8125_set_features,
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
        .ndo_poll_controller    = rtl8125_netpoll,
#endif
};
#endif

static int __devinit
rtl8125_init_one(struct pci_dev *pdev,
                 const struct pci_device_id *ent)
{
        struct net_device *dev = NULL;
        struct rtl8125_private *tp;
        void __iomem *ioaddr = NULL;
        static int board_idx = -1;

        int rc;

        assert(pdev != NULL);
        assert(ent != NULL);

        board_idx++;

        if (netif_msg_drv(&debug))
                printk(KERN_INFO "%s 2.5Gigabit Ethernet driver %s loaded\n",
                       MODULENAME, RTL8125_VERSION);

        rc = rtl8125_init_board(pdev, &dev, &ioaddr);
        if (rc)
                goto out;

        tp = netdev_priv(dev);
        assert(ioaddr != NULL);

        tp->mmio_addr = ioaddr;
        tp->set_speed = rtl8125_set_speed_xmii;
        tp->get_settings = rtl8125_gset_xmii;
        tp->phy_reset_enable = rtl8125_xmii_reset_enable;
        tp->phy_reset_pending = rtl8125_xmii_reset_pending;
        tp->link_ok = rtl8125_xmii_link_ok;

        tp->features |= rtl8125_try_msi(pdev, tp);

        RTL_NET_DEVICE_OPS(rtl8125_netdev_ops);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
        SET_ETHTOOL_OPS(dev, &rtl8125_ethtool_ops);
#endif

        dev->watchdog_timeo = RTL8125_TX_TIMEOUT;
        dev->irq = pdev->irq;
        dev->base_addr = (unsigned long) ioaddr;

#ifdef CONFIG_R8125_NAPI
        RTL_NAPI_CONFIG(dev, tp, rtl8125_poll, R8125_NAPI_WEIGHT);
#endif

#ifdef CONFIG_R8125_VLAN
        if (tp->mcfg != CFG_METHOD_DEFAULT) {
                dev->features |= NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
                dev->vlan_rx_kill_vid = rtl8125_vlan_rx_kill_vid;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        }
#endif

        tp->cp_cmd |= RTL_R16(CPlusCmd);
        if (tp->mcfg != CFG_METHOD_DEFAULT) {
                dev->features |= NETIF_F_IP_CSUM;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
                tp->cp_cmd |= RxChkSum;
#else
                dev->features |= NETIF_F_RXCSUM | NETIF_F_SG | NETIF_F_TSO;
                dev->hw_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                                   NETIF_F_RXCSUM | NETIF_F_HW_VLAN_TX | NETIF_F_HW_VLAN_RX;
                dev->vlan_features = NETIF_F_SG | NETIF_F_IP_CSUM | NETIF_F_TSO |
                                     NETIF_F_HIGHDMA;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
                dev->priv_flags |= IFF_LIVE_ADDR_CHANGE;
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0)
                dev->hw_features |= NETIF_F_RXALL;
                dev->hw_features |= NETIF_F_RXFCS;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
                dev->hw_features |= NETIF_F_IPV6_CSUM | NETIF_F_TSO6;
                dev->features |=  NETIF_F_IPV6_CSUM | NETIF_F_TSO6;
                netif_set_gso_max_size(dev, LSO_64K);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)
                dev->gso_max_segs = NIC_MAX_PHYS_BUF_COUNT_LSO2;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
                dev->gso_min_segs = NIC_MIN_PHYS_BUF_COUNT;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0)

#endif //LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        }

        tp->pci_dev = pdev;

        spin_lock_init(&tp->lock);

        rtl8125_init_software_variable(dev);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH)
                AllocateDashShareMemory(dev);
#endif

        rtl8125_exit_oob(dev);

        rtl8125_hw_init(dev);

        rtl8125_hw_reset(dev);

        /* Get production from EEPROM */
        rtl8125_eeprom_type(tp);

        if (tp->eeprom_type == EEPROM_TYPE_93C46 || tp->eeprom_type == EEPROM_TYPE_93C56)
                rtl8125_set_eeprom_sel_low(ioaddr);

        rtl8125_get_mac_address(dev);

        tp->tally_vaddr = pci_alloc_consistent(pdev, sizeof(*tp->tally_vaddr), &tp->tally_paddr);
        if (!tp->tally_vaddr) {
                rc = -ENOMEM;
                goto err_out;
        }

        rtl8125_tally_counter_clear(tp);

        pci_set_drvdata(pdev, dev);

        rc = register_netdev(dev);
        if (rc)
                goto err_out;

        printk(KERN_INFO "%s: This product is covered by one or more of the following patents: US6,570,884, US6,115,776, and US6,327,625.\n", MODULENAME);

        rtl8125_disable_rxdvgate(dev);

        device_set_wakeup_enable(&pdev->dev, tp->wol_enabled);

        netif_carrier_off(dev);

        printk("%s", GPL_CLAIM);

out:
        return rc;

err_out:
        if (tp->tally_vaddr != NULL) {
                pci_free_consistent(pdev, sizeof(*tp->tally_vaddr), tp->tally_vaddr,
                                    tp->tally_paddr);

                tp->tally_vaddr = NULL;
        }
#ifdef  CONFIG_R8125_NAPI
        RTL_NAPI_DEL(tp);
#endif
        rtl8125_disable_msi(pdev, tp);
        rtl8125_release_board(pdev, dev, ioaddr);

        goto out;
}

static void __devexit
rtl8125_remove_one(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8125_private *tp = netdev_priv(dev);

        assert(dev != NULL);
        assert(tp != NULL);

#ifdef  CONFIG_R8125_NAPI
        RTL_NAPI_DEL(tp);
#endif
        if (tp->DASH)
                rtl8125_driver_stop(tp);

        unregister_netdev(dev);
        rtl8125_disable_msi(pdev, tp);
#ifdef ENABLE_R8125_PROCFS
        rtl8125_proc_remove(dev);
#endif
        if (tp->tally_vaddr != NULL) {
                pci_free_consistent(pdev, sizeof(*tp->tally_vaddr), tp->tally_vaddr, tp->tally_paddr);
                tp->tally_vaddr = NULL;
        }

        rtl8125_release_board(pdev, dev, tp->mmio_addr);
        pci_set_drvdata(pdev, NULL);
}

static void
rtl8125_set_rxbufsize(struct rtl8125_private *tp,
                      struct net_device *dev)
{
        unsigned int mtu = dev->mtu;

        tp->rx_buf_sz = (mtu > ETH_DATA_LEN) ? mtu + ETH_HLEN + 8 + 1 : RX_BUF_SIZE;
}

static int rtl8125_open(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        unsigned long flags;
        int retval;

        retval = -ENOMEM;

#ifdef ENABLE_R8125_PROCFS
        rtl8125_proc_init(dev);
#endif
        rtl8125_set_rxbufsize(tp, dev);
        /*
         * Rx and Tx descriptors needs 256 bytes alignment.
         * pci_alloc_consistent provides more.
         */
        tp->TxDescArray = pci_alloc_consistent(pdev, R8125_TX_RING_BYTES,
                                               &tp->TxPhyAddr);
        if (!tp->TxDescArray)
                goto err_free_all_allocated_mem;

        tp->RxDescArray = pci_alloc_consistent(pdev, R8125_RX_RING_BYTES,
                                               &tp->RxPhyAddr);
        if (!tp->RxDescArray)
                goto err_free_all_allocated_mem;

        if (tp->UseSwPaddingShortPkt) {
                tp->ShortPacketEmptyBuffer = pci_alloc_consistent(pdev, SHORT_PACKET_PADDING_BUF_SIZE,
                                             &tp->ShortPacketEmptyBufferPhy);
                if (!tp->ShortPacketEmptyBuffer)
                        goto err_free_all_allocated_mem;

                memset(tp->ShortPacketEmptyBuffer, 0x0, SHORT_PACKET_PADDING_BUF_SIZE);
        }

        retval = rtl8125_init_ring(dev);
        if (retval < 0)
                goto err_free_all_allocated_mem;

        if (netif_msg_probe(tp)) {
                printk(KERN_INFO "%s: 0x%lx, "
                       "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, "
                       "IRQ %d\n",
                       dev->name,
                       dev->base_addr,
                       dev->dev_addr[0], dev->dev_addr[1],
                       dev->dev_addr[2], dev->dev_addr[3],
                       dev->dev_addr[4], dev->dev_addr[5], dev->irq);
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        INIT_WORK(&tp->task, NULL, dev);
#else
        INIT_DELAYED_WORK(&tp->task, NULL);
#endif

#ifdef  CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_exit_oob(dev);

        rtl8125_hw_init(dev);

        rtl8125_hw_reset(dev);

        rtl8125_powerup_pll(dev);

        rtl8125_hw_ephy_config(dev);

        rtl8125_hw_phy_config(dev);

        rtl8125_hw_config(dev);

        rtl8125_set_speed(dev, tp->autoneg, tp->speed, tp->duplex, tp->advertising);

        spin_unlock_irqrestore(&tp->lock, flags);

        retval = request_irq(dev->irq, rtl8125_interrupt, (tp->features & RTL_FEATURE_MSI) ? 0 : SA_SHIRQ, dev->name, dev);
        if (retval<0)
                goto err_free_all_allocated_mem;

        if (tp->esd_flag == 0)
                rtl8125_request_esd_timer(dev);

        rtl8125_request_link_timer(dev);

out:

        return retval;

err_free_all_allocated_mem:
        if (tp->RxDescArray != NULL) {
                pci_free_consistent(pdev, R8125_RX_RING_BYTES, tp->RxDescArray,
                                    tp->RxPhyAddr);
                tp->RxDescArray = NULL;
        }

        if (tp->TxDescArray != NULL) {
                pci_free_consistent(pdev, R8125_TX_RING_BYTES, tp->TxDescArray,
                                    tp->TxPhyAddr);
                tp->TxDescArray = NULL;
        }

        if (tp->ShortPacketEmptyBuffer != NULL) {
                pci_free_consistent(pdev, ETH_ZLEN, tp->ShortPacketEmptyBuffer,
                                    tp->ShortPacketEmptyBufferPhy);
                tp->ShortPacketEmptyBuffer = NULL;
        }

        goto out;
}

static void
set_offset70F(struct rtl8125_private *tp, u8 setting)
{
        u32 csi_tmp;
        u32 temp = (u32)setting;
        temp = temp << 24;
        /*set PCI configuration space offset 0x70F to setting*/
        /*When the register offset of PCI configuration space larger than 0xff, use CSI to access it.*/

        csi_tmp = rtl8125_csi_read(tp, 0x70c) & 0x00ffffff;
        rtl8125_csi_write(tp, 0x70c, csi_tmp | temp);
}

static void
set_offset79(struct rtl8125_private *tp, u8 setting)
{
        //Set PCI configuration space offset 0x79 to setting

        struct pci_dev *pdev = tp->pci_dev;
        u8 device_control;

        if (hwoptimize & HW_PATCH_SOC_LAN) return;

        pci_read_config_byte(pdev, 0x79, &device_control);
        device_control &= ~0x70;
        device_control |= setting;
        pci_write_config_byte(pdev, 0x79, device_control);
}

static void
rtl8125_hw_set_rx_packet_filter(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 mc_filter[2];   /* Multicast hash filter */
        int rx_mode;
        u32 tmp = 0;

        if (dev->flags & IFF_PROMISC) {
                /* Unconditionally log net taps. */
                if (netif_msg_link(tp))
                        printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n",
                               dev->name);

                rx_mode =
                        AcceptBroadcast | AcceptMulticast | AcceptMyPhys |
                        AcceptAllPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else if ((netdev_mc_count(dev) > multicast_filter_limit)
                   || (dev->flags & IFF_ALLMULTI)) {
                /* Too many to filter perfectly -- accept all multicasts. */
                rx_mode = AcceptBroadcast | AcceptMulticast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0xffffffff;
        } else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
                struct dev_mc_list *mclist;
                unsigned int i;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
                     i++, mclist = mclist->next) {
                        int bit_nr = ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#else
                struct netdev_hw_addr *ha;

                rx_mode = AcceptBroadcast | AcceptMyPhys;
                mc_filter[1] = mc_filter[0] = 0;
                netdev_for_each_mc_addr(ha, dev) {
                        int bit_nr = ether_crc(ETH_ALEN, ha->addr) >> 26;
                        mc_filter[bit_nr >> 5] |= 1 << (bit_nr & 31);
                        rx_mode |= AcceptMulticast;
                }
#endif
        }

        if (dev->features & NETIF_F_RXALL)
                rx_mode |= (AcceptErr | AcceptRunt);

        tmp = mc_filter[0];
        mc_filter[0] = swab32(mc_filter[1]);
        mc_filter[1] = swab32(tmp);

        tp->rtl8125_rx_config = rtl_chip_info[tp->chipset].RCR_Cfg;
        tmp = tp->rtl8125_rx_config | rx_mode | (RTL_R32(RxConfig) & rtl_chip_info[tp->chipset].RxConfigMask);

        RTL_W32(RxConfig, tmp);
        RTL_W32(MAR0 + 0, mc_filter[0]);
        RTL_W32(MAR0 + 4, mc_filter[1]);
}

static void
rtl8125_set_rx_mode(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_hw_set_rx_packet_filter(dev);

        spin_unlock_irqrestore(&tp->lock, flags);
}

static void
rtl8125_hw_config(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        struct pci_dev *pdev = tp->pci_dev;
        u16 mac_ocp_data;
        int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        if (dev->mtu > ETH_DATA_LEN) {
                dev->features &= ~(NETIF_F_IP_CSUM);
        } else {
                dev->features |= NETIF_F_IP_CSUM;
        }
#endif

        RTL_W32(RxConfig, (RX_DMA_BURST << RxCfgDMAShift));

        rtl8125_hw_reset(dev);

        rtl8125_enable_cfg9346_write(tp);
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(0xF1, RTL_R8(0xF1) & ~BIT_7);
                RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                break;
        }

        //clear io_rdy_l23
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                RTL_W8(Config3, RTL_R8(Config3) & ~BIT_1);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                //IntMITI_0-IntMITI_31
                for (i=0xA00; i<0xB00; i+=4)
                        RTL_W32(i, 0x00000000);
                break;
        }

        //keep magic packet only
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xC0B6);
                mac_ocp_data &= ~(BIT_0);
                rtl8125_mac_ocp_write(tp, 0xC0B6, mac_ocp_data);
                break;
        }

        rtl8125_tally_counter_addr_fill(tp);

        rtl8125_desc_addr_fill(tp);

        /* Set DMA burst size and Interframe Gap Time */
        RTL_W32(TxConfig, (TX_DMA_BURST_unlimited << TxDMAShift) |
                (InterFrameGap << TxInterFrameGapShift));

        if (tp->mcfg == CFG_METHOD_2 ||
            tp->mcfg == CFG_METHOD_3) {
                set_offset70F(tp, 0x27);
                set_offset79(tp, 0x50);

                RTL_W16(0x382, 0x221B);

                RTL_W8(0x4500, 0x00);
                RTL_W16(0x4800, 0x0000);

                RTL_W8(Config1, RTL_R8(Config1) & ~0x10);

                rtl8125_mac_ocp_write(tp, 0xC140, 0xFFFF);
                rtl8125_mac_ocp_write(tp, 0xC142, 0xFFFF);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xD3E2);
                mac_ocp_data &= 0xF000;
                mac_ocp_data |= 0x3A9;
                rtl8125_mac_ocp_write(tp, 0xD3E2, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xD3E4);
                mac_ocp_data &= 0xFF00;
                rtl8125_mac_ocp_write(tp, 0xD3E4, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE860);
                mac_ocp_data |= (BIT_7);
                rtl8125_mac_ocp_write(tp, 0xE860, mac_ocp_data);

                //new tx desc format
                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xEB58);
                mac_ocp_data |= (BIT_0);
                rtl8125_mac_ocp_write(tp, 0xEB58, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE614);
                mac_ocp_data &= ~( BIT_10 | BIT_9 | BIT_8);
                if (tp->DASH && !(rtl8125_csi_fun0_read_byte(tp, 0x79) & BIT_0))
                        mac_ocp_data |= ((3 & 0x07) << 8);
                else
                        mac_ocp_data |= ((4 & 0x07) << 8);
                rtl8125_mac_ocp_write(tp, 0xE614, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE63E);
                mac_ocp_data &= ~(BIT_11 | BIT_10);
                mac_ocp_data |= ((0 & 0x03) << 10);
                rtl8125_mac_ocp_write(tp, 0xE63E, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE63E);
                mac_ocp_data &= ~(BIT_5 | BIT_4);
                mac_ocp_data |= ((0x02 & 0x03) << 4);
                rtl8125_mac_ocp_write(tp, 0xE63E, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xC0B4);
                mac_ocp_data |= (BIT_3|BIT_2);
                rtl8125_mac_ocp_write(tp, 0xC0B4, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xEB6A);
                mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                mac_ocp_data |= (BIT_5 | BIT_4 | BIT_1 | BIT_0);
                rtl8125_mac_ocp_write(tp, 0xEB6A, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xEB50);
                mac_ocp_data &= ~(BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5);
                mac_ocp_data |= (BIT_6);
                rtl8125_mac_ocp_write(tp, 0xEB50, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE056);
                mac_ocp_data &= ~(BIT_7 | BIT_6 | BIT_5 | BIT_4);
                mac_ocp_data |= (BIT_4 | BIT_5);
                rtl8125_mac_ocp_write(tp, 0xE056, mac_ocp_data);

                RTL_W8(TDFNR, 0x10);

                //RTL_W8(0xD0, RTL_R8(0xD0) | BIT_7);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE040);
                mac_ocp_data &= ~(BIT_12);
                rtl8125_mac_ocp_write(tp, 0xE040, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE0C0);
                mac_ocp_data &= ~(BIT_14 | BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                mac_ocp_data |= (BIT_14 | BIT_10 | BIT_1 | BIT_0);
                rtl8125_mac_ocp_write(tp, 0xE0C0, mac_ocp_data);

                SetMcuAccessRegBit(tp, 0xE052, (BIT_6|BIT_5|BIT_3));
                ClearMcuAccessRegBit(tp, 0xE052, BIT_7);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xC0AC);
                mac_ocp_data &= ~(BIT_7);
                mac_ocp_data |= (BIT_8|BIT_9|BIT_10|BIT_11|BIT_12);
                rtl8125_mac_ocp_write(tp, 0xC0AC, mac_ocp_data);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xD430);
                mac_ocp_data &= ~(BIT_11 | BIT_10 | BIT_9 | BIT_8 | BIT_7 | BIT_6 | BIT_5 | BIT_4 | BIT_3 | BIT_2 | BIT_1 | BIT_0);
                mac_ocp_data |= 0x47F;
                rtl8125_mac_ocp_write(tp, 0xD430, mac_ocp_data);

                //rtl8125_mac_ocp_write(tp, 0xE0C0, 0x4F87);
                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xE84C);
                mac_ocp_data |= (BIT_7 | BIT_6);
                rtl8125_mac_ocp_write(tp, 0xE84C, mac_ocp_data);

                rtl8125_disable_eee_plus(tp);

                mac_ocp_data = rtl8125_mac_ocp_read(tp, 0xEA1C);
                mac_ocp_data &= ~(BIT_2);
                rtl8125_mac_ocp_write(tp, 0xEA1C, mac_ocp_data);

                SetMcuAccessRegBit(tp, 0xEB54, BIT_0);
                udelay(1);
                ClearMcuAccessRegBit(tp, 0xEB54, BIT_0);
                RTL_W16(0x1880, RTL_R16(0x1880)&~(BIT_4 | BIT_5));
        }

        /* csum offload command for RTL8125 */
        tp->tx_tcp_csum_cmd = TxTCPCS_C;
        tp->tx_udp_csum_cmd = TxUDPCS_C;
        tp->tx_ip_csum_cmd = TxIPCS_C;
        tp->tx_ipv6_csum_cmd = TxIPV6F_C;


        //other hw parameters
        rtl8125_hw_clear_timer_int(dev);

        rtl8125_hw_clear_int_miti(dev);

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                rtl8125_mac_ocp_write(tp, 0xE098, 0xC302);
                break;
        }

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                if (aspm) {
                        rtl8125_init_pci_offset_99(tp);
                }
                break;
        }
        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                if (aspm) {
                        rtl8125_init_pci_offset_180(tp);
                }
                break;
        }

        tp->cp_cmd &= ~(EnableBist | Macdbgo_oe | Force_halfdup |
                        Force_rxflow_en | Force_txflow_en | Cxpl_dbg_sel |
                        ASF | Macdbgo_sel);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0)
        RTL_W16(CPlusCmd, tp->cp_cmd);
#else
        rtl8125_hw_set_features(dev, dev->features);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3: {
                int timeout;
                for (timeout = 0; timeout < 10; timeout++) {
                        if ((rtl8125_mac_ocp_read(tp, 0xE00E) & BIT_13)==0)
                                break;
                        mdelay(1);
                }
        }
        break;
        }

        RTL_W16(RxMaxSize, tp->rx_buf_sz);

        rtl8125_disable_rxdvgate(dev);

        if (!tp->pci_cfg_is_read) {
                pci_read_config_byte(pdev, PCI_COMMAND, &tp->pci_cfg_space.cmd);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0, &tp->pci_cfg_space.io_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_0 + 2, &tp->pci_cfg_space.io_base_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_2, &tp->pci_cfg_space.mem_base_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_2 + 2, &tp->pci_cfg_space.mem_base_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_3, &tp->pci_cfg_space.resv_0x1c_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_3 + 2, &tp->pci_cfg_space.resv_0x1c_h);
                pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &tp->pci_cfg_space.ilr);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4, &tp->pci_cfg_space.resv_0x20_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_4 + 2, &tp->pci_cfg_space.resv_0x20_h);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5, &tp->pci_cfg_space.resv_0x24_l);
                pci_read_config_word(pdev, PCI_BASE_ADDRESS_5 + 2, &tp->pci_cfg_space.resv_0x24_h);
                pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &tp->pci_cfg_space.resv_0x2c_l);
                pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID + 2, &tp->pci_cfg_space.resv_0x2c_h);
                if (tp->HwPcieSNOffset > 0) {
                        tp->pci_cfg_space.pci_sn_l = rtl8125_csi_read(tp, tp->HwPcieSNOffset);
                        tp->pci_cfg_space.pci_sn_h = rtl8125_csi_read(tp, tp->HwPcieSNOffset + 4);
                }

                tp->pci_cfg_is_read = 1;
        }

        /* Set Rx packet filter */
        rtl8125_hw_set_rx_packet_filter(dev);

#ifdef ENABLE_DASH_SUPPORT
        if (tp->DASH && !tp->dash_printer_enabled)
                NICChkTypeEnableDashInterrupt(tp);
#endif

        switch (tp->mcfg) {
        case CFG_METHOD_2:
        case CFG_METHOD_3:
                if (aspm) {
                        RTL_W8(Config5, RTL_R8(Config5) | BIT_0);
                        RTL_W8(Config2, RTL_R8(Config2) | BIT_7);
                } else {
                        RTL_W8(Config2, RTL_R8(Config2) & ~BIT_7);
                        RTL_W8(Config5, RTL_R8(Config5) & ~BIT_0);
                }
                break;
        }

        rtl8125_disable_cfg9346_write(tp);

        udelay(10);
}

static void
rtl8125_hw_start(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        RTL_W8(ChipCmd, CmdTxEnb | CmdRxEnb);

        rtl8125_enable_hw_interrupt(tp, ioaddr);
}


static int
rtl8125_change_mtu(struct net_device *dev,
                   int new_mtu)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        int ret = 0;
        unsigned long flags;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
        if (new_mtu < ETH_ZLEN)
                return -EINVAL;
        else if (new_mtu > tp->max_jumbo_frame_size)
                new_mtu = tp->max_jumbo_frame_size;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)

        spin_lock_irqsave(&tp->lock, flags);
        dev->mtu = new_mtu;
        spin_unlock_irqrestore(&tp->lock, flags);

        if (!netif_running(dev))
                goto out;

        rtl8125_down(dev);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_set_rxbufsize(tp, dev);

        ret = rtl8125_init_ring(dev);

        if (ret < 0) {
                spin_unlock_irqrestore(&tp->lock, flags);
                goto err_out;
        }

#ifdef CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8125_NAPI

        netif_stop_queue(dev);
        netif_carrier_off(dev);
        rtl8125_hw_config(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        rtl8125_set_speed(dev, tp->autoneg, tp->speed, tp->duplex, tp->advertising);

        mod_timer(&tp->esd_timer, jiffies + RTL8125_ESD_TIMEOUT);
        mod_timer(&tp->link_timer, jiffies + RTL8125_LINK_TIMEOUT);
out:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
        netdev_update_features(dev);
#endif

err_out:
        return ret;
}

static inline void
rtl8125_make_unusable_by_asic(struct RxDesc *desc)
{
        desc->addr = 0x0badbadbadbadbadull;
        desc->opts1 &= ~cpu_to_le32(DescOwn | RsvdMask);
}

static void
rtl8125_free_rx_skb(struct rtl8125_private *tp,
                    struct sk_buff **sk_buff,
                    struct RxDesc *desc)
{
        struct pci_dev *pdev = tp->pci_dev;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), tp->rx_buf_sz,
                         PCI_DMA_FROMDEVICE);
        dev_kfree_skb(*sk_buff);
        *sk_buff = NULL;
        rtl8125_make_unusable_by_asic(desc);
}

static inline void
rtl8125_mark_to_asic(struct RxDesc *desc,
                     u32 rx_buf_sz)
{
        u32 eor = le32_to_cpu(desc->opts1) & RingEnd;

        desc->opts1 = cpu_to_le32(DescOwn | eor | rx_buf_sz);
}

static inline void
rtl8125_map_to_asic(struct RxDesc *desc,
                    dma_addr_t mapping,
                    u32 rx_buf_sz)
{
        desc->addr = cpu_to_le64(mapping);
        wmb();
        rtl8125_mark_to_asic(desc, rx_buf_sz);
}

static int
rtl8125_alloc_rx_skb(struct rtl8125_private *tp,
                     struct sk_buff **sk_buff,
                     struct RxDesc *desc,
                     int rx_buf_sz,
                     u8 in_intr)
{
        struct sk_buff *skb;
        dma_addr_t mapping;
        int ret = 0;

        if (in_intr)
                skb = RTL_ALLOC_SKB_INTR(tp, rx_buf_sz + RTK_RX_ALIGN);
        else
                skb = dev_alloc_skb(rx_buf_sz + RTK_RX_ALIGN);

        if (unlikely(!skb))
                goto err_out;

        skb_reserve(skb, RTK_RX_ALIGN);

        mapping = pci_map_single(tp->pci_dev, skb->data, rx_buf_sz,
                                 PCI_DMA_FROMDEVICE);
        if (unlikely(dma_mapping_error(&tp->pci_dev->dev, mapping))) {
                if (unlikely(net_ratelimit()))
                        netif_err(tp, drv, tp->dev, "Failed to map RX DMA!\n");
                goto err_out;
        }

        *sk_buff = skb;
        rtl8125_map_to_asic(desc, mapping, rx_buf_sz);
out:
        return ret;

err_out:
        if (skb)
                dev_kfree_skb(skb);
        ret = -ENOMEM;
        rtl8125_make_unusable_by_asic(desc);
        goto out;
}

static void
rtl8125_rx_clear(struct rtl8125_private *tp)
{
        int i;

        for (i = 0; i < NUM_RX_DESC; i++) {
                if (tp->Rx_skbuff[i])
                        rtl8125_free_rx_skb(tp, tp->Rx_skbuff + i,
                                            tp->RxDescArray + i);
        }
}

static u32
rtl8125_rx_fill(struct rtl8125_private *tp,
                struct net_device *dev,
                u32 start,
                u32 end,
                u8 in_intr)
{
        u32 cur;

        for (cur = start; end - cur > 0; cur++) {
                int ret, i = cur % NUM_RX_DESC;

                if (tp->Rx_skbuff[i])
                        continue;

                ret = rtl8125_alloc_rx_skb(tp, tp->Rx_skbuff + i,
                                           tp->RxDescArray + i,
                                           tp->rx_buf_sz,
                                           in_intr);
                if (ret < 0)
                        break;
        }
        return cur - start;
}

static inline void
rtl8125_mark_as_last_descriptor(struct RxDesc *desc)
{
        desc->opts1 |= cpu_to_le32(RingEnd);
}

static void
rtl8125_desc_addr_fill(struct rtl8125_private *tp)
{
        void __iomem *ioaddr = tp->mmio_addr;

        if (!tp->TxPhyAddr || !tp->RxPhyAddr)
                return;

        RTL_W32(TxDescStartAddrLow, ((u64) tp->TxPhyAddr & DMA_BIT_MASK(32)));
        RTL_W32(TxDescStartAddrHigh, ((u64) tp->TxPhyAddr >> 32));
        RTL_W32(RxDescAddrLow, ((u64) tp->RxPhyAddr & DMA_BIT_MASK(32)));
        RTL_W32(RxDescAddrHigh, ((u64) tp->RxPhyAddr >> 32));
}

static void
rtl8125_tx_desc_init(struct rtl8125_private *tp)
{
        int i = 0;

        memset(tp->TxDescArray, 0x0, NUM_TX_DESC * sizeof(struct TxDesc));

        for (i = 0; i < NUM_TX_DESC; i++) {
                if (i == (NUM_TX_DESC - 1))
                        tp->TxDescArray[i].opts1 = cpu_to_le32(RingEnd);
        }
}

static void
rtl8125_rx_desc_init(struct rtl8125_private *tp)
{
        memset(tp->RxDescArray, 0x0, NUM_RX_DESC * sizeof(struct RxDesc));
}

static int
rtl8125_init_ring(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        rtl8125_init_ring_indexes(tp);

        memset(tp->tx_skb, 0x0, NUM_TX_DESC * sizeof(struct ring_info));
        memset(tp->Rx_skbuff, 0x0, NUM_RX_DESC * sizeof(struct sk_buff *));

        rtl8125_tx_desc_init(tp);
        rtl8125_rx_desc_init(tp);

        if (rtl8125_rx_fill(tp, dev, 0, NUM_RX_DESC, 0) != NUM_RX_DESC)
                goto err_out;

        rtl8125_mark_as_last_descriptor(tp->RxDescArray + NUM_RX_DESC - 1);

        return 0;

err_out:
        rtl8125_rx_clear(tp);
        return -ENOMEM;
}

static void
rtl8125_unmap_tx_skb(struct pci_dev *pdev,
                     struct ring_info *tx_skb,
                     struct TxDesc *desc)
{
        unsigned int len = tx_skb->len;

        pci_unmap_single(pdev, le64_to_cpu(desc->addr), len, PCI_DMA_TODEVICE);
        desc->opts1 = 0x00;
        desc->opts2 = 0x00;
        desc->addr = 0x00;
        tx_skb->len = 0;
}

static void rtl8125_tx_clear_range(struct rtl8125_private *tp, u32 start,
                                   unsigned int n)
{
        unsigned int i;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
        struct net_device *dev = tp->dev;
#endif

        for (i = 0; i < n; i++) {
                unsigned int entry = (start + i) % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                unsigned int len = tx_skb->len;

                if (len) {
                        struct sk_buff *skb = tx_skb->skb;

                        rtl8125_unmap_tx_skb(tp->pci_dev, tx_skb,
                                             tp->TxDescArray + entry);
                        if (skb) {
                                RTLDEV->stats.tx_dropped++;
                                dev_kfree_skb_any(skb);
                                tx_skb->skb = NULL;
                        }
                }
        }
}

static void
rtl8125_tx_clear(struct rtl8125_private *tp)
{
        rtl8125_tx_clear_range(tp, tp->dirty_tx, NUM_TX_DESC);
        tp->cur_tx = tp->dirty_tx = 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8125_schedule_work(struct net_device *dev, void (*task)(void *))
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        struct rtl8125_private *tp = netdev_priv(dev);

        INIT_WORK(&tp->task, task, dev);
        schedule_delayed_work(&tp->task, 4);
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
}

#define rtl8125_cancel_schedule_work(a)

#else
static void rtl8125_schedule_work(struct net_device *dev, work_func_t task)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        INIT_DELAYED_WORK(&tp->task, task);
        schedule_delayed_work(&tp->task, 4);
}

static void rtl8125_cancel_schedule_work(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);

        cancel_delayed_work_sync(&tp->task);
}
#endif

static void
rtl8125_wait_for_quiescence(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;

        synchronize_irq(dev->irq);

        /* Wait for any pending NAPI task to complete */
#ifdef CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_DISABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8125_NAPI

        rtl8125_irq_mask_and_ack(tp, ioaddr);

#ifdef CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_ENABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8125_NAPI
}

#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8125_reinit_task(void *_data)
#else
static void rtl8125_reinit_task(struct work_struct *work)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        struct net_device *dev = _data;
#else
        struct rtl8125_private *tp =
                container_of(work, struct rtl8125_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        int ret;

        if (netif_running(dev)) {
                rtl8125_wait_for_quiescence(dev);
                rtl8125_close(dev);
        }

        ret = rtl8125_open(dev);
        if (unlikely(ret < 0)) {
                if (unlikely(net_ratelimit())) {
                        struct rtl8125_private *tp = netdev_priv(dev);

                        if (netif_msg_drv(tp)) {
                                printk(PFX KERN_ERR
                                       "%s: reinit failure (status = %d)."
                                       " Rescheduling.\n", dev->name, ret);
                        }
                }
                rtl8125_schedule_work(dev, rtl8125_reinit_task);
        }
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static void rtl8125_reset_task(void *_data)
{
        struct net_device *dev = _data;
        struct rtl8125_private *tp = netdev_priv(dev);
#else
static void rtl8125_reset_task(struct work_struct *work)
{
        struct rtl8125_private *tp =
                container_of(work, struct rtl8125_private, task.work);
        struct net_device *dev = tp->dev;
#endif
        u32 budget = ~(u32)0;
        unsigned long flags;

        if (!netif_running(dev))
                return;

        rtl8125_wait_for_quiescence(dev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
        rtl8125_rx_interrupt(dev, tp, tp->mmio_addr, &budget);
#else
        rtl8125_rx_interrupt(dev, tp, tp->mmio_addr, budget);
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_tx_clear(tp);

        if (tp->dirty_rx == tp->cur_rx) {
                rtl8125_rx_clear(tp);
                rtl8125_init_ring(dev);
                rtl8125_set_speed(dev, tp->autoneg, tp->speed, tp->duplex, tp->advertising);
                spin_unlock_irqrestore(&tp->lock, flags);
        } else {
                spin_unlock_irqrestore(&tp->lock, flags);
                if (unlikely(net_ratelimit())) {
                        struct rtl8125_private *tp = netdev_priv(dev);

                        if (netif_msg_intr(tp)) {
                                printk(PFX KERN_EMERG
                                       "%s: Rx buffers shortage\n", dev->name);
                        }
                }
                rtl8125_schedule_work(dev, rtl8125_reset_task);
        }
}

static void
rtl8125_tx_timeout(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        spin_lock_irqsave(&tp->lock, flags);
        netif_stop_queue(dev);
        netif_carrier_off(dev);
        rtl8125_hw_reset(dev);
        spin_unlock_irqrestore(&tp->lock, flags);

        /* Let's wait a bit while any (async) irq lands on */
        rtl8125_schedule_work(dev, rtl8125_reset_task);
}

static int
rtl8125_xmit_frags(struct rtl8125_private *tp,
                   struct sk_buff *skb,
                   u32 opts1,
                   u32 opts2)
{
        struct skb_shared_info *info = skb_shinfo(skb);
        unsigned int cur_frag, entry;
        struct TxDesc *txd = NULL;
        const unsigned char nr_frags = info->nr_frags;
        unsigned long PktLenCnt = 0;

        entry = tp->cur_tx;
        for (cur_frag = 0; cur_frag < nr_frags; cur_frag++) {
                skb_frag_t *frag = info->frags + cur_frag;
                dma_addr_t mapping;
                u32 status, len;
                void *addr;

                entry = (entry + 1) % NUM_TX_DESC;

                txd = tp->TxDescArray + entry;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
                len = frag->size;
                addr = ((void *) page_address(frag->page)) + frag->page_offset;
#else
                len = skb_frag_size(frag);
                addr = skb_frag_address(frag);
#endif
                if ((cur_frag == nr_frags - 1) &&
                    (opts1 & (GiantSendv4|GiantSendv6)) &&
                    PktLenCnt < ETH_FRAME_LEN &&
                    len > 1) {
                        len -= 1;
                        mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

                        if (unlikely(dma_mapping_error(&tp->pci_dev->dev, mapping))) {
                                if (unlikely(net_ratelimit()))
                                        netif_err(tp, drv, tp->dev,
                                                  "Failed to map TX fragments DMA!\n");
                                goto err_out;
                        }

                        /* anti gcc 2.95.3 bugware (sic) */
                        status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

                        txd->addr = cpu_to_le64(mapping);

                        tp->tx_skb[entry].len = len;

                        txd->opts1 = cpu_to_le32(status);
                        txd->opts2 = cpu_to_le32(opts2);

                        //second txd
                        addr += len;
                        len = 1;
                        entry = (entry + 1) % NUM_TX_DESC;
                        txd = tp->TxDescArray + entry;
                        cur_frag += 1;
                }

                mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);

                if (unlikely(dma_mapping_error(&tp->pci_dev->dev, mapping))) {
                        if (unlikely(net_ratelimit()))
                                netif_err(tp, drv, tp->dev,
                                          "Failed to map TX fragments DMA!\n");
                        goto err_out;
                }

                /* anti gcc 2.95.3 bugware (sic) */
                status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

                txd->addr = cpu_to_le64(mapping);

                tp->tx_skb[entry].len = len;

                txd->opts1 = cpu_to_le32(status);
                txd->opts2 = cpu_to_le32(opts2);

                PktLenCnt += len;
        }

        if (cur_frag) {
                tp->tx_skb[entry].skb = skb;
                wmb();
                txd->opts1 |= cpu_to_le32(LastFrag);
        }

        return cur_frag;

err_out:
        rtl8125_tx_clear_range(tp, tp->cur_tx + 1, cur_frag);
        return -EIO;
}

static inline
__be16 get_protocol(struct sk_buff *skb)
{
        __be16 protocol;

        if (skb->protocol == htons(ETH_P_8021Q))
                protocol = vlan_eth_hdr(skb)->h_vlan_encapsulated_proto;
        else
                protocol = skb->protocol;

        return protocol;
}

static inline u32
rtl8125_tx_csum(struct sk_buff *skb,
                struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        u32 csum_cmd = 0;
        u8 sw_calc_csum = FALSE;

        if (skb->ip_summed == CHECKSUM_PARTIAL) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
                const struct iphdr *ip = skb->nh.iph;

                if (dev->features & NETIF_F_IP_CSUM) {
                        if (ip->protocol == IPPROTO_TCP)
                                csum_cmd = tp->tx_ip_csum_cmd | tp->tx_tcp_csum_cmd;
                        else if (ip->protocol == IPPROTO_UDP)
                                csum_cmd = tp->tx_ip_csum_cmd | tp->tx_udp_csum_cmd;
                        else if (ip->protocol == IPPROTO_IP)
                                csum_cmd = tp->tx_ip_csum_cmd;
                }
#else
                u8 ip_protocol = IPPROTO_RAW;

                switch (get_protocol(skb)) {
                case  __constant_htons(ETH_P_IP):
                        if (dev->features & NETIF_F_IP_CSUM) {
                                ip_protocol = ip_hdr(skb)->protocol;
                                csum_cmd = tp->tx_ip_csum_cmd;
                        }
                        break;
                case  __constant_htons(ETH_P_IPV6):
                        if (dev->features & NETIF_F_IPV6_CSUM) {
                                u32 transport_offset = (u32)skb_transport_offset(skb);
                                if (transport_offset > 0 && transport_offset <= TCPHO_MAX) {
                                        ip_protocol = ipv6_hdr(skb)->nexthdr;
                                        csum_cmd = tp->tx_ipv6_csum_cmd;
                                        csum_cmd |= transport_offset << TCPHO_SHIFT;
                                }
                        }
                        break;
                default:
                        if (unlikely(net_ratelimit()))
                                dprintk("checksum_partial proto=%x!\n", skb->protocol);
                        break;
                }

                if (ip_protocol == IPPROTO_TCP)
                        csum_cmd |= tp->tx_tcp_csum_cmd;
                else if (ip_protocol == IPPROTO_UDP)
                        csum_cmd |= tp->tx_udp_csum_cmd;
#endif
                if (csum_cmd == 0) {
                        sw_calc_csum = TRUE;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
                        WARN_ON(1); /* we need a WARN() */
#endif
                }
        }

        if (tp->ShortPacketSwChecksum && skb->len < 60 && csum_cmd != 0)
                sw_calc_csum = TRUE;

        if (sw_calc_csum) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
                skb_checksum_help(&skb, 0);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) && LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
                skb_checksum_help(skb, 0);
#else
                skb_checksum_help(skb);
#endif
                csum_cmd = 0;
        }

        return csum_cmd;
}

static int
rtl8125_sw_padding_short_pkt(struct rtl8125_private *tp,
                             struct sk_buff *skb,
                             u32 opts1,
                             u32 opts2)
{
        unsigned int entry;
        dma_addr_t mapping;
        u32 status, len;
        void *addr;
        struct TxDesc *txd = NULL;
        int ret = 0;

        if (skb->len >= ETH_ZLEN)
                goto out;

        entry = tp->cur_tx;

        entry = (entry + 1) % NUM_TX_DESC;

        txd = tp->TxDescArray + entry;
        len = ETH_ZLEN - skb->len;
        addr = tp->ShortPacketEmptyBuffer;
        mapping = pci_map_single(tp->pci_dev, addr, len, PCI_DMA_TODEVICE);
        if (unlikely(dma_mapping_error(&tp->pci_dev->dev, mapping))) {
                if (unlikely(net_ratelimit()))
                        netif_err(tp, drv, tp->dev,
                                  "Failed to map Short Packet Buffer DMA!\n");
                ret = -ENOMEM;
                goto out;
        }
        status = opts1 | len | (RingEnd * !((entry + 1) % NUM_TX_DESC));

        txd->addr = cpu_to_le64(mapping);

        txd->opts1 = cpu_to_le32(status);
        txd->opts2 = cpu_to_le32(opts2);

        wmb();
        txd->opts1 |= cpu_to_le32(LastFrag);
out:
        return ret;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
/* r8169_csum_workaround()
  * The hw limites the value the transport offset. When the offset is out of the
  * range, calculate the checksum by sw.
  */
static void r8125_csum_workaround(struct rtl8125_private *tp,
                                  struct sk_buff *skb)
{
        if (skb_shinfo(skb)->gso_size) {
                netdev_features_t features = tp->dev->features;
                struct sk_buff *segs, *nskb;

                features &= ~(NETIF_F_SG | NETIF_F_IPV6_CSUM | NETIF_F_TSO6);
                segs = skb_gso_segment(skb, features);
                if (IS_ERR(segs) || !segs)
                        goto drop;

                do {
                        nskb = segs;
                        segs = segs->next;
                        nskb->next = NULL;
                        rtl8125_start_xmit(nskb, tp->dev);
                } while (segs);

                dev_consume_skb_any(skb);
        } else if (skb->ip_summed == CHECKSUM_PARTIAL) {
                if (skb_checksum_help(skb) < 0)
                        goto drop;

                rtl8125_start_xmit(skb, tp->dev);
        } else {
                struct net_device_stats *stats;

drop:
                stats = &tp->dev->stats;
                stats->tx_dropped++;
                dev_kfree_skb_any(skb);
        }
}

/* msdn_giant_send_check()
 * According to the document of microsoft, the TCP Pseudo Header excludes the
 * packet length for IPv6 TCP large packets.
 */
static int msdn_giant_send_check(struct sk_buff *skb)
{
        const struct ipv6hdr *ipv6h;
        struct tcphdr *th;
        int ret;

        ret = skb_cow_head(skb, 0);
        if (ret)
                return ret;

        ipv6h = ipv6_hdr(skb);
        th = tcp_hdr(skb);

        th->check = 0;
        th->check = ~tcp_v6_check(0, &ipv6h->saddr, &ipv6h->daddr, 0);

        return ret;
}
#endif

static int
rtl8125_start_xmit(struct sk_buff *skb,
                   struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned int entry;
        struct TxDesc *txd;
        void __iomem *ioaddr = tp->mmio_addr;
        dma_addr_t mapping;
        u32 len;
        u32 opts1;
        u32 opts2;
        int ret = NETDEV_TX_OK;
        unsigned long flags, large_send;
        int frags;

        spin_lock_irqsave(&tp->lock, flags);

        if (unlikely(TX_BUFFS_AVAIL(tp) < skb_shinfo(skb)->nr_frags)) {
                if (netif_msg_drv(tp)) {
                        printk(KERN_ERR
                               "%s: BUG! Tx Ring full when queue awake!\n",
                               dev->name);
                }
                goto err_stop;
        }

        entry = tp->cur_tx % NUM_TX_DESC;
        txd = tp->TxDescArray + entry;

        if (unlikely(le32_to_cpu(txd->opts1) & DescOwn))
                goto err_stop;

        opts1 = DescOwn;
        opts2 = rtl8125_tx_vlan_tag(tp, skb);

        large_send = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        if (dev->features & (NETIF_F_TSO | NETIF_F_TSO6)) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
                u32 mss = skb_shinfo(skb)->tso_size;
#else
                u32 mss = skb_shinfo(skb)->gso_size;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)

                /* TCP Segmentation Offload (or TCP Large Send) */
                if (mss) {
                        u32 transport_offset = (u32)skb_transport_offset(skb);
                        assert((transport_offset%2) == 0);
                        switch (get_protocol(skb)) {
                        case __constant_htons(ETH_P_IP):
                                if (transport_offset <= 128) {
                                        opts1 |= GiantSendv4;
                                        opts1 |= transport_offset << GTTCPHO_SHIFT;
                                        opts2 |= min(mss, MSS_MAX) << 18;
                                        large_send = 1;
                                }
                                break;
                        case __constant_htons(ETH_P_IPV6):
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
                                if (msdn_giant_send_check(skb)) {
                                        spin_unlock_irqrestore(&tp->lock, flags);
                                        r8125_csum_workaround(tp, skb);
                                        goto out;
                                }
#endif
                                if (transport_offset <= 128) {
                                        opts1 |= GiantSendv6;
                                        opts1 |= transport_offset << GTTCPHO_SHIFT;
                                        opts2 |= min(mss, MSS_MAX) << 18;
                                        large_send = 1;
                                }
                                break;
                        default:
                                if (unlikely(net_ratelimit()))
                                        dprintk("tso proto=%x!\n", skb->protocol);
                                break;
                        }

                        if (large_send == 0)
                                goto err_dma_0;
                }
        }
#endif //LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)

        if (large_send == 0) {
                if (skb->ip_summed == CHECKSUM_PARTIAL) {
                        opts2 |= rtl8125_tx_csum(skb, dev);
                }
        }

        frags = rtl8125_xmit_frags(tp, skb, opts1, opts2);
        if (unlikely(frags < 0))
                goto err_dma_0;
        if (frags) {
                len = skb_headlen(skb);
                opts1 |= FirstFrag;
        } else {
                len = skb->len;

                tp->tx_skb[entry].skb = skb;

                if (tp->UseSwPaddingShortPkt && len < 60) {
                        if (unlikely(rtl8125_sw_padding_short_pkt(tp, skb, opts1, opts2)))
                                goto err_dma_1;
                        opts1 |= FirstFrag;
                        frags++;
                } else {
                        opts1 |= FirstFrag | LastFrag;
                }
        }

        opts1 |= len | (RingEnd * !((entry + 1) % NUM_TX_DESC));
        mapping = pci_map_single(tp->pci_dev, skb->data, len, PCI_DMA_TODEVICE);
        if (unlikely(dma_mapping_error(&tp->pci_dev->dev, mapping))) {
                if (unlikely(net_ratelimit()))
                        netif_err(tp, drv, dev, "Failed to map TX DMA!\n");
                goto err_dma_1;
        }
        tp->tx_skb[entry].len = len;
        txd->addr = cpu_to_le64(mapping);
        txd->opts2 = cpu_to_le32(opts2);
        txd->opts1 = cpu_to_le32(opts1&~DescOwn);
        wmb();
        txd->opts1 = cpu_to_le32(opts1);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
        dev->trans_start = jiffies;
#else
        skb_tx_timestamp(skb);
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)

        tp->cur_tx += frags + 1;

        wmb();

        RTL_W16(TPPOLL_8125, BIT_0);    /* set polling bit */

        if (TX_BUFFS_AVAIL(tp) < MAX_SKB_FRAGS) {
                netif_stop_queue(dev);
                smp_rmb();
                if (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)
                        netif_wake_queue(dev);
        }

        spin_unlock_irqrestore(&tp->lock, flags);
out:
        return ret;
err_dma_1:
        tp->tx_skb[entry].skb = NULL;
        rtl8125_tx_clear_range(tp, tp->cur_tx + 1, frags);
err_dma_0:
        RTLDEV->stats.tx_dropped++;
        spin_unlock_irqrestore(&tp->lock, flags);
        dev_kfree_skb_any(skb);
        ret = NETDEV_TX_OK;
        goto out;
err_stop:
        netif_stop_queue(dev);
        ret = NETDEV_TX_BUSY;
        RTLDEV->stats.tx_dropped++;

        spin_unlock_irqrestore(&tp->lock, flags);
        goto out;
}

static void
rtl8125_tx_interrupt(struct net_device *dev,
                     struct rtl8125_private *tp,
                     void __iomem *ioaddr)
{
        unsigned int dirty_tx, tx_left;

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        dirty_tx = tp->dirty_tx;
        smp_rmb();
        tx_left = tp->cur_tx - dirty_tx;

        while (tx_left > 0) {
                unsigned int entry = dirty_tx % NUM_TX_DESC;
                struct ring_info *tx_skb = tp->tx_skb + entry;
                u32 len = tx_skb->len;
                u32 status;

                rmb();
                status = le32_to_cpu(tp->TxDescArray[entry].opts1);
                if (status & DescOwn)
                        break;

                RTLDEV->stats.tx_bytes += len;
                RTLDEV->stats.tx_packets++;

                rtl8125_unmap_tx_skb(tp->pci_dev,
                                     tx_skb,
                                     tp->TxDescArray + entry);

                if (tx_skb->skb!=NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
                        dev_consume_skb_any(tx_skb->skb);
#else
                        dev_kfree_skb_any(tx_skb->skb);
#endif
                        tx_skb->skb = NULL;
                }
                dirty_tx++;
                tx_left--;
        }

        if (tp->dirty_tx != dirty_tx) {
                tp->dirty_tx = dirty_tx;
                smp_wmb();
                if (netif_queue_stopped(dev) &&
                    (TX_BUFFS_AVAIL(tp) >= MAX_SKB_FRAGS)) {
                        netif_wake_queue(dev);
                }
                smp_rmb();
                if (tp->cur_tx != dirty_tx) {
                        RTL_W16(TPPOLL_8125, BIT_0);
                }
        }
}

static inline int
rtl8125_fragmented_frame(u32 status)
{
        return (status & (FirstFrag | LastFrag)) != (FirstFrag | LastFrag);
}

static inline void
rtl8125_rx_csum(struct rtl8125_private *tp,
                struct sk_buff *skb,
                struct RxDesc *desc)
{
        u32 opts1 = le32_to_cpu(desc->opts1);
        u32 opts2 = le32_to_cpu(desc->opts2);

        /* rx csum offload for RTL8125 */
        if (((opts2 & RxV4F) && !(opts1 & RxIPF)) || (opts2 & RxV6F)) {
                if (((opts1 & RxTCPT) && !(opts1 & RxTCPF)) ||
                    ((opts1 & RxUDPT) && !(opts1 & RxUDPF)))
                        skb->ip_summed = CHECKSUM_UNNECESSARY;
                else
                        skb->ip_summed = CHECKSUM_NONE;
        } else
                skb->ip_summed = CHECKSUM_NONE;
}

static inline int
rtl8125_try_rx_copy(struct rtl8125_private *tp,
                    struct sk_buff **sk_buff,
                    int pkt_size,
                    struct RxDesc *desc,
                    int rx_buf_sz)
{
        int ret = -1;

        if (pkt_size < rx_copybreak) {
                struct sk_buff *skb;

                skb = RTL_ALLOC_SKB_INTR(tp, pkt_size + RTK_RX_ALIGN);
                if (skb) {
                        u8 *data;

                        data = sk_buff[0]->data;
                        skb_reserve(skb, RTK_RX_ALIGN);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                        prefetch(data - RTK_RX_ALIGN);
#endif
                        eth_copy_and_sum(skb, data, pkt_size, 0);
                        *sk_buff = skb;
                        rtl8125_mark_to_asic(desc, rx_buf_sz);
                        ret = 0;
                }
        }
        return ret;
}

static inline void
rtl8125_rx_skb(struct rtl8125_private *tp,
               struct sk_buff *skb)
{
#ifdef CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
        netif_receive_skb(skb);
#else
        napi_gro_receive(&tp->napi, skb);
#endif
#else
        netif_rx(skb);
#endif
}

static int
rtl8125_rx_interrupt(struct net_device *dev,
                     struct rtl8125_private *tp,
                     void __iomem *ioaddr, napi_budget budget)
{
        unsigned int cur_rx, rx_left;
        unsigned int delta, count = 0;
        unsigned int entry;
        struct RxDesc *desc;
        u32 status;
        u32 rx_quota;

        assert(dev != NULL);
        assert(tp != NULL);
        assert(ioaddr != NULL);

        if ((tp->RxDescArray == NULL) || (tp->Rx_skbuff == NULL))
                goto rx_out;

        rx_quota = RTL_RX_QUOTA(budget);
        cur_rx = tp->cur_rx;
        entry = cur_rx % NUM_RX_DESC;
        desc = tp->RxDescArray + entry;
        rx_left = NUM_RX_DESC + tp->dirty_rx - cur_rx;
        rx_left = rtl8125_rx_quota(rx_left, (u32)rx_quota);

        for (; rx_left > 0; rx_left--) {
                rmb();
                status = le32_to_cpu(desc->opts1);
                if (status & DescOwn)
                        break;
                if (unlikely(status & RxRES)) {
                        if (netif_msg_rx_err(tp)) {
                                printk(KERN_INFO
                                       "%s: Rx ERROR. status = %08x\n",
                                       dev->name, status);
                        }

                        RTLDEV->stats.rx_errors++;

                        if (status & (RxRWT | RxRUNT))
                                RTLDEV->stats.rx_length_errors++;
                        if (status & RxCRC)
                                RTLDEV->stats.rx_crc_errors++;
                        if (dev->features & NETIF_F_RXALL)
                                goto process_pkt;

                        rtl8125_mark_to_asic(desc, tp->rx_buf_sz);
                } else {
                        struct sk_buff *skb;
                        int pkt_size;
                        void (*pci_action)(struct pci_dev *, dma_addr_t,
                                           size_t, int);

process_pkt:
                        if (likely(!(dev->features & NETIF_F_RXFCS)))
                                pkt_size = (status & 0x00003fff) - 4;
                        else
                                pkt_size = status & 0x00003fff;

                        /*
                         * The driver does not support incoming fragmented
                         * frames. They are seen as a symptom of over-mtu
                         * sized frames.
                         */
                        if (unlikely(rtl8125_fragmented_frame(status))) {
                                RTLDEV->stats.rx_dropped++;
                                RTLDEV->stats.rx_length_errors++;
                                rtl8125_mark_to_asic(desc, tp->rx_buf_sz);
                                continue;
                        }

                        skb = tp->Rx_skbuff[entry];
                        if (tp->cp_cmd & RxChkSum)
                                rtl8125_rx_csum(tp, skb, desc);

                        pci_dma_sync_single_for_cpu(tp->pci_dev,
                                                    le64_to_cpu(desc->addr), tp->rx_buf_sz,
                                                    PCI_DMA_FROMDEVICE);

                        pci_action = pci_dma_sync_single_for_device;
                        if (rtl8125_try_rx_copy(tp, &skb, pkt_size,
                                                desc, tp->rx_buf_sz)) {
                                pci_action = pci_unmap_single;
                                tp->Rx_skbuff[entry] = NULL;
                        }

                        pci_action(tp->pci_dev, le64_to_cpu(desc->addr),
                                   tp->rx_buf_sz, PCI_DMA_FROMDEVICE);

                        skb->dev = dev;
                        skb_put(skb, pkt_size);
                        skb->protocol = eth_type_trans(skb, dev);

                        if (skb->pkt_type == PACKET_MULTICAST)
                                RTLDEV->stats.multicast++;

                        if (rtl8125_rx_vlan_skb(tp, desc, skb) < 0)
                                rtl8125_rx_skb(tp, skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
                        dev->last_rx = jiffies;
#endif //LINUX_VERSION_CODE < KERNEL_VERSION(4,11,0)
                        RTLDEV->stats.rx_bytes += pkt_size;
                        RTLDEV->stats.rx_packets++;
                }

                cur_rx++;
                entry = cur_rx % NUM_RX_DESC;
                desc = tp->RxDescArray + entry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,37)
                prefetch(desc);
#endif
        }

        count = cur_rx - tp->cur_rx;
        tp->cur_rx = cur_rx;

        delta = rtl8125_rx_fill(tp, dev, tp->dirty_rx, tp->cur_rx, 1);
        if (!delta && count && netif_msg_intr(tp))
                printk(KERN_INFO "%s: no Rx buffer allocated\n", dev->name);
        tp->dirty_rx += delta;

        /*
         * FIXME: until there is periodic timer to try and refill the ring,
         * a temporary shortage may definitely kill the Rx process.
         * - disable the asic to try and avoid an overflow and kick it again
         *   after refill ?
         * - how do others driver handle this condition (Uh oh...).
         */
        if ((tp->dirty_rx + NUM_RX_DESC == tp->cur_rx) && netif_msg_intr(tp))
                printk(KERN_EMERG "%s: Rx buffers exhausted\n", dev->name);

rx_out:
        return count;
}

/*
 *The interrupt handler does all of the Rx thread work and cleans up after
 *the Tx thread.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
static irqreturn_t rtl8125_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
#else
static irqreturn_t rtl8125_interrupt(int irq, void *dev_instance)
#endif
{
        struct net_device *dev = (struct net_device *) dev_instance;
        struct rtl8125_private *tp = netdev_priv(dev);
        void __iomem *ioaddr = tp->mmio_addr;
        u32 status;
        int handled = 0;

        do {
                status = RTL_R32(ISR0_8125);

                if (!(tp->features & RTL_FEATURE_MSI)) {
                        /* hotplug/major error/no more work/shared irq */
                        if (!status)
                                break;

                        if ((status == 0xFFFFFFFF))
                                break;

                        if (!(status & (tp->intr_mask | tp->timer_intr_mask)))
                                break;
                }

                handled = 1;

                rtl8125_disable_hw_interrupt(tp, ioaddr);

                RTL_W32(ISR0_8125, status&~RxFIFOOver);

#ifdef ENABLE_DASH_SUPPORT
                if (tp->DASH) {
                        if (HW_DASH_SUPPORT_TYPE_3(tp)) {
                                u8 DashIntType2Status;

                                if (status & ISRIMR_DASH_INTR_CMAC_RESET)
                                        tp->CmacResetIntr = TRUE;

                                DashIntType2Status = RTL_CMAC_R8(CMAC_IBISR0);
                                if (DashIntType2Status & ISRIMR_DASH_TYPE2_ROK) {
                                        tp->RcvFwDashOkEvt = TRUE;
                                }
                                if (DashIntType2Status & ISRIMR_DASH_TYPE2_TOK) {
                                        tp->SendFwHostOkEvt = TRUE;
                                }
                                if (DashIntType2Status & ISRIMR_DASH_TYPE2_RX_DISABLE_IDLE) {
                                        tp->DashFwDisableRx = TRUE;
                                }

                                RTL_CMAC_W8(CMAC_IBISR0, DashIntType2Status);
                        }
                }
#endif

#ifdef CONFIG_R8125_NAPI
                if (status & tp->intr_mask || tp->keep_intr_cnt-- > 0) {
                        if (status & tp->intr_mask)
                                tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;

                        if (likely(RTL_NETIF_RX_SCHEDULE_PREP(dev, &tp->napi)))
                                __RTL_NETIF_RX_SCHEDULE(dev, &tp->napi);
                        else if (netif_msg_intr(tp))
                                printk(KERN_INFO "%s: interrupt %04x in poll\n",
                                       dev->name, status);
                } else {
                        tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
                        rtl8125_switch_to_hw_interrupt(tp, ioaddr);
                }
#else
                if (status & tp->intr_mask || tp->keep_intr_cnt-- > 0) {
                        u32 budget = ~(u32)0;

                        if (status & tp->intr_mask)
                                tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
                        rtl8125_rx_interrupt(dev, tp, tp->mmio_addr, &budget);
#else
                        rtl8125_rx_interrupt(dev, tp, tp->mmio_addr, budget);
#endif	//LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
                        rtl8125_tx_interrupt(dev, tp, ioaddr);

#ifdef ENABLE_DASH_SUPPORT
                        if (tp->DASH) {
                                struct net_device *dev = tp->dev;

                                HandleDashInterrupt(dev);
                        }
#endif

                        rtl8125_switch_to_timer_interrupt(tp, ioaddr);
                } else {
                        tp->keep_intr_cnt = RTK_KEEP_INTERRUPT_COUNT;
                        rtl8125_switch_to_hw_interrupt(tp, ioaddr);
                }
#endif

        } while (false);

        return IRQ_RETVAL(handled);
}

#ifdef CONFIG_R8125_NAPI
static int rtl8125_poll(napi_ptr napi, napi_budget budget)
{
        struct rtl8125_private *tp = RTL_GET_PRIV(napi, struct rtl8125_private);
        void __iomem *ioaddr = tp->mmio_addr;
        RTL_GET_NETDEV(tp)
        unsigned int work_to_do = RTL_NAPI_QUOTA(budget, dev);
        unsigned int work_done;
        unsigned long flags;

        work_done = rtl8125_rx_interrupt(dev, tp, ioaddr, budget);

        spin_lock_irqsave(&tp->lock, flags);
        rtl8125_tx_interrupt(dev, tp, ioaddr);
        spin_unlock_irqrestore(&tp->lock, flags);

        RTL_NAPI_QUOTA_UPDATE(dev, work_done, budget);

        if (work_done < work_to_do) {
#ifdef ENABLE_DASH_SUPPORT
                if (tp->DASH) {
                        struct net_device *dev = tp->dev;

                        spin_lock_irqsave(&tp->lock, flags);
                        HandleDashInterrupt(dev);
                        spin_unlock_irqrestore(&tp->lock, flags);
                }
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0)
                if (RTL_NETIF_RX_COMPLETE(dev, napi, work_done) == FALSE) return RTL_NAPI_RETURN_VALUE;
#else
                RTL_NETIF_RX_COMPLETE(dev, napi, work_done);
#endif
                /*
                 * 20040426: the barrier is not strictly required but the
                 * behavior of the irq handler could be less predictable
                 * without it. Btw, the lack of flush for the posted pci
                 * write is safe - FR
                 */
                smp_wmb();

                rtl8125_switch_to_timer_interrupt(tp, ioaddr);
        }

        return RTL_NAPI_RETURN_VALUE;
}
#endif//CONFIG_R8125_NAPI

static void rtl8125_down(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;

        rtl8125_delete_esd_timer(dev, &tp->esd_timer);

        rtl8125_delete_link_timer(dev, &tp->link_timer);

#ifdef CONFIG_R8125_NAPI
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        RTL_NAPI_DISABLE(dev, &tp->napi);
#endif
#endif//CONFIG_R8125_NAPI

        netif_stop_queue(dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        /* Give a racing hard_start_xmit a few cycles to complete. */
        synchronize_rcu();  /* FIXME: should this be synchronize_irq()? */
#endif

        spin_lock_irqsave(&tp->lock, flags);

        netif_carrier_off(dev);

        rtl8125_hw_reset(dev);

        spin_unlock_irqrestore(&tp->lock, flags);

        synchronize_irq(dev->irq);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_tx_clear(tp);

        rtl8125_rx_clear(tp);

        spin_unlock_irqrestore(&tp->lock, flags);
}

static int rtl8125_close(struct net_device *dev)
{
        struct rtl8125_private *tp = netdev_priv(dev);
        struct pci_dev *pdev = tp->pci_dev;
        unsigned long flags;

        if (tp->TxDescArray!=NULL && tp->RxDescArray!=NULL) {
                rtl8125_cancel_schedule_work(dev);

                rtl8125_down(dev);

                spin_lock_irqsave(&tp->lock, flags);

                rtl8125_hw_d3_para(dev);

                rtl8125_powerdown_pll(dev);

                spin_unlock_irqrestore(&tp->lock, flags);

                free_irq(dev->irq, dev);

                pci_free_consistent(pdev, R8125_RX_RING_BYTES, tp->RxDescArray,
                                    tp->RxPhyAddr);
                pci_free_consistent(pdev, R8125_TX_RING_BYTES, tp->TxDescArray,
                                    tp->TxPhyAddr);
                tp->TxDescArray = NULL;
                tp->RxDescArray = NULL;

                if (tp->ShortPacketEmptyBuffer != NULL) {
                        pci_free_consistent(pdev, SHORT_PACKET_PADDING_BUF_SIZE, tp->ShortPacketEmptyBuffer,
                                            tp->ShortPacketEmptyBufferPhy);
                        tp->ShortPacketEmptyBuffer = NULL;
                }
        } else {
                spin_lock_irqsave(&tp->lock, flags);

                rtl8125_hw_d3_para(dev);

                rtl8125_powerdown_pll(dev);

                spin_unlock_irqrestore(&tp->lock, flags);
        }

        return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
static void rtl8125_shutdown(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8125_private *tp = netdev_priv(dev);

        if (tp->DASH)
                rtl8125_driver_stop(tp);

        rtl8125_set_bios_setting(dev);
        if (s5_keep_curr_mac == 0 && tp->random_mac == 0)
                rtl8125_rar_set(tp, tp->org_mac_addr);

        if (s5wol == 0)
                tp->wol_enabled = WOL_DISABLED;

        rtl8125_close(dev);
        rtl8125_disable_msi(pdev, tp);
}
#endif

/**
 *  rtl8125_get_stats - Get rtl8125 read/write statistics
 *  @dev: The Ethernet Device to get statistics for
 *
 *  Get TX/RX statistics for rtl8125
 */
static struct
net_device_stats *rtl8125_get_stats(struct net_device *dev)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
        struct rtl8125_private *tp = netdev_priv(dev);
#endif
        if (netif_running(dev)) {
//      spin_lock_irqsave(&tp->lock, flags);
//      spin_unlock_irqrestore(&tp->lock, flags);
        }

        return &RTLDEV->stats;
}

#ifdef CONFIG_PM

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,11)
static int
rtl8125_suspend(struct pci_dev *pdev, u32 state)
#else
static int
rtl8125_suspend(struct pci_dev *pdev, pm_message_t state)
#endif
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8125_private *tp = netdev_priv(dev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = pci_choose_state(pdev, state);
#endif
        unsigned long flags;

        if (!netif_running(dev))
                goto out;

        rtl8125_cancel_schedule_work(dev);

        rtl8125_delete_esd_timer(dev, &tp->esd_timer);

        rtl8125_delete_link_timer(dev, &tp->link_timer);

        netif_stop_queue(dev);

        netif_carrier_off(dev);

        netif_device_detach(dev);

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_hw_reset(dev);

        rtl8125_hw_d3_para(dev);

        rtl8125_powerdown_pll(dev);

        spin_unlock_irqrestore(&tp->lock, flags);

        if (tp->DASH)
                rtl8125_driver_stop(tp);
out:

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_save_state(pdev, &pci_pm_state);
#else
        pci_save_state(pdev);
#endif
        pci_enable_wake(pdev, pci_choose_state(pdev, state), tp->wol_enabled);
//  pci_set_power_state(pdev, pci_choose_state(pdev, state));

        return 0;
}

static int
rtl8125_resume(struct pci_dev *pdev)
{
        struct net_device *dev = pci_get_drvdata(pdev);
        struct rtl8125_private *tp = netdev_priv(dev);
        unsigned long flags;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        u32 pci_pm_state = PCI_D0;
#endif

        pci_set_power_state(pdev, PCI_D0);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
        pci_restore_state(pdev, &pci_pm_state);
#else
        pci_restore_state(pdev);
#endif
        pci_enable_wake(pdev, PCI_D0, 0);

        spin_lock_irqsave(&tp->lock, flags);

        /* restore last modified mac address */
        rtl8125_rar_set(tp, dev->dev_addr);

        spin_unlock_irqrestore(&tp->lock, flags);

        if (!netif_running(dev))
                goto out;

        spin_lock_irqsave(&tp->lock, flags);

        rtl8125_exit_oob(dev);

        rtl8125_hw_init(dev);

        rtl8125_powerup_pll(dev);

        rtl8125_hw_ephy_config(dev);

        rtl8125_hw_phy_config(dev);

        rtl8125_schedule_work(dev, rtl8125_reset_task);

        spin_unlock_irqrestore(&tp->lock, flags);

        netif_device_attach(dev);

        mod_timer(&tp->esd_timer, jiffies + RTL8125_ESD_TIMEOUT);
        mod_timer(&tp->link_timer, jiffies + RTL8125_LINK_TIMEOUT);
out:
        return 0;
}

#endif /* CONFIG_PM */

static struct pci_driver rtl8125_pci_driver = {
        .name       = MODULENAME,
        .id_table   = rtl8125_pci_tbl,
        .probe      = rtl8125_init_one,
        .remove     = __devexit_p(rtl8125_remove_one),
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11)
        .shutdown   = rtl8125_shutdown,
#endif
#ifdef CONFIG_PM
        .suspend    = rtl8125_suspend,
        .resume     = rtl8125_resume,
#endif
};

static int __init
rtl8125_init_module(void)
{
#ifdef ENABLE_R8125_PROCFS
        rtl8125_proc_module_init();
#endif
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0)
        return pci_register_driver(&rtl8125_pci_driver);
#else
        return pci_module_init(&rtl8125_pci_driver);
#endif
}

static void __exit
rtl8125_cleanup_module(void)
{
        pci_unregister_driver(&rtl8125_pci_driver);
#ifdef ENABLE_R8125_PROCFS
        if (rtl8125_proc) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                remove_proc_subtree(MODULENAME, init_net.proc_net);
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
                remove_proc_entry(MODULENAME, init_net.proc_net);
#else
                remove_proc_entry(MODULENAME, proc_net);
#endif  //LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
#endif  //LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
                rtl8125_proc = NULL;
        }
#endif
}

module_init(rtl8125_init_module);
module_exit(rtl8125_cleanup_module);
