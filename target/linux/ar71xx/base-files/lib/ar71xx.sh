#!/bin/sh
#
# Copyright (C) 2009-2011 OpenWrt.org
#

AR71XX_BOARD_NAME=
AR71XX_MODEL=

ar71xx_get_mem_total() {
	$(awk '/MemTotal:/ {print($2)}' /proc/meminfo)
}

ar71xx_get_mtd_part_magic() {
	local part="$1"
	local mtd

	mtd=$(find_mtd_part $part)
	[ -z "$mtd" ] && return

	dd if=$mtd bs=4 count=1 2>/dev/null | hexdump -v -n 4 -e '1/1 "%02x"'
}

wndr3700_board_detect() {
	local machine="$1"
	local magic
	local name

	name="wndr3700"

	magic="$(ar71xx_get_mtd_part_magic firmware)"
	case $magic in
	"33373030")
		machine="NETGEAR WNDR3700"
		;;
	"33373031")
		local mt

		mt=$(ar71xx_get_mem_total)
		if [ "$mt" -lt "65536" ]; then
			machine="NETGEAR WNDR3700v2"
		else
			machine="NETGEAR WNDR3800"
		fi
		;;
	esac

	AR71XX_BOARD_NAME="$name"
	AR71XX_MODEL="$machine"
}

ar71xx_board_detect() {
	local machine
	local name

	machine=$(awk 'BEGIN{FS="[ \t]+:[ \t]"} /machine/ {print $2}' /proc/cpuinfo)

	case "$machine" in
	*"AirRouter")
		name="airrouter"
		;;
	*ALL0258N)
		name="all0258n"
		;;
	*AP121)
		name="ap121"
		;;
	*AP121-MINI)
		name="ap121-mini"
		;;
	*AP81)
		name="ap81"
		;;
	*AP83)
		name="ap83"
		;;
	*AP96)
		name="ap96"
		;;
	*AW-NR580)
		name="aw-nr580"
		;;
	*DB120)
		name="db120"
		;;
	*"DIR-600 rev. A1")
		name="dir-600-a1"
		;;
	*"DIR-825 rev. B1")
		name="dir-825-b1"
		;;
	*EAP7660D)
		name="eap7660d"
		;;
	*JA76PF)
		name="ja76pf"
		;;
	*"Bullet M")
		name="bullet-m"
		;;
	*"Nanostation M")
		name="nanostation-m"
		;;
	*JWAP003)
		name="jwap003"
		;;
	*LS-SR71)
		name="ls-sr71"
		;;
	*MZK-W04NU)
		name="mzk-w04nu"
		;;
	*MZK-W300NH)
		name="mzk-w300nh"
		;;
	*"NBG460N/550N/550NH")
		name="nbg460n_550n_550nh"
		;;
	*OM2P)
		name="om2p"
		;;
	*PB42)
		name="pb42"
		;;
	*PB44)
		name="pb44"
		;;
	*PB92)
		name="pb92"
		;;
	*"RouterBOARD 411/A/AH")
		name="rb-411"
		;;
	*"RouterBOARD 411U")
		name="rb-411u"
		;;
	*"RouterBOARD 433/AH")
		name="rb-433"
		;;
	*"RouterBOARD 433UAH")
		name="rb-433u"
		;;
	*"RouterBOARD 450")
		name="rb-450"
		;;
	*"RouterBOARD 450G")
		name="rb-450g"
		;;
	*"RouterBOARD 493/AH")
		name="rb-493"
		;;
	*"RouterBOARD 493G")
		name="rb-493g"
		;;
	*"RouterBOARD 750")
		name="rb-750"
		;;
	*"Rocket M")
		name="rocket-m"
		;;
	*RouterStation)
		name="routerstation"
		;;
	*"RouterStation Pro")
		name="routerstation-pro"
		;;
	*TEW-632BRP)
		name="tew-632brp"
		;;
	*TL-WR1043ND)
		name="tl-wr1043nd"
		;;
	*"DIR-615 rev. C1")
		name="dir-615-c1"
		;;
	*TL-MR3220)
		name="tl-mr3220"
		;;
	*TL-MR3420)
		name="tl-mr3420"
		;;
	*TL-WA901ND)
		name="tl-wa901nd"
		;;
	*"TL-WA901ND v2")
		name="tl-wa901nd-v2"
		;;
	*TL-WR741ND)
		name="tl-wr741nd"
		;;
	*"TL-WR741ND v4")
		name="tl-wr741nd-v4"
		;;
	*"TL-WR841N v1")
		name="tl-wr841n-v1"
		;;
	*TL-WR941ND)
		name="tl-wr941nd"
		;;
	*"TL-WR703N v1")
		name="tl-wr703n"
		;;
	*UniFi)
		name="unifi"
		;;
	*WHR-G301N)
		name="whr-g301n"
		;;
	*WHR-HP-GN)
		name="whr-hp-gn"
		;;
	*WP543)
		name="wp543"
		;;
	*"WNDR3700/WNDR3800")
		wndr3700_board_detect "$machine"
		;;
	*WNR2000)
		name="wnr2000"
		;;
	*WRT160NL)
		name="wrt160nl"
		;;
	*WRT400N)
		name="wrt400n"
		;;
	*WZR-HP-AG300H)
		name="wzr-hp-ag300h"
		;;
	*WZR-HP-G300NH)
		name="wzr-hp-g300nh"
		;;
	*WZR-HP-G450H)
		name="wzr-hp-g450h"
		;;
	*WHR-HP-G300N)
		name="whr-hp-g300n"
		;;
	*ZCN-1523H-2)
		name="zcn-1523h-2"
		;;
	*ZCN-1523H-5)
		name="zcn-1523h-5"
		;;
	esac

	[ -z "$name" ] && name="unknown"

	[ -z "$AR71XX_BOARD_NAME" ] && AR71XX_BOARD_NAME="$name"
	[ -z "$AR71XX_MODEL" ] && AR71XX_MODEL="$machine"

	[ -e "/tmp/sysinfo/" ] || mkdir -p "/tmp/sysinfo/"

	echo "$AR71XX_BOARD_NAME" > /tmp/sysinfo/board_name
	echo "$AR71XX_MODEL" > /tmp/sysinfo/model
}

ar71xx_board_name() {
	local name

	[ -f /tmp/sysinfo/board_name ] && name=$(cat /tmp/sysinfo/board_name)
	[ -z "$name" ] && name="unknown"

	echo "$name"
}
