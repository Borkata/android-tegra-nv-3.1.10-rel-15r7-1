/*
 * arch/arm/mach-tegra/board-enterprise.c
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
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/usb/android_composite.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/spdif.h>
#include <mach/audio.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <sound/max98088.h>

#include "board.h"
#include "clock.h"
#include "board-enterprise.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"


static struct usb_mass_storage_platform_data tegra_usb_fsg_platform = {
	.vendor = "NVIDIA",
	.product = "Tegra 3",
	.nluns = 1,
};

static struct platform_device tegra_usb_fsg_device = {
	.name = "usb_mass_storage",
	.id = -1,
	.dev = {
		.platform_data = &tegra_usb_fsg_platform,
	},
};

/* !!!TODO: Change for enterprise (Taken from Cardhu) */
static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[1] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[2] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 8,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
};

#ifdef CONFIG_BCM4329_RFKILL
static struct resource enterprise_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PE6,
		.end    = TEGRA_GPIO_PE6,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device enterprise_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id		= -1,
	.num_resources  = ARRAY_SIZE(enterprise_bcm4329_rfkill_resources),
	.resource       = enterprise_bcm4329_rfkill_resources,
};

static noinline void __init enterprise_bt_rfkill(void)
{
	platform_device_register(&enterprise_bcm4329_rfkill_device);

	return;
}
#else
static inline void enterprise_bt_rfkill(void) { }
#endif

static void __init enterprise_setup_bluesleep(void)
{
	struct platform_device *pdev = NULL;
	struct resource *res;

	pdev = platform_device_alloc("bluesleep", 0);
	if (!pdev) {
		pr_err("unable to allocate platform device for bluesleep");
		return;
	}

	res = kzalloc(sizeof(struct resource) * 3, GFP_KERNEL);
	if (!res) {
		pr_err("unable to allocate resource for bluesleep\n");
		goto err_free_dev;
	}

	res[0].name   = "gpio_host_wake";
	res[0].start  = TEGRA_GPIO_PS2;
	res[0].end    = TEGRA_GPIO_PS2;
	res[0].flags  = IORESOURCE_IO;

	res[1].name   = "gpio_ext_wake";
	res[1].start  = TEGRA_GPIO_PE7;
	res[1].end    = TEGRA_GPIO_PE7;
	res[1].flags  = IORESOURCE_IO;

	res[2].name   = "host_wake";
	res[2].start  = gpio_to_irq(TEGRA_GPIO_PS2);
	res[2].end    = gpio_to_irq(TEGRA_GPIO_PS2);
	res[2].flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE ;

	if (platform_device_add_resources(pdev, res, 3)) {
		pr_err("unable to add resources to bluesleep device\n");
		goto err_free_res;
	}

	if (platform_device_add(pdev)) {
		pr_err("unable to add bluesleep device\n");
		goto err_free_res;
	}
	tegra_gpio_enable(TEGRA_GPIO_PS2);
	tegra_gpio_enable(TEGRA_GPIO_PE7);

	return;

err_free_res:
	kfree(res);
err_free_dev:
	platform_device_put(pdev);
	return;
}

static __initdata struct tegra_clk_init_table enterprise_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		true},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},
	{ "pwm",	"clk_32k",	32768,		false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "pll_a",	NULL,		552960000,	false},
	{ "pll_a_out0",	NULL,	12288000,	false},
	{ "d_audio",	"pll_a_out0",	12288000,	false},
	{ NULL,		NULL,		0,		0},
};

static char *usb_functions[] = { "mtp", "usb_mass_storage" };
static char *usb_functions_adb[] = { "mtp", "adb", "usb_mass_storage" };

static struct android_usb_product usb_products[] = {
	{
		.product_id     = 0x7102,
		.num_functions  = ARRAY_SIZE(usb_functions),
		.functions      = usb_functions,
	},
	{
		.product_id     = 0x7100,
		.num_functions  = ARRAY_SIZE(usb_functions_adb),
		.functions      = usb_functions_adb,
	},
};

/* standard android USB platform data */
static struct android_usb_platform_data andusb_plat = {
	.vendor_id              = 0x0955,
	.product_id             = 0x7100,
	.manufacturer_name      = "NVIDIA",
	.product_name           = "Enterprise",
	.serial_number          = NULL,
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_adb),
	.functions = usb_functions_adb,
};

static struct platform_device androidusb_device = {
	.name   = "android_usb",
	.id     = -1,
	.dev    = {
		.platform_data  = &andusb_plat,
	},
};

/* Equalizer filter coefs generated from the MAXIM MAX98088
 * evkit software tool */
static struct max98088_eq_cfg max98088_eq_cfg[] = {
	{
		.name = "FLAT",
		.rate = 44100,
		.band1 = {0x2000, 0xC002, 0x4000, 0x00E9, 0x0000},
		.band2 = {0x2000, 0xC00F, 0x4000, 0x02BC, 0x0000},
		.band3 = {0x2000, 0xC0A7, 0x4000, 0x0916, 0x0000},
		.band4 = {0x2000, 0xC5C2, 0x4000, 0x1A87, 0x0000},
		.band5 = {0x2000, 0xF6B0, 0x4000, 0x3F51, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 44100,
		.band1 = {0x205D, 0xC001, 0x3FEF, 0x002E, 0x02E0},
		.band2 = {0x5B9A, 0xC093, 0x3AB2, 0x088B, 0x1981},
		.band3 = {0x0D22, 0xC170, 0x26EA, 0x0D79, 0x32CF},
		.band4 = {0x0894, 0xC612, 0x01B3, 0x1B34, 0x3FFA},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=5KHz */
		.name = "HIBOOST",
		.rate = 44100,
		.band1 = {0x0815, 0xC001, 0x3AA4, 0x0003, 0x19A2},
		.band2 = {0x0815, 0xC103, 0x092F, 0x0B55, 0x3F56},
		.band3 = {0x0E0A, 0xC306, 0x1E5C, 0x136E, 0x3856},
		.band4 = {0x2459, 0xF665, 0x0CAA, 0x3F46, 0x3EBB},
		.band5 = {0x5BBB, 0x3FFF, 0xCEB0, 0x0000, 0x28CA},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 44100,
		.band1 = {0x7FC1, 0xC001, 0x3EE8, 0x0020, 0x0BC7},
		.band2 = {0x51E9, 0xC016, 0x3C7C, 0x033F, 0x14E9},
		.band3 = {0x1745, 0xC12C, 0x1680, 0x0C2F, 0x3BE9},
		.band4 = {0x4536, 0xD7E2, 0x0ED4, 0x31DD, 0x3E42},
		.band5 = {0x7FEF, 0x3FFF, 0x0BAB, 0x0000, 0x3EED},
	},
	{
		.name = "FLAT",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x2000, 0xC033, 0x4000, 0x0505, 0x0000},
		.band3 = {0x2000, 0xC268, 0x4000, 0x115F, 0x0000},
		.band4 = {0x2000, 0xDA62, 0x4000, 0x33C6, 0x0000},
		.band5 = {0x2000, 0x4000, 0x4000, 0x0000, 0x0000},
	},
	{
		.name = "LOWPASS1K",
		.rate = 16000,
		.band1 = {0x2000, 0xC004, 0x4000, 0x0141, 0x0000},
		.band2 = {0x5BE8, 0xC3E0, 0x3307, 0x15ED, 0x26A0},
		.band3 = {0x0F71, 0xD15A, 0x08B3, 0x2BD0, 0x3F67},
		.band4 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
		.band5 = {0x0815, 0x3FFF, 0xCF78, 0x0000, 0x29B7},
	},
	{ /* BASS=-12dB, TREBLE=+9dB, Fc=2KHz */
		.name = "HIBOOST",
		.rate = 16000,
		.band1 = {0x0815, 0xC001, 0x3BD2, 0x0009, 0x16BF},
		.band2 = {0x080E, 0xC17E, 0xF653, 0x0DBD, 0x3F43},
		.band3 = {0x0F80, 0xDF45, 0xEE33, 0x36FE, 0x3D79},
		.band4 = {0x590B, 0x3FF0, 0xE882, 0x02BD, 0x3B87},
		.band5 = {0x4C87, 0xF3D0, 0x063F, 0x3ED4, 0x3FB1},
	},
	{ /* BASS=12dB, TREBLE=+12dB */
		.name = "LOUD12DB",
		.rate = 16000,
		.band1 = {0x7FC1, 0xC001, 0x3D07, 0x0058, 0x1344},
		.band2 = {0x2DA6, 0xC013, 0x3CF1, 0x02FF, 0x138B},
		.band3 = {0x18F1, 0xC08E, 0x244D, 0x0863, 0x34B5},
		.band4 = {0x2BE0, 0xF385, 0x04FD, 0x3EC5, 0x3FCE},
		.band5 = {0x7FEF, 0x4000, 0x0BAB, 0x0000, 0x3EED},
	},
};


static struct max98088_pdata max98088_pdata = {
	/* equalizer configuration */
	.eq_cfg = max98088_eq_cfg,
	.eq_cfgcnt = ARRAY_SIZE(max98088_eq_cfg),

	/* microphone configuration */
	.digmic_left_mode = 1,	/* 1 = digital mic */
	.digmic_right_mode = 1,	/* 1 = digital mic */

	/* receiver output configuration */
	.receiver_mode = 0,	/* 0 = amplifier, 1 = line output */
};
static struct i2c_board_info __initdata enterprise_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("max98088", 0x10),
		.platform_data = &max98088_pdata,
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PW3),
	},
};

static struct tegra_i2c_platform_data enterprise_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data enterprise_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
};

static struct tegra_i2c_platform_data enterprise_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data enterprise_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_i2c_platform_data enterprise_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
};

static struct tegra_audio_platform_data tegra_i2s_pdata[] = {
	[0] = {
		.i2s_master	= true,
		.dma_on		= true,  /* use dma by default */
		.i2s_master_clk = 48000,
		.dev_clk_rate	= 12288000,
		.mode		= AUDIO_FRAME_FORMAT_I2S,
		.fifo_fmt	= AUDIO_FIFO_PACK_16,
		.bit_size	= AUDIO_BIT_SIZE_16,
		.i2s_bus_width	= 32,
		.dsp_bus_width	= 16,
	},
	[1] = {
		.i2s_master	= true,
		.dma_on		= true,  /* use dma by default */
		.i2s_master_clk = 8000,
		.dev_clk_rate	= 1024000,
		.mode		= AUDIO_FRAME_FORMAT_DSP,
		.fifo_fmt	= AUDIO_FIFO_NOP,
		.bit_size	= AUDIO_BIT_SIZE_16,
		.i2s_bus_width	= 32,
		.dsp_bus_width	= 16,
	},
	[2] = {
		.i2s_master	= true,
		.dma_on		= true,  /* use dma by default */
		.i2s_master_clk = 8000,
		.dev_clk_rate	= 1024000,
		.mode		= AUDIO_FRAME_FORMAT_DSP,
		.fifo_fmt	= AUDIO_FIFO_NOP,
		.bit_size	= AUDIO_BIT_SIZE_16,
		.i2s_bus_width	= 32,
		.dsp_bus_width	= 16,
	},
};

static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on = true,  /* use dma by default */
	.dev_clk_rate = 5644800,
	.mode = SPDIF_BIT_MODE_MODE16BIT,
	.fifo_fmt = AUDIO_FIFO_PACK_16,
};

struct wired_jack_conf audio_wr_jack_conf = {
	.hp_det_n = TEGRA_GPIO_PW2,
	.en_mic_ext = TEGRA_GPIO_PX1,
	.en_mic_int = TEGRA_GPIO_PX0,
};

static void enterprise_audio_init(void)
{
#if defined(CONFIG_SND_HDA_TEGRA)
	platform_device_register(&tegra_hda_device);
#endif

	gpio_request(TEGRA_GPIO_PW3, "audio_int");
	gpio_direction_input(TEGRA_GPIO_PW3);
	tegra_gpio_enable(TEGRA_GPIO_PW3);

	tegra_i2s_device0.dev.platform_data = &tegra_i2s_pdata[0];
	platform_device_register(&tegra_i2s_device0);

	tegra_i2s_device2.dev.platform_data = &tegra_i2s_pdata[1];
	platform_device_register(&tegra_i2s_device2);

	tegra_i2s_device3.dev.platform_data = &tegra_i2s_pdata[2];
	platform_device_register(&tegra_i2s_device3);

	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;
	platform_device_register(&tegra_spdif_device);
}

static void enterprise_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &enterprise_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &enterprise_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &enterprise_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &enterprise_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &enterprise_i2c5_platform_data;

	i2c_register_board_info(0, enterprise_i2c_bus1_board_info,
				ARRAY_SIZE(enterprise_i2c_bus1_board_info));

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);
}

static struct platform_device *enterprise_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};

struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};
static struct clk *debug_uart_clk;
static struct tegra_uart_platform_data enterprise_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	enterprise_uart_devices[3] = &debug_uartd_device;
	debug_uart_clk = clk_get_sys("serial8250.0", "uartd");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
				debug_uart_clk->name);
	}
}

static void __init enterprise_uart_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	enterprise_uart_pdata.parent_clk_list = uart_parent_clk;
	enterprise_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartb_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartc_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uartd_device.dev.platform_data = &enterprise_uart_pdata;
	tegra_uarte_device.dev.platform_data = &enterprise_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(enterprise_uart_devices,
				ARRAY_SIZE(enterprise_uart_devices));
}



static struct resource tegra_rtc_resources[] = {
	[0] = {
		.start = TEGRA_RTC_BASE,
		.end = TEGRA_RTC_BASE + TEGRA_RTC_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = INT_RTC,
		.end = INT_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device tegra_rtc_device = {
	.name = "tegra_rtc",
	.id   = -1,
	.resource = tegra_rtc_resources,
	.num_resources = ARRAY_SIZE(tegra_rtc_resources),
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct platform_device *enterprise_devices[] __initdata = {
	&tegra_usb_fsg_device,
	&androidusb_device,
	&pmu_device,
	&tegra_rtc_device,
	&tegra_udc_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt_device,
	&tegra_avp_device,
	&tegra_camera,
	&tegra_spi_device4,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
};

static struct usb_phy_plat_data tegra_usb_phy_pdata[] = {
	[0] = {
			.instance = 0,
			.vbus_gpio = -1,
			.vbus_reg_supply = "usb_vbus",
	},
	[1] = {
			.instance = 1,
			.vbus_gpio = -1,
	},
	[2] = {
			.instance = 2,
			.vbus_gpio = -1,
	},
};


static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
			.phy_config = &utmi_phy_config[0],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[1] = {
			.phy_config = &utmi_phy_config[1],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
	[2] = {
			.phy_config = &utmi_phy_config[2],
			.operating_mode = TEGRA_USB_HOST,
			.power_down_on_bus_suspend = 1,
	},
};

struct platform_device *tegra_usb_otg_host_register(void)
{
	struct platform_device *pdev;
	void *platform_data;
	int val;

	pdev = platform_device_alloc(tegra_ehci1_device.name,
		tegra_ehci1_device.id);
	if (!pdev)
		return NULL;

	val = platform_device_add_resources(pdev, tegra_ehci1_device.resource,
		tegra_ehci1_device.num_resources);
	if (val)
		goto error;

	pdev->dev.dma_mask =  tegra_ehci1_device.dev.dma_mask;
	pdev->dev.coherent_dma_mask = tegra_ehci1_device.dev.coherent_dma_mask;

	platform_data = kmalloc(sizeof(struct tegra_ehci_platform_data),
		GFP_KERNEL);
	if (!platform_data)
		goto error;

	memcpy(platform_data, &tegra_ehci_pdata[0],
				sizeof(struct tegra_ehci_platform_data));
	pdev->dev.platform_data = platform_data;

	val = platform_device_add(pdev);
	if (val)
		goto error_add;

	return pdev;

error_add:
	kfree(platform_data);
error:
	pr_err("%s: failed to add the host contoller device\n", __func__);
	platform_device_put(pdev);
	return NULL;
}

void tegra_usb_otg_host_unregister(struct platform_device *pdev)
{
	platform_device_unregister(pdev);
}

static struct tegra_otg_platform_data tegra_otg_pdata = {
	.host_register = &tegra_usb_otg_host_register,
	.host_unregister = &tegra_usb_otg_host_unregister,
};

static void enterprise_usb_init(void)
{
	tegra_usb_phy_init(tegra_usb_phy_pdata, ARRAY_SIZE(tegra_usb_phy_pdata));

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);

}

static void enterprise_gps_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PE4);
	tegra_gpio_enable(TEGRA_GPIO_PE5);
}

static void enterprise_baseband_init(void)
{
	int modem_id = tegra_get_modem_id();

	switch (modem_id) {
	case 1: /* PH450 ULPI */
		enterprise_modem_init();
		break;
	case 2: /* 6260 HSIC */
		break;
	}
}

static void __init tegra_enterprise_init(void)
{
	char serial[20];

	tegra_common_init();
	tegra_clk_init_from_table(enterprise_clk_init_table);
	enterprise_pinmux_init();
	enterprise_i2c_init();
	enterprise_uart_init();
	snprintf(serial, sizeof(serial), "%llx", tegra_chip_uid());
	andusb_plat.serial_number = kstrdup(serial, GFP_KERNEL);
	platform_add_devices(enterprise_devices, ARRAY_SIZE(enterprise_devices));
	enterprise_regulator_init();
	enterprise_audio_init();
	enterprise_sdhci_init();
	touch_init();
	enterprise_usb_init();
	enterprise_kbc_init();
	enterprise_gps_init();
	enterprise_baseband_init();
	enterprise_panel_init();
	enterprise_bt_rfkill();
	enterprise_setup_bluesleep();
	audio_wired_jack_init();
	enterprise_emc_init();
	enterprise_sensors_init();
	enterprise_suspend_init();
	tegra_release_bootloader_fb();
}

static void __init tegra_enterprise_reserve(void)
{
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	tegra_reserve(0, SZ_4M, SZ_8M);
#else
	tegra_reserve(SZ_128M, SZ_4M, SZ_8M);
#endif
}

MACHINE_START(TEGRA_ENTERPRISE, "tegra_enterprise")
	.boot_params    = 0x80000100,
	.phys_io        = IO_APB_PHYS,
	.io_pg_offst    = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.init_irq       = tegra_init_irq,
	.init_machine   = tegra_enterprise_init,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_enterprise_reserve,
	.timer          = &tegra_timer,
MACHINE_END
