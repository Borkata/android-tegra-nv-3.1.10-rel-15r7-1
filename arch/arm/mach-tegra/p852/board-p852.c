/*
 * arch/arm/mach-tegra/board-p852.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "board-p852.h"
#include <mach/spdif.h>
#include <mach/tegra_das.h>

unsigned int p852_sku_peripherals;
unsigned int p852_spi_peripherals;
unsigned int p852_i2s_peripherals;
unsigned int p852_uart_peripherals;
unsigned int p852_i2c_peripherals;
unsigned int p852_sdhci_peripherals;
unsigned int p852_display_peripherals;

/* If enable_usb3 can have two options ehci3=eth or usb*/
static char enable_usb3[4];

int __init parse_enable_usb3(char *arg)
{
	if (!arg)
		return 0;

	strncpy(enable_usb3, arg, sizeof(enable_usb3));
	return 0;
}

early_param("ehci3", parse_enable_usb3);

static __initdata struct tegra_clk_init_table p852_clk_init_table[] = {
	/* name         parent          rate       enabled */
	{"uarta",       "pll_p",      216000000,    true},
	{"uartb",       "pll_p",      216000000,    true},
	{"uartc",       "pll_p",      216000000,    true},
	{"uartd",       "pll_p",      216000000,    true},
	{"pll_m",       "clk_m",      600000000,    true},
	{"pll_m_out1",  "pll_m",      240000000,    true},
	{"pll_p_out4",  "pll_p",      240000000,    true},
	{"host1x",      "pll_p",      166000000,    true},
	{"disp1",       "pll_p",      216000000,    true},
	{"vi",          "pll_m",      100000000,    true},
	{"csus",        "pll_m",      100000000,    true},
	{"emc",         "pll_m",      600000000,    true},
	{"pll_c",       "clk_m",      600000000,    true},
	{"pll_c_out1",  "pll_c",      240000000,    true},
	{"pwm",         "clk_32k",    32768,        false},
	{"clk_32k",     NULL,         32768,        true},
	{"pll_a",       NULL,         56448000,     true},
	{"pll_a_out0",  "pll_a",      11289600,     true},
	{"audio",       "pll_a_out0", 11289600,     true},
	{"audio_2x",    "audio",      22579200,     false},
	{"vde",         "pll_c",      240000000,    false},
	{"vi_sensor",   "pll_m",      111000000,    true},
	{"epp",         "pll_m",      111000000,    true},
	{"mpe",         "pll_m",      111000000,    true},
	{"i2s1",        "pll_a_out0", 11289600,     true},
	{"i2s2",        "pll_a_out0", 11289600,     true},
	{"ndflash",     "pll_p",      86500000,     true},
	{"sbc1",        "pll_p",      12000000,     false},
	{"spdif_in",    "pll_m",      22579000,     true},
	{"spdif_out",   "pll_a_out0", 5644800,      true},
	{"sbc2",        "pll_p",      12000000,     false},
	{"sbc3",        "pll_p",      12000000,     false},
	{"sbc4",        "pll_p",      12000000,     false},
	{"nor",         "pll_p",      86500000,     true},
	{NULL,          NULL,         0,            0},
};

struct tegra_audio_platform_data tegra_i2s_audio_pdata[] = {
	/* For I2S1 */
	[0] = {
	       .i2s_master = false,
	       .dma_on = true,	/* use dma by default */
	       .dev_clk_rate = 2822400,
	       .dap_clk = "",
	       .audio_sync_clk = "audio_2x",
	       .mode = AUDIO_FRAME_FORMAT_I2S,
	       .fifo_fmt = I2S_FIFO_PACKED,
	       .bit_size = I2S_BIT_SIZE_16,
		   .i2s_bus_width = 32,
	       },
	/* For I2S2 */
	[1] = {
	       .i2s_master = true,
	       .dma_on = true,	/* use dma by default */
	       .dev_clk_rate = 2822400,
	       .dap_clk = "",
	       .audio_sync_clk = "audio_2x",
	       .mode = AUDIO_FRAME_FORMAT_I2S,
	       .fifo_fmt = I2S_FIFO_PACKED,
	       .bit_size = I2S_BIT_SIZE_16,
	       .i2s_bus_width = 32,
	       },
};

struct tegra_audio_platform_data tegra_tdm_audio_pdata[] = {
	/* For TDM1 */
	[0] = {
	       .tdm_enable = true,
	       .total_slots = 4,
	       .tdm_bitsize = 32,
	       .rx_bit_offset = 0,
	       .tx_bit_offset = 0,
	       .fsync_width = 32,
	       .rx_slot_enables = 0xff,
	       .tx_slot_enables = 0xff,
	       .mode = AUDIO_FRAME_FORMAT_TDM,
	       .i2s_bus_width = 32,
	       },
	/* For TDM2 */
	[1] = {
	       .dma_on = true,	/* use dma by default */
	       .tdm_enable = true,
	       .total_slots = 8,
	       .tdm_bitsize = 32,
	       .rx_bit_offset = 0,
	       .tx_bit_offset = 0,
	       .fsync_width = 32,
	       .rx_slot_enables = 0xff,
	       .tx_slot_enables = 0xff,
	       .mode = AUDIO_FRAME_FORMAT_TDM,
	       .i2s_bus_width = 32,
	       },
};

#ifdef CONFIG_EMBEDDED_TEGRA_ALSA_SPDIF
static struct tegra_audio_platform_data tegra_spdif_audio_pdata = {
	.tdm_enable = true,
	.total_slots = 4,
	.dma_on = 0,
	.fifo_fmt = 0,
	.mode = SPDIF_BIT_MODE_MODE16BIT,
};
#endif

static struct tegra_nand_chip_parms nand_chip_parms[] = {
	/* Micron 29F4G08ABADA */
	[0] = {
	       .vendor_id = 0x2C,
	       .device_id = 0xDC,
	       .capacity = 512,
	       .read_id_fourth_byte = 0x95,
	       .timing = {
			  .trp = 1,
			  .trh = 1,
			  .twp = 12,
			  .twh = 12,
			  .tcs = 24,
			  .twhr = 58,
			  .tcr_tar_trr = 12,
			  .twb = 116,
			  .trp_resp = 24,
			  .tadl = 24,
			  },
	       },
	/* Micron 29F4G16ABADA */
	[1] = {
	       .vendor_id = 0x2C,
	       .device_id = 0xCC,
	       .capacity = 512,
	       .read_id_fourth_byte = 0xD5,
	       .timing = {
			  .trp = 10,
			  .trh = 7,
			  .twp = 10,
			  .twh = 7,
			  .tcs = 15,
			  .twhr = 60,
			  .tcr_tar_trr = 20,
			  .twb = 100,
			  .trp_resp = 20,
			  .tadl = 70,
			  },
	       },
	/* Hynix HY27UF084G2B */
	[2] = {
	       .vendor_id = 0xAD,
	       .device_id = 0xDC,
	       .read_id_fourth_byte = 0x95,
	       .capacity = 512,
	       .timing = {
			  .trp = 12,
			  .trh = 1,
			  .twp = 12,
			  .twh = 0,
			  .tcs = 24,
			  .twhr = 58,
			  .tcr_tar_trr = 0,
			  .twb = 116,
			  .trp_resp = 24,
			  .tadl = 24,
			  },
	       },
};

struct tegra_nand_platform p852_nand_data = {
	.max_chips = 8,
	.chip_parms = nand_chip_parms,
	.nr_chip_parms = ARRAY_SIZE(nand_chip_parms),
	.wp_gpio = TEGRA_GPIO_PC7,
};

static struct resource resources_nand[] = {
	[0] = {
	       .start = INT_NANDFLASH,
	       .end = INT_NANDFLASH,
	       .flags = IORESOURCE_IRQ,
	       },
};

static struct platform_device p852_nand_device = {
	.name = "tegra_nand",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nand),
	.resource = resources_nand,
	.dev = {
		.platform_data = &p852_nand_data,
		},
};

unsigned int p852_uart_irqs[] = {
	INT_UARTA,
	INT_UARTB,
	INT_UARTC,
	INT_UARTD,
};

unsigned int p852_uart_bases[] = {
	TEGRA_UARTA_BASE,
	TEGRA_UARTB_BASE,
	TEGRA_UARTC_BASE,
	TEGRA_UARTD_BASE,
};

static struct platform_device *p852_spi_devices[] __initdata = {
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	&tegra_spi_device4,
};

static struct plat_serial8250_port debug_uart_platform_data[] = {
	{
	 .flags = UPF_BOOT_AUTOCONF,
	 .iotype = UPIO_MEM,
	 .regshift = 2,
	 .uartclk = 216000000,
	 },
	{
	 .flags = 0,
	 }
};

#define	DEF_8250_PLATFORM_DATA(_base, _irq) {	\
	.flags = UPF_BOOT_AUTOCONF,		\
	.iotype = UPIO_MEM,			\
	.membase = IO_ADDRESS(_base),		\
	.mapbase = _base,			\
	.irq = _irq,				\
	.regshift = 2,				\
	.uartclk = 216000000,			\
}

static struct plat_serial8250_port tegra_8250_uarta_platform_data[] = {
	DEF_8250_PLATFORM_DATA(TEGRA_UARTA_BASE, INT_UARTA),
	{
	 .flags = 0,
	 }
};

static struct plat_serial8250_port tegra_8250_uartb_platform_data[] = {
	DEF_8250_PLATFORM_DATA(TEGRA_UARTB_BASE, INT_UARTB),
	{
	 .flags = 0,
	 }
};

static struct plat_serial8250_port tegra_8250_uartc_platform_data[] = {
	DEF_8250_PLATFORM_DATA(TEGRA_UARTC_BASE, INT_UARTC),
	{
	 .flags = 0,
	 }
};

static struct plat_serial8250_port tegra_8250_uartd_platform_data[] = {
	DEF_8250_PLATFORM_DATA(TEGRA_UARTD_BASE, INT_UARTD),
	{
	 .flags = 0,
	 }
};

static struct plat_serial8250_port tegra_8250_uarte_platform_data[] = {
	DEF_8250_PLATFORM_DATA(TEGRA_UARTE_BASE, INT_UARTE),
	{
	 .flags = 0,
	 }
};

struct platform_device tegra_8250_uarta_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = tegra_8250_uarta_platform_data,
		},
};

struct platform_device tegra_8250_uartb_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM1,
	.dev = {
		.platform_data = tegra_8250_uartb_platform_data,
		},
};

struct platform_device tegra_8250_uartc_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM2,
	.dev = {
		.platform_data = tegra_8250_uartc_platform_data,
		},
};

struct platform_device tegra_8250_uartd_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_FOURPORT,
	.dev = {
		.platform_data = tegra_8250_uartd_platform_data,
		},
};

struct platform_device tegra_8250_uarte_device = {
	.name = "serial8250",
	.id = PLAT8250_DEV_ACCENT,
	.dev = {
		.platform_data = tegra_8250_uarte_platform_data,
		},
};

static struct platform_device debug_uart = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uart_platform_data,
		},
};

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
	       .xcvr_setup = 8,
	       .xcvr_lsfslew = 2,
	       .xcvr_lsrslew = 2,
	       },
};

static struct tegra_ulpi_config ulpi_usb2_config = {
	.reset_gpio = TEGRA_GPIO_PI5,
};

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
	       .phy_config = &utmi_phy_config[0],
	       .operating_mode = TEGRA_USB_HOST,
	       .power_down_on_bus_suspend = 0,
	       },
	[1] = {
	       .phy_config = &ulpi_usb2_config,
	       .operating_mode = TEGRA_USB_HOST,
	       .power_down_on_bus_suspend = 0,
	       .phy_type = TEGRA_USB_PHY_TYPE_LINK_ULPI,
	       },
	[2] = {
	       .phy_config = &utmi_phy_config[1],
	       .operating_mode = TEGRA_USB_HOST,
	       .power_down_on_bus_suspend = 0,
	       },
};

static void p852_usb_gpio_config(void)
{
	unsigned int usbeth_mux_gpio = 0, usb_ena_val;
	unsigned int has_onboard_ethernet = 0;
	unsigned int p852_eth_reset = TEGRA_GPIO_PD3;

	switch (system_rev) {
	case P852_SKU13_B00:
	case P852_SKU23_B00:
	case P852_SKU23_C01:
	case P852_SKU8_B00:
	case P852_SKU8_C01:
	case P852_SKU9_B00:
	case P852_SKU9_C01:
		{
			usbeth_mux_gpio = TEGRA_GPIO_PS3;
			has_onboard_ethernet = 1;
			usb_ena_val = 1;
		}
		break;
	case P852_SKU5_B00:
	case P852_SKU5_C01:
		{
			usb_ena_val = 1;
			has_onboard_ethernet = 0;
		}
		break;
	case P852_SKU1:
		{
			has_onboard_ethernet = 0;
			usb_ena_val = 0;
			strncpy(enable_usb3, "usb", sizeof(enable_usb3));
		}
		break;
	case P852_SKU1_B00:
	case P852_SKU1_C0X:
		{
			has_onboard_ethernet = 0;
			usb_ena_val = 1;
			strncpy(enable_usb3, "usb", sizeof(enable_usb3));
		}
		break;
	default:
		{
			usbeth_mux_gpio = TEGRA_GPIO_PD4;
			has_onboard_ethernet = 1;
			usb_ena_val = 0;
		}
	}

	if (has_onboard_ethernet) {
		gpio_request_one(usbeth_mux_gpio, GPIOF_OUT_INIT_LOW,
				 "eth_ena");
		tegra_gpio_enable(usbeth_mux_gpio);

		/* eth reset */
		gpio_request_one(p852_eth_reset, GPIOF_OUT_INIT_LOW,
				 "eth_reset");
		tegra_gpio_enable(p852_eth_reset);
		udelay(1);
		gpio_direction_output(p852_eth_reset, 1);

		if (!strcmp(enable_usb3, "eth"))
			gpio_direction_output(usbeth_mux_gpio, 1);

		/* exporting usbeth_mux_gpio */
		gpio_export(usbeth_mux_gpio, true);
	}

	if (!strcmp(enable_usb3, "usb")) {
		gpio_direction_output(TEGRA_GPIO_PB2, usb_ena_val);
		gpio_direction_output(TEGRA_GPIO_PW1, usb_ena_val);
	}
}

static struct platform_device *p852_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};

static struct platform_device *p852_8250_uart_devices[] __initdata = {
	&tegra_8250_uarta_device,
	&tegra_8250_uartb_device,
	&tegra_8250_uartc_device,
	&tegra_8250_uartd_device,
	&tegra_8250_uarte_device,
};

static struct platform_device tegra_itu656 = {
	.name = "tegra_itu656",
	.id = -1,
};

static struct platform_device *p852_devices[] __initdata = {
	&pmu_device,
	&tegra_gart_device,
	&tegra_avp_device,
	&tegra_itu656,
};

#ifdef CONFIG_MTD_NOR_TEGRA
static struct tegra_nor_chip_parms nor_chip_parms[] = {
	[0] = {
	       .timing_default = {
				  .pg_rdy = 120,
				  .pg_seq = 35,
				  .mux = 5,
				  .hold = 25,
				  .adv = 50,
				  .ce = 35,
				  .we = 50,
				  .oe = 50,
				  .wait = 70,
				  },
	       .timing_read = {
			       .pg_rdy = 120,
			       .pg_seq = 25,
			       .mux = 5,
			       .hold = 25,
			       .adv = 50,
			       .ce = 35,
			       .we = 5,
			       .oe = 120,
			       .wait = 5,
			       },
	       },
};

static struct flash_platform_data p852_flash = {
	.map_name = "cfi_probe",
	.width = 2,
};

static struct tegra_nor_platform p852_nor_data = {
	.flash = &p852_flash,
	.chip_parms = nor_chip_parms,
	.nr_chip_parms = ARRAY_SIZE(nor_chip_parms),
};

static struct resource resources_nor[] = {
	[0] = {
	       .start = INT_SNOR,
	       .end = INT_SNOR,
	       .flags = IORESOURCE_IRQ,
	       },
	[1] = {
	       /* Map the size of flash */
	       .start = 0xd0000000,
	       .end = 0xd0000000 + SZ_64M - 1,
	       .flags = IORESOURCE_MEM,
	       }
};

struct platform_device p852_nor_device = {
	.name = "tegra_nor",
	.id = -1,
	.num_resources = ARRAY_SIZE(resources_nor),
	.resource = resources_nor,
	.dev = {
		.platform_data = &p852_nor_data,
		.coherent_dma_mask = 0xffffffff,
		},
};
#endif

#ifdef CONFIG_TEGRA_SPI_I2S
struct spi_board_info tegra_spi_i2s_device __initdata = {
	.modalias = "spi_i2s_pcm",
	.bus_num = 2,
	.chip_select = 2,
	.mode = SPI_MODE_0,
	.max_speed_hz = 18000000,
	.platform_data = NULL,
	.irq = 0,
};

void __init p852_spi_i2s_init(void)
{
	struct tegra_spi_i2s_platform_data *pdata;

	pdata = (struct tegra_spi_i2s_platform_data *)
	    tegra_spi_i2s_device.platform_data;
	if (pdata->gpio_i2s.active_state) {
		gpio_request_one(pdata->gpio_i2s.gpio_no, GPIOF_OUT_INIT_LOW,
				 "i2s_cpld_dir1");
	} else {
		gpio_request_one(pdata->gpio_i2s.gpio_no, GPIOF_OUT_INIT_HIGH,
				 "i2s_cpld_dir1");
	}
	tegra_gpio_enable(pdata->gpio_i2s.gpio_no);
	if (pdata->gpio_spi.active_state) {
		gpio_request_one(pdata->gpio_spi.gpio_no, GPIOF_OUT_INIT_LOW,
				 "spi_cpld_dir2");
	} else {
		gpio_request_one(pdata->gpio_spi.gpio_no, GPIOF_OUT_INIT_HIGH,
				 "spi_cpld_dir2");
	}

	tegra_gpio_enable(pdata->gpio_spi.gpio_no);
	spi_register_board_info(&tegra_spi_i2s_device, 1);
}
#endif

#if defined(CONFIG_SPI_TEGRA) && defined(CONFIG_SPI_SPIDEV)
static struct spi_board_info tegra_spi_devices[] __initdata = {
	{
	 .modalias = "spidev",
	 .bus_num = 0,
	 .chip_select = 0,
	 .mode = SPI_MODE_0,
	 .max_speed_hz = 18000000,
	 .platform_data = NULL,
	 .irq = 0,
	 },
	{
	 .modalias = "spidev",
	 .bus_num = 1,
	 .chip_select = 1,
	 .mode = SPI_MODE_0,
	 .max_speed_hz = 18000000,
	 .platform_data = NULL,
	 .irq = 0,
	 },
	{
	 .modalias = "spidev",
	 .bus_num = 3,
	 .chip_select = 1,
	 .mode = SPI_MODE_0,
	 .max_speed_hz = 18000000,
	 .platform_data = NULL,
	 .irq = 0,
	 },
};

static void __init p852_register_spidev(void)
{
	spi_register_board_info(tegra_spi_devices,
				ARRAY_SIZE(tegra_spi_devices));
}
#else
#define p852_register_spidev() do {} while (0)
#endif

static void __init p852_usb_init(void)
{

	p852_usb_gpio_config();
	/*
	   if (system_rev == P852_SKU8)
	   {
	   platform_device_register(&tegra_udc_device);
	   }
	   else
	 */
	{
		tegra_ehci1_device.dev.platform_data = &tegra_ehci_pdata[0];
		platform_device_register(&tegra_ehci1_device);
	}

	if (!(p852_sku_peripherals & P852_SKU_ULPI_DISABLE)) {
		tegra_ehci2_device.dev.platform_data = &tegra_ehci_pdata[1];
		platform_device_register(&tegra_ehci2_device);
	}

	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];
	platform_device_register(&tegra_ehci3_device);
}

static void __init spi3_pingroup_clear_tristate(void)
{
	/* spi3 mosi, miso, cs, clk */
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_LSDI, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_LSDA, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_LCSN, TEGRA_TRI_NORMAL);
	tegra_pinmux_set_tristate(TEGRA_PINGROUP_LSCK, TEGRA_TRI_NORMAL);
}

static void __init p852_spi_init(void)
{
	if (p852_sku_peripherals & P852_SKU_SPI_ENABLE) {
		int i = 0;
		unsigned int spi_config = 0;
		unsigned int spi3_config =
		    (p852_spi_peripherals >> P852_SPI3_SHIFT) & P852_SPI_MASK;

		for (i = 0; i < P852_MAX_SPI; i++) {
			spi_config =
			    (p852_spi_peripherals >> (P852_SPI_SHIFT * i)) &
			    P852_SPI_MASK;
			if (spi_config & P852_SPI_ENABLE) {
				if (spi_config & P852_SPI_SLAVE)
					p852_spi_devices[i]->name =
					    "tegra_spi_slave";
				platform_device_register(p852_spi_devices[i]);
			}
		}
		/* Default spi3 pingroups are in tristate */
		if (spi3_config & P852_SPI_ENABLE)
			spi3_pingroup_clear_tristate();
	}
}

static void __init p852_uart_init(void)
{
	if (p852_sku_peripherals & P852_SKU_UART_ENABLE) {
		int i = 0;
		unsigned int uart_config = 0, uart8250Id = 0;
		int debug_console = -1;

		/* register the debug console as the first serial console */
		for (i = 0; i < P852_MAX_UART; i++) {
			uart_config =
			    (p852_uart_peripherals >> (P852_UART_SHIFT * i));
			if (uart_config & P852_UART_DB) {
				debug_console = i;
				debug_uart_platform_data[0].membase =
				    IO_ADDRESS(p852_uart_bases[i]);
				debug_uart_platform_data[0].mapbase =
				    p852_uart_bases[i];
				debug_uart_platform_data[0].irq =
				    p852_uart_irqs[i];
				uart8250Id++;
				platform_device_register(&debug_uart);
				break;
			}
		}

		/* register remaining UARTS */
		for (i = 0; i < P852_MAX_UART; i++) {
			uart_config =
			    (p852_uart_peripherals >> (P852_UART_SHIFT * i)) &
			    P852_UART_MASK;
			if ((uart_config & P852_UART_ENABLE)
			    && i != debug_console) {
				if (uart_config & P852_UART_HS) {
					platform_device_register
					    (p852_uart_devices[i]);
				} else {
					p852_8250_uart_devices[i]->id =
					    uart8250Id++;
					platform_device_register
					    (p852_8250_uart_devices[i]);
				}
			}
		}
	}
}

struct tegra_das_platform_data tegra_das_pdata = {
	.dap_clk = "clk_dev1",
	.tegra_dap_port_info_table = {
		/* I2S1 <--> DAC1 <--> DAP1 <--> Hifi Codec */
		[0] = {
			.dac_port = tegra_das_port_i2s1,
			.dap_port = tegra_das_port_dap1,
			.codec_type = tegra_audio_codec_type_hifi,
			.device_property = {
				.num_channels = 8,
				.bits_per_sample = 16,
				.rate = 44100,
				.dac_dap_data_comm_format =
					dac_dap_data_format_rjm,
			},
		},
		[1] = {
			.dac_port = tegra_das_port_i2s2,
			.dap_port = tegra_das_port_dap2,
			.codec_type = tegra_audio_codec_type_hifi,
			.device_property = {
				.num_channels = 16,
				.bits_per_sample = 16,
				.rate = 44100,
				.dac_dap_data_comm_format =
						dac_dap_data_format_rjm,
			},
		},
		[2] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format = 0,
			},
	    },
	    [3] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format =
						dac_dap_data_format_dsp,
			},
		},
	    [4] = {
			.dac_port = tegra_das_port_none,
			.dap_port = tegra_das_port_none,
			.codec_type = tegra_audio_codec_type_none,
			.device_property = {
				.num_channels = 0,
				.bits_per_sample = 0,
				.rate = 0,
				.dac_dap_data_comm_format = 0,
			},
		},
	},
		.tegra_das_con_table = {
	    [0] = {
			.con_id = tegra_das_port_con_id_hifi,
			.num_entries = 4,
			.con_line = {
				[0] = {tegra_das_port_i2s1,
						tegra_das_port_dap1, false},
				[1] = {tegra_das_port_dap1,
						tegra_das_port_i2s1, true},
				[2] = {tegra_das_port_i2s2,
						tegra_das_port_dap2, false},
				[3] = {tegra_das_port_dap2,
						tegra_das_port_i2s2, true},
			},
		},
	}
};

static struct platform_device generic_codec_driver = {
	.name = "generic-dit",
};

static void __init p852_audio_init(void)
{
	struct audio_dev_property *pdev_property;

	/* Using the Generic Codec Driver in Kernel */
	platform_device_register(&generic_codec_driver);

	if (p852_sku_peripherals & P852_SKU_I2S_ENABLE) {
		unsigned int i2s_config1 =
		    (p852_i2s_peripherals >> P852_I2S1_SHIFT) & P852_I2S_MASK;
		unsigned int i2s_config2 =
		    (p852_i2s_peripherals >> P852_I2S2_SHIFT) & P852_I2S_MASK;
		if (i2s_config1 & P852_I2S_ENABLE) {
			tegra_i2s_device1.dev.platform_data =
			    &tegra_i2s_audio_pdata[0];

			if (i2s_config1 & P852_I2S_TDM) {
				tegra_i2s_device1.dev.platform_data =
						&tegra_tdm_audio_pdata[0];
				pdev_property =
					&(tegra_das_pdata.tegra_dap_port_info_table[0].device_property);
				pdev_property->dac_dap_data_comm_format =
						dac_dap_data_format_tdm;
				pdev_property =
					&(tegra_das_pdata.tegra_dap_port_info_table[1].device_property);
				pdev_property->dac_dap_data_comm_format =
						dac_dap_data_format_tdm;
			}
			platform_device_register(&tegra_i2s_device1);
		}
		if (i2s_config2 & P852_I2S_ENABLE) {
			tegra_i2s_device2.dev.platform_data =
			    &tegra_i2s_audio_pdata[1];

			if (i2s_config2 & P852_I2S_TDM) {
				tegra_i2s_device2.dev.platform_data =
				    &tegra_tdm_audio_pdata[1];
			}
			platform_device_register(&tegra_i2s_device2);
		}
	}
	tegra_das_device.dev.platform_data = &tegra_das_pdata;
	platform_device_register(&tegra_das_device);
#ifdef CONFIG_EMBEDDED_TEGRA_ALSA_SPDIF
	tegra_spdif_input_device.name = "spdif";
	tegra_spdif_input_device.dev.platform_data = &tegra_spdif_audio_pdata;
	platform_device_register(&tegra_spdif_input_device);
#endif
}

static void __init p852_flash_init(void)
{
	if (p852_sku_peripherals & P852_SKU_NAND_ENABLE)
		platform_device_register(&p852_nand_device);
#ifdef CONFIG_MTD_NOR_TEGRA
	if (p852_sku_peripherals & P852_SKU_NOR_ENABLE)
		platform_device_register(&p852_nor_device);
#endif
}

void __init p852_common_init(void)
{
	tegra_common_init();

	tegra_clk_init_from_table(p852_clk_init_table);

	p852_pinmux_init();

	p852_i2c_init();

	p852_regulator_init();

	p852_uart_init();

	p852_flash_init();

	p852_audio_init();

	platform_add_devices(p852_devices, ARRAY_SIZE(p852_devices));

	p852_panel_init();

	p852_spi_init();

	p852_register_spidev();

	p852_usb_init();

	p852_sdhci_init();

	p852_gpio_init();

	p852_power_off_init();
}

void __init tegra_p852_init(void)
{
	switch (system_rev) {
	case P852_SKU3:
		p852_sku3_init();
		break;
	case P852_SKU13:
		p852_sku13_init();
		break;
	case P852_SKU13_B00:
	case P852_SKU13_C01:
		p852_sku13_b00_init();
		break;
	case P852_SKU23:
		p852_sku23_init();
		break;
	case P852_SKU23_B00:
		p852_sku23_b00_init();
		break;
	case P852_SKU23_C01:
		p852_sku23_c01_init();
		break;
	case P852_SKU1:
		p852_sku1_init();
		break;
	case P852_SKU11:
	case P852_SKU1_B00:
		p852_sku1_b00_init();
		break;
	case P852_SKU1_C0X:
		p852_sku1_c0x_init();
		break;
	case P852_SKU5_B00:
		p852_sku5_b00_init();
		break;
	case P852_SKU5_C01:
		p852_sku5_c01_init();
		break;
	case P852_SKU8_B00:
		p852_sku8_b00_init();
		break;
	case P852_SKU8_C01:
		p852_sku8_c00_init();
		break;
	case P852_SKU9_B00:
		p852_sku9_b00_init();
		break;
	case P852_SKU9_C01:
		p852_sku9_c00_init();
		break;
	default:
		printk(KERN_ERR "Unknow Board Revision\n");
		break;
	}
}

static void __init tegra_p852_reserve(void)
{
	switch (system_rev) {
	case P852_SKU3:
	case P852_SKU5_B00:
	case P852_SKU5_C01:
	case P852_SKU9_B00:
	case P852_SKU9_C01:
		tegra_reserve(SZ_64M + SZ_16M, SZ_8M, 0);
		break;
	default:
		tegra_reserve(SZ_128M, SZ_8M, 0);
		break;
	}
}

MACHINE_START(P852, "Tegra P852")
	.boot_params = 0x00000100,
	.phys_io = IO_APB_PHYS,
	.io_pg_offst = ((IO_APB_VIRT) >> 18) & 0xfffc,
	.init_irq = tegra_init_irq,
	.init_machine = tegra_p852_init,
	.map_io = tegra_map_common_io,
	.reserve = tegra_p852_reserve,
	.timer = &tegra_timer,
MACHINE_END
