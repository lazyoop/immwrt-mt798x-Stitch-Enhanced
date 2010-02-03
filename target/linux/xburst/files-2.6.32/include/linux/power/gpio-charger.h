/*
 *  Copyright (C) 2010, Lars-Peter Clausen <lars@metafoo.de>
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __LINUX_POWER_GPIO_CHARGER_H__
#define __LINUX_POWER_GPIO_CHARGER_H__

struct gpio_charger_platform_data {
	const char *name;
	enum power_supply_type type;
	int gpio;
	int gpio_active_low;

	char **batteries;
	size_t num_batteries;
};

#endif
