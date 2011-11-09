/*
 * arch/arm/mach-tegra/board-cardhu.c
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
#include <linux/i2c/panjit_ts.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/usb/android_composite.h>
#include <linux/usb/f_accessory.h>
#include <linux/spi/spi.h>
#include <linux/tegra_uart.h>
#include <linux/memblock.h>
#include <linux/spi-tegra.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/spdif.h>
#include <mach/audio.h>
#include <mach/tegra_das.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#include <sound/wm8903.h>
#include <mach/suspend.h>
#include <mach/pci.h>

#include "board.h"
#include "clock.h"
#include "board-cardhu.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "baseband-xmm-power.h"

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

/* !!!TODO: Change for cardhu (Taken from Ventana) */
static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_setup_offset = 0,
			.xcvr_use_fuses = 1,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[1] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 15,
			.xcvr_setup_offset = 0,
			.xcvr_use_fuses = 1,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
	[2] = {
			.hssync_start_delay = 0,
			.idle_wait_delay = 17,
			.elastic_limit = 16,
			.term_range_adj = 6,
			.xcvr_setup = 8,
			.xcvr_setup_offset = 0,
			.xcvr_use_fuses = 1,
			.xcvr_lsfslew = 2,
			.xcvr_lsrslew = 2,
	},
};

#ifdef CONFIG_BCM4329_RFKILL
static struct resource cardhu_bcm4329_rfkill_resources[] = {
	{
		.name   = "bcm4329_nshutdown_gpio",
		.start  = TEGRA_GPIO_PU0,
		.end    = TEGRA_GPIO_PU0,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device cardhu_bcm4329_rfkill_device = {
	.name = "bcm4329_rfkill",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(cardhu_bcm4329_rfkill_resources),
	.resource       = cardhu_bcm4329_rfkill_resources,
};

static noinline void __init cardhu_bt_rfkill(void)
{
	platform_device_register(&cardhu_bcm4329_rfkill_device);

	return;
}
#else
static inline void cardhu_bt_rfkill(void) { }
#endif

#ifdef CONFIG_BT_BLUESLEEP
static noinline void __init tegra_setup_bluesleep(void)
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
	res[0].start  = TEGRA_GPIO_PU6;
	res[0].end    = TEGRA_GPIO_PU6;
	res[0].flags  = IORESOURCE_IO;

	res[1].name   = "gpio_ext_wake";
	res[1].start  = TEGRA_GPIO_PU1;
	res[1].end    = TEGRA_GPIO_PU1;
	res[1].flags  = IORESOURCE_IO;

	res[2].name   = "host_wake";
	res[2].start  = gpio_to_irq(TEGRA_GPIO_PU6);
	res[2].end    = gpio_to_irq(TEGRA_GPIO_PU6);
	res[2].flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE ;

	if (platform_device_add_resources(pdev, res, 3)) {
		pr_err("unable to add resources to bluesleep device\n");
		goto err_free_res;
	}

	if (platform_device_add(pdev)) {
		pr_err("unable to add bluesleep device\n");
		goto err_free_res;
	}
	tegra_gpio_enable(TEGRA_GPIO_PU6);
	tegra_gpio_enable(TEGRA_GPIO_PU1);

return;

err_free_res:
	kfree(res);
err_free_dev:
	platform_device_put(pdev);
	return;
}
#else
static inline void tegra_setup_bluesleep(void) { }
#endif

static __initdata struct tegra_clk_init_table cardhu_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "pll_m",	NULL,		0,		false},
	{ "hda",	"pll_p",	108000000,	false},
	{ "hda2codec_2x","pll_p",	48000000,	false},
	{ "pwm",	"pll_p",	3187500,	false},
	{ "blink",	"clk_32k",	32768,		true},
	{ "pll_a",	NULL,		552960000,	false},
	{ "pll_a_out0",	NULL,		12288000,	false},
	{ "d_audio",	"pll_a_out0",	12288000,	false},
	{ "vi_sensor",	"pll_p",	150000000,	false},
	{ "i2c1",	"pll_p",	3200000,	false},
	{ "i2c2",	"pll_p",	3200000,	false},
	{ "i2c3",	"pll_p",	3200000,	false},
	{ "i2c4",	"pll_p",	3200000,	false},
	{ "i2c5",	"pll_p",	3200000,	false},
	{ NULL,		NULL,		0,		0},
};

#define USB_MANUFACTURER_NAME	"NVIDIA"
#define USB_PRODUCT_NAME		"Cardhu"
#define USB_PRODUCT_ID_MTP_ADB	0x7100
#define USB_PRODUCT_ID_MTP		0x7102
#define USB_PRODUCT_ID_RNDIS	0x7103
#define USB_VENDOR_ID			0x0955

static char *usb_functions_mtp_ums[] = { "mtp", "usb_mass_storage" };
static char *usb_functions_mtp_adb_ums[] = { "mtp", "adb", "usb_mass_storage" };
#ifdef CONFIG_USB_ANDROID_ACCESSORY
static char *usb_functions_accessory[] = { "accessory" };
static char *usb_functions_accessory_adb[] = { "accessory", "adb" };
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
static char *usb_functions_rndis[] = { "rndis" };
static char *usb_functions_rndis_adb[] = { "rndis", "adb" };
#endif
static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_ACCESSORY
	"accessory",
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"mtp",
	"adb",
	"usb_mass_storage"
};

static struct android_usb_product usb_products[] = {
	{
		.product_id     = USB_PRODUCT_ID_MTP,
		.num_functions  = ARRAY_SIZE(usb_functions_mtp_ums),
		.functions      = usb_functions_mtp_ums,
	},
	{
		.product_id     = USB_PRODUCT_ID_MTP_ADB,
		.num_functions  = ARRAY_SIZE(usb_functions_mtp_adb_ums),
		.functions      = usb_functions_mtp_adb_ums,
	},
#ifdef CONFIG_USB_ANDROID_ACCESSORY
	{
		.vendor_id	= USB_ACCESSORY_VENDOR_ID,
		.product_id	= USB_ACCESSORY_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_accessory),
		.functions	= usb_functions_accessory,
	},
	{
		.vendor_id	= USB_ACCESSORY_VENDOR_ID,
		.product_id	= USB_ACCESSORY_ADB_PRODUCT_ID,
		.num_functions	= ARRAY_SIZE(usb_functions_accessory_adb),
		.functions	= usb_functions_accessory_adb,
	},
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	{
		.product_id     = USB_PRODUCT_ID_RNDIS,
		.num_functions  = ARRAY_SIZE(usb_functions_rndis),
		.functions      = usb_functions_rndis,
	},
	{
		.product_id     = USB_PRODUCT_ID_RNDIS,
		.num_functions  = ARRAY_SIZE(usb_functions_rndis_adb),
		.functions      = usb_functions_rndis_adb,
	},
#endif
};

/* standard android USB platform data */
static struct android_usb_platform_data andusb_plat = {
	.vendor_id              = USB_VENDOR_ID,
	.product_id             = USB_PRODUCT_ID_MTP_ADB,
	.manufacturer_name      = USB_MANUFACTURER_NAME,
	.product_name           = USB_PRODUCT_NAME,
	.serial_number          = NULL,
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
};

static struct platform_device androidusb_device = {
	.name   = "android_usb",
	.id     = -1,
	.dev    = {
		.platform_data  = &andusb_plat,
	},
};

#ifdef CONFIG_USB_ANDROID_RNDIS
static struct usb_ether_platform_data rndis_pdata = {
	.ethaddr = {0, 0, 0, 0, 0, 0},
	.vendorID = USB_VENDOR_ID,
	.vendorDescr = USB_MANUFACTURER_NAME,
};

static struct platform_device rndis_device = {
	.name   = "rndis",
	.id     = -1,
	.dev    = {
		.platform_data  = &rndis_pdata,
	},
};
#endif

static struct wm8903_platform_data wm8903_pdata = {
	.irq_active_low = 0,
	.micdet_cfg = 0x83,           /* enable mic bias current */
	.micdet_delay = 0,
	.gpio_base = WM8903_GPIO_BASE,
	.gpio_cfg = {
		WM8903_GPIO_NO_CONFIG,
		WM8903_GPIO_NO_CONFIG,
		0,                     /* as output pin */
		WM8903_GPn_FN_GPIO_MICBIAS_CURRENT_DETECT
		<< WM8903_GP4_FN_SHIFT, /* as micbias current detect */
		WM8903_GPIO_NO_CONFIG,
	},
};

static struct i2c_board_info __initdata cardhu_i2c_bus1_board_info[] = {
	{
		I2C_BOARD_INFO("wm8903", 0x1a),
		.platform_data = &wm8903_pdata,
	},
};

static struct tegra_i2c_platform_data cardhu_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data cardhu_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_clkon_always = true,
	.scl_gpio		= {TEGRA_GPIO_PT5, 0},
	.sda_gpio		= {TEGRA_GPIO_PT6, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data cardhu_i2c3_platform_data = {
	.adapter_nr	= 2,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PBB1, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB2, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data cardhu_i2c4_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PV4, 0},
	.sda_gpio		= {TEGRA_GPIO_PV5, 0},
	.arb_recovery = arb_lost_recovery,
};

static struct tegra_i2c_platform_data cardhu_i2c5_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},
	.arb_recovery = arb_lost_recovery,
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
	.dev_clk_rate = 6144000,
	.mode = SPDIF_BIT_MODE_MODE16BIT,
	.fifo_fmt = AUDIO_FIFO_PACK_16,
};

struct wired_jack_conf audio_wr_jack_conf = {
	.hp_det_n = TEGRA_GPIO_PW2,
	.cdc_irq = TEGRA_GPIO_PW3,
	.en_spkr = WM8903_GP3,
	.spkr_amp_reg = "vdd_3v3_spk_amp"
};

struct tegra_das_platform_data tegra_das_pdata = {
	.tegra_dap_port_info_table = {
		/* I2S0 <--> NULL */
		[0] = {
			.dac_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format = 0,
			},
		},
		/* I2S1 <--> Hifi Codec */
		[1] = {
			.dac_port = tegra_das_port_i2s1,
			.codec_type = tegra_audio_codec_type_hifi,
			.device_property = {
				.num_channels = 2,
				.bits_per_sample = 16,
				.rate = 48000,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format =
						dac_dap_data_format_i2s,
			},
		},
		/* I2s2 <--> BB */
		[2] = {
			.dac_port = tegra_das_port_i2s2,
			.codec_type = tegra_audio_codec_type_baseband,
			.device_property = {
				.num_channels = 1,
				.bits_per_sample = 16,
				.rate = 16000,
				.master = 0,
				.lrck_high_left = true,
				.dac_dap_data_comm_format =
					dac_dap_data_format_dsp,
			},
		},
		/* I2s3 <--> BT */
		[3] = {
			.dac_port = tegra_das_port_i2s3,
			.codec_type = tegra_audio_codec_type_bluetooth,
			.device_property = {
				.num_channels = 1,
				.bits_per_sample = 16,
				.rate = 8000,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format =
					dac_dap_data_format_dsp,
			},
		},
		[4] = {
			.dac_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.master = 0,
				.lrck_high_left = false,
				.dac_dap_data_comm_format = 0,
			},
		},
	},
};

static void cardhu_audio_init(void)
{
#if defined(CONFIG_SND_HDA_TEGRA)
	platform_device_register(&tegra_hda_device);
#endif

	tegra_i2s_device1.dev.platform_data = &tegra_i2s_pdata[0];
	platform_device_register(&tegra_i2s_device1);

	tegra_i2s_device2.dev.platform_data = &tegra_i2s_pdata[1];
	platform_device_register(&tegra_i2s_device2);

	tegra_i2s_device3.dev.platform_data = &tegra_i2s_pdata[2];
	platform_device_register(&tegra_i2s_device3);

	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;
	platform_device_register(&tegra_spdif_device);
}

static void cardhu_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &cardhu_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &cardhu_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &cardhu_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &cardhu_i2c4_platform_data;
	tegra_i2c_device5.dev.platform_data = &cardhu_i2c5_platform_data;

	/* setting audio codec on i2c_4 */
	i2c_register_board_info(4, cardhu_i2c_bus1_board_info, 1);

	platform_device_register(&tegra_i2c_device5);
	platform_device_register(&tegra_i2c_device4);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device1);
}

static struct platform_device *cardhu_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
};
struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "clk_m"},
	[1] = {.name = "pll_p"},
	[2] = {.name = "pll_m"},
};

static struct tegra_uart_platform_data cardhu_uart_pdata;

static void __init uart_debug_init(void)
{
	struct board_info board_info;
	int debug_port_id;

	tegra_get_board_info(&board_info);

	debug_port_id = get_tegra_uart_debug_port_id();
	if (debug_port_id < 0) {
		debug_port_id = 0;
			/* UARTB is debug port
			 *       for SLT - E1186/E1187/PM269
			 *       for E1256/E1257
			 */
		if (((board_info.sku & SKU_SLT_ULPI_SUPPORT) &&
			((board_info.board_id == BOARD_E1186) ||
			(board_info.board_id == BOARD_E1187) ||
			(board_info.board_id == BOARD_PM269))) ||
			(board_info.board_id == BOARD_E1256) ||
			(board_info.board_id == BOARD_E1257))
				debug_port_id = 1;
	}
	switch (debug_port_id) {
	case 0:
		/* UARTA is the debug port. */
		pr_info("Selecting UARTA as the debug console\n");
		cardhu_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;

	case 1:
		/* UARTB is the debug port. */
		pr_info("Selecting UARTB as the debug console\n");
		cardhu_uart_devices[1] = &debug_uartb_device;
		debug_uart_clk =  clk_get_sys("serial8250.0", "uartb");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartb_device.dev.platform_data))->mapbase;
		break;

	case 2:
		/* UARTC is the debug port. */
		pr_info("Selecting UARTC as the debug console\n");
		cardhu_uart_devices[2] = &debug_uartc_device;
		debug_uart_clk =  clk_get_sys("serial8250.0", "uartc");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartc_device.dev.platform_data))->mapbase;
		break;

	case 3:
		/* UARTD is the debug port. */
		pr_info("Selecting UARTD as the debug console\n");
		cardhu_uart_devices[3] = &debug_uartd_device;
		debug_uart_clk =  clk_get_sys("serial8250.0", "uartd");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
		break;

	case 4:
		/* UARTE is the debug port. */
		pr_info("Selecting UARTE as the debug console\n");
		cardhu_uart_devices[4] = &debug_uarte_device;
		debug_uart_clk =  clk_get_sys("serial8250.0", "uarte");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarte_device.dev.platform_data))->mapbase;
		break;

	default:
		pr_info("The debug console id %d is invalid, Assuming UARTA", debug_port_id);
		cardhu_uart_devices[0] = &debug_uarta_device;
		debug_uart_clk = clk_get_sys("serial8250.0", "uarta");
		debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uarta_device.dev.platform_data))->mapbase;
		break;
	}
	return;
}

static void __init cardhu_uart_init(void)
{
	struct clk *c;
	int i;

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
	cardhu_uart_pdata.parent_clk_list = uart_parent_clk;
	cardhu_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uarta_device.dev.platform_data = &cardhu_uart_pdata;
	tegra_uartb_device.dev.platform_data = &cardhu_uart_pdata;
	tegra_uartc_device.dev.platform_data = &cardhu_uart_pdata;
	tegra_uartd_device.dev.platform_data = &cardhu_uart_pdata;
	tegra_uarte_device.dev.platform_data = &cardhu_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs()) {
		uart_debug_init();
		/* Clock enable for the debug channel */
		if (!IS_ERR_OR_NULL(debug_uart_clk)) {
			pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
			c = tegra_get_clock_by_name("pll_p");
			if (IS_ERR_OR_NULL(c))
				pr_err("Not getting the parent clock pll_p\n");
			else
				clk_set_parent(debug_uart_clk, c);

			clk_enable(debug_uart_clk);
			clk_set_rate(debug_uart_clk, clk_get_rate(c));
		} else {
			pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
		}
	}

	platform_add_devices(cardhu_uart_devices,
				ARRAY_SIZE(cardhu_uart_devices));
}

static struct platform_device *cardhu_spi_devices[] __initdata = {
	&tegra_spi_device4,
};

struct spi_clk_parent spi_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};

static struct tegra_spi_platform_data cardhu_spi_pdata = {
	.is_dma_based		= true,
	.max_dma_buffer		= (16 * 1024),
	.is_clkon_always	= false,
	.max_rate		= 100000000,
};

static void __init cardhu_spi_init(void)
{
	int i;
	struct clk *c;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	for (i = 0; i < ARRAY_SIZE(spi_parent_clk); ++i) {
		c = tegra_get_clock_by_name(spi_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						spi_parent_clk[i].name);
			continue;
		}
		spi_parent_clk[i].parent_clk = c;
		spi_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	cardhu_spi_pdata.parent_clk_list = spi_parent_clk;
	cardhu_spi_pdata.parent_clk_count = ARRAY_SIZE(spi_parent_clk);
	tegra_spi_device4.dev.platform_data = &cardhu_spi_pdata;
	platform_add_devices(cardhu_spi_devices,
				ARRAY_SIZE(cardhu_spi_devices));

	if (board_info.board_id == BOARD_E1198) {
		tegra_spi_device2.dev.platform_data = &cardhu_spi_pdata;
		platform_device_register(&tegra_spi_device2);
	}
}

#if defined(CONFIG_RTC_DRV_TEGRA)
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
#endif

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct resource ram_console_resources[] = {
	{
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name 		= "ram_console",
	.id 		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};

static struct platform_device *cardhu_devices[] __initdata = {
	&pmu_device,
#if defined(CONFIG_RTC_DRV_TEGRA)
	&tegra_rtc_device,
#endif
	&tegra_udc_device,
	&androidusb_device,
	&tegra_usb_fsg_device,
#if defined(CONFIG_TEGRA_IOVMM_SMMU)
	&tegra_smmu_device,
#endif
	&tegra_wdt_device,
#if defined(CONFIG_TEGRA_AVP)
	&tegra_avp_device,
#endif
	&tegra_camera,
#if defined(CONFIG_CRYPTO_DEV_TEGRA_SE)
	&tegra_se_device,
#endif
#if defined(CONFIG_CRYPTO_DEV_TEGRA_AES)
	&tegra_aes_device,
#endif
	&ram_console_device,
};


static struct tegra_uhsic_config uhsic_phy_config = {
	.enable_gpio = EN_HSIC_GPIO,
	.reset_gpio = -1,
	.sync_start_delay = 9,
	.idle_wait_delay = 17,
	.term_range_adj = 0,
	.elastic_underrun_limit = 16,
	.elastic_overrun_limit = 16,
};

static struct tegra_ehci_platform_data tegra_ehci_uhsic_pdata = {
	.phy_type = TEGRA_USB_PHY_TYPE_HSIC,
	.phy_config = &uhsic_phy_config,
	.operating_mode = TEGRA_USB_HOST,
	.power_down_on_bus_suspend = 1,
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
			.hotplug = 1,
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


#define SERIAL_NUMBER_LENGTH 20
static char usb_serial_num[SERIAL_NUMBER_LENGTH];

#ifdef CONFIG_USB_SUPPORT
static struct usb_phy_plat_data tegra_usb_phy_pdata[] = {
	[0] = {
			.instance = 0,
			.vbus_gpio = -1,
			.vbus_reg_supply = "vdd_vbus_micro_usb",
	},
	[1] = {
			.instance = 1,
			.vbus_gpio = -1,
	},
	[2] = {
			.instance = 2,
			.vbus_gpio = -1,
			.vbus_reg_supply = "vdd_vbus_typea_usb",
	},
};

static struct tegra_otg_platform_data tegra_otg_pdata = {
	.host_register = &tegra_usb_otg_host_register,
	.host_unregister = &tegra_usb_otg_host_unregister,
};

static int cardu_usb_hsic_postsupend(void)
{
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2);
#endif
	return 0;
}

static int cardu_usb_hsic_preresume(void)
{
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L2TOL0);
#endif
	return 0;
}

static int cardu_usb_hsic_phy_ready(void)
{
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L0);
#endif
	return 0;
}

static int cardu_usb_hsic_phy_off(void)
{
#ifdef CONFIG_TEGRA_BB_XMM_POWER
	baseband_xmm_set_power_status(BBXMM_PS_L3);
#endif
	return 0;
}

static void cardhu_usb_init(void)
{
	struct board_info bi;
	char *src = NULL;
	int i;

	tegra_get_board_info(&bi);

	tegra_usb_phy_init(tegra_usb_phy_pdata,
			ARRAY_SIZE(tegra_usb_phy_pdata));

	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);
	if (bi.board_id == BOARD_PM267) {
		uhsic_phy_config.reset_gpio =
			PM267_SMSC4640_HSIC_HUB_RESET_GPIO;
		tegra_ehci2_device.dev.platform_data = &tegra_ehci_uhsic_pdata;
		platform_device_register(&tegra_ehci2_device);
	} else if (bi.board_id == BOARD_E1256) {
		tegra_ehci2_device.dev.platform_data = &tegra_ehci_uhsic_pdata;
		platform_device_register(&tegra_ehci2_device);
	} else if (bi.board_id == BOARD_E1186) {
		/* for baseband devices do not switch off phy during suspend */
		tegra_ehci_uhsic_pdata.power_down_on_bus_suspend = 0;
		uhsic_phy_config.postsuspend = cardu_usb_hsic_postsupend;
		uhsic_phy_config.preresume = cardu_usb_hsic_preresume;
		uhsic_phy_config.usb_phy_ready = cardu_usb_hsic_phy_ready;
		uhsic_phy_config.post_phy_off = cardu_usb_hsic_phy_off;
		tegra_ehci2_device.dev.platform_data = &tegra_ehci_uhsic_pdata;
		/* baseband registration happens in baseband-xmm-power  */
	} else {
		tegra_ehci2_device.dev.platform_data = &tegra_ehci_pdata[1];
		platform_device_register(&tegra_ehci2_device);
	}

	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);

#ifdef CONFIG_USB_ANDROID_RNDIS
	src = usb_serial_num;

	/* create a fake MAC address from our serial number.
	 * first byte is 0x02 to signify locally administered.
	 */
	rndis_pdata.ethaddr[0] = 0x02;
	for (i = 0; *src; i++) {
		/* XOR the USB serial across the remaining bytes */
		rndis_pdata.ethaddr[i % (ETH_ALEN - 1) + 1] ^= *src++;
	}
	platform_device_register(&rndis_device);
#endif

}
#else
static void cardhu_usb_init(void) { }
#endif

static void cardhu_gps_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PU2);
	tegra_gpio_enable(TEGRA_GPIO_PU3);
}

static struct baseband_power_platform_data tegra_baseband_power_data = {
	.baseband_type = BASEBAND_XMM,
	.modem = {
	.xmm = {
			.bb_rst = XMM_GPIO_BB_RST,
			.bb_on = XMM_GPIO_BB_ON,
			.ipc_bb_wake = XMM_GPIO_IPC_BB_WAKE,
			.ipc_ap_wake = XMM_GPIO_IPC_AP_WAKE,
			.ipc_hsic_active = XMM_GPIO_IPC_HSIC_ACTIVE,
			.ipc_hsic_sus_req = XMM_GPIO_IPC_HSIC_SUS_REQ,
			.hsic_device = &tegra_ehci2_device,
		},
	},
};

static struct platform_device tegra_baseband_power_device = {
	.name = "baseband_xmm_power",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};

static struct platform_device tegra_baseband_power2_device = {
	.name = "baseband_xmm_power2",
	.id = -1,
	.dev = {
		.platform_data = &tegra_baseband_power_data,
	},
};


static struct tegra_pci_platform_data cardhu_pci_platform_data = {
	.port_status[0]	= 1,
	.port_status[1]	= 1,
	.port_status[2]	= 1,
	.use_dock_detect	= 0,
	.gpio		= 0,
};

static void cardhu_pci_init(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);
	if (board_info.board_id == BOARD_E1291) {
		cardhu_pci_platform_data.port_status[0] = 0;
		cardhu_pci_platform_data.port_status[1] = 0;
		cardhu_pci_platform_data.port_status[2] = 1;
		cardhu_pci_platform_data.use_dock_detect = 1;
		cardhu_pci_platform_data.gpio = DOCK_DETECT_GPIO;
	}
	tegra_pci_device.dev.platform_data = &cardhu_pci_platform_data;
	platform_device_register(&tegra_pci_device);
}

static void cardhu_modem_init(void)
{
	struct board_info board_info;
	int w_disable_gpio, ret;

	tegra_get_board_info(&board_info);
	switch (board_info.board_id) {
	case BOARD_E1291:
	case BOARD_E1198:
		if (((board_info.board_id == BOARD_E1291) &&
				(board_info.fab < BOARD_FAB_A03)) ||
			((board_info.board_id == BOARD_E1198) &&
					(board_info.fab < BOARD_FAB_A02))) {
			w_disable_gpio = TEGRA_GPIO_PH5;
		} else {
			w_disable_gpio = TEGRA_GPIO_PDD5;
		}
		tegra_gpio_enable(w_disable_gpio);
		ret = gpio_request(w_disable_gpio, "w_disable_gpio");
		if (ret < 0)
			pr_err("%s: gpio_request failed for gpio %d\n",
				__func__, w_disable_gpio);
		else
			gpio_direction_input(w_disable_gpio);

		/* E1291-A04 & E1198:A02: Set PERST signal to low */
		if (((board_info.board_id == BOARD_E1291) &&
				(board_info.fab >= BOARD_FAB_A04)) ||
			((board_info.board_id == BOARD_E1198) &&
					(board_info.fab >= BOARD_FAB_A02))) {
			ret = gpio_request(TEGRA_GPIO_PH7, "modem_perst");
			if (ret < 0) {
				pr_err("%s(): Error in allocating gpio "
					"TEGRA_GPIO_PH7\n", __func__);
				break;
			}
			gpio_direction_output(TEGRA_GPIO_PH7, 0);
			tegra_gpio_enable(TEGRA_GPIO_PH7);
		}
		break;
	case BOARD_E1186:
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.bb_rst);
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.bb_on);
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.ipc_bb_wake);
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.ipc_ap_wake);
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.ipc_hsic_active);
		tegra_gpio_enable(
			tegra_baseband_power_data.modem.xmm.ipc_hsic_sus_req);
		platform_device_register(&tegra_baseband_power_device);
		platform_device_register(&tegra_baseband_power2_device);
		break;
	default:
		break;
	}

}

#ifdef CONFIG_SATA_AHCI_TEGRA
static void cardhu_sata_init(void)
{
	platform_device_register(&tegra_sata_device);
}
#else
static void cardhu_sata_init(void) { }
#endif

static void __init tegra_cardhu_init(void)
{
	tegra_common_init();
	tegra_clk_init_from_table(cardhu_clk_init_table);
	cardhu_pinmux_init();
	cardhu_i2c_init();
	cardhu_spi_init();
	cardhu_usb_init();
#ifdef CONFIG_TEGRA_EDP_LIMITS
	cardhu_edp_init();
#endif
	cardhu_uart_init();
	snprintf(usb_serial_num, sizeof(usb_serial_num), "%llx", tegra_chip_uid());
	andusb_plat.serial_number = kstrdup(usb_serial_num, GFP_KERNEL);
	platform_add_devices(cardhu_devices, ARRAY_SIZE(cardhu_devices));
	cardhu_audio_init();
	cardhu_sdhci_init();
	cardhu_regulator_init();
	cardhu_gpio_switch_regulator_init();
	cardhu_suspend_init();
	cardhu_power_off_init();
	touch_init();
	cardhu_gps_init();
	cardhu_modem_init();
	cardhu_kbc_init();
	cardhu_scroll_init();
	cardhu_keys_init();
	cardhu_panel_init();
	cardhu_pmon_init();
	cardhu_sensors_init();
	cardhu_bt_rfkill();
	tegra_setup_bluesleep();
	cardhu_sata_init();
	audio_wired_jack_init();
	cardhu_pins_state_init();
	cardhu_emc_init();
	tegra_release_bootloader_fb();
	cardhu_pci_init();
}

static void __init tegra_cardhu_reserve(void)
{
	struct resource *res;
	long ret;
#if defined(CONFIG_NVMAP_CONVERT_CARVEOUT_TO_IOVMM)
	/* support 1920X1200 with 24bpp */
	tegra_reserve(0, SZ_8M + SZ_1M, SZ_8M + SZ_1M);
#else
	tegra_reserve(SZ_128M, SZ_8M, SZ_8M);
#endif

	res = platform_get_resource(&ram_console_device, IORESOURCE_MEM, 0);
	res->start = memblock_end_of_DRAM() - SZ_1M;
	res->end = res->start + SZ_1M - 1;
	ret = memblock_remove(res->start, SZ_1M);
	if (ret) {
		ram_console_device.resource = NULL;
		ram_console_device.num_resources = 0;
		pr_err("Failed to reserve memory block for ram console\n");
	}
}

MACHINE_START(CARDHU, "cardhu")
	.boot_params    = 0x80000100,
	.phys_io        = IO_APB_PHYS,
	.io_pg_offst    = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.init_irq       = tegra_init_irq,
	.init_machine   = tegra_cardhu_init,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_cardhu_reserve,
	.timer          = &tegra_timer,
MACHINE_END
