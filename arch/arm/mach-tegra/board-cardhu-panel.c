/*
 * arch/arm/mach-tegra/board-cardhu-panel.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/pwm_backlight.h>
#include <asm/atomic.h>
#include <mach/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "board.h"
#include "board-cardhu.h"
#include "devices.h"
#include "gpio-names.h"

/* Select panel to be used. */
#define DSI_PANEL_219 0
#define DSI_PANEL_218 1
#define AVDD_LCD PMU_TCA6416_GPIO_PORT17
#define DSI_PANEL_RESET 1

#define cardhu_lvds_shutdown	TEGRA_GPIO_PL2
#define cardhu_bl_enb		TEGRA_GPIO_PH2
#define cardhu_bl_pwm		TEGRA_GPIO_PH0
#define cardhu_hdmi_hpd		TEGRA_GPIO_PN7

#ifdef DSI_PANEL_219
#define cardhu_dsia_bl_enb	TEGRA_GPIO_PW1
#define cardhu_dsib_bl_enb	TEGRA_GPIO_PW0
#define cardhu_dsi_panel_reset	TEGRA_GPIO_PD2
#endif

static struct regulator *cardhu_hdmi_reg = NULL;
static struct regulator *cardhu_hdmi_pll = NULL;
static struct regulator *cardhu_hdmi_vddio = NULL;

static atomic_t sd_brightness = ATOMIC_INIT(255);

static struct regulator *cardhu_lvds_reg = NULL;
static struct regulator *cardhu_lvds_vdd_bl = NULL;
static struct regulator *cardhu_lvds_vdd_panel = NULL;

static struct board_info board_info;

static int cardhu_backlight_init(struct device *dev) {
	int ret;

#ifndef CONFIG_TEGRA_CARDHU_DSI
	if (board_info.board_id != BOARD_PM269) {
		tegra_gpio_disable(cardhu_bl_pwm);

		ret = gpio_request(cardhu_bl_enb, "backlight_enb");
		if (ret < 0)
			return ret;

		ret = gpio_direction_output(cardhu_bl_enb, 1);
		if (ret < 0)
			gpio_free(cardhu_bl_enb);
		else
			tegra_gpio_enable(cardhu_bl_enb);

		return ret;
	}
#endif

#if DSI_PANEL_219 || DSI_PANEL_218
	/* Enable back light for DSIa panel */
	printk("cardhu_dsi_backlight_init\n");
	ret = gpio_request(cardhu_dsia_bl_enb, "dsia_bl_enable");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(cardhu_dsia_bl_enb, 1);
	if (ret < 0)
		gpio_free(cardhu_dsia_bl_enb);
	else
		tegra_gpio_enable(cardhu_dsia_bl_enb);

	/* Enable back light for DSIb panel */
	ret = gpio_request(cardhu_dsib_bl_enb, "dsib_bl_enable");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(cardhu_dsib_bl_enb, 1);
	if (ret < 0)
		gpio_free(cardhu_dsib_bl_enb);
	else
		tegra_gpio_enable(cardhu_dsib_bl_enb);
#endif

	return ret;
};

static void cardhu_backlight_exit(struct device *dev) {
#ifndef CONFIG_TEGRA_CARDHU_DSI
	if (board_info.board_id != BOARD_PM269) {
		/* int ret; */
		/*ret = gpio_request(cardhu_bl_enb, "backlight_enb");*/
		gpio_set_value(cardhu_bl_enb, 0);
		gpio_free(cardhu_bl_enb);
		tegra_gpio_disable(cardhu_bl_enb);
		return;
	}
#endif
#if DSI_PANEL_219 || DSI_PANEL_218
	/* Disable back light for DSIa panel */
	gpio_set_value(cardhu_dsia_bl_enb, 0);
	gpio_free(cardhu_dsia_bl_enb);
	tegra_gpio_disable(cardhu_dsia_bl_enb);

	/* Disable back light for DSIb panel */
	gpio_set_value(cardhu_dsib_bl_enb, 0);
	gpio_free(cardhu_dsib_bl_enb);
	tegra_gpio_disable(cardhu_dsib_bl_enb);

	gpio_set_value(TEGRA_GPIO_PL2, 1);
	mdelay(20);
#endif
}

static int cardhu_backlight_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);
	int orig_brightness = brightness;

#ifndef CONFIG_TEGRA_CARDHU_DSI
	if (board_info.board_id != BOARD_PM269) {
		/* Set the backlight GPIO pin mode to 'backlight_enable' */
		gpio_request(cardhu_bl_enb, "backlight_enb");
		gpio_set_value(cardhu_bl_enb, !!brightness);
		goto final;
	}
#endif
#if DSI_PANEL_219 || DSI_PANEL_218
	/* DSIa */
	gpio_set_value(cardhu_dsia_bl_enb, !!brightness);

	/* DSIb */
	gpio_set_value(cardhu_dsib_bl_enb, !!brightness);
#endif

final:
	/* SD brightness is a percentage, 8-bit value. */
	brightness = (brightness * cur_sd_brightness) / 255;
	if (cur_sd_brightness != 255) {
		printk("NVSD BL - in: %d, sd: %d, out: %d\n",
			orig_brightness, cur_sd_brightness, brightness);
	}

	return brightness;
}

static struct platform_pwm_backlight_data cardhu_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= cardhu_backlight_init,
	.exit		= cardhu_backlight_exit,
	.notify		= cardhu_backlight_notify,
};

static struct platform_device cardhu_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &cardhu_backlight_data,
	},
};

static int cardhu_panel_enable(void)
{
	if (cardhu_lvds_reg == NULL) {
		cardhu_lvds_reg = regulator_get(NULL, "vdd_lvds");
		if (WARN_ON(IS_ERR(cardhu_lvds_reg)))
			pr_err("%s: couldn't get regulator vdd_lvds: %ld\n",
			       __func__, PTR_ERR(cardhu_lvds_reg));
		else
			regulator_enable(cardhu_lvds_reg);
	}

	if (cardhu_lvds_vdd_bl == NULL) {
		cardhu_lvds_vdd_bl = regulator_get(NULL, "vdd_backlight");
		if (WARN_ON(IS_ERR(cardhu_lvds_vdd_bl)))
			pr_err("%s: couldn't get regulator vdd_backlight: %ld\n",
			       __func__, PTR_ERR(cardhu_lvds_vdd_bl));
		else
			regulator_enable(cardhu_lvds_vdd_bl);
	}

	if (cardhu_lvds_vdd_panel == NULL) {
		cardhu_lvds_vdd_panel = regulator_get(NULL, "vdd_lcd_panel");
		if (WARN_ON(IS_ERR(cardhu_lvds_vdd_panel)))
			pr_err("%s: couldn't get regulator vdd_lcd_panel: %ld\n",
			       __func__, PTR_ERR(cardhu_lvds_vdd_panel));
		else
			regulator_enable(cardhu_lvds_vdd_panel);
	}
	gpio_set_value(cardhu_lvds_shutdown, 1);
	return 0;
}

static int cardhu_panel_disable(void)
{
	regulator_disable(cardhu_lvds_reg);
	regulator_put(cardhu_lvds_reg);
	cardhu_lvds_reg = NULL;

	regulator_disable(cardhu_lvds_vdd_bl);
	regulator_put(cardhu_lvds_vdd_bl);
	cardhu_lvds_vdd_bl = NULL;

	regulator_disable(cardhu_lvds_vdd_panel);
	regulator_put(cardhu_lvds_vdd_panel);
	cardhu_lvds_vdd_panel= NULL;

	gpio_set_value(cardhu_lvds_shutdown, 0);
	return 0;
}

static int cardhu_hdmi_enable(void)
{
	int ret;
	if (!cardhu_hdmi_reg) {
		cardhu_hdmi_reg = regulator_get(NULL, "avdd_hdmi");
		if (IS_ERR_OR_NULL(cardhu_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			cardhu_hdmi_reg = NULL;
			return PTR_ERR(cardhu_hdmi_reg);
		}
	}
	ret = regulator_enable(cardhu_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!cardhu_hdmi_pll) {
		cardhu_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(cardhu_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			cardhu_hdmi_pll = NULL;
			regulator_put(cardhu_hdmi_reg);
			cardhu_hdmi_reg = NULL;
			return PTR_ERR(cardhu_hdmi_pll);
		}
	}
	ret = regulator_enable(cardhu_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	if (!cardhu_hdmi_vddio) {
		cardhu_hdmi_vddio = regulator_get(NULL, "vdd_hdmi_con");
		if (IS_ERR_OR_NULL(cardhu_hdmi_vddio)) {
			pr_err("hdmi: couldn't get regulator vdd_hdmi_con\n");
			cardhu_hdmi_vddio = NULL;
			regulator_put(cardhu_hdmi_pll);
			cardhu_hdmi_pll = NULL;
			regulator_put(cardhu_hdmi_reg);
			cardhu_hdmi_reg = NULL;

			return PTR_ERR(cardhu_hdmi_vddio);
		}
	}
	ret = regulator_enable(cardhu_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator vdd_hdmi_con\n");
		return ret;
	}
	return 0;
}

static int cardhu_hdmi_disable(void)
{

	regulator_disable(cardhu_hdmi_reg);
	regulator_put(cardhu_hdmi_reg);
	cardhu_hdmi_reg = NULL;

	regulator_disable(cardhu_hdmi_pll);
	regulator_put(cardhu_hdmi_pll);
	cardhu_hdmi_pll = NULL;

	regulator_disable(cardhu_hdmi_vddio);
	regulator_put(cardhu_hdmi_vddio);
	cardhu_hdmi_vddio = NULL;
	return 0;
}
static struct resource cardhu_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,	/* Filled in by cardhu_panel_init() */
		.end	= 0,	/* Filled in by cardhu_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
#ifdef CONFIG_TEGRA_DSI_INSTANCE_1
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSIB_BASE,
		.end	= TEGRA_DSIB_BASE + TEGRA_DSIB_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#else
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
#endif
};

static struct resource cardhu_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode cardhu_panel_modes[] = {
	{
		/* 1366x768@62.3Hz */
		.pclk = 72000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 32,
		.v_sync_width = 5,
		.h_back_porch = 20,
		.v_back_porch = 12,
		.h_active = 1366,
		.v_active = 768,
		.h_front_porch = 48,
		.v_front_porch = 3,
	},
};

static struct tegra_dc_sd_settings cardhu_sd_settings = {
	.enable = 1, /* Normal mode operation */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = 0,
	.aggressiveness = 5,
	.use_vid_luma = true,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Default BL TF */
	.bltf = {
			{128, 136, 144, 152},
			{160, 168, 176, 184},
			{192, 200, 208, 216},
			{224, 232, 240, 248}
		},
	/* Default LUT */
	.lut = {
			{255, 255, 255},
			{199, 199, 199},
			{153, 153, 153},
			{116, 116, 116},
			{85, 85, 85},
			{59, 59, 59},
			{36, 36, 36},
			{17, 17, 17},
			{0, 0, 0}
		},
	.sd_brightness = &sd_brightness,
	.bl_device = &cardhu_backlight_device,
};

static struct tegra_fb_data cardhu_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
};

static struct tegra_fb_data cardhu_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out cardhu_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 3,
	.hotplug_gpio	= cardhu_hdmi_hpd,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= cardhu_hdmi_enable,
	.disable	= cardhu_hdmi_disable,
};

static struct tegra_dc_platform_data cardhu_disp2_pdata = {
	.flags		= 0,
	.default_out	= &cardhu_disp2_out,
	.fb		= &cardhu_hdmi_fb_data,
	.emc_clk_rate	= 300000000,
};

static int cardhu_dsi_panel_enable(void)
{
	static struct regulator *reg = NULL;
	int ret;

	if (reg == NULL) {
		reg = regulator_get(NULL, "avdd_dsi_csi");
		if (IS_ERR_OR_NULL(reg)) {
		pr_err("dsi: Could not get regulator avdd_dsi_csi\n");
			reg = NULL;
			return PTR_ERR(reg);
		}
	}
	regulator_enable(reg);

#if DSI_PANEL_219
	ret = gpio_request(TEGRA_GPIO_PL2, "pl2");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PL2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PL2);
		return ret;
	}
	else
		tegra_gpio_enable(TEGRA_GPIO_PL2);

	ret = gpio_request(TEGRA_GPIO_PH0, "ph0");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PH0, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PH0);
		return ret;
	}
	else
		tegra_gpio_enable(TEGRA_GPIO_PH0);

	ret = gpio_request(TEGRA_GPIO_PH2, "ph2");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PH2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PH2);
		return ret;
	}
	else
		tegra_gpio_enable(TEGRA_GPIO_PH2);

	ret = gpio_request(TEGRA_GPIO_PU2, "pu2");
	if (ret < 0)
		return ret;
	ret = gpio_direction_output(TEGRA_GPIO_PU2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PU2);
		return ret;
	}
	else
		tegra_gpio_enable(TEGRA_GPIO_PU2);

	gpio_set_value(TEGRA_GPIO_PL2, 1);
	mdelay(20);
	gpio_set_value(TEGRA_GPIO_PH0, 1);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PH2, 1);
	mdelay(15);
	gpio_set_value(TEGRA_GPIO_PU2, 0);
	gpio_set_value(TEGRA_GPIO_PU2, 1);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PU2, 0);
	mdelay(10);
	gpio_set_value(TEGRA_GPIO_PU2, 1);
	mdelay(15);
#endif

#if DSI_PANEL_218
	printk("DSI_PANEL_218 is enabled\n");
	ret = gpio_request(AVDD_LCD, 1);
	if(ret < 0)
		gpio_free(AVDD_LCD);
	ret = gpio_direction_output(AVDD_LCD, 1);
	if(ret < 0)
		gpio_free(AVDD_LCD);
	else
		tegra_gpio_enable(AVDD_LCD);

#if DSI_PANEL_RESET
	ret = gpio_request(TEGRA_GPIO_PD2, "pd2");
	if (ret < 0){
		return ret;
	}
	ret = gpio_direction_output(TEGRA_GPIO_PD2, 0);
	if (ret < 0) {
		gpio_free(TEGRA_GPIO_PD2);
		return ret;
	}
	else
		tegra_gpio_enable(TEGRA_GPIO_PD2);

	gpio_set_value(TEGRA_GPIO_PD2, 1);
	gpio_set_value(TEGRA_GPIO_PD2, 0);
	mdelay(2);
	gpio_set_value(TEGRA_GPIO_PD2, 1);
	mdelay(2);
#endif
#endif

	return 0;
}

static int cardhu_dsi_panel_disable(void)
{
	return 0;
}

static struct tegra_dsi_cmd dsi_init_cmd[]= {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(150),
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

struct tegra_dsi_out cardhu_dsi = {
	.n_data_lanes = 2,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_has_frame_buffer = true,
#ifdef CONFIG_TEGRA_DSI_INSTANCE_1
	.dsi_instance = 1,
#else
	.dsi_instance = 0,
#endif
	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_init_cmd = dsi_init_cmd,

	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
};

static struct tegra_dc_mode cardhu_dsi_modes[] = {
#if DSI_PANEL_219
	{
		.pclk = 10000000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 1,
		.h_back_porch = 32,
		.v_back_porch = 1,
		.h_active = 540,
		.v_active = 960,
		.h_front_porch = 32,
		.v_front_porch = 2,
	},
#endif

#if DSI_PANEL_218
	{
		.pclk = 323000000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 4,
		.h_back_porch = 16,
		.v_back_porch = 4,
		.h_active = 864,
		.v_active = 480,
		.h_front_porch = 16,
		.v_front_porch = 4,
	},
#endif

};


static struct tegra_fb_data cardhu_dsi_fb_data = {
#if DSI_PANEL_219
	.win		= 0,
	.xres		= 540,
	.yres		= 960,
	.bits_per_pixel	= 32,
#endif

#if DSI_PANEL_218
	.win		= 0,
	.xres		= 864,
	.yres		= 480,
	.bits_per_pixel	= 32,
#endif
};


static struct tegra_dc_out cardhu_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.sd_settings	= &cardhu_sd_settings,

#ifndef CONFIG_TEGRA_CARDHU_DSI
	.type		= TEGRA_DC_OUT_RGB,

	.modes	 	= cardhu_panel_modes,
	.n_modes 	= ARRAY_SIZE(cardhu_panel_modes),

	.enable		= cardhu_panel_enable,
	.disable	= cardhu_panel_disable,
#else
	.type		= TEGRA_DC_OUT_DSI,

	.modes	 	= cardhu_dsi_modes,
	.n_modes 	= ARRAY_SIZE(cardhu_dsi_modes),

	.dsi		= &cardhu_dsi,

	.enable		= cardhu_dsi_panel_enable,
	.disable	= cardhu_dsi_panel_disable,
#endif
};
static struct tegra_dc_platform_data cardhu_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &cardhu_disp1_out,
	.emc_clk_rate	= 300000000,
#ifndef CONFIG_TEGRA_CARDHU_DSI
	.fb		= &cardhu_fb_data,
#else
	.fb		= &cardhu_dsi_fb_data,
#endif
};
static struct nvhost_device cardhu_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= cardhu_disp1_resources,
	.num_resources	= ARRAY_SIZE(cardhu_disp1_resources),
	.dev = {
		.platform_data = &cardhu_disp1_pdata,
	},
};

static struct nvhost_device cardhu_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= cardhu_disp2_resources,
	.num_resources	= ARRAY_SIZE(cardhu_disp2_resources),
	.dev = {
		.platform_data = &cardhu_disp2_pdata,
	},
};

static struct nvmap_platform_carveout cardhu_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE + TEGRA_RESET_HANDLER_SIZE,
		.size		= TEGRA_IRAM_SIZE - TEGRA_RESET_HANDLER_SIZE,
		.buddy_size	= 0, /* no buddy allocation for IRAM */
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by cardhu_panel_init() */
		.size		= 0,	/* Filled in by cardhu_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data cardhu_nvmap_data = {
	.carveouts	= cardhu_carveouts,
	.nr_carveouts	= ARRAY_SIZE(cardhu_carveouts),
};

static struct platform_device cardhu_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &cardhu_nvmap_data,
	},
};

static struct platform_device *cardhu_gfx_devices[] __initdata = {
	&cardhu_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm0_device,
	&cardhu_backlight_device,
};


#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend cardhu_panel_early_suspender;

static void cardhu_panel_early_suspend(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
}

static void cardhu_panel_late_resume(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_UNBLANK);
}
#endif

int __init cardhu_panel_init(void)
{
	int err;
	struct resource *res;

	tegra_get_board_info(&board_info);

	if (board_info.board_id == BOARD_PM269) {
		cardhu_disp1_out.type = TEGRA_DC_OUT_DSI;
		cardhu_disp1_out.modes = cardhu_dsi_modes;
		cardhu_disp1_out.n_modes = ARRAY_SIZE(cardhu_dsi_modes);
		cardhu_disp1_out.dsi = &cardhu_dsi;
		cardhu_disp1_out.enable = cardhu_dsi_panel_enable;
		cardhu_disp1_out.disable = cardhu_dsi_panel_disable;
		cardhu_disp1_pdata.fb = &cardhu_dsi_fb_data;
	}

	cardhu_carveouts[1].base = tegra_carveout_start;
	cardhu_carveouts[1].size = tegra_carveout_size;

	tegra_gpio_enable(cardhu_hdmi_hpd);
	gpio_request(cardhu_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(cardhu_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	cardhu_panel_early_suspender.suspend = cardhu_panel_early_suspend;
	cardhu_panel_early_suspender.resume = cardhu_panel_late_resume;
	cardhu_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&cardhu_panel_early_suspender);
#endif

	err = platform_add_devices(cardhu_gfx_devices,
				ARRAY_SIZE(cardhu_gfx_devices));

	res = nvhost_get_resource_byname(&cardhu_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	if (!err)
		err = nvhost_device_register(&cardhu_disp1_device);

	res = nvhost_get_resource_byname(&cardhu_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
	if (!err)
		err = nvhost_device_register(&cardhu_disp2_device);
	return err;
}
