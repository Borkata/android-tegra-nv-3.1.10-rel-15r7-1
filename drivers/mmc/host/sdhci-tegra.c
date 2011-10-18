/*
 * drivers/mmc/host/sdhci-tegra.c
 *
 * Copyright (C) 2009 Palm, Inc.
 * Author: Yvonne Yip <y@palm.com>
 *
 * Copyright (C) 2010-2011 NVIDIA Corporation
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

#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/mmc/card.h>
#include <linux/regulator/consumer.h>
#include <linux/mmc/host.h>

#include <mach/sdhci.h>

#include "sdhci.h"

#define DRIVER_NAME    "sdhci-tegra"

#define SDHCI_TEGRA_MIN_CONTROLLER_CLOCK	12000000
#define SDHCI_TEGRA_STANDARD_CONTROLLER_CLOCK	104000000
#define SDHCI_TEGRA_MAX_CONTROLLER_CLOCK	208000000
#define SDHCI_VENDOR_CLOCK_CNTRL       0x100

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
#define SDHCI_VENDOR_CLOCK_CNTRL_SDR50_TUNING_OVERRIDE	0x20
#define SDHCI_VENDOR_CLOCK_CNTRL_SDMMC_CLK_ENABLE	0x1
#define SDHCI_VENDOR_CLOCK_CNTRL_INPUT_IO_CLOCK_INTERNAL	0x2
#define SDHCI_VENDOR_CLOCK_CNTRL_SPI_MODE_CLKEN_OVERRIDE	0x4
#define SDHCI_VENDOR_CLOCK_CNTRL_PADPIPE_CLKEN_OVERRIDE	0x8
#define SDHCI_VENDOR_CLOCK_CNTRL_TAP_VAL_SHIFT	0x10
#define SDHCI_VENDOR_CLOCK_CNTRL_BASE_CLK_FREQ_SHIFT	0x8

#define SDMMC_VENDOR_MISC_CNTRL	0x120
#define SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_SW_RESET_CLKEN_OVERRIDE	0x2
#define SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_ENABLE_SDR104	0x8
#define SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_ENABLE_SDR50	0x10
#define SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_ENABLE_SD3_0_SUPPORT	0x20

#define SDMMC_AUTO_CAL_CONFIG	0x1E4
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE	0x20000000
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT	0x8
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET	0x70
#define SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PU_OFFSET	0x62
#endif

struct tegra_sdhci_host {
	struct sdhci_host *sdhci;
	struct clk *clk;
	int clk_enabled;
	bool card_always_on;
	u32 sdhci_ints;
	int cd_gpio;
	int cd_gpio_polarity;
	int wp_gpio;
	int wp_gpio_polarity;
	int is_rail_enabled;
	unsigned int tap_delay;
	unsigned int max_clk;
	unsigned int clk_limit;
	unsigned int card_present;
	struct regulator *reg_vdd_slot;
	struct regulator *reg_vddio;
	unsigned int acquire_spinlock;
};

static irqreturn_t carddetect_irq(int irq, void *data)
{
	struct sdhci_host *sdhost = (struct sdhci_host *)data;
	struct tegra_sdhci_host *host = sdhci_priv(sdhost);

	host->card_present =
		(gpio_get_value(host->cd_gpio) == host->cd_gpio_polarity);

	if(host->card_present){
		if(host->is_rail_enabled == 0){
			if (host->reg_vdd_slot)
				regulator_enable(host->reg_vdd_slot);
			if (host->reg_vddio)
				regulator_enable(host->reg_vddio);
			host->is_rail_enabled = 1;
		}
	}

	tasklet_schedule(&sdhost->card_tasklet);
	return IRQ_HANDLED;
};

static int tegra_sdhci_card_detect(struct sdhci_host *sdhci)
{
	struct tegra_sdhci_host *host = sdhci_priv(sdhci);

	return host->card_present;
}

static void tegra_sdhci_status_notify_cb(int card_present, void *dev_id)
{
	struct sdhci_host *sdhci = (struct sdhci_host *)dev_id;
	pr_debug("%s: card_present %d\n",
		mmc_hostname(sdhci->mmc), card_present);
	sdhci_card_detect_callback(sdhci);
}

static int tegra_sdhci_enable_dma(struct sdhci_host *host)
{
	return 0;
}

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
static void tegra_sdhci_configure_tap_value(struct sdhci_host *sdhci, unsigned int tap_delay)
{
	u32 ctrl;

	BUG_ON(tap_delay > 0xFF);

	ctrl = sdhci_readl(sdhci, SDHCI_VENDOR_CLOCK_CNTRL);
	ctrl &= ~(0xFF << SDHCI_VENDOR_CLOCK_CNTRL_TAP_VAL_SHIFT);
	ctrl |= (tap_delay << SDHCI_VENDOR_CLOCK_CNTRL_TAP_VAL_SHIFT);
	sdhci_writel(sdhci, ctrl, SDHCI_VENDOR_CLOCK_CNTRL);
}
#endif

static void tegra_sdhci_configure_capabilities(struct sdhci_host *sdhci)
{
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	u32 ctrl;
	struct tegra_sdhci_host *host = sdhci_priv(sdhci);

	/*
	 * Configure clock override bits and SDR50 tuning requirement in
	 * the vendor clock control register.
	 */
	ctrl = sdhci_readl(sdhci, SDHCI_VENDOR_CLOCK_CNTRL);
	ctrl |= SDHCI_VENDOR_CLOCK_CNTRL_PADPIPE_CLKEN_OVERRIDE;
	ctrl &= ~(0xFF << SDHCI_VENDOR_CLOCK_CNTRL_BASE_CLK_FREQ_SHIFT);
	ctrl |= ((host->max_clk/1000000) << SDHCI_VENDOR_CLOCK_CNTRL_BASE_CLK_FREQ_SHIFT);
	sdhci_writel(sdhci, ctrl, SDHCI_VENDOR_CLOCK_CNTRL);

	/* Enable support for SD 3.0 */
	ctrl = sdhci_readl(sdhci, SDMMC_VENDOR_MISC_CNTRL);
	ctrl |= SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_ENABLE_SDR104;
	ctrl |= SDMMC_VENDOR_MISC_CNTRL_SDMMC_SPARE0_ENABLE_SD3_0_SUPPORT;
	sdhci_writel(sdhci, ctrl, SDMMC_VENDOR_MISC_CNTRL);

	tegra_sdhci_configure_tap_value(sdhci, host->tap_delay);
#endif
}

static void tegra_sdhci_enable_clock(struct tegra_sdhci_host *host, int clock)
{
	u8 val;

	if (spin_is_locked(&host->sdhci->lock)) {
		spin_unlock_irqrestore(&host->sdhci->lock, host->sdhci->spinlock_flags);
		host->acquire_spinlock = 1;
	}

	if (clock) {
		if (!host->clk_enabled) {
			clk_enable(host->clk);
			val = sdhci_readb(host->sdhci, SDHCI_VENDOR_CLOCK_CNTRL);
			val |= 1;
			sdhci_writeb(host->sdhci, val, SDHCI_VENDOR_CLOCK_CNTRL);
			host->clk_enabled = 1;
		}
		if (host->clk_limit && (clock > host->clk_limit))
			clock = host->clk_limit;
		if (clock < SDHCI_TEGRA_MIN_CONTROLLER_CLOCK)
			clk_set_rate(host->clk, SDHCI_TEGRA_MIN_CONTROLLER_CLOCK);
		else
			clk_set_rate(host->clk, clock);
	} else if (host->clk_enabled) {
		val = sdhci_readb(host->sdhci, SDHCI_VENDOR_CLOCK_CNTRL);
		val &= ~(0x1);
		sdhci_writeb(host->sdhci, val, SDHCI_VENDOR_CLOCK_CNTRL);
		/*
		 * Read back the register to ensure all writes on AHB are flushed prior
		 * to switching OFF the clock.
		 */
		val = sdhci_readb(host->sdhci, SDHCI_VENDOR_CLOCK_CNTRL);
		clk_disable(host->clk);
		host->clk_enabled = 0;
	}

	if (host->acquire_spinlock) {
		spin_lock_irqsave(&host->sdhci->lock, host->sdhci->spinlock_flags);
		host->acquire_spinlock = 0;
	}

	if (host->clk_enabled)
		host->sdhci->max_clk = clk_get_rate(host->clk);
	else
		host->sdhci->max_clk = 0;
}

static void tegra_sdhci_set_clock(struct sdhci_host *sdhci, unsigned int clock)
{
	struct tegra_sdhci_host *host = sdhci_priv(sdhci);

	if (clock > SDHCI_TEGRA_MIN_CONTROLLER_CLOCK &&
			clock <= SDHCI_TEGRA_STANDARD_CONTROLLER_CLOCK)
		clock = SDHCI_TEGRA_STANDARD_CONTROLLER_CLOCK;
	else if (clock > SDHCI_TEGRA_STANDARD_CONTROLLER_CLOCK)
			clock = SDHCI_TEGRA_MAX_CONTROLLER_CLOCK;
	pr_debug("tegra sdhci clock %s %u enabled=%d\n",
		mmc_hostname(sdhci->mmc), clock, host->clk_enabled);

	tegra_sdhci_enable_clock(host, clock);
}

static void tegra_sdhci_set_signalling_voltage(struct sdhci_host *sdhci,
	unsigned int signalling_voltage)
{
	struct tegra_sdhci_host *host = sdhci_priv(sdhci);
	unsigned int minV = 3280000;
	unsigned int maxV = 3320000;
	unsigned int rc;

	if (signalling_voltage == MMC_1_8_VOLT_SIGNALLING) {
		minV = 1800000;
		maxV = 1800000;
	}

	rc = regulator_set_voltage(host->reg_vddio, minV, maxV);
	if (rc)
		printk(KERN_ERR "%s switching to %dV failed %d\n",
			mmc_hostname(sdhci->mmc), (maxV/1000000), rc);
	else {
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		if (signalling_voltage == MMC_1_8_VOLT_SIGNALLING) {
			unsigned int val;
			/* Do Auto Calibration */
			val = sdhci_readl(sdhci, SDMMC_AUTO_CAL_CONFIG);
			val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_ENABLE;
			/* Program Auto cal PD offset(bits 8:14) */
			val &= ~(0x7F << SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
			val |= (SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET <<
				SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PD_OFFSET_SHIFT);
			/* Program Auto cal PU offset(bits 0:6) */
			val &= ~0x7F;
			val |= SDMMC_AUTO_CAL_CONFIG_AUTO_CAL_PU_OFFSET;
			sdhci_writel(sdhci, val, SDMMC_AUTO_CAL_CONFIG);
		}
#endif
	}
}

static int tegra_sdhci_get_ro(struct sdhci_host *sdhci)
{
	struct tegra_sdhci_host *host = sdhci_priv(sdhci);

	if (gpio_is_valid(host->wp_gpio))
		return (gpio_get_value(host->wp_gpio) == host->wp_gpio_polarity);
	else
		return 0;
}

static struct sdhci_ops tegra_sdhci_ops = {
	.enable_dma = tegra_sdhci_enable_dma,
	.set_clock = tegra_sdhci_set_clock,
	.configure_capabilities = tegra_sdhci_configure_capabilities,
	.get_cd = tegra_sdhci_card_detect,
#ifdef CONFIG_MMC_TEGRA_TAP_DELAY
	.configure_tap_value = tegra_sdhci_configure_tap_value,
#endif
	.get_ro = tegra_sdhci_get_ro,
};

static int __devinit tegra_sdhci_probe(struct platform_device *pdev)
{
	int rc;
	struct tegra_sdhci_platform_data *plat;
	struct sdhci_host *sdhci;
	struct tegra_sdhci_host *host;
	struct resource *res;
	int irq;
	void __iomem *ioaddr;

	plat = pdev->dev.platform_data;
	if (plat == NULL)
		return -ENXIO;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL)
		return -ENODEV;

	irq = res->start;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -ENODEV;

	ioaddr = ioremap(res->start, res->end - res->start);

	sdhci = sdhci_alloc_host(&pdev->dev, sizeof(struct tegra_sdhci_host));
	if (IS_ERR(sdhci)) {
		rc = PTR_ERR(sdhci);
		goto err_unmap;
	}

	host = sdhci_priv(sdhci);
	host->sdhci = sdhci;
	host->card_always_on = (plat->power_gpio == -1) ? 1 : 0;
	host->max_clk = plat->max_clk;
	host->clk_limit = plat->clk_limit;
	host->tap_delay = plat->tap_delay;
	host->cd_gpio = plat->cd_gpio;
	host->cd_gpio_polarity = plat->cd_gpio_polarity;
	host->wp_gpio = plat->wp_gpio;
	host->wp_gpio_polarity = plat->wp_gpio_polarity;
#ifdef CONFIG_MMC_TEGRA_TAP_DELAY
	host->sdhci->tap_value = plat->tap_delay;
#endif
	host->acquire_spinlock = 0;
	host->clk = clk_get(&pdev->dev, plat->clk_id);
	if (IS_ERR(host->clk)) {
		rc = PTR_ERR(host->clk);
		goto err_free_host;
	}

	rc = clk_enable(host->clk);
	if (rc != 0)
		goto err_clkput;

	if (host->cd_gpio != -1) {
		/* Enabling the slot power rails */
		dev_info(&pdev->dev, "get slot power rail regulator\n");
		if (host->reg_vdd_slot == NULL) {
			host->reg_vdd_slot = regulator_get(&pdev->dev, "vddio_sd_slot");
			if (IS_ERR_OR_NULL(host->reg_vdd_slot)) {
				dev_warn(&pdev->dev, "vddio_sd_slot regulator_get()"
					"failed: %ld\n", PTR_ERR(host->reg_vdd_slot));
				host->reg_vdd_slot = NULL;
			} else {
				regulator_enable(host->reg_vdd_slot);
			}
		}

		/* Enable vdd power rail */
		dev_info(&pdev->dev, "Getting regulator for rail %s\n",
			plat->vdd_rail_name);
		if (host->reg_vddio == NULL) {
			host->reg_vddio = regulator_get(&pdev->dev, "vddio_sdmmc");
			if (IS_ERR_OR_NULL(host->reg_vddio)) {
				dev_warn(&pdev->dev, "vddio_sdmmc regulator_get()"
					"failed: %ld\n", PTR_ERR(host->reg_vddio));
				host->reg_vddio = NULL;
			} else {
				rc = regulator_set_voltage(host->reg_vddio,
						plat->vdd_min_uv, plat->vdd_max_uv);
				if (rc != 0) {
					dev_err(&pdev->dev, "regulator_set_voltage()"
						"failed for vddio_sdmmc %d\n", rc);
					regulator_put(host->reg_vddio);
					host->reg_vddio = NULL;
				} else {
					regulator_enable(host->reg_vddio);
					host->is_rail_enabled = 1;
				}
			}
		}
	}

	if (plat->is_voltage_switch_supported && host->reg_vddio)
		tegra_sdhci_ops.set_signalling_voltage =
			tegra_sdhci_set_signalling_voltage;

	host->clk_enabled = 1;
	sdhci->hw_name = "tegra";
	sdhci->ops = &tegra_sdhci_ops;
	sdhci->irq = irq;
	sdhci->ioaddr = ioaddr;
	sdhci->version = SDHCI_SPEC_200;
	sdhci->quirks = SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
			SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK |
			SDHCI_QUIRK_BROKEN_VOLTAGE_SWITCHING |
#else
			SDHCI_QUIRK_ENABLE_INTERRUPT_AT_BLOCK_GAP |
			SDHCI_QUIRK_NO_VERSION_REG |
#endif
			SDHCI_QUIRK_SINGLE_POWER_WRITE |
			SDHCI_QUIRK_BROKEN_WRITE_PROTECT |
			SDHCI_QUIRK_BROKEN_CTRL_HISPD |
			SDHCI_QUIRK_NO_HISPD_BIT |
			SDHCI_QUIRK_BROKEN_ADMA_ZEROLEN_DESC |
			SDHCI_QUIRK_RUNTIME_DISABLE |
			SDHCI_QUIRK_BROKEN_CARD_DETECTION;

	if (plat->is_8bit_supported)
		sdhci->quirks |= SDHCI_QUIRK_8_BIT_DATA;

	if (plat->force_hs != 0)
		sdhci->quirks |= SDHCI_QUIRK_FORCE_HIGH_SPEED_MODE;
#ifdef CONFIG_MMC_EMBEDDED_SDIO
	mmc_set_embedded_sdio_data(sdhci->mmc,
			&plat->cis,
			&plat->cccr,
			plat->funcs,
			plat->num_funcs);
#endif
	if (host->card_always_on)
		sdhci->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

	platform_set_drvdata(pdev, host);

	/*
	 * If the card detect gpio is not present, treat the card as
	 * non-removable.
	 */
	if (plat->cd_gpio == -1)
		host->card_present = 1;

	if (plat->cd_gpio != -1) {
		rc = request_threaded_irq(gpio_to_irq(plat->cd_gpio), NULL, carddetect_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			mmc_hostname(sdhci->mmc), sdhci);
		if (rc)
			goto err_remove_host;

		host->card_present =
			(gpio_get_value(plat->cd_gpio) == host->cd_gpio_polarity);
		if(!host->card_present){
			if (host->reg_vddio)
				regulator_disable(host->reg_vddio);
			if (host->reg_vdd_slot)
				regulator_disable(host->reg_vdd_slot);
			host->is_rail_enabled = 0;
		}
	} else if (plat->register_status_notify) {
		plat->register_status_notify(
			tegra_sdhci_status_notify_cb, sdhci);
	}

	rc = sdhci_add_host(sdhci);
	if (rc)
		goto err_clk_disable;

	if (plat->board_probe)
		plat->board_probe(pdev->id, sdhci->mmc);

	printk(KERN_INFO "sdhci%d: initialized irq %d ioaddr %p\n", pdev->id,
			sdhci->irq, sdhci->ioaddr);

	return 0;

err_remove_host:
	sdhci_remove_host(sdhci, 1);
err_clk_disable:
	clk_disable(host->clk);
	host->clk_enabled = 0;
err_clkput:
	clk_put(host->clk);
err_free_host:
	if (sdhci)
		sdhci_free_host(sdhci);
err_unmap:
	iounmap(sdhci->ioaddr);

	return rc;
}

static int tegra_sdhci_remove(struct platform_device *pdev)
{
	struct tegra_sdhci_host *host = platform_get_drvdata(pdev);
	unsigned int rc = 0;
	if (host) {
		struct tegra_sdhci_platform_data *plat;
		plat = pdev->dev.platform_data;
		if (plat && plat->board_probe)
			plat->board_probe(pdev->id, host->sdhci->mmc);

		sdhci_remove_host(host->sdhci, 0);

		if (host->reg_vddio) {
			rc = regulator_disable(host->reg_vddio);
			if (!rc)
				regulator_put(host->reg_vddio);
		}

		if (host->reg_vdd_slot) {
			rc = regulator_disable(host->reg_vdd_slot);
			if (!rc)
				regulator_put(host->reg_vdd_slot);
		}

		sdhci_free_host(host->sdhci);
	}
	return 0;
}


#define is_card_sdio(_card) \
((_card) && ((_card)->type == MMC_TYPE_SDIO))

#ifdef CONFIG_PM

static void tegra_sdhci_restore_interrupts(struct sdhci_host *sdhost)
{
	u32 ierr;
	u32 clear = SDHCI_INT_ALL_MASK;
	struct tegra_sdhci_host *host = sdhci_priv(sdhost);

	/* enable required interrupts */
	ierr = sdhci_readl(sdhost, SDHCI_INT_ENABLE);
	ierr &= ~clear;
	ierr |= host->sdhci_ints;
	sdhci_writel(sdhost, ierr, SDHCI_INT_ENABLE);
	sdhci_writel(sdhost, ierr, SDHCI_SIGNAL_ENABLE);

	if ((host->sdhci_ints & SDHCI_INT_CARD_INT) &&
		(sdhost->quirks & SDHCI_QUIRK_ENABLE_INTERRUPT_AT_BLOCK_GAP)) {
		u8 gap_ctrl = sdhci_readb(sdhost, SDHCI_BLOCK_GAP_CONTROL);
		gap_ctrl |= 0x8;
		sdhci_writeb(sdhost, gap_ctrl, SDHCI_BLOCK_GAP_CONTROL);
	}
}

static int tegra_sdhci_restore(struct sdhci_host *sdhost)
{
	unsigned long timeout;
	u8 mask = SDHCI_RESET_ALL;
	u8 pwr;

	sdhci_writeb(sdhost, mask, SDHCI_SOFTWARE_RESET);

	sdhost->clock = 0;

	/* Wait max 100 ms */
	timeout = 100;

	/* hw clears the bit when it's done */
	while (sdhci_readb(sdhost, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			printk(KERN_ERR "%s: Reset 0x%x never completed.\n",
				mmc_hostname(sdhost->mmc), (int)mask);
			return -EIO;
		}
		timeout--;
		mdelay(1);
	}

	tegra_sdhci_restore_interrupts(sdhost);

	pwr = SDHCI_POWER_ON;
	sdhci_writeb(sdhost, pwr, SDHCI_POWER_CONTROL);
	sdhost->pwr = 0;

	return 0;
}

static int tegra_sdhci_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct tegra_sdhci_host *host = platform_get_drvdata(pdev);
	int ret = 0;

	if (host->card_always_on && is_card_sdio(host->sdhci->mmc->card)) {
		int div = 0;
		u16 clk;
		unsigned int clock = 100000;

		if (device_may_wakeup(&pdev->dev))
			enable_irq_wake(host->sdhci->irq);

		/* save interrupt status before suspending */
		host->sdhci_ints = sdhci_readl(host->sdhci, SDHCI_INT_ENABLE);

		/* reduce host controller clk and card clk to 100 KHz */
		tegra_sdhci_set_clock(host->sdhci, clock);
		sdhci_writew(host->sdhci, 0, SDHCI_CLOCK_CONTROL);

		if (host->sdhci->max_clk > clock) {
			div =  1 << (fls(host->sdhci->max_clk / clock) - 2);
			if (div > 128)
				div = 128;
		}

		clk = div << SDHCI_DIVIDER_SHIFT;
		clk |= SDHCI_CLOCK_INT_EN | SDHCI_CLOCK_CARD_EN;
		sdhci_writew(host->sdhci, clk, SDHCI_CLOCK_CONTROL);

		return ret;
	}

	ret = sdhci_suspend_host(host->sdhci, state);
	if (ret)
		pr_err("%s: failed, error = %d\n", __func__, ret);

	tegra_sdhci_enable_clock(host, 0);
	if(host->is_rail_enabled == 1){
		if (host->reg_vddio)
			ret = regulator_disable(host->reg_vddio);
		if (host->reg_vdd_slot)
			ret = regulator_disable(host->reg_vdd_slot);
		host->is_rail_enabled = 0;
	}

	return ret;
}

static int tegra_sdhci_resume(struct platform_device *pdev)
{
	struct tegra_sdhci_host *host = platform_get_drvdata(pdev);
	int ret = 0;

	if (host->card_always_on && is_card_sdio(host->sdhci->mmc->card)) {
		int ret = 0;

		if (device_may_wakeup(&pdev->dev))
			disable_irq_wake(host->sdhci->irq);

		/* soft reset SD host controller and enable interrupts */
		ret = tegra_sdhci_restore(host->sdhci);
		if (ret) {
			pr_err("%s: failed, error = %d\n", __func__, ret);
			return ret;
		}

		mmiowb();
		host->sdhci->mmc->ops->set_ios(host->sdhci->mmc,
			&host->sdhci->mmc->ios);
		return 0;
	}

	if (host->cd_gpio != -1) {
		int prev_card_present_stat = 0;

		prev_card_present_stat = host->card_present;

		host->card_present =
			(gpio_get_value(host->cd_gpio) == host->cd_gpio_polarity);

		if (prev_card_present_stat != host->card_present)
			sdhci_card_detect_callback(host->sdhci);
	}
	if(host->card_present){
		if(host->is_rail_enabled == 0){
			if (host->reg_vdd_slot)
				ret = regulator_enable(host->reg_vdd_slot);
			if (host->reg_vddio)
				ret = regulator_enable(host->reg_vddio);
			host->is_rail_enabled = 1;
		}
	}

        tegra_sdhci_enable_clock(host, SDHCI_TEGRA_MIN_CONTROLLER_CLOCK);

        ret = sdhci_resume_host(host->sdhci);
        if (ret)
                pr_err("%s: failed, error = %d\n", __func__, ret);


	return ret;
}
#else
#define tegra_sdhci_suspend    NULL
#define tegra_sdhci_resume     NULL
#endif

static struct platform_driver tegra_sdhci_driver = {
	.probe = tegra_sdhci_probe,
	.remove = tegra_sdhci_remove,
	.suspend = tegra_sdhci_suspend,
	.resume = tegra_sdhci_resume,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init tegra_sdhci_init(void)
{
	return platform_driver_register(&tegra_sdhci_driver);
}

static void __exit tegra_sdhci_exit(void)
{
	platform_driver_unregister(&tegra_sdhci_driver);
}

module_init(tegra_sdhci_init);
module_exit(tegra_sdhci_exit);

MODULE_DESCRIPTION("Tegra SDHCI controller driver");
MODULE_LICENSE("GPL");
