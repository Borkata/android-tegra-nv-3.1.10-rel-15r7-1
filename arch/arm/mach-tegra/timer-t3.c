/*
 * arch/arch/mach-tegra/timer-t3.c
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

#include <linux/init.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/cnt32_to_63.h>
#include <linux/smp.h>

#include <asm/mach/time.h>
#include <asm/mach/time.h>
#include <asm/localtimer.h>

#include <mach/hardware.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/suspend.h>

#include "board.h"
#include "clock.h"
#include "power.h"

/*
 * Timers usage:
 * TMR1 - used as general cpu timer.
 * TMR2 - used by AVP.
 * TMR3 - used by CPU0 for Lp2 wakeup.
 * TMR4 - used by CPU1 for Lp2 wakeup.
 * TMR5 - used by CPU2 for Lp2 wakeup.
 * TMR6 - used by CPU3 for Lp2 wakeup.
 * TMR7 - Free.
 * TMR8 - Free.
 * TMR9 - Free.
 * TMR10 - used as src for watchdog controller 0.
*/

#define RTC_SECONDS		0x08
#define RTC_SHADOW_SECONDS	0x0c
#define RTC_MILLISECONDS	0x10

#define TIMERUS_CNTR_1US 0x10
#define TIMERUS_USEC_CFG 0x14
#define TIMERUS_CNTR_FREEZE 0x4c

#define TIMER1_OFFSET (TEGRA_TMR1_BASE-TEGRA_TMR1_BASE)
#define TIMER2_OFFSET (TEGRA_TMR2_BASE-TEGRA_TMR1_BASE)
#define TIMER3_OFFSET (TEGRA_TMR3_BASE-TEGRA_TMR1_BASE)
#define TIMER4_OFFSET (TEGRA_TMR4_BASE-TEGRA_TMR1_BASE)
#define TIMER5_OFFSET (TEGRA_TMR5_BASE-TEGRA_TMR1_BASE)
#define TIMER6_OFFSET (TEGRA_TMR6_BASE-TEGRA_TMR1_BASE)

#define TIMER_PTV 0x0
#define TIMER_PCR 0x4

static void __iomem *timer_reg_base = IO_ADDRESS(TEGRA_TMR1_BASE);
static void __iomem *rtc_base = IO_ADDRESS(TEGRA_RTC_BASE);

#define timer_writel(value, reg) \
	__raw_writel(value, (u32)timer_reg_base + (reg))
#define timer_readl(reg) \
	__raw_readl((u32)timer_reg_base + (reg))

static u64 tegra_sched_clock_offset;
static u64 tegra_sched_clock_suspend_val;
static u64 tegra_sched_clock_suspend_rtc;

static int tegra_timer_set_next_event(unsigned long cycles,
					 struct clock_event_device *evt)
{
	u32 reg;

	reg = 0x80000000 | ((cycles > 1) ? (cycles-1) : 0);
	timer_writel(reg, TIMER1_OFFSET + TIMER_PTV);

	return 0;
}

static void tegra_timer_set_mode(enum clock_event_mode mode,
				    struct clock_event_device *evt)
{
	u32 reg;

	timer_writel(0, TIMER1_OFFSET + TIMER_PTV);

	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		reg = 0xC0000000 | ((1000000/HZ)-1);
		timer_writel(reg, TIMER1_OFFSET + TIMER_PTV);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
		break;
	case CLOCK_EVT_MODE_UNUSED:
	case CLOCK_EVT_MODE_SHUTDOWN:
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static cycle_t tegra_clocksource_read(struct clocksource *cs)
{
	return timer_readl(TIMERUS_CNTR_1US);
}

static struct clock_event_device tegra_clockevent = {
	.name		= "timer0",
	.rating		= 300,
	.features	= CLOCK_EVT_FEAT_ONESHOT | CLOCK_EVT_FEAT_PERIODIC,
	.set_next_event	= tegra_timer_set_next_event,
	.set_mode	= tegra_timer_set_mode,
};

static struct clocksource tegra_clocksource = {
	.name	= "timer_us",
	.rating	= 300,
	.read	= tegra_clocksource_read,
	.mask	= CLOCKSOURCE_MASK(32),
	.flags	= CLOCK_SOURCE_IS_CONTINUOUS,
};

unsigned long long sched_clock(void)
{
	return tegra_sched_clock_offset +
		cnt32_to_63(timer_readl(TIMERUS_CNTR_1US)) * NSEC_PER_USEC;
}

static void tegra_sched_clock_suspend(void)
{
	tegra_sched_clock_suspend_val = sched_clock();
	tegra_sched_clock_suspend_rtc = tegra_rtc_read_ms();
}

static void tegra_sched_clock_resume(void)
{
	u64 rtc_offset_ms = tegra_rtc_read_ms() - tegra_sched_clock_suspend_rtc;
	tegra_sched_clock_offset = tegra_sched_clock_suspend_val +
		rtc_offset_ms * NSEC_PER_MSEC -
		(sched_clock() - tegra_sched_clock_offset);
}

/*
 * tegra_rtc_read - Reads the Tegra RTC registers
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
u64 tegra_rtc_read_ms(void)
{
	u32 ms = readl(rtc_base + RTC_MILLISECONDS);
	u32 s = readl(rtc_base + RTC_SHADOW_SECONDS);
	return (u64)s * MSEC_PER_SEC + ms;
}

/*
 * read_persistent_clock -  Return time from a persistent clock.
 *
 * Reads the time from a source which isn't disabled during PM, the
 * 32k sync timer.  Convert the cycles elapsed since last read into
 * nsecs and adds to a monotonically increasing timespec.
 * Care must be taken that this funciton is not called while the
 * tegra_rtc driver could be executing to avoid race conditions
 * on the RTC shadow register
 */
static struct timespec persistent_ts;
static u64 persistent_ms, last_persistent_ms;
void read_persistent_clock(struct timespec *ts)
{
	u64 delta;
	struct timespec *tsp = &persistent_ts;

	last_persistent_ms = persistent_ms;
	persistent_ms = tegra_rtc_read_ms();
	delta = persistent_ms - last_persistent_ms;

	timespec_add_ns(tsp, delta * NSEC_PER_MSEC);
	*ts = *tsp;
}

static irqreturn_t tegra_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = (struct clock_event_device *)dev_id;
	timer_writel(1<<30, TIMER1_OFFSET + TIMER_PCR);
	evt->event_handler(evt);
	return IRQ_HANDLED;
}

static struct irqaction tegra_timer_irq = {
	.name		= "timer0",
	.flags		= IRQF_DISABLED | IRQF_TIMER | IRQF_TRIGGER_HIGH,
	.handler	= tegra_timer_interrupt,
	.dev_id		= &tegra_clockevent,
	.irq		= INT_TMR1,
};

static int lp2_wake_timers[] = {
	TIMER3_OFFSET,
	TIMER4_OFFSET,
	TIMER5_OFFSET,
	TIMER6_OFFSET,
};

static irqreturn_t tegra_lp2wake_interrupt(int irq, void *dev_id)
{
	int cpu = (int)dev_id;
	int timer_base;

	timer_base = lp2_wake_timers[cpu];
	timer_writel(1<<30, timer_base + TIMER_PCR);
	return IRQ_HANDLED;
}

#define LP2_TIMER_IRQ_ACTION(cpu, irqnum) {			\
	.name		= "tmr_lp2wake_cpu" __stringify(cpu),	\
	.flags		= IRQF_DISABLED,			\
	.handler	= tegra_lp2wake_interrupt,		\
	.dev_id		= (void*)cpu,				\
	.irq		= irqnum,				\
}

static struct irqaction lp2wake_actions[] = {
	LP2_TIMER_IRQ_ACTION(0, INT_TMR3),
#ifdef CONFIG_SMP
	LP2_TIMER_IRQ_ACTION(1, INT_TMR4),
	LP2_TIMER_IRQ_ACTION(2, INT_TMR5),
	LP2_TIMER_IRQ_ACTION(3, INT_TMR6),
#endif
};

/*
 * To sanity test timer interrupts for cpu 0-3, enable this flag and check
 * /proc/interrupts for timer interrupts. Cpu's 0-3 would have one interrupt
 * counted against them for tmr_lp2wake_cpu0,1,2,3.
 */
#define TEST_LP2_WAKE_TIMERS 0
#if TEST_LP2_WAKE_TIMERS
static void test_lp2_wake_timers(void)
{
	unsigned int cpu;
	unsigned int timer_base;
	unsigned long cycles = 50000;

	for_each_present_cpu(cpu) {
		timer_base = lp2_wake_timers[cpu];
		timer_writel(0, timer_base + TIMER_PTV);
		if (cycles) {
			u32 reg = 0x80000000ul | min(0x1ffffffful, cycles);
			timer_writel(reg, timer_base + TIMER_PTV);
		}
	}
}
#else
static void test_lp2_wake_timers(void){}
#endif

static void __init tegra_init_timer(void)
{
	unsigned long rate = clk_measure_input_freq();
	unsigned int cpu;
	int ret;

#ifdef CONFIG_HAVE_ARM_TWD
	twd_base = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x600);
#endif

	switch (rate) {
	case 12000000:
		timer_writel(0x000b, TIMERUS_USEC_CFG);
		break;
	case 13000000:
		timer_writel(0x000c, TIMERUS_USEC_CFG);
		break;
	case 19200000:
		timer_writel(0x045f, TIMERUS_USEC_CFG);
		break;
	case 26000000:
		timer_writel(0x0019, TIMERUS_USEC_CFG);
		break;
	case 16800000:
		timer_writel(0x0453, TIMERUS_USEC_CFG);
		break;
	case 38400000:
		timer_writel(0x04BF, TIMERUS_USEC_CFG);
		break;
	case 48000000:
		timer_writel(0x002F, TIMERUS_USEC_CFG);
		break;
	default:
		WARN(1, "Unknown clock rate");
	}

	if (clocksource_register_hz(&tegra_clocksource, 1000000)) {
		printk(KERN_ERR "Failed to register clocksource\n");
		BUG();
	}

	ret = setup_irq(tegra_timer_irq.irq, &tegra_timer_irq);
	if (ret) {
		printk(KERN_ERR "Failed to register timer IRQ: %d\n", ret);
		BUG();
	}

#ifdef CONFIG_SMP
	/* For T30.A01 use INT_TMR_SHARED instead of INT_TMR6 for CPU3. */
	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) &&
		(tegra_get_revision() == TEGRA_REVISION_A01))
			lp2wake_actions[3].irq = INT_TMR_SHARED;
#endif

	for_each_present_cpu(cpu) {
		ret = setup_irq(lp2wake_actions[cpu].irq, &lp2wake_actions[cpu]);
		if (ret) {
			printk(KERN_ERR "Failed to register LP2 timer IRQ: "
				"irq=%d, ret=%d\n",
				lp2wake_actions[cpu].irq, ret);
			BUG();
		}
#ifdef CONFIG_SMP
		ret = irq_set_affinity(lp2wake_actions[cpu].irq,
				       cpumask_of(cpu));
		if (ret) {
			printk(KERN_ERR "Failed to set affinity for LP2 timer "
				"IRQ: irq=%d, ret=%d\n",
				lp2wake_actions[cpu].irq, ret);
			BUG();
		}
#endif
	}

	clockevents_calc_mult_shift(&tegra_clockevent, 1000000, 5);
	tegra_clockevent.max_delta_ns =
		clockevent_delta2ns(0x1fffffff, &tegra_clockevent);
	tegra_clockevent.min_delta_ns =
		clockevent_delta2ns(0x1, &tegra_clockevent);
	tegra_clockevent.cpumask = cpu_all_mask;
	tegra_clockevent.irq = tegra_timer_irq.irq;
	clockevents_register_device(&tegra_clockevent);

	test_lp2_wake_timers();
	return;
}

struct sys_timer tegra_timer = {
	.init = tegra_init_timer,
};

#ifdef CONFIG_SMP
#define cpu_number()	hard_smp_processor_id()
#else
#define cpu_number()	0
#endif

void tegra_lp2_set_trigger(unsigned long cycles)
{
	int cpu = cpu_number();
	int timer_base;

	timer_base = lp2_wake_timers[cpu];
	timer_writel(0, timer_base + TIMER_PTV);
	if (cycles) {
		u32 reg = 0x80000000ul | min(0x1ffffffful, cycles);
		timer_writel(reg, timer_base + TIMER_PTV);
	}
}
EXPORT_SYMBOL(tegra_lp2_set_trigger);

unsigned long tegra_lp2_timer_remain(void)
{
	int cpu = cpu_number();
	int timer_base;

	timer_base = lp2_wake_timers[cpu];
	return timer_readl(timer_base + TIMER_PCR) & 0x1ffffffful;
}

static u32 usec_config;
void tegra_timer_suspend(void)
{
	tegra_sched_clock_suspend();
	usec_config = timer_readl(TIMERUS_USEC_CFG);
}

void tegra_timer_resume(void)
{
	timer_writel(usec_config, TIMERUS_USEC_CFG);
	tegra_sched_clock_resume();
}
