#
# Copyright (C) 2012 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

define KernelPackage/sound-arm-bcm2835
  TITLE:=BCM2835 ALSA driver
  KCONFIG:= \
	CONFIG_SND_ARM=y \
	CONFIG_SND_BCM2835 \
	CONFIG_SND_ARMAACI=n
  FILES:= \
	$(LINUX_DIR)/sound/arm/snd-bcm2835.ko
  AUTOLOAD:=$(call AutoLoad,68,snd-bcm2835)
  DEPENDS:=@TARGET_brcm2708
  $(call AddDepends/sound)
endef

define KernelPackage/sound-arm-bcm2835/description
  This package contains the BCM2835 ALSA pcm card driver
endef

$(eval $(call KernelPackage,sound-arm-bcm2835))


define KernelPackage/random-bcm2708
  SUBMENU:=$(OTHER_MENU)
  TITLE:=BCM2708 HW Random Number Generator
  KCONFIG:=CONFIG_HW_RANDOM_BCM2708
  FILES:=$(LINUX_DIR)/drivers/char/hw_random/bcm2708-rng.ko
  AUTOLOAD:=$(call AutoLoad,11,bcm2708-rng)
  DEPENDS:=@TARGET_brcm2708 +kmod-random-core
endef

define KernelPackage/random-bcm2708/description
  This package contains the Broadcom 2708 HW random number generator driver
endef

$(eval $(call KernelPackage,random-bcm2708))

define KernelPackage/random-bcm2835
  SUBMENU:=$(OTHER_MENU)
  TITLE:=BCM2835 HW Random Number Generator
  KCONFIG:=CONFIG_HW_RANDOM_BCM2835
  FILES:=$(LINUX_DIR)/drivers/char/hw_random/bcm2835-rng.ko
  AUTOLOAD:=$(call AutoLoad,11,bcm2835-rng)
  DEPENDS:=@TARGET_brcm2708 +kmod-random-core
endef

define KernelPackage/random-bcm2835/description
  This package contains the Broadcom 2835 HW random number generator driver
endef

$(eval $(call KernelPackage,random-bcm2835))


define KernelPackage/spi-bcm2708
  SUBMENU:=$(SPI_MENU)
  TITLE:=BCM2708 SPI controller driver
  KCONFIG:= \
    CONFIG_BCM2708_SPIDEV=n \
    CONFIG_SPI=y \
    CONFIG_SPI_BCM2708 \
    CONFIG_SPI_MASTER=y
  FILES:=$(LINUX_DIR)/drivers/spi/spi-bcm2708.ko
  AUTOLOAD:=$(call AutoLoad,89,spi-bcm2708)
  DEPENDS:=@TARGET_brcm2708
endef

define KernelPackage/spi-bcm2708/description
  This package contains the Broadcom 2708 SPI master controller driver
endef

$(eval $(call KernelPackage,spi-bcm2708))

define KernelPackage/spi-bcm2835
  SUBMENU:=$(SPI_MENU)
  TITLE:=BCM2835 SPI controller driver
  KCONFIG:=\
    CONFIG_BCM2708_SPIDEV=n \
    CONFIG_SPI=y \
    CONFIG_SPI_BCM2835 \
    CONFIG_SPI_MASTER=y
  FILES:=$(LINUX_DIR)/drivers/spi/spi-bcm2835.ko
  AUTOLOAD:=$(call AutoLoad,89,spi-bcm2835)
  DEPENDS:=@TARGET_brcm2708
endef

define KernelPackage/spi-bcm2835/description
  This package contains the Broadcom 2835 SPI master controller driver
endef

$(eval $(call KernelPackage,spi-bcm2835))


define KernelPackage/hwmon-bcm2835
  TITLE:=BCM2835 HWMON driver
  KCONFIG:=CONFIG_SENSORS_BCM2835
  FILES:=$(LINUX_DIR)/drivers/hwmon/bcm2835-hwmon.ko
  AUTOLOAD:=$(call AutoLoad,60,bcm2835-hwmon)
  $(call AddDepends/hwmon,@TARGET_brcm2708)
endef

define KernelPackage/hwmon-bcm2835/description
  Kernel module for BCM2835 thermal monitor chip
endef

$(eval $(call KernelPackage,hwmon-bcm2835))


I2C_BCM2708_MODULES:=\
  CONFIG_I2C_BCM2708:drivers/i2c/busses/i2c-bcm2708

define KernelPackage/i2c-bcm2708
  $(call i2c_defaults,$(I2C_BCM2708_MODULES),59)
  TITLE:=Broadcom BCM2708 I2C master controller driver
  KCONFIG+= CONFIG_I2C_BCM2708_BAUDRATE=100000
  DEPENDS:=@TARGET_brcm2708 +kmod-i2c-core
endef

define KernelPackage/i2c-bcm2708/description
  This package contains the Broadcom 2708 I2C master controller driver
endef

$(eval $(call KernelPackage,i2c-bcm2708))

I2C_BCM2835_MODULES:=\
  CONFIG_I2C_BCM2835:drivers/i2c/busses/i2c-bcm2835

define KernelPackage/i2c-bcm2835
  $(call i2c_defaults,$(I2C_BCM2835_MODULES),59)
  TITLE:=Broadcom BCM2835 I2C master controller driver
  DEPENDS:=@TARGET_brcm2708 +kmod-i2c-core
endef

define KernelPackage/i2c-bcm2835/description
  This package contains the Broadcom 2835 I2C master controller driver
endef

$(eval $(call KernelPackage,i2c-bcm2835))
