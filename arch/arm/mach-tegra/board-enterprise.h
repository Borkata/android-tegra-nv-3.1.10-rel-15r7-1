/*
 * arch/arm/mach-tegra/board-enterprise.h
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

#ifndef _MACH_TEGRA_BOARD_ENTERPRISE_H
#define _MACH_TEGRA_BOARD_ENTERPRISE_H

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/mfd/tps80031.h>

int enterprise_charge_init(void);
int enterprise_sdhci_init(void);
int enterprise_pinmux_init(void);
int enterprise_panel_init(void);
int enterprise_sensors_init(void);
int touch_init(void);
int enterprise_kbc_init(void);
int enterprise_emc_init(void);
int enterprise_regulator_init(void);
int enterprise_modem_init(void);
int enterprise_suspend_init(void);
int enterprise_edp_init(void);

/* Touchscreen GPIO addresses   */
#define TOUCH_GPIO_IRQ_ATMEL_T9	TEGRA_GPIO_PH6
#define TOUCH_GPIO_RST_ATMEL_T9	TEGRA_GPIO_PF5
#define TOUCH_BUS_ATMEL_T9	1

/*****************External GPIO tables ******************/
/* External peripheral gpio base. */
#define ENT_TPS80031_GPIO_BASE	   TEGRA_NR_GPIOS
#define ENT_TPS80031_GPIO_REGEN1 (ENT_TPS80031_GPIO_BASE + TPS80031_GPIO_REGEN1)
#define ENT_TPS80031_GPIO_REGEN2 (ENT_TPS80031_GPIO_BASE + TPS80031_GPIO_REGEN2)
#define ENT_TPS80031_GPIO_SYSEN	 (ENT_TPS80031_GPIO_BASE + TPS80031_GPIO_SYSEN)
#define ENT_TPS80031_GPIO_END	(ENT_TPS80031_GPIO_BASE + TPS80031_GPIO_NR)

/*****************External Interrupt tables ******************/
/* External peripheral irq base */
#define ENT_TPS80031_IRQ_BASE	TEGRA_NR_IRQS
#define ENT_TPS80031_IRQ_END  (ENT_TPS80031_IRQ_BASE + TPS80031_INT_NR)

/*****************Camera GPIOs ******************/
#define CAM_CSI_MUX_SEL_GPIO	TEGRA_GPIO_PM3
#define CAM_CSI_MUX_SEL_REAR	1
#define CAM_CSI_MUX_SEL_FRONT	0

#define CAM1_RST_L_GPIO		TEGRA_GPIO_PM5 /*REAR RIGHT*/
#define CAM1_PWDN_GPIO		TEGRA_GPIO_PF3 /*REAR RIGHT*/
#define CAM2_RST_L_GPIO		TEGRA_GPIO_PF4 /*REAR LEFT*/
#define CAM2_PWDN_GPIO		TEGRA_GPIO_PF2 /*REAR LEFT*/
#define CAM3_RST_L_GPIO		TEGRA_GPIO_PM2 /*FRONT*/
#define CAM3_RST_L_TRUE		0
#define CAM3_RST_L_FALSE	1
#define CAM3_PWDN_GPIO		TEGRA_GPIO_PN4 /*FRONT*/
#define CAM3_PWDN_TRUE		1
#define CAM3_PWDN_FALSE		0
#define CAM_FLASH_EN_GPIO	TEGRA_GPIO_PBB3
#define CAM_FLASH_MAX_TORCH_AMP	7
#define CAM_FLASH_MAX_FLASH_AMP	7

/* Baseband GPIO addresses */

#define BB_GPIO_MDM_PWRON_AP2BB		TEGRA_GPIO_PE0 /* LCD_D0 */
#define BB_GPIO_RESET_AP2BB		TEGRA_GPIO_PE1 /* LCD_D1 */
#define BB_GPIO_LCD_PWR1		TEGRA_GPIO_PC1
#define BB_GPIO_LCD_PWR2		TEGRA_GPIO_PC6
#define BB_GPIO_HS1_AP2BB		TEGRA_GPIO_PE3 /* LCD_D3 */
#define BB_GPIO_HS1_BB2AP		TEGRA_GPIO_PU5

#define XMM_GPIO_BB_ON			BB_GPIO_MDM_PWRON_AP2BB
#define XMM_GPIO_BB_RST			BB_GPIO_RESET_AP2BB
#define XMM_GPIO_IPC_HSIC_ACTIVE	BB_GPIO_LCD_PWR1
#define XMM_GPIO_IPC_HSIC_SUS_REQ	BB_GPIO_LCD_PWR2
#define XMM_GPIO_IPC_BB_WAKE		BB_GPIO_HS1_AP2BB
#define XMM_GPIO_IPC_AP_WAKE		BB_GPIO_HS1_BB2AP

/* Board IDs */

#define BOARD_1205		(0x0C05)
#define BOARD_E1197		(0x0B61)
#define ENTERPRISE_FAB_A01	(0x01)
#define SKU_BATTERY_SUPPORT	0x1

#endif
