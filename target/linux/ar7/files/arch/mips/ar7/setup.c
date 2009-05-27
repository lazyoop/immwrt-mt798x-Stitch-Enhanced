/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/pm.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serial_8250.h>

#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/ar7/ar7.h>
#include <asm/ar7/prom.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24) /* TODO remove when 2.6.24 is stable */
extern void ar7_time_init(void);
#endif
static void ar7_machine_restart(char *command);
static void ar7_machine_halt(void);
static void ar7_machine_power_off(void);

static void ar7_machine_restart(char *command)
{
	u32 *softres_reg = (u32 *)ioremap(AR7_REGS_RESET +
					  AR7_RESET_SOFTWARE, 1);
	writel(1, softres_reg);
}

static void ar7_machine_halt(void)
{
	while (1);
}

static void ar7_machine_power_off(void)
{
	u32 *power_reg = (u32 *)ioremap(AR7_REGS_POWER, 1);
	u32 power_state = readl(power_reg) | (3 << 30);
	writel(power_state, power_reg);
	ar7_machine_halt();
}

const char *get_system_type(void)
{
	u16 chip_id = ar7_chip_id();
	switch (chip_id) {
	case AR7_CHIP_7300:
		return "TI AR7 (TNETD7300)";
	case AR7_CHIP_7100:
		return "TI AR7 (TNETD7100)";
	case AR7_CHIP_7200:
		return "TI AR7 (TNETD7200)";
	default:
		return "TI AR7 (Unknown)";
	}
}

static int __init ar7_init_console(void)
{
	int res;

	static struct uart_port uart_port[2];

	memset(uart_port, 0, sizeof(struct uart_port) * 2);

	uart_port[0].type = PORT_AR7;
	uart_port[0].line = 0;
	uart_port[0].irq = AR7_IRQ_UART0;
	uart_port[0].uartclk = ar7_bus_freq() / 2;
	uart_port[0].iotype = UPIO_MEM;
	uart_port[0].mapbase = AR7_REGS_UART0;
	uart_port[0].membase = ioremap(uart_port[0].mapbase, 256);
	uart_port[0].regshift = 2;
	res = early_serial_setup(&uart_port[0]);
	if (res)
		return res;

	/* Only TNETD73xx have a second serial port */
	if (ar7_has_second_uart()) {
		uart_port[1].type = PORT_AR7;
		uart_port[1].line = 1;
		uart_port[1].irq = AR7_IRQ_UART1;
		uart_port[1].uartclk = ar7_bus_freq() / 2;
		uart_port[1].iotype = UPIO_MEM;
		uart_port[1].mapbase = UR8_REGS_UART1;
		uart_port[1].membase = ioremap(uart_port[1].mapbase, 256);
		uart_port[1].regshift = 2;
		res = early_serial_setup(&uart_port[1]);
		if (res)
			return res;
	}

	return add_preferred_console("ttyS", 0, NULL);
}

/*
 * Initializes basic routines and structures pointers, memory size (as
 * given by the bios and saves the command line.
 */

extern void ar7_init_clocks(void);

void __init plat_mem_setup(void)
{
	unsigned long io_base;

	_machine_restart = ar7_machine_restart;
	_machine_halt = ar7_machine_halt;
	pm_power_off = ar7_machine_power_off;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24) /* TODO remove when 2.6.24 is stable */
	board_time_init = ar7_time_init;
#endif
	panic_timeout = 3;

	io_base = (unsigned long)ioremap(AR7_REGS_BASE, 0x10000);
	if (!io_base) panic("Can't remap IO base!\n");
	set_io_port_base(io_base);

	prom_meminit();
	ar7_init_clocks();

	ioport_resource.start = 0;
	ioport_resource.end   = ~0;
	iomem_resource.start  = 0;
	iomem_resource.end    = ~0;

	printk(KERN_INFO "%s, ID: 0x%04x, Revision: 0x%02x\n",
					get_system_type(),
		ar7_chip_id(), ar7_chip_rev());
}

console_initcall(ar7_init_console);
