#!/bin/sh
# Copyright (C) 2010-2013 OpenWrt.org

. /lib/functions.sh
. /lib/functions/leds.sh

get_status_led() {
	board=$(board_name)
	boardname="${board##*,}"

	case $board in
	3g150b|\
	3g300m|\
	w150m)
		status_led="$boardname:blue:ap"
		;;
	3g-6200n|\
	ar670w|\
	ar725w|\
	br-6475nd|\
	c50|\
	dch-m225|\
	dir-860l-b1|\
	e1700|\
	elecom,wrc-1167ghbk2-s|\
	ex2700|\
	ex3700|\
	fonera20n|\
	firewrt|\
	hg255d|\
	iodata,wn-ax1167gr|\
	iodata,wn-gx300gr|\
	kn|\
	kn_rc|\
	kn_rf|\
	kng_rc|\
	mzk-750dhp|\
	mzk-dp150n|\
	mzk-w300nh2|\
	nbg-419n|\
	nbg-419n2|\
	pwh2004|\
	r6220|\
	tplink,c20-v4|\
	tplink,c50-v3|\
	tplink,tl-wa801nd-v5|\
	tplink,tl-mr3420-v5|\
	tplink,tl-wr842n-v5|\
	tplink,tl-wr902ac-v3|\
	tl-wr840n-v4|\
	tl-wr840n-v5|\
	tl-wr841n-v13|\
	vr500|\
	wnce2001|\
	wndr3700v5|\
	x5|\
	x8|\
	xdxrn502j|\
	wn3000rpv3|\
	zyxel,keenetic-extra-ii)
		status_led="$boardname:green:power"
		;;
	3g-6200nl)
		status_led="$boardname:green:internet"
		;;
	a5-v11|\
	cs-qr10|\
	d105|\
	dcs-930l-b1|\
	hlk-rm04|\
	jhr-n825r|\
	mpr-a1|\
	mpr-a2|\
	mzk-ex750np)
		status_led="$boardname:red:power"
		;;
	ai-br100|\
	ht-tm02)
		status_led="$boardname:blue:wlan"
		;;
	alfa-network,ac1200rm|\
	awapn2403|\
	dir-645|\
	sk-wb8|\
	tplink,c2-v1|\
	wrh-300cr)
		status_led="$boardname:green:wps"
		;;
	alfa-network,awusfree1)
		status_led="$boardname:orange:system"
		;;
	all0239-3g|\
	dcs-930|\
	dir-300-b1|\
	dir-300-b7|\
	dir-320-b1|\
	dir-600-b1|\
	dir-610-a1|\
	dir-615-d|\
	dir-615-h1|\
	dir-620-a1|\
	dir-620-d1|\
	dwr-512-b|\
	dlink,dwr-116-a1|\
	gnubee,gb-pc1|\
	gnubee,gb-pc2|\
	hpm|\
	hw550-3g|\
	mac1200rv2|\
	miniembwifi|\
	mofi3500-3gn|\
	rut5xx|\
	v11st-fe|\
	wmr-300|\
	zbt-wg2626)
		status_led="$boardname:green:status"
		;;
	dlink,dwr-921-c1)
		status_led="$boardname:green:sigstrength"
		;;
	asl26555-8M|\
	asl26555-16M)
		status_led="asl26555:green:power"
		;;
	atp-52b|\
	ew1200|\
	ip2202)
		status_led="$boardname:green:run"
		;;
	c108)
		status_led="$boardname:green:lan"
		;;
	cf-wr800n|\
	psg1208)
		status_led="$boardname:white:wps"
		;;
	psg1218a|\
	psg1218b)
		status_led="$boardname:yellow:status"
		;;
	cy-swr1100|\
	w502u|\
	youhua,wr1200js)
		status_led="$boardname:blue:wps"
		;;
	d240|\
	dap-1350|\
	na930|\
	d-team,newifi-d2|\
	pbr-m1|\
	re350-v1|\
	rt-ac51u|\
	rt-n13u|\
	rt-n14u|\
	rt-n15|\
	rt-n56u|\
	tplink,c20-v1|\
	wl-330n|\
	wl-330n3g|\
	wli-tx4-ag300n|\
	y1|\
	y1s|\
	youku-yk1|\
	zorlik,zl5900v2)
		status_led="$boardname:blue:power"
		;;
	dlink,dap-1522-a1|\
	phicomm,k2g|\
	k2p|\
	m3|\
	mir3g|\
	miwifi-nano)
		status_led="$boardname:blue:status"
		;;
	db-wrt01|\
	esr-9753|\
	pbr-d1)
		status_led="$boardname:orange:power"
		;;
	f5d8235-v1)
		status_led="$boardname:blue:wired"
		;;
	f5d8235-v2)
		status_led="$boardname:blue:router"
		;;
	f7c027|\
	timecloud)
		status_led="$boardname:orange:status"
		;;
	hc5*61|\
	hc5661a|\
	jhr-n805r|\
	jhr-n926r|\
	mlw221|\
	mlwg2|\
	vonets,var11n-300)
		status_led="$boardname:blue:system"
		;;
	hc5962)
		status_led="$boardname:white:status"
		;;
	kimax,u35wf|\
	m2m)
		status_led="$boardname:blue:wifi"
		;;
	linkits7688)
		status_led="linkit-smart-7688:orange:wifi"
		;;
	gl-mt300n-v2)
		status_led="$boardname:green:power"
		;;
	m4-4M|\
	m4-8M)
		status_led="m4:blue:status"
		;;
	mikrotik,rbm11g|\
	mikrotik,rbm33g)
		status_led="$boardname:green:usr"
		;;
	miwifi-mini|\
	zte-q7)
		status_led="$boardname:red:status"
		;;
	mr-102n)
		status_led="$boardname:amber:status"
		;;
	mr200)
		status_led="$boardname:white:power"
		;;
	nw718)
		status_led="$boardname:amber:cpu"
		;;
	newifi-d1)
		status_led="$boardname:blue:status"
		;;
	omega2| \
	omega2p)
		status_led="$boardname:amber:system"
		;;
	oy-0001|\
	sl-r7205)
		status_led="$boardname:green:wifi"
		;;
	psr-680w)
		status_led="$boardname:red:wan"
		;;
	px-4885-4M|\
	px-4885-8M)
		status_led="px-4885:orange:wifi"
		;;
	re6500|\
	whr-1166d|\
	whr-600d)
		status_led="$boardname:orange:wifi"
		;;
	mzk-ex300np|\
	rt-n10-plus|\
	tew-638apb-v2|\
	tew-691gr|\
	tew-692gr|\
	ur-326n4g|\
	ur-336un|\
	wf-2881)
		status_led="$boardname:green:wps"
		;;
	rb750gr3)
		status_led="$boardname:blue:pwr"
		;;
	sap-g3200u3)
		status_led="$boardname:green:usb"
		;;
	u25awf-h1)
		status_led="u25awf:red:wifi"
		;;
	u7621-06-256M-16M)
		status_led="u7621-06:green:status"
		;;
	u7628-01-128M-16M)
		status_led="u7628-01:green:power"
		;;
	v22rw-2x2)
		status_led="$boardname:green:security"
		;;
	vocore-8M|\
	vocore-16M)
		status_led="vocore:green:status"
		;;
	vocore2)
		status_led="$boardname:fuchsia:status"
		;;
	vocore2lite)
		status_led="$boardname:green:status"
		;;
	w306r-v20|\
	mqmaker,witi-256m|\
	mqmaker,witi-512m|\
	zbt-wr8305rt)
		status_led="$boardname:green:sys"
		;;
	wcr-1166ds|\
	whr-300hp2|\
	wsr-1166|\
	wsr-600)
		status_led="$boardname:green:power"
		;;
	wcr-150gn|\
	wl-351)
		status_led="$boardname:amber:power"
		;;
	whr-g300n|\
	wlr-6000|\
	zbt-we2026)
		status_led="$boardname:red:power"
		;;
	widora,neo-16m|\
	widora,neo-32m)
		status_led="widora:orange:wifi"
		;;
	wzr-agl300nh)
		status_led="$boardname:green:router"
		;;
	wizfi630a)
		status_led="$boardname::run"
		;;
	wr512-3gn-4M|\
	wr512-3gn-8M)
		status_led="wr512-3gn:green:wps"
		;;
	wrtnode2r | \
	wrtnode2p | \
	wrtnode)
		status_led="wrtnode:blue:indicator"
		;;
	wt3020-4M|\
	wt3020-8M)
		status_led="wt3020:blue:power"
		;;
	zbt-cpe102)
		status_led="$boardname:green:4g-0"
		;;
	zbt-we826-16M|\
	zbt-we826-32M)
		status_led="zbt-we826:green:power"
		;;
	zbtlink,zbt-we1226)
		status_led="$boardname:green:wlan"
		;;
	zbt-wg3526-16M|\
	zbt-wg3526-32M)
		status_led="zbt-wg3526:green:status"
		;;
	esac
}

set_state() {
	get_status_led $1

	case "$1" in
	preinit)
		status_led_blink_preinit
		;;
	failsafe)
		status_led_blink_failsafe
		;;
	upgrade | \
	preinit_regular)
		status_led_blink_preinit_regular
		;;
	done)
		status_led_on
		;;
	esac
}
