/*
 * arch/arm/mach-tegra/board-enterprise-panel.c
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
#include "board-enterprise.h"
#include "devices.h"
#include "gpio-names.h"

/* Select panel to be used. */
#define AVDD_LCD PMU_TCA6416_GPIO_PORT17
#define DSI_PANEL_RESET 0

#define enterprise_lvds_shutdown	TEGRA_GPIO_PL2
#define enterprise_bl_enb		TEGRA_GPIO_PH2
#define enterprise_bl_pwm		TEGRA_GPIO_PH0
#define enterprise_hdmi_hpd		TEGRA_GPIO_PN7

#define enterprise_dsia_bl_enb	TEGRA_GPIO_PW1
#define enterprise_dsi_panel_reset	TEGRA_GPIO_PW0

static struct regulator *enterprise_hdmi_reg = NULL;
static struct regulator *enterprise_hdmi_pll = NULL;
static struct regulator *enterprise_hdmi_vddio = NULL;

static atomic_t sd_brightness = ATOMIC_INIT(255);

static int enterprise_backlight_init(struct device *dev)
{
	int ret;

	/* Enable back light for DSIa panel */
	printk("enterprise_dsi_backlight_init\n");

	ret = gpio_request(enterprise_dsia_bl_enb, "dsia_bl_enable");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(enterprise_dsia_bl_enb, 1);
	if (ret < 0)
		gpio_free(enterprise_dsia_bl_enb);
	else
		tegra_gpio_enable(enterprise_dsia_bl_enb);

	return ret;
}

static void enterprise_backlight_exit(struct device *dev)
{
	/* Disable back light for DSIa panel */
	gpio_set_value(enterprise_dsia_bl_enb, 0);
	gpio_free(enterprise_dsia_bl_enb);
	tegra_gpio_disable(enterprise_dsia_bl_enb);

	gpio_set_value(TEGRA_GPIO_PL2, 1);
	mdelay(20);
}

static int enterprise_backlight_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);
	int orig_brightness = brightness;

	/* DSIa */
	gpio_set_value(enterprise_dsia_bl_enb, !!brightness);

	/* SD brightness is a percentage, 8-bit value. */
	brightness = (brightness * cur_sd_brightness) / 255;
	if (cur_sd_brightness != 255) {
		printk("NVSD BL - in: %d, sd: %d, out: %d\n",
			orig_brightness, cur_sd_brightness, brightness);
	}

	return brightness;
}

static struct platform_pwm_backlight_data enterprise_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= enterprise_backlight_init,
	.exit		= enterprise_backlight_exit,
	.notify		= enterprise_backlight_notify,
};

static struct platform_device enterprise_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_backlight_data,
	},
};

static int enterprise_hdmi_enable(void)
{
	int ret;
	if (!enterprise_hdmi_reg) {
		enterprise_hdmi_reg = regulator_get(NULL, "avdd_hdmi");
		if (IS_ERR_OR_NULL(enterprise_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_reg);
		}
	}
	ret = regulator_enable(enterprise_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!enterprise_hdmi_pll) {
		enterprise_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(enterprise_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			enterprise_hdmi_pll = NULL;
			regulator_put(enterprise_hdmi_reg);
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_pll);
		}
	}
	ret = regulator_enable(enterprise_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	if (!enterprise_hdmi_vddio) {
		enterprise_hdmi_vddio = regulator_get(NULL, "vdd_hdmi_con");
		if (IS_ERR_OR_NULL(enterprise_hdmi_vddio)) {
			pr_err("hdmi: couldn't get regulator vdd_hdmi_con\n");
			enterprise_hdmi_vddio = NULL;
			regulator_put(enterprise_hdmi_pll);
			enterprise_hdmi_pll = NULL;
			regulator_put(enterprise_hdmi_reg);
			enterprise_hdmi_reg = NULL;

			return PTR_ERR(enterprise_hdmi_vddio);
		}
	}
	ret = regulator_enable(enterprise_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator vdd_hdmi_con\n");
		return ret;
	}
	return 0;
}

static int enterprise_hdmi_disable(void)
{

	regulator_disable(enterprise_hdmi_reg);
	regulator_put(enterprise_hdmi_reg);
	enterprise_hdmi_reg = NULL;

	regulator_disable(enterprise_hdmi_pll);
	regulator_put(enterprise_hdmi_pll);
	enterprise_hdmi_pll = NULL;

	regulator_disable(enterprise_hdmi_vddio);
	regulator_put(enterprise_hdmi_vddio);
	enterprise_hdmi_vddio = NULL;
	return 0;
}
static struct resource enterprise_disp1_resources[] = {
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
		.start	= 0,	/* Filled in by enterprise_panel_init() */
		.end	= 0,	/* Filled in by enterprise_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource enterprise_disp2_resources[] = {
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

static struct tegra_dc_sd_settings enterprise_sd_settings = {
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
	.bl_device = &enterprise_backlight_device,
};

static struct tegra_fb_data enterprise_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out enterprise_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 3,
	.hotplug_gpio	= enterprise_hdmi_hpd,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= enterprise_hdmi_enable,
	.disable	= enterprise_hdmi_disable,
};

static struct tegra_dc_platform_data enterprise_disp2_pdata = {
	.flags		= 0,
	.default_out	= &enterprise_disp2_out,
	.fb		= &enterprise_hdmi_fb_data,
	.emc_clk_rate	= 300000000,
};

static int enterprise_dsi_panel_enable(void)
{
	static struct regulator *reg = NULL;

	if (reg == NULL) {
		reg = regulator_get(NULL, "avdd_dsi_csi");
		if (IS_ERR_OR_NULL(reg)) {
		pr_err("dsi: Could not get regulator avdd_dsi_csi\n");
			reg = NULL;
			return PTR_ERR(reg);
		}
	}
	regulator_enable(reg);

	return 0;
}

static int enterprise_dsi_panel_disable(void)
{
	return 0;
}

static struct tegra_dsi_cmd dsi_init_cmd[]= {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(150),
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

struct tegra_dsi_out enterprise_dsi = {
	.n_data_lanes = 2,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
	.refresh_rate = 60,
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_has_frame_buffer = true,
	.dsi_instance = 0,
	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_init_cmd = dsi_init_cmd,

	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
};

static struct tegra_dc_mode enterprise_dsi_modes[] = {
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
};


static struct tegra_fb_data enterprise_dsi_fb_data = {
	.win		= 0,
	.xres		= 540,
	.yres		= 960,
	.bits_per_pixel	= 32,
};


static struct tegra_dc_out enterprise_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.sd_settings	= &enterprise_sd_settings,

	.type		= TEGRA_DC_OUT_DSI,

	.modes	 	= enterprise_dsi_modes,
	.n_modes 	= ARRAY_SIZE(enterprise_dsi_modes),

	.dsi		= &enterprise_dsi,

	.enable		= enterprise_dsi_panel_enable,
	.disable	= enterprise_dsi_panel_disable,
};
static struct tegra_dc_platform_data enterprise_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &enterprise_disp1_out,
	.emc_clk_rate	= 300000000,
	.fb		= &enterprise_dsi_fb_data,
};
static struct nvhost_device enterprise_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= enterprise_disp1_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp1_resources),
	.dev = {
		.platform_data = &enterprise_disp1_pdata,
	},
};

static struct nvhost_device enterprise_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= enterprise_disp2_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp2_resources),
	.dev = {
		.platform_data = &enterprise_disp2_pdata,
	},
};

static struct nvmap_platform_carveout enterprise_carveouts[] = {
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
		.base		= 0,	/* Filled in by enterprise_panel_init() */
		.size		= 0,	/* Filled in by enterprise_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data enterprise_nvmap_data = {
	.carveouts	= enterprise_carveouts,
	.nr_carveouts	= ARRAY_SIZE(enterprise_carveouts),
};

static struct platform_device enterprise_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_nvmap_data,
	},
};

static struct platform_device *enterprise_gfx_devices[] __initdata = {
	&enterprise_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm0_device,
	&enterprise_backlight_device,
};


#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend enterprise_panel_early_suspender;

static void enterprise_panel_early_suspend(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
}

static void enterprise_panel_late_resume(struct early_suspend *h)
{
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_UNBLANK);
}
#endif

int __init enterprise_panel_init(void)
{
	int err;
	struct resource *res;

	enterprise_carveouts[1].base = tegra_carveout_start;
	enterprise_carveouts[1].size = tegra_carveout_size;

	tegra_gpio_enable(enterprise_hdmi_hpd);
	gpio_request(enterprise_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(enterprise_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	enterprise_panel_early_suspender.suspend = enterprise_panel_early_suspend;
	enterprise_panel_early_suspender.resume = enterprise_panel_late_resume;
	enterprise_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&enterprise_panel_early_suspender);
#endif

	err = platform_add_devices(enterprise_gfx_devices,
				ARRAY_SIZE(enterprise_gfx_devices));

	res = nvhost_get_resource_byname(&enterprise_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

	if (!err)
		err = nvhost_device_register(&enterprise_disp1_device);

	res = nvhost_get_resource_byname(&enterprise_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
	if (!err)
		err = nvhost_device_register(&enterprise_disp2_device);
	return err;
}
