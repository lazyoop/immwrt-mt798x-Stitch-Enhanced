#!/bin/sh
# Copyright (C) 2009-2013 OpenWrt.org

. /lib/functions.sh
. /lib/functions/leds.sh

get_status_led() {
	local board=$(board_name)

	case $board in
	a40)
		status_led="a40:green:status"
		;;
	a60)
		status_led="a60:green:status"
		;;
	alfa-nx)
		status_led="alfa:green:led_8"
		;;
	all0305)
		status_led="eap7660d:green:ds4"
		;;
	antminer-s1|\
	antminer-s3|\
	antminer-r1|\
	e1700ac-v2|\
	e600gac-v2|\
	eap120|\
	minibox-v1|\
	som9331|\
	sr3200|\
	tl-wr802n-v2|\
	xd3200)
		status_led="$board:green:system"
		;;
	ap121f)
		status_led="$board:green:vpn"
		;;
	ap132|\
	ap531b0|\
	cpe505n|\
	db120|\
	dr342|\
	dr344|\
	rut900|\
	tew-632brp|\
	tl-wr942n-v1|\
	wpj344|\
	zbt-we1526)
		status_led="$board:green:status"
		;;
	ap136-010|\
	ap136-020)
		status_led="ap136:green:status"
		;;
	ap147-010)
		status_led="ap147:green:status"
		;;
	ap135-020)
		status_led="ap135:green:status"
		;;
	archer-c25-v1|\
	archer-c58-v1|\
	archer-c59-v1|\
	archer-c60-v1|\
	archer-c7-v4|\
	fritz300e|\
	gl-usb150|\
	mr12|\
	mr16|\
	nbg6616|\
	sc1750|\
	sc450|\
	tl-wpa8630|\
	tl-wr902ac-v1)
		status_led="$board:green:power"
		;;
	archer-c5|\
	archer-c7|\
	tl-mr10u|\
	tl-mr12u|\
	tl-mr13u|\
	tl-wdr4300|\
	tl-wdr4900-v2|\
	tl-wr703n|\
	tl-wr710n|\
	tl-wr720n-v3|\
	tl-wr802n-v1|\
	tl-wr810n|\
	tl-wr810n-v2|\
	tl-wr940n-v4|\
	tl-wr941nd-v6)
		status_led="tp-link:blue:system"
		;;
	ap90q|\
	cpe830|\
	cpe870|\
	gl-ar300m|\
	gl-inet|\
	gl-mifi)
		status_led="$board:green:lan"
		;;
	ap91-5g|\
	n5q)
		status_led="$board:green:signal4"
		;;
	ap96)
		status_led="$board:green:led2"
		;;
	aw-nr580)
		status_led="$board:green:ready"
		;;
	bhr-4grv2|\
	wzr-hp-ag300h|\
	wzr-hp-g300nh2)
		status_led="buffalo:red:diag"
		;;
	bsb)
		status_led="$board:red:sys"
		;;
	bullet-m|\
	loco-m-xw|\
	nano-m|\
	nanostation-m|\
	nanostation-m-xw|\
	rocket-m|\
	rocket-m-xw)
		status_led="ubnt:green:link4"
		;;
	bxu2000n-2-a1)
		status_led="bhu:green:status"
		;;
	cap324)
		status_led="pcs:green:power"
		;;
	c-55|\
	c-60)
		status_led="$board:green:pwr"
		;;
	cap4200ag)
		status_led="senao:green:pwr"
		;;
	cf-e316n-v2|\
	cf-e520n|\
	cf-e530n)
		status_led="$board:blue:wan"
		;;
	cf-e320n-v2)
		status_led="$board:blue:wlan"
		;;
	cf-e380ac-v1|\
	cf-e380ac-v2|\
	cf-e385ac)
		status_led="$board:blue:wlan2g"
		;;
	cpe510)
		status_led="tp-link:green:link4"
		;;
	cr3000|\
	cr5000)
		status_led="pcs:amber:power"
		;;
	dgl-5500-a1|\
	dhp-1565-a1|\
	dir-505-a1|\
	dir-600-a1|\
	dir-615-e1|\
	dir-615-i1|\
	dir-615-e4)
		status_led="d-link:green:power"
		;;
	dir-615-c1)
		status_led="d-link:green:status"
		;;
	dir-825-b1)
		status_led="d-link:orange:power"
		;;
	dir-825-c1|\
	dir-835-a1)
		status_led="d-link:amber:power"
		;;
	dir-869-a1)
		status_led="d-link:white:status"
		;;
	dlan-hotspot)
		status_led="devolo:green:wifi"
		;;
	dlan-pro-500-wp)
		status_led="devolo:green:wlan-2g"
		;;
	dlan-pro-1200-ac)
		status_led="devolo:status:wlan"
		;;
	dr531)
		status_led="$board:green:sig4"
		;;
	dragino2|\
	oolite)
		status_led="$board:red:system"
		;;
	dw33d|\
	r36a)
		status_led="$board:blue:status"
		;;
	e600g-v2)
		status_led="$board:blue:system"
		;;
	eap300v2)
		status_led="engenius:blue:power"
		;;
	ens202ext|\
	esr900)
		status_led="engenius:amber:power"
		;;
	eap7660d)
		status_led="$board:green:ds4"
		;;
	el-m150|\
	el-mini)
		status_led="easylink:green:system"
		;;
	ew-balin)
		status_led="balin:green:status"
		;;
	ew-dorin|\
	ew-dorin-router)
		status_led="dorin:green:status"
		;;
	f9k1115v2)
		status_led="belkin:blue:status"
		;;
	epg5000|\
	esr1750)
		status_led="$board:amber:power"
		;;
	gl-ar750|\
	hiveap-121|\
	nbg6716)
		status_led="$board:white:power"
		;;
	hiwifi-hc6361)
		status_led="hiwifi:blue:system"
		;;
	hornet-ub|\
	hornet-ub-x2)
		status_led="alfa:blue:wps"
		;;
	ja76pf|\
	ja76pf2)
		status_led="jjplus:green:led1"
		;;
	jwap230)
		status_led="$board:green:led1"
		;;
	ls-sr71)
		status_led="ubnt:green:d22"
		;;
	mc-mac1200r)
		status_led="mercury:green:system"
		;;
	mr18|\
	z1)
		status_led="$board:green:tricolor0"
		;;
	mr600)
		status_led="$board:orange:power"
		;;
	mr600v2)
		status_led="mr600:blue:power"
		;;
	mr1750|\
	mr1750v2)
		status_led="mr1750:blue:power"
		;;
	mr900|\
	mr900v2)
		status_led="mr900:blue:power"
		;;
	mynet-n600|\
	mynet-n750|\
	mynet-rext)
		status_led="wd:blue:power"
		;;
	mzk-w04nu|\
	mzk-w300nh)
		status_led="planex:green:status"
		;;
	nbg460n_550n_550nh)
		status_led="nbg460n:green:power"
		;;
	om2p|\
	om2p-hs|\
	om2p-hsv2|\
	om2p-hsv3|\
	om2p-hsv4|\
	om2p-lc|\
	om2pv2|\
	om2pv4)
		status_led="om2p:blue:power"
		;;
	om5p|\
	om5p-an)
		status_led="om5p:blue:power"
		;;
	om5p-ac|\
	om5p-acv2)
		status_led="om5pac:blue:power"
		;;
	omy-g1)
		status_led="omy:green:wlan"
		;;
	omy-x1)
		status_led="omy:green:power"
		;;
	onion-omega)
		status_led="onion:amber:system"
		;;
	pb44)
		status_led="$board:amber:jump1"
		;;
	r602n)
		status_led="$board:green:wan"
		;;
	rb-2011il|\
	rb-2011l|\
	rb-2011uas|\
	rb-2011uas-2hnd)
		status_led="rb:green:usr"
		;;
	rb-411|\
	rb-411u|\
	rb-433|\
	rb-433u|\
	rb-450|\
	rb-450g|\
	rb-493)
		status_led="rb4xx:yellow:user"
		;;
	rb-750)
		status_led="rb750:green:act"
		;;
	rb-750-r2|\
	rb-750p-pbr2|\
	rb-750up-r2|\
	rb-911-2hn|\
	rb-911-5hn|\
	rb-911g-2hpnd|\
	rb-911g-5hpacd|\
	rb-911g-5hpnd|\
	rb-941-2nd|\
	rb-951ui-2nd|\
	rb-952ui-5ac2nd|\
	rb-962uigs-5hact2hnt|\
	rb-lhg-5nd|\
	rb-map-2nd|\
	rb-mapl-2nd)
		status_led="rb:green:user"
		;;
	rb-951ui-2hnd)
		status_led="rb:green:act"
		;;
	rb-912uag-2hpnd|\
	rb-912uag-5hpnd|\
	rb-sxt2n|\
	rb-sxt5n)
		status_led="rb:green:power"
		;;
	re355|\
	re450|\
	sc300m)
		status_led="$board:blue:power"
		;;
	rocket-m-ti)
		status_led="ubnt:green:link6"
		;;
	routerstation|\
	routerstation-pro)
		status_led="ubnt:green:rf"
		;;
	rw2458n)
		status_led="$board:green:d3"
		;;
	smart-300)
		status_led="nc-link:green:system"
		;;
	qihoo-c301)
		status_led="qihoo:green:status"
		;;
	tellstick-znet-lite)
		status_led="tellstick:white:system"
		;;
	tew-673gru)
		status_led="trendnet:blue:wps"
		;;
	tew-712br|\
	tew-732br|\
	tew-823dru)
		status_led="trendnet:green:power"
		;;
	tl-mr3020|\
	tl-wr2543n)
		status_led="tp-link:green:wps"
		;;
	tl-wa750re)
		status_led="tp-link:orange:re"
		;;
	tl-wa850re|\
	tl-wa850re-v2)
		status_led="tp-link:blue:re"
		;;
	tl-wa855re-v1|\
	tl-wa860re)
		status_led="tp-link:green:power"
		;;
	tl-mr6400)
		status_led="tp-link:white:power"
		;;
	tl-mr3220|\
	tl-mr3220-v2|\
	tl-mr3420|\
	tl-mr3420-v2|\
	tl-wa701nd-v2|\
	tl-wa801nd-v2|\
	tl-wa801nd-v3|\
	tl-wa830re-v2|\
	tl-wa901nd|\
	tl-wa901nd-v2|\
	tl-wa901nd-v3|\
	tl-wa901nd-v4|\
	tl-wa901nd-v5|\
	tl-wdr3320-v2|\
	tl-wdr3500|\
	tl-wr1041n-v2|\
	tl-wr1043n-v5|\
	tl-wr1043nd|\
	tl-wr1043nd-v2|\
	tl-wr1043nd-v4|\
	tl-wr740n-v6|\
	tl-wr741nd|\
	tl-wr741nd-v4|\
	tl-wr840n-v2|\
	tl-wr840n-v3|\
	tl-wr841n-v1|\
	tl-wr841n-v7|\
	tl-wr841n-v8|\
	tl-wr841n-v11|\
	tl-wr842n-v2|\
	tl-wr842n-v3|\
	tl-wr941nd|\
	tl-wr941nd-v5)
		status_led="tp-link:green:system"
		;;
	tl-wr841n-v9)
		status_led="tp-link:green:qss"
		;;
	tl-wdr6500-v2)
		status_led="tp-link:white:system"
		;;
	tube2h)
		status_led="alfa:green:signal4"
		;;
	unifi)
		status_led="ubnt:green:dome"
		;;
	uap-pro|\
	unifiac-lite|\
	unifiac-pro)
		status_led="ubnt:white:dome"
		;;
	unifi-outdoor-plus)
		status_led="ubnt:white:front"
		;;
	airgateway|\
	airgatewaypro)
		status_led="ubnt:white:status"
		;;
	wi2a-ac200i)
		status_led="nokia:green:ctrl"
		;;
	whr-g301n|\
	whr-hp-g300n|\
	whr-hp-gn|\
	wzr-hp-g300nh)
		status_led="buffalo:green:router"
		;;
	wlae-ag300n)
		status_led="buffalo:green:status"
		;;
	r6100|\
	wndap360|\
	wndr3700|\
	wndr3700v4|\
	wndr4300|\
	wnr2000|\
	wnr2000-v3|\
	wnr2200|\
	wnr612-v2|\
	wnr1000-v2|\
	wpn824n)
		status_led="netgear:green:power"
		;;
	wp543)
		status_led="$board:green:diag"
		;;
	wpj342|\
	wpj531|\
	wpj558)
		status_led="$board:green:sig3"
		;;
	wpj563)
		status_led="$board:green:sig1"
		;;
	wrt160nl|\
	wrt400n)
		status_led="$board:blue:wps"
		;;
	zcn-1523h-2|\
	zcn-1523h-5)
		status_led="zcn-1523h:amber:init"
		;;
	wlr8100)
		status_led="sitecom:amber:status"
		;;
	esac
}

set_state() {
	get_status_led

	case "$1" in
	preinit)
		status_led_blink_preinit
		;;
	failsafe)
		status_led_blink_failsafe
		;;
	preinit_regular)
		status_led_blink_preinit_regular
		;;
	done)
		status_led_on
		case $(board_name) in
		gl-ar300m|\
		gl-ar750)
			fw_printenv lc >/dev/null 2>&1 && fw_setenv "bootcount" 0
			;;
		qihoo-c301)
			local n=$(fw_printenv activeregion | cut -d = -f 2)
			fw_setenv "image${n}trynum" 0
			;;
		wi2a-ac200i)
			fw_setenv PKRstCnt 0
			;;
		esac
		;;
	esac
}
