/*
 * arch/arm/mach-tegra/board-touch-atmel_maxtouch.c
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c/atmel_maxtouch.h>

#if defined (CONFIG_MACH_CARDHU)
#include "board-cardhu.h"
#endif

#if defined (CONFIG_MACH_TEGRA_ENTERPRISE)
#include "board-enterprise.h"
#endif

#if defined (CONFIG_MACH_VENTANA)
#include "board-ventana.h"
#endif

#include "gpio-names.h"
#include "touch.h"


/* Atmel MaxTouch touchscreen              Driver data */
/*-----------------------------------------------------*/
/*
 * Reads the CHANGELINE state; interrupt is valid if the changeline
 * is low.
 */
static u8 read_chg(void)
{
	return gpio_get_value(TOUCH_GPIO_IRQ_ATMEL_T9);
}

static u8 valid_interrupt(void)
{
	return !read_chg();
}


static struct mxt_platform_data atmel_mxt_info = {
	/* Maximum number of simultaneous touches to report. */
	.numtouch = 10,
	// TODO: no need for any hw-specific things at init/exit?
	.init_platform_hw = NULL,
	.exit_platform_hw = NULL,
#if defined (CONFIG_MACH_TEGRA_ENTERPRISE)
	.max_x = 540,
	.max_y = 960,
#else
	.max_x = 1366,
	.max_y = 768,
#endif
	.valid_interrupt = &valid_interrupt,
	.read_chg = &read_chg,
};

static struct i2c_board_info __initdata atmxt_i2c_info[] = {
	{
#if defined (CONFIG_MACH_TEGRA_ENTERPRISE)
	 I2C_BOARD_INFO("maXTouch", MXT224_I2C_ADDR1),
#else
	 I2C_BOARD_INFO("maXTouch", MXT_I2C_ADDRESS),
#endif
	 .irq = TEGRA_GPIO_TO_IRQ(TOUCH_GPIO_IRQ_ATMEL_T9),
	 .platform_data = &atmel_mxt_info,
	 },
};

struct tegra_touchscreen_init __initdata atmel_mxt_init_data = {
	.irq_gpio = TOUCH_GPIO_IRQ_ATMEL_T9,			/* GPIO1 Value for IRQ */
	.rst_gpio = TOUCH_GPIO_RST_ATMEL_T9,			/* GPIO2 Value for RST */
	.sv_gpio1 = {1, TOUCH_GPIO_RST_ATMEL_T9, 0, 1},		/* Valid, GPIOx, Set value, Delay      */
	.sv_gpio2 = {1, TOUCH_GPIO_RST_ATMEL_T9, 1, 100},	/* Valid, GPIOx, Set value, Delay      */
	.ts_boardinfo = {TOUCH_BUS_ATMEL_T9, atmxt_i2c_info, 1}	/* BusNum, BoardInfo, Value     */
};

