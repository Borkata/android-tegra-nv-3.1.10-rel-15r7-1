/*
 * arch/arm/mach-tegra/board-harmony-panel.c
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
#include <linux/nvhost.h>
#include <mach/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "devices.h"
#include "gpio-names.h"
#include "board.h"

#define harmony_bl_enb		TEGRA_GPIO_PB5
#define harmony_lvds_shutdown	TEGRA_GPIO_PB2
#define harmony_en_vdd_pnl	TEGRA_GPIO_PC6
#define harmony_bl_vdd		TEGRA_GPIO_PW0
#define harmony_bl_pwm		TEGRA_GPIO_PB4
#define harmony_hdmi_hpd	TEGRA_GPIO_PN7

/*panel power on sequence timing*/
#define harmony_pnl_to_lvds_ms	0
#define harmony_lvds_to_bl_ms	200

static int harmony_backlight_init(struct device *dev)
{
	int ret;

	ret = gpio_request(harmony_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(harmony_bl_enb, 1);
	if (ret < 0)
		gpio_free(harmony_bl_enb);
	else
		tegra_gpio_enable(harmony_bl_enb);

	return ret;
};

static void harmony_backlight_exit(struct device *dev)
{
	gpio_set_value(harmony_bl_enb, 0);
	gpio_free(harmony_bl_enb);
	tegra_gpio_disable(harmony_bl_enb);
}

static int harmony_backlight_notify(struct device *unused, int brightness)
{
	gpio_set_value(harmony_en_vdd_pnl, !!brightness);
	gpio_set_value(harmony_lvds_shutdown, !!brightness);
	gpio_set_value(harmony_bl_enb, !!brightness);
	return brightness;
}

static int harmony_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data harmony_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 5000000,
	.init		= harmony_backlight_init,
	.exit		= harmony_backlight_exit,
	.notify		= harmony_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= harmony_disp1_check_fb,
};

static struct platform_device harmony_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &harmony_backlight_data,
	},
};

static int harmony_panel_enable(void)
{
	gpio_set_value(harmony_en_vdd_pnl, 1);
	mdelay(harmony_pnl_to_lvds_ms);
	gpio_set_value(harmony_lvds_shutdown, 1);
	mdelay(harmony_lvds_to_bl_ms);
	return 0;
}

static int harmony_panel_disable(void)
{
	gpio_set_value(harmony_lvds_shutdown, 0);
	gpio_set_value(harmony_en_vdd_pnl, 0);
	return 0;
}

static int harmony_set_hdmi_power(bool enable)
{
	static struct {
		struct regulator *regulator;
		const char *name;
	} regs[] = {
		{ .name = "avdd_hdmi" },
		{ .name = "avdd_hdmi_pll" },
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		if (!regs[i].regulator) {
			regs[i].regulator = regulator_get(NULL, regs[i].name);

			if (IS_ERR(regs[i].regulator)) {
				int ret = PTR_ERR(regs[i].regulator);
				regs[i].regulator = NULL;
				return ret;
			}
		}

		if (enable)
			regulator_enable(regs[i].regulator);
		else
			regulator_disable(regs[i].regulator);
	}

	return 0;
}

static int harmony_hdmi_enable(void)
{
	return harmony_set_hdmi_power(true);
}

static int harmony_hdmi_disable(void)
{
	return harmony_set_hdmi_power(false);
}

static struct resource harmony_disp1_resources[] = {
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
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource harmony_disp2_resources[] = {
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
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_mode harmony_panel_modes[] = {
	{
		.pclk = 42430000,
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 2,
		.h_sync_width = 136,
		.v_sync_width = 4,
		.h_back_porch = 138,
		.v_back_porch = 21,
		.h_active = 1024,
		.v_active = 600,
		.h_front_porch = 34,
		.v_front_porch = 4,
	},
};

static struct tegra_fb_data harmony_fb_data = {
	.win		= 0,
	.xres		= 1024,
	.yres		= 600,
	.bits_per_pixel	= 32,
};

static struct tegra_fb_data harmony_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 720,
	.bits_per_pixel	= 16,
};

static struct tegra_dc_out harmony_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.modes		= harmony_panel_modes,
	.n_modes	= ARRAY_SIZE(harmony_panel_modes),

	.enable		= harmony_panel_enable,
	.disable	= harmony_panel_disable,
};

static struct tegra_dc_out harmony_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= harmony_hdmi_hpd,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= harmony_hdmi_enable,
	.disable	= harmony_hdmi_disable,
};

static struct tegra_dc_platform_data harmony_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &harmony_disp1_out,
	.fb		= &harmony_fb_data,
};

static struct tegra_dc_platform_data harmony_disp2_pdata = {
	.flags		= 0,
	.default_out	= &harmony_disp2_out,
	.fb		= &harmony_hdmi_fb_data,
};

static struct nvhost_device harmony_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= harmony_disp1_resources,
	.num_resources	= ARRAY_SIZE(harmony_disp1_resources),
	.dev = {
		.platform_data = &harmony_disp1_pdata,
	},
};

static int harmony_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &harmony_disp1_device.dev;
}

static struct nvhost_device harmony_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= harmony_disp2_resources,
	.num_resources	= ARRAY_SIZE(harmony_disp2_resources),
	.dev = {
		.platform_data = &harmony_disp2_pdata,
	},
};

static struct nvmap_platform_carveout harmony_carveouts[] = {
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
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data harmony_nvmap_data = {
	.carveouts	= harmony_carveouts,
	.nr_carveouts	= ARRAY_SIZE(harmony_carveouts),
};

static struct platform_device harmony_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &harmony_nvmap_data,
	},
};

static struct platform_device *harmony_gfx_devices[] __initdata = {
	&harmony_nvmap_device,
	&tegra_grhost_device,
	&tegra_pwfm0_device,
	&harmony_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/*
 * Put early_suspend/late_resume handlers here for the display in order to
 * keep the code out of the display driver, keeping it closer to upstream.
 */
struct early_suspend harmony_panel_early_suspender;

static void harmony_panel_early_suspend(struct early_suspend *h)
{
	unsigned i;
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_POWERDOWN);
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_save_default_governor();
	cpufreq_set_conservative_governor();
	cpufreq_set_conservative_governor_param(
		SET_CONSERVATIVE_GOVERNOR_UP_THRESHOLD,
		SET_CONSERVATIVE_GOVERNOR_DOWN_THRESHOLD);
#endif
}

static void harmony_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
#ifdef CONFIG_TEGRA_CONVSERVATIVE_GOV_ON_EARLYSUPSEND
	cpufreq_restore_default_governor();
#endif
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif

int __init harmony_panel_init(void)
{
	int err;
	struct resource *res;

	gpio_request(harmony_en_vdd_pnl, "en_vdd_pnl");
	gpio_direction_output(harmony_en_vdd_pnl, 1);
	tegra_gpio_enable(harmony_en_vdd_pnl);

	gpio_request(harmony_bl_vdd, "bl_vdd");
	gpio_direction_output(harmony_bl_vdd, 1);
	tegra_gpio_enable(harmony_bl_vdd);

	gpio_request(harmony_lvds_shutdown, "lvds_shdn");
	gpio_direction_output(harmony_lvds_shutdown, 1);
	tegra_gpio_enable(harmony_lvds_shutdown);

	gpio_request(harmony_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(harmony_hdmi_hpd);
	tegra_gpio_enable(harmony_hdmi_hpd);

#ifdef CONFIG_HAS_EARLYSUSPEND
	harmony_panel_early_suspender.suspend = harmony_panel_early_suspend;
	harmony_panel_early_suspender.resume = harmony_panel_late_resume;
	harmony_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&harmony_panel_early_suspender);
#endif

	harmony_carveouts[1].base = tegra_carveout_start;
	harmony_carveouts[1].size = tegra_carveout_size;

	err = platform_add_devices(harmony_gfx_devices,
				   ARRAY_SIZE(harmony_gfx_devices));
	if (err)
		goto fail;

	res = nvhost_get_resource_byname(&harmony_disp1_device,
		IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb_start;
		res->end = tegra_fb_start + tegra_fb_size - 1;
	}

	res = nvhost_get_resource_byname(&harmony_disp2_device,
		IORESOURCE_MEM, "fbmem");
	if (res) {
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;
	}

	/* Copy the bootloader fb to the fb. */
	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

	if (!err)
		err = nvhost_device_register(&harmony_disp1_device);

	if (!err)
		err = nvhost_device_register(&harmony_disp2_device);

fail:
	return err;
}

