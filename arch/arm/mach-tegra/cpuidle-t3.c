/*
 * arch/arm/mach-tegra/cpuidle-t3.c
 *
 * CPU idle driver for Tegra3 CPUs
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

#include <linux/kernel.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/suspend.h>
#include <linux/tick.h>

#include <asm/cacheflush.h>
#include <asm/hardware/gic.h>
#include <asm/localtimer.h>

#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/legacy_irq.h>
#include <mach/suspend.h>

#include <trace/events/power.h>

#include "power.h"
#include "reset.h"
#include "clock.h"
#include "dvfs.h"
#include "fuse.h"

#ifdef CONFIG_SMP
static s64 tegra_cpu_wake_by_time[4] = {LLONG_MAX, LLONG_MAX, LLONG_MAX, LLONG_MAX};
#endif
extern int tegra_lp2_exit_latency;

static struct clk *cpu_clk_for_dvfs;

static struct {
	unsigned int cpu_ready_count[5];
	unsigned int tear_down_count[5];
	unsigned long long cpu_wants_lp2_time[5];
	unsigned long long in_lp2_time[5];
	unsigned int lp2_count;
	unsigned int lp2_completed_count;
	unsigned int lp2_count_bin[32];
	unsigned int lp2_completed_count_bin[32];
	unsigned int lp2_int_count[NR_IRQS];
	unsigned int last_lp2_int_count[NR_IRQS];
} idle_stats;

static inline unsigned int time_to_bin(unsigned int time)
{
	return fls(time);
}

static inline void tegra_unmask_irq(int irq)
{
	struct irq_chip *chip = get_irq_chip(irq);
	chip->unmask(irq);
}

static inline int tegra_pending_interrupt(void)
{
	void __iomem *gic_cpu = IO_ADDRESS(TEGRA_ARM_PERIF_BASE + 0x100);
	u32 reg = readl(gic_cpu + 0x18);
	reg &= 0x3FF;

	return reg;
}

static unsigned int cpu_number(unsigned int n)
{
	return (is_lp_cluster() ? 4 : n);
}

void tegra_idle_stats_lp2_ready(unsigned int cpu)
{
	idle_stats.cpu_ready_count[cpu_number(cpu)]++;
}

void tegra_idle_stats_lp2_time(unsigned int cpu, s64 us)
{
	idle_stats.cpu_wants_lp2_time[cpu_number(cpu)] += us;
}

bool tegra_lp2_is_allowed(struct cpuidle_device *dev,
	struct cpuidle_state *state)
{
	if (!tegra_all_cpus_booted)
		return false;

	/* On A01, lp2 on slave cpu's cause cpu hang randomly.
	 * Refer to Bug 804085.
	 */
	if ( (tegra_get_revision() == TEGRA_REVISION_A01) &&
		num_online_cpus() > 1)
		return false;
	/* FIXME: all cpu's entering lp2 is not working.
	 * don't let cpu0 enter lp2 when any of slave  cpu is alive.
	 */
	if ( (dev->cpu == 0) && (num_online_cpus() > 1) )
		return false;

	if (dev->cpu == 0) {
		u32 reg = readl(CLK_RST_CONTROLLER_CPU_CMPLX_STATUS);
		if ((reg & 0xE) != 0xE) {
			return false;
		}

		if (tegra_dvfs_rail_updating(cpu_clk_for_dvfs))
			return false;
	}
	return true;
}

void tegra_idle_enter_lp2_cpu_0(struct cpuidle_device *dev,
	struct cpuidle_state *state)
{
	s64 request;
	ktime_t enter;
	ktime_t exit;
	bool sleep_completed = false;
	int bin;

	if (need_resched())
		return;

	request = ktime_to_us(tick_nohz_get_sleep_length());
	if (request < state->target_residency) {
		/* Not enough time left to enter LP2 */
		tegra_flow_wfi(dev);
		return;
	}

	/* LP2 entry time */
	enter = ktime_get();

#ifdef CONFIG_SMP
	if (!is_lp_cluster() && (num_online_cpus() > 1)) {
		s64 wake_time;
		unsigned int i;

		/* Disable the distributor -- this is the only way to
		   prevent the other CPUs from responding to interrupts
		   and potentially fiddling with the distributor
		   registers while we're fiddling with them. */
		gic_dist_exit(0);

		/* Did an interrupt come in for another CPU before we
		   could disable the distributor? */
		if (!tegra_lp2_is_allowed(dev, state)) {
			/* Yes, re-enable the distributor and LP3. */
			gic_dist_enable(0);
			tegra_flow_wfi(dev);
			return;
		}

		/* Save and disable the affinity setting for the other
		   CPUs and route all interrupts to CPU0. */
		tegra_irq_disable_affinity();

		/* Re-enable the distributor. */
		gic_dist_enable(0);

		/* LP2 initial targeted wake time */
		wake_time = ktime_to_us(enter) + request;

		/* CPU0 must wake up before any of the other CPUs. */
		smp_rmb();
		for (i = 1; i < CONFIG_NR_CPUS; i++)
			wake_time = min_t(s64, wake_time,
				tegra_cpu_wake_by_time[i]);

		/* LP2 actual targeted wake time */
		request = wake_time - ktime_to_us(enter);
		BUG_ON(wake_time < 0LL);
	}
#endif

	if (request > state->target_residency) {
		s64 sleep_time = request - tegra_lp2_exit_latency;

		bin = time_to_bin((u32)request / 1000);
		idle_stats.tear_down_count[cpu_number(dev->cpu)]++;
		idle_stats.lp2_count++;
		idle_stats.lp2_count_bin[bin]++;

		trace_power_start(POWER_CSTATE, 2, dev->cpu);
		if (!is_lp_cluster())
			tegra_dvfs_rail_off(tegra_cpu_rail, enter);

		if (tegra_suspend_lp2(sleep_time, 0) == 0)
			sleep_completed = true;
		else
			idle_stats.lp2_int_count[tegra_pending_interrupt()]++;

		exit = ktime_get();
		if (!is_lp_cluster())
			tegra_dvfs_rail_on(tegra_cpu_rail, exit);
	} else
		exit = ktime_get();

#ifdef CONFIG_SMP
	if (!is_lp_cluster() && (num_online_cpus() > 1)) {

		/* Disable the distributor. */
		gic_dist_exit(0);

		/* Restore the other CPU's interrupt affinity. */
		tegra_irq_restore_affinity();

		/* Re-enable the distributor. */
		gic_dist_enable(0);
	}
#endif

	if (sleep_completed) {
		/*
		 * Stayed in LP2 for the full time until the next tick,
		 * adjust the exit latency based on measurement
		 */
		int offset = ktime_to_us(ktime_sub(exit, enter)) - request;
		int latency = tegra_lp2_exit_latency + offset / 16;
		latency = clamp(latency, 0, 10000);
		tegra_lp2_exit_latency = latency;
		smp_wmb();

		idle_stats.lp2_completed_count++;
		idle_stats.lp2_completed_count_bin[bin]++;
		idle_stats.in_lp2_time[cpu_number(dev->cpu)] +=
			ktime_to_us(ktime_sub(exit, enter));

		pr_debug("%lld %lld %d %d\n", request,
			ktime_to_us(ktime_sub(exit, enter)),
			offset, bin);
	}
}

#ifdef CONFIG_SMP
void tegra_idle_enter_lp2_cpu_n(struct cpuidle_device *dev,
	struct cpuidle_state *state)
{
	u32 twd_ctrl;
	u32 twd_load;
	s64 request;
	s64 sleep_time;
	ktime_t enter;
	ktime_t exit;

	if (need_resched())
		return;

	request = ktime_to_us(tick_nohz_get_sleep_length());
	if (request < tegra_lp2_exit_latency) {
		/*
		 * Not enough time left to enter LP2
		 */
		tegra_flow_wfi(dev);
		return;
	}
	sleep_time = request - tegra_lp2_exit_latency;
	tegra_lp2_set_trigger(sleep_time);

	idle_stats.tear_down_count[cpu_number(dev->cpu)]++;

	trace_power_start(POWER_CSTATE, 2, dev->cpu);

	enter = ktime_get();

	/* Save time this CPU must be awakened by. */
	tegra_cpu_wake_by_time[dev->cpu] = ktime_to_us(enter) + request;
	smp_wmb();

	/* Powergate CPUn. */
	stop_critical_timings();
	/* gic_cpu_exit(0); - we want to wake cpu_n on gic interrupt */
	barrier();
	twd_ctrl = readl(twd_base + 0x8);
	twd_load = readl(twd_base + 0);

	spin_lock(&lp2_map_lock);
	tegra_cpu_lp2_map |= (1 << dev->cpu);
	spin_unlock(&lp2_map_lock);

	flush_cache_all();
	tegra_cpu_reset_handler_flush(false);
	barrier();
	__cortex_a9_save(0);
	/* CPUn is powergated */

	/* CPUn woke up */
	barrier();
	if (suspend_wfi_failed()) {
		tegra_wfi_fail_count[dev->cpu]++;
		pr_err_ratelimited("WFI for LP2 failed for CPU %d: count %lu\n",
				    dev->cpu, tegra_wfi_fail_count[dev->cpu]);
	}

	spin_lock(&lp2_map_lock);
	tegra_cpu_lp2_map &= ~(1 << dev->cpu);
	spin_unlock(&lp2_map_lock);

	writel(twd_ctrl, twd_base + 0x8);
	writel(twd_load, twd_base + 0);
	gic_cpu_init(0, IO_ADDRESS(TEGRA_ARM_PERIF_BASE) + 0x100);
	tegra_unmask_irq(IRQ_LOCALTIMER);

	tegra_cpu_wake_by_time[dev->cpu] = LLONG_MAX;
	writel(smp_processor_id(), EVP_CPU_RSVD_VECTOR);
	start_critical_timings();

	if (sleep_time)
		tegra_lp2_set_trigger(0);
	/*
	 * TODO: is it worth going back to wfi if no interrupt is pending
	 * and the requested sleep time has not passed?
	 */

	exit = ktime_get();
	idle_stats.in_lp2_time[cpu_number(dev->cpu)] +=
		ktime_to_us(ktime_sub(exit, enter));

	return;
}
#endif

int tegra_cpudile_init_soc(void)
{
	cpu_clk_for_dvfs = tegra_get_clock_by_name("cpu_g");
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int tegra_lp2_debug_show(struct seq_file *s, void *data)
{
	int bin;
	int i;
	seq_printf(s, "                                    cpu0     cpu1     cpu2     cpu3     cpulp\n");
	seq_printf(s, "-----------------------------------------------------------------------------\n");
	seq_printf(s, "lp2 ready count:                %8u %8u %8u %8u %8u\n",
		idle_stats.cpu_ready_count[0],
		idle_stats.cpu_ready_count[1],
		idle_stats.cpu_ready_count[2],
		idle_stats.cpu_ready_count[3],
		idle_stats.cpu_ready_count[4]);
	seq_printf(s, "tear down count:                %8u %8u %8u %8u %8u\n",
		idle_stats.tear_down_count[0],
		idle_stats.tear_down_count[1],
		idle_stats.tear_down_count[2],
		idle_stats.tear_down_count[3],
		idle_stats.tear_down_count[4]);
	seq_printf(s, "\n");
	seq_printf(s, "lp2 count:      %8u\n", idle_stats.lp2_count);
	seq_printf(s, "lp2 completed:  %8u %7u%%\n",
		idle_stats.lp2_completed_count,
		idle_stats.lp2_completed_count * 100 /
			(idle_stats.lp2_count ?: 1));

	seq_printf(s, "\n");
	seq_printf(s, "lp2 ready time:%16s %8llu %8llu %8llu %8llu %8llu ms\n",
		"",
		div64_u64(idle_stats.cpu_wants_lp2_time[0], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[1], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[2], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[3], 1000),
		div64_u64(idle_stats.cpu_wants_lp2_time[4], 1000));
	seq_printf(s, "lp2 time:%22s %8llu %8llu %8llu %8llu %8llu ms\n",
		"",
		div64_u64(idle_stats.in_lp2_time[0], 1000),
		div64_u64(idle_stats.in_lp2_time[1], 1000),
		div64_u64(idle_stats.in_lp2_time[2], 1000),
		div64_u64(idle_stats.in_lp2_time[3], 1000),
		div64_u64(idle_stats.in_lp2_time[4], 1000));
	seq_printf(s, "lp2 %%:%26s %7d%% %7d%% %7d%% %7d%% %7d%%\n",
		"",
		(int)(idle_stats.cpu_wants_lp2_time[0] ?
			div64_u64(idle_stats.in_lp2_time[0] * 100,
			idle_stats.cpu_wants_lp2_time[0]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[1] ?
			div64_u64(idle_stats.in_lp2_time[1] * 100,
			idle_stats.cpu_wants_lp2_time[1]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[2] ?
			div64_u64(idle_stats.in_lp2_time[2] * 100,
			idle_stats.cpu_wants_lp2_time[2]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[3] ?
			div64_u64(idle_stats.in_lp2_time[3] * 100,
			idle_stats.cpu_wants_lp2_time[3]) : 0),
		(int)(idle_stats.cpu_wants_lp2_time[4] ?
			div64_u64(idle_stats.in_lp2_time[4] * 100,
			idle_stats.cpu_wants_lp2_time[4]) : 0));
	seq_printf(s, "\n");

	seq_printf(s, "%19s %8s %8s %8s\n", "", "lp2", "comp", "%");
	seq_printf(s, "-------------------------------------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (idle_stats.lp2_count_bin[bin] == 0)
			continue;
		seq_printf(s, "%6u - %6u ms: %8u %8u %7u%%\n",
			1 << (bin - 1), 1 << bin,
			idle_stats.lp2_count_bin[bin],
			idle_stats.lp2_completed_count_bin[bin],
			idle_stats.lp2_completed_count_bin[bin] * 100 /
				idle_stats.lp2_count_bin[bin]);
	}

	seq_printf(s, "\n");
	seq_printf(s, "%3s %20s %6s %10s\n",
		"int", "name", "count", "last count");
	seq_printf(s, "--------------------------------------------\n");
	for (i = 0; i < NR_IRQS; i++) {
		if (idle_stats.lp2_int_count[i] == 0)
			continue;
		seq_printf(s, "%3d %20s %6d %10d\n",
			i, irq_to_desc(i)->action ?
				irq_to_desc(i)->action->name ?: "???" : "???",
			idle_stats.lp2_int_count[i],
			idle_stats.lp2_int_count[i] -
				idle_stats.last_lp2_int_count[i]);
		idle_stats.last_lp2_int_count[i] = idle_stats.lp2_int_count[i];
	};
	return 0;
}

static int tegra_lp2_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_lp2_debug_show, inode->i_private);
}

static const struct file_operations tegra_lp2_debug_ops = {
	.open		= tegra_lp2_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_cpuidle_debug_init(void)
{
	struct dentry *dir;
	struct dentry *d;

	dir = debugfs_create_dir("cpuidle", NULL);
	if (!dir)
		return -ENOMEM;

	d = debugfs_create_file("lp2", S_IRUGO, dir, NULL,
		&tegra_lp2_debug_ops);
	if (!d)
		return -ENOMEM;

	return 0;
}
#endif

late_initcall(tegra_cpuidle_debug_init);
