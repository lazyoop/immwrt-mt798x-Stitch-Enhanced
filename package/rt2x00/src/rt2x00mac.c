/*
	Copyright (C) 2004 - 2007 rt2x00 SourceForge Project
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2x00lib
	Abstract: rt2x00 generic mac80211 routines.
	Supported chipsets: RT2460, RT2560, RT2570,
	rt2561, rt2561s, rt2661, rt2571W & rt2671.
 */

/*
 * Set enviroment defines for rt2x00.h
 */
#define DRV_NAME "rt2x00lib"

#include <linux/netdevice.h>

#include "rt2x00.h"
#include "rt2x00dev.h"

static int rt2x00_tx_rts_cts(struct rt2x00_dev *rt2x00dev,
	struct data_ring *ring, struct sk_buff *frag_skb,
	struct ieee80211_tx_control *control)
{
	struct sk_buff *skb;
	int size;

	if (control->flags & IEEE80211_TXCTL_USE_CTS_PROTECT)
		size = sizeof(struct ieee80211_cts);
	else
		size = sizeof(struct ieee80211_rts);

	skb = dev_alloc_skb(size + rt2x00dev->hw->extra_tx_headroom);
	if (!skb) {
		WARNING(rt2x00dev, "Failed to create RTS/CTS frame.\n");
		return NETDEV_TX_BUSY;
	}

	skb_reserve(skb, rt2x00dev->hw->extra_tx_headroom);
	skb_put(skb, size);

	if (control->flags & IEEE80211_TXCTL_USE_CTS_PROTECT)
		ieee80211_ctstoself_get(rt2x00dev->hw,
			frag_skb->data, frag_skb->len, control,
			(struct ieee80211_cts*)(skb->data));
	else
		ieee80211_rts_get(rt2x00dev->hw,
			frag_skb->data, frag_skb->len, control,
			(struct ieee80211_rts*)(skb->data));

	if (rt2x00dev->ops->lib->write_tx_data(rt2x00dev, ring, skb, control)) {
		WARNING(rt2x00dev, "Failed to send RTS/CTS frame.\n");
		return NETDEV_TX_BUSY;
	}

	return NETDEV_TX_OK;
}

int rt2x00lib_tx(struct ieee80211_hw *hw, struct sk_buff *skb,
	struct ieee80211_tx_control *control)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct ieee80211_hdr *ieee80211hdr = (struct ieee80211_hdr*)skb->data;
	struct data_ring *ring;
	u16 frame_control;

	/*
	 * Determine which ring to put packet on.
	 */
	ring = rt2x00_get_ring(rt2x00dev, control->queue);
	if (unlikely(!ring)) {
		ERROR(rt2x00dev,
			"Attempt to send packet over invalid queue %d.\n"
			"Please file bug report to %s.\n",
			control->queue, DRV_PROJECT);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/*
	 * If CTS/RTS is required. and this frame is not CTS or RTS,
	 * create and queue that frame first. But make sure we have
	 * at least enough entries available to send this CTS/RTS
	 * frame as well as the data frame.
	 */
	frame_control = le16_to_cpu(ieee80211hdr->frame_control);
	if (control->flags & IEEE80211_TXCTL_USE_RTS_CTS &&
	    !is_cts_frame(frame_control) && !is_rts_frame(frame_control)) {
		if (rt2x00_ring_free(ring) <= 1)
			return NETDEV_TX_BUSY;

		if (rt2x00_tx_rts_cts(rt2x00dev, ring, skb, control))
			return NETDEV_TX_BUSY;
	}

	if (rt2x00dev->ops->lib->write_tx_data(rt2x00dev, ring, skb, control))
		return NETDEV_TX_BUSY;

	if (rt2x00dev->ops->lib->kick_tx_queue)
		rt2x00dev->ops->lib->kick_tx_queue(rt2x00dev, control->queue);

	return NETDEV_TX_OK;
}
EXPORT_SYMBOL_GPL(rt2x00lib_tx);

int rt2x00lib_reset(struct ieee80211_hw *hw)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;

	rt2x00lib_disable_radio(rt2x00dev);
	return rt2x00lib_enable_radio(rt2x00dev);
}
EXPORT_SYMBOL_GPL(rt2x00lib_reset);

int rt2x00lib_open(struct ieee80211_hw *hw)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	int status;

	/*
	 * We must wait on the firmware before
	 * we can safely continue.
	 */
	status = rt2x00lib_load_firmware_wait(rt2x00dev);
	if (status)
		return status;

	/*
	 * Initialize the device.
	 */
	status = rt2x00lib_initialize(rt2x00dev);
	if (status)
		return status;

	/*
	 * Enable radio.
	 */
	status = rt2x00lib_enable_radio(rt2x00dev);
	if (status) {
		rt2x00lib_uninitialize(rt2x00dev);
		return status;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_open);

int rt2x00lib_stop(struct ieee80211_hw *hw)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;

	rt2x00lib_disable_radio(rt2x00dev);

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_stop);

int rt2x00lib_add_interface(struct ieee80211_hw *hw,
	struct ieee80211_if_init_conf *conf)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct interface *intf = &rt2x00dev->interface;

	/*
	 * We only support 1 non-monitor interface.
	 */
	if (conf->type != IEEE80211_IF_TYPE_MNTR &&
	    is_interface_present(&rt2x00dev->interface))
		return -ENOBUFS;

	/*
	 * We support muliple monitor mode interfaces.
	 * All we need to do is increase the monitor_count.
	 */
	if (conf->type == IEEE80211_IF_TYPE_MNTR) {
		intf->monitor_count++;
	} else {
		intf->id = conf->if_id;
		intf->type = conf->type;
		if (conf->type == IEEE80211_IF_TYPE_AP)
			memcpy(&intf->bssid, conf->mac_addr, ETH_ALEN);
		intf->promisc = 0;
	}

	/*
	 * If this is the first interface which is being added,
	 * we should write the MAC address to the device.
	 */
	if (!test_bit(DEVICE_ENABLED_RADIO, &rt2x00dev->flags))
		rt2x00dev->ops->lib->config_mac_addr(rt2x00dev, conf->mac_addr);

	/*
	 * Enable periodic link tuning if this is a non-monitor interface.
	 */
	if (conf->type != IEEE80211_IF_TYPE_MNTR)
		rt2x00_start_link_tune(rt2x00dev);

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_add_interface);

void rt2x00lib_remove_interface(struct ieee80211_hw *hw,
	struct ieee80211_if_init_conf *conf)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct interface *intf = &rt2x00dev->interface;

	/*
	 * We only support 1 non-monitor interface.
	 */
	if (conf->type != IEEE80211_IF_TYPE_MNTR &&
	    !is_interface_present(&rt2x00dev->interface))
		return;

	/*
	 * We support muliple monitor mode interfaces.
	 * All we need to do is decrease the monitor_count.
	 */
	if (conf->type == IEEE80211_IF_TYPE_MNTR) {
		intf->monitor_count--;
	} else if (intf->type == conf->type) {
		intf->id = 0;
		intf->type = -EINVAL;
		memset(&intf->bssid, 0x00, ETH_ALEN);
		intf->promisc = 0;
	}

	/*
	 * When this is a non-monitor mode, stop the periodic link tuning.
	 */
	if (conf->type != IEEE80211_IF_TYPE_MNTR)
		rt2x00_stop_link_tune(rt2x00dev);

	/*
	 * Check if we still have 1 non-monitor or a monitor
	 * interface enabled. In that case we should update the
	 * registers.
	 */
	if (is_monitor_present(&rt2x00dev->interface) ^
	    is_interface_present(&rt2x00dev->interface)) {
		if (is_interface_present(&rt2x00dev->interface))
			rt2x00lib_config_type(rt2x00dev,
				rt2x00dev->interface.type);
		else
			rt2x00lib_config_type(rt2x00dev,
				IEEE80211_IF_TYPE_MNTR);
	}

	/*
	 * Check which interfaces have been disabled.
	 */
	if (!is_interface_present(&rt2x00dev->interface))
		__clear_bit(INTERFACE_ENABLED, &rt2x00dev->flags);
	else if (!is_monitor_present(&rt2x00dev->interface))
		__clear_bit(INTERFACE_ENABLED_MONITOR, &rt2x00dev->flags);
}
EXPORT_SYMBOL_GPL(rt2x00lib_remove_interface);

int rt2x00lib_config(struct ieee80211_hw *hw, struct ieee80211_conf *conf)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;

	/*
	 * Check if we need to disable the radio,
	 * if this is not the case, at least the RX must be disabled.
	 */
	if (test_bit(DEVICE_ENABLED_RADIO, &rt2x00dev->flags)) {
		if (!conf->radio_enabled)
			rt2x00lib_disable_radio(rt2x00dev);
		else
			rt2x00lib_toggle_rx(rt2x00dev, 0);
	}

	rt2x00lib_config_phymode(rt2x00dev, conf->phymode);
	rt2x00lib_config_channel(rt2x00dev, conf->channel_val,
		conf->channel, conf->freq, conf->power_level);
	rt2x00lib_config_txpower(rt2x00dev, conf->power_level);
	rt2x00lib_config_antenna(rt2x00dev,
		conf->antenna_sel_tx, conf->antenna_sel_rx);
	rt2x00dev->ops->lib->config_duration(rt2x00dev,
		(conf->flags & IEEE80211_CONF_SHORT_SLOT_TIME),
		conf->beacon_int);

	/*
	 * Reenable RX only if the radio should be on.
	 */
	if (test_bit(DEVICE_ENABLED_RADIO, &rt2x00dev->flags))
		rt2x00lib_toggle_rx(rt2x00dev, 1);
	else if (conf->radio_enabled)
		return rt2x00lib_enable_radio(rt2x00dev);

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_config);

int rt2x00lib_config_interface(struct ieee80211_hw *hw, int if_id,
	struct ieee80211_if_conf *conf)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct interface *intf = &rt2x00dev->interface;
	int status;

	/*
	 * Monitor mode does not need configuring.
	 * If the given type does not match the configured type,
	 * there has been a problem.
	 */
	if (conf->type == IEEE80211_IF_TYPE_MNTR)
		return 0;
	else if (conf->type != intf->type)
		return -EINVAL;

	/*
	 * If the interface does not work in master mode,
	 * then the bssid value in the interface structure
	 * should now be set.
	 */
	if (conf->type != IEEE80211_IF_TYPE_AP)
		memcpy(&intf->bssid, conf->bssid, ETH_ALEN);

	/*
	 * Enable configuration.
	 * For Monitor mode, promisc mode will be forced on.
	 */
	rt2x00lib_config_type(rt2x00dev, conf->type);
	rt2x00lib_config_promisc(rt2x00dev, rt2x00dev->interface.promisc);
	rt2x00dev->ops->lib->config_bssid(rt2x00dev, intf->bssid);

	/*
	 * We only need to initialize the beacon when master mode is enabled.
	 */
	if (conf->type != IEEE80211_IF_TYPE_AP || !conf->beacon)
		return 0;

	status = rt2x00dev->ops->hw->beacon_update(rt2x00dev->hw,
		conf->beacon, conf->beacon_control);
	if (status)
		dev_kfree_skb(conf->beacon);

	return status;
}
EXPORT_SYMBOL_GPL(rt2x00lib_config_interface);

void rt2x00lib_set_multicast_list(struct ieee80211_hw *hw,
	unsigned short flags, int mc_count)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;

	/*
	 * Promisc mode is forced on for Monitor interfaces.
	 */
	if (is_monitor_present(&rt2x00dev->interface))
		return;

	/*
	 * Check if the new state is different then the old state.
	 */
	if (test_bit(INTERFACE_ENABLED_PROMISC, &rt2x00dev->flags) ==
	    (flags & IFF_PROMISC))
		return;

	rt2x00dev->interface.promisc = (flags & IFF_PROMISC);

	/*
	 * Schedule the link tuner if this does not run
	 * automatically. The link tuner will be automatically
	 * switched off when it is not required.
	 */
	if (!work_pending(&rt2x00dev->link.work.work))
		queue_work(rt2x00dev->workqueue, &rt2x00dev->link.work.work);
}
EXPORT_SYMBOL_GPL(rt2x00lib_set_multicast_list);

int rt2x00lib_get_tx_stats(struct ieee80211_hw *hw,
	struct ieee80211_tx_queue_stats *stats)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	unsigned int i;

	for (i = 0; i < hw->queues; i++)
		memcpy(&stats->data[i], &rt2x00dev->tx[i].stats,
			sizeof(rt2x00dev->tx[i].stats));

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_get_tx_stats);

int rt2x00lib_conf_tx(struct ieee80211_hw *hw, int queue,
	const struct ieee80211_tx_queue_params *params)
{
	struct rt2x00_dev *rt2x00dev = hw->priv;
	struct data_ring *ring;

	ring = rt2x00_get_ring(rt2x00dev, queue);
	if (unlikely(!ring))
		return -EINVAL;

	/*
	 * The passed variables are stored as real value ((2^n)-1).
	 * Ralink registers require to know the bit number 'n'.
	 */
	if (params->cw_min)
		ring->tx_params.cw_min = fls(params->cw_min);
	else
		ring->tx_params.cw_min = 5; /* cw_min: 2^5 = 32. */

	if (params->cw_max)
		ring->tx_params.cw_max = fls(params->cw_max);
	else
		ring->tx_params.cw_max = 10; /* cw_min: 2^10 = 1024. */

	if (params->aifs)
		ring->tx_params.aifs = params->aifs;
	else
		ring->tx_params.aifs = 2;

	INFO(rt2x00dev,
		"Configured TX ring %d - CWmin: %d, CWmax: %d, Aifs: %d.\n",
		queue, ring->tx_params.cw_min, ring->tx_params.cw_max,
		ring->tx_params.aifs);

	return 0;
}
EXPORT_SYMBOL_GPL(rt2x00lib_conf_tx);
