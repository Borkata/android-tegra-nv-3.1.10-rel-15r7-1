/*
 *  arch/arm/mach-tegra/localtimer.c
 *
 *  Copyright (C) 2002 ARM Ltd.
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/clockchips.h>
#include <asm/irq.h>
#include <asm/smp_twd.h>
#include <asm/localtimer.h>

#include "power.h"

/*
 * Setup the local clock events for a CPU.
 */
void __cpuinit local_timer_setup(struct clock_event_device *evt)
{
	evt->irq = IRQ_LOCALTIMER;
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	twd_timer_setup_scalable(evt, 2500000, 4);
#else
	twd_timer_setup_scalable(evt, TWD_MHZ * 1000000, 2);
#endif
}
