/*
 * arch/arm/mach-tegra/suspend.c
 *
 * CPU complex suspend & resume functions for Tegra SoCs
 *
 * Copyright (c) 2009-2011, NVIDIA Corporation.
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
#include <linux/cpumask.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/serial_reg.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/console.h>

#include <linux/regulator/machine.h>

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <asm/localtimer.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/iovmm.h>
#include <mach/irqs.h>
#include <mach/legacy_irq.h>
#include <mach/powergate.h>
#include <mach/suspend.h>

#include "board.h"
#include "clock.h"
#include "power.h"
#include "reset.h"
#include "fuse.h"
#include "dvfs.h"

#ifdef CONFIG_TRUSTED_FOUNDATIONS
void callGenericSMC(u32 param0, u32 param1, u32 param2)
{
	__asm__ volatile(
		"mov r0, %2\n"
		"mov r1, %3\n"
		"mov r2, %4\n"
		"mov r3, #0\n"
		"mov r4, #0\n"
		".word    0xe1600070              @ SMC 0\n"
		"mov %0, r0\n"
		"mov %1, r1\n"
		: "=r" (param0), "=r" (param1)
		: "r" (param0), "r" (param1),
		  "r" (param2)
		: "r0", "r1", "r2", "r3", "r4");
}
#endif

/**************** END TL *********************/

DEFINE_SPINLOCK(lp2_map_lock);

struct suspend_context {
	/*
	 * The next 7 values are referenced by offset in __restart_plls
	 * in headsmp-t2.S, and should not be moved
	 */
	u32 pllx_misc;
	u32 pllx_base;
	u32 pllp_misc;
	u32 pllp_base;
	u32 pllp_outa;
	u32 pllp_outb;
	u32 pll_timeout;

	u32 cpu_burst;
	u32 clk_csite_src;
	u32 twd_ctrl;
	u32 twd_load;
	u32 cclk_divider;
};

volatile struct suspend_context tegra_sctx;

#if defined(CONFIG_PM) || defined(CONFIG_CPU_IDLE) || !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static void __iomem *clk_rst = IO_ADDRESS(TEGRA_CLK_RESET_BASE);
static void __iomem *tmrus = IO_ADDRESS(TEGRA_TMRUS_BASE);
static void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
#endif
#ifdef CONFIG_PM
static unsigned long wb0_restore = 0;
#endif
unsigned long tegra_wfi_fail_count[CONFIG_NR_CPUS];

#define PMC_CTRL		0x0
#define PMC_CTRL_LATCH_WAKEUPS	(1 << 5)
#define PMC_WAKE_MASK		0xc
#define PMC_WAKE_LEVEL		0x10
#define PMC_DPAD_ORIDE		0x1C
#define PMC_WAKE_DELAY		0xe0
#define PMC_DPD_SAMPLE		0x20

#define PMC_WAKE_STATUS		0x14
#define PMC_SW_WAKE_STATUS	0x18
#define PMC_COREPWRGOOD_TIMER	0x3c
#define PMC_SCRATCH0		0x50
#define PMC_SCRATCH1		0x54
#define PMC_SCRATCH4		0x60
#define PMC_CPUPWRGOOD_TIMER	0xc8
#define PMC_CPUPWROFF_TIMER	0xcc
#define PMC_COREPWROFF_TIMER	PMC_WAKE_DELAY
#define PMC_SCRATCH38		0x134
#define PMC_SCRATCH39		0x138
#define PMC_SCRATCH41		0x140

#define CLK_RESET_CCLK_BURST	0x20
#define CLK_RESET_CCLK_DIVIDER  0x24
#define CLK_RESET_PLLC_BASE	0x80
#define CLK_RESET_PLLM_BASE	0x90
#define CLK_RESET_PLLX_BASE	0xe0
#define CLK_RESET_PLLX_MISC	0xe4
#define CLK_RESET_PLLP_BASE	0xa0
#define CLK_RESET_PLLP_OUTA	0xa4
#define CLK_RESET_PLLP_OUTB	0xa8
#define CLK_RESET_PLLP_MISC	0xac

#define CLK_RESET_SOURCE_CSITE	0x1d4

#define CLK_RESET_CCLK_BURST_POLICY_SHIFT 28
#define CLK_RESET_CCLK_BURST_POLICY_PLLM   3
#define CLK_RESET_CCLK_BURST_POLICY_PLLX   8

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
#define FLOW_CTRL_BITMAP_MASK	(3<<4)
#define FLOW_CTRL_BITMAP_CPU0	(1<<4)	/* CPU0 WFE bitmap */
#else
#define FLOW_CTRL_BITMAP_MASK	((0xF<<4) | (0xF<<8))
#define FLOW_CTRL_BITMAP_CPU0	(1<<8)	/* CPU0 WFI bitmap */
#endif

#ifndef CONFIG_ARCH_TEGRA_2x_SOC
#define PMC_SCRATCH4_WAKE_CLUSTER_MASK	(1<<31)
#endif

#define EMC_MRW_0		0x0e8
#define EMC_MRW_DEV_SELECTN     30
#define EMC_MRW_DEV_NONE	(3 << EMC_MRW_DEV_SELECTN)

phys_addr_t tegra_pgd_phys;  /* pgd used by hotplug & LP2 bootup */
static pgd_t *tegra_pgd;
void *tegra_context_area = NULL;

struct dvfs_rail *tegra_cpu_rail = NULL;
struct dvfs_rail *tegra_core_rail = NULL;

static struct clk *tegra_pclk = NULL;
static const struct tegra_suspend_platform_data *pdata = NULL;
static enum tegra_suspend_mode current_suspend_mode;

static unsigned int tegra_time_in_suspend[32];

struct kobject *suspend_kobj;

static const char *tegra_suspend_name[TEGRA_MAX_SUSPEND_MODE] = {
	[TEGRA_SUSPEND_NONE]	= "none",
	[TEGRA_SUSPEND_LP2]	= "lp2",
	[TEGRA_SUSPEND_LP1]	= "lp1",
	[TEGRA_SUSPEND_LP0]	= "lp0",
};

#if INSTRUMENT_CLUSTER_SWITCH
enum tegra_cluster_switch_time_id {
	tegra_cluster_switch_time_id_start = 0,
	tegra_cluster_switch_time_id_prolog,
	tegra_cluster_switch_time_id_switch,
	tegra_cluster_switch_time_id_epilog,
	tegra_cluster_switch_time_id_max
};

static unsigned long
		tegra_cluster_switch_times[tegra_cluster_switch_time_id_max];
#define tegra_cluster_switch_time(flags, id) \
	do { \
		barrier(); \
		if (flags & TEGRA_POWER_CLUSTER_MASK) { \
			void __iomem *timer_us = \
						IO_ADDRESS(TEGRA_TMRUS_BASE); \
			if (id < tegra_cluster_switch_time_id_max) \
				tegra_cluster_switch_times[id] = \
							readl(timer_us); \
				wmb(); \
		} \
		barrier(); \
	} while (0)
#else
#define tegra_cluster_switch_time(flags, id) do {} while (0)
#endif

#ifdef CONFIG_SMP
#define cpu_number()	hard_smp_processor_id()
#else
#define cpu_number()	0
#endif

static inline unsigned int time_to_bin(unsigned int time)
{
	return fls(time);
}

unsigned long tegra_cpu_power_good_time(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 5000;

	return pdata->cpu_timer;
}

unsigned long tegra_cpu_power_off_time(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 5000;

	return pdata->cpu_off_timer;
}

unsigned long tegra_cpu_lp2_min_residency(void)
{
	if (WARN_ON_ONCE(!pdata))
		return 2000;

	return pdata->cpu_lp2_min_residency;
}

enum tegra_suspend_mode tegra_get_suspend_mode(void)
{
	if (!pdata)
		return TEGRA_SUSPEND_NONE;

	return current_suspend_mode;
}

#if defined(CONFIG_PM) || defined(CONFIG_CPU_IDLE) || !defined(CONFIG_ARCH_TEGRA_2x_SOC)
static void set_power_timers(unsigned long us_on, unsigned long us_off,
			     long rate)
{
	static unsigned long last_us_off = 0;
	static int last_pclk = 0;
	unsigned long long ticks;
	unsigned long long pclk;

	if (WARN_ON_ONCE(rate <= 0))
		pclk = 100000000;
	else
		pclk = rate;

	if ((rate != last_pclk) || (us_off != last_us_off)) {
		ticks = (us_on * pclk) + 999999ull;
		do_div(ticks, 1000000);
		writel((unsigned long)ticks, pmc + PMC_CPUPWRGOOD_TIMER);

		ticks = (us_off * pclk) + 999999ull;
		do_div(ticks, 1000000);
		writel((unsigned long)ticks, pmc + PMC_CPUPWROFF_TIMER);
		wmb();
	}
	last_pclk = pclk;
	last_us_off = us_off;
}
#endif

static int create_suspend_pgtable(void)
{
	int i;
	pmd_t *pmd;
	void __put_cpu_in_reset(void);

	/* arrays of virtual-to-physical mappings which must be
	 * present to safely boot hotplugged / LP2-idled CPUs.
	 * tegra_hotplug_startup (hotplug reset vector) is mapped
	 * VA=PA so that the translation post-MMU is the same as
	 * pre-MMU, IRAM is mapped VA=PA so that SDRAM self-refresh
	 * can safely disable the MMU */
	unsigned long addr_v[] = {
		PHYS_OFFSET,
		IO_IRAM_PHYS,
		(unsigned long)tegra_context_area,
#ifdef CONFIG_HOTPLUG_CPU
		(unsigned long)virt_to_phys(tegra_hotplug_startup),
#endif
		(unsigned long)__cortex_a9_restore,
		(unsigned long)virt_to_phys(__shut_off_mmu),
		(unsigned long)virt_to_phys(__put_cpu_in_reset),
	};
	phys_addr_t addr_p[] = {
		(phys_addr_t)PHYS_OFFSET,
		(phys_addr_t)IO_IRAM_PHYS,
		virt_to_phys(tegra_context_area),
#ifdef CONFIG_HOTPLUG_CPU
		virt_to_phys(tegra_hotplug_startup),
#endif
		virt_to_phys(__cortex_a9_restore),
		virt_to_phys(__shut_off_mmu),
		virt_to_phys(__put_cpu_in_reset),
	};
	unsigned int flags = PMD_TYPE_SECT | PMD_SECT_AP_WRITE |
		PMD_SECT_WBWA | PMD_SECT_S;

	tegra_pgd = pgd_alloc(&init_mm);
	if (!tegra_pgd)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(addr_p); i++) {
		unsigned long v = addr_v[i];
		pmd = pmd_offset(tegra_pgd + pgd_index(v), v);
		*pmd = __pmd((addr_p[i] & PGDIR_MASK) | flags);
		flush_pmd_entry(pmd);
		outer_clean_range(__pa(pmd), __pa(pmd + 1));
	}

	tegra_pgd_phys = virt_to_phys(tegra_pgd);
	__cpuc_flush_dcache_area(&tegra_pgd_phys,
		sizeof(tegra_pgd_phys));
	outer_clean_range(__pa(&tegra_pgd_phys),
		__pa(&tegra_pgd_phys+1));

	__cpuc_flush_dcache_area(&tegra_context_area,
		sizeof(tegra_context_area));
	outer_clean_range(__pa(&tegra_context_area),
		__pa(&tegra_context_area+1));

	return 0;
}

#if defined(CONFIG_PM) || defined(CONFIG_CPU_IDLE) || !defined(CONFIG_ARCH_TEGRA_2x_SOC)
/*
 * suspend_cpu_complex
 *
 *   disable periodic IRQs used for DVFS to prevent suspend wakeups
 *   disable coresight debug interface
 *
 *
 */
static noinline void restore_cpu_complex(void)
{
	unsigned int reg;

	/* Is CPU complex already running on PLLX? */
	reg = readl(clk_rst + CLK_RESET_CCLK_BURST);
	reg &= 0xF;
	if (reg != 0x8) {
		/* restore original burst policy setting; PLLX state restored
		 * by CPU boot-up code - wait for PLL stabilization if PLLX
		 * was enabled */

		reg = readl(clk_rst + CLK_RESET_PLLX_BASE);
		/* mask out bit 27 - not to check PLL lock bit */
		BUG_ON((reg & (~(1 << 27))) !=
				(tegra_sctx.pllx_base & (~(1 << 27))));

		if (tegra_sctx.pllx_base & (1<<30)) {
#if USE_PLL_LOCK_BITS
			/* Enable lock detector */
			reg = readl(clk_rst + CLK_RESET_PLLX_MISC);
			reg |= 1<<18;
			writel(reg, clk_rst + CLK_RESET_PLLX_MISC);
			while (!(readl(clk_rst + CLK_RESET_PLLX_BASE) &&
				 (1<<27)))
				cpu_relax();
#else
			while (readl(tmrus)-tegra_sctx.pll_timeout
			       >= 0x80000000UL)
				cpu_relax();
#endif
		}
		writel(tegra_sctx.cclk_divider, clk_rst +
		       CLK_RESET_CCLK_DIVIDER);
		writel(tegra_sctx.cpu_burst, clk_rst +
		       CLK_RESET_CCLK_BURST);
	}

	writel(tegra_sctx.clk_csite_src, clk_rst + CLK_RESET_SOURCE_CSITE);

	/* do not power-gate the CPU when flow controlled */
	reg = readl(FLOW_CTRL_CPUx_CSR(0));
	reg |= (1<<15)|(1<<14);	/* write to clear: INTR_FLAG|EVENT_FLAG */
	/* Clear the WFE/WFI bitmaps and power-gate enable. */
	reg &= ~(FLOW_CTRL_BITMAP_MASK | 1);
	flowctrl_writel(reg, FLOW_CTRL_CPUx_CSR(0));

#ifdef CONFIG_HAVE_ARM_TWD
	writel(tegra_sctx.twd_ctrl, twd_base + 0x8);
	writel(tegra_sctx.twd_load, twd_base + 0);
#endif

	gic_dist_restore(0);
	get_irq_chip(IRQ_LOCALTIMER)->unmask(IRQ_LOCALTIMER);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	enable_irq(INT_SYS_STATS_MON);
#endif
}

static noinline void suspend_cpu_complex(void)
{
	unsigned int reg;
	int i;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	disable_irq_nosync(INT_SYS_STATS_MON);
#endif
	/* switch coresite to clk_m, save off original source */
	tegra_sctx.clk_csite_src = readl(clk_rst + CLK_RESET_SOURCE_CSITE);
	writel(3<<30, clk_rst + CLK_RESET_SOURCE_CSITE);

	tegra_sctx.cpu_burst = readl(clk_rst + CLK_RESET_CCLK_BURST);
	tegra_sctx.pllx_base = readl(clk_rst + CLK_RESET_PLLX_BASE);
	tegra_sctx.pllx_misc = readl(clk_rst + CLK_RESET_PLLX_MISC);
	tegra_sctx.pllp_base = readl(clk_rst + CLK_RESET_PLLP_BASE);
	tegra_sctx.pllp_outa = readl(clk_rst + CLK_RESET_PLLP_OUTA);
	tegra_sctx.pllp_outb = readl(clk_rst + CLK_RESET_PLLP_OUTB);
	tegra_sctx.pllp_misc = readl(clk_rst + CLK_RESET_PLLP_MISC);
	tegra_sctx.cclk_divider = readl(clk_rst + CLK_RESET_CCLK_DIVIDER);

#ifdef CONFIG_HAVE_ARM_TWD
	tegra_sctx.twd_ctrl = readl(twd_base + 0x8);
	tegra_sctx.twd_load = readl(twd_base + 0);
	local_timer_stop();
#endif

	reg = readl(FLOW_CTRL_CPUx_CSR(0));
	reg |= (1 << 15) | (1 << 14);	/* write to clear: INTR_FLAG|EVENT_FLAG */
	reg &= ~FLOW_CTRL_BITMAP_MASK;	/* WFE/WFI bit maps*/
	/* Set the flow controller bitmap to specify just CPU0. */
	reg |= FLOW_CTRL_BITMAP_CPU0 | 1; /* CPU0 bitmap | power-gate enable */
	flowctrl_writel(reg, FLOW_CTRL_CPUx_CSR(0));

	for (i = 1; i < num_present_cpus(); i++) {
		reg = readl(FLOW_CTRL_CPUx_CSR(i));
		/* write to clear: EVENT_FLAG | INTR_FLAG*/
		reg |= (1<<15) | (1<<14);
		flowctrl_writel(reg, FLOW_CTRL_CPUx_CSR(i));
	}

	gic_cpu_exit(0);
	gic_dist_save(0);
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
	/* tegra3 enters lpx states via WFI - do not propagate legacy IRQs
	   to CPU core to avoid fall through WFI (IRQ-to-flow controller wake
	   path is not affected) */
	tegra_irq_pass_through_disable();
#endif
}

unsigned int tegra_suspend_lp2(unsigned int us, unsigned int flags)
{
	unsigned int mode;
	unsigned long reg;
	unsigned int remain;
	unsigned int cpu = cpu_number();

	reg = readl(pmc + PMC_CTRL);
	mode = (reg >> TEGRA_POWER_PMC_SHIFT) & TEGRA_POWER_PMC_MASK;
	mode |= flags;
	mode |= TEGRA_POWER_CPU_PWRREQ_OE;
	if (pdata->separate_req)
		mode |= TEGRA_POWER_PWRREQ_OE;
	else
		mode &= ~TEGRA_POWER_PWRREQ_OE;
	mode &= ~TEGRA_POWER_EFFECT_LP0;

	/* pr_info_ratelimited("CPU %d entering LP2\n", cpu); */
	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_start);

	if (flags & TEGRA_POWER_CLUSTER_MASK) {
		/* power_off time is overlapped with LP mode residency */
		set_power_timers(pdata->cpu_timer, 0,
				 clk_get_rate_all_locked(tegra_pclk));
		tegra_cluster_switch_prolog(mode);
	} else {
		set_power_timers(pdata->cpu_timer, pdata->cpu_off_timer,
				 clk_get_rate_all_locked(tegra_pclk));
	}

	if (us)
		tegra_lp2_set_trigger(us);

	suspend_cpu_complex();
	stop_critical_timings();
	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_prolog);

	spin_lock(&lp2_map_lock);
	tegra_cpu_lp2_map |= (1 << cpu);
	spin_unlock(&lp2_map_lock);

	flush_cache_all();
	/* structure is read by reset code, so the L2 lines
	 * must be invalidated */
	outer_flush_range(__pa(&tegra_sctx), __pa(&tegra_sctx+1));
	tegra_cpu_reset_handler_flush(false);
	barrier();

#ifdef CONFIG_TRUSTED_FOUNDATIONS
	{
		extern void __tegra_cpu_reset_handler(void);
		extern void __tegra_cpu_reset_handler_start(void);
		/* TRUSTED LOGIC SMC_STOP/Save State */
		callGenericSMC(0xFFFFFFFC, 0xFFFFFFE4,
			TEGRA_RESET_HANDLER_BASE +
			(__tegra_cpu_reset_handler -
			 __tegra_cpu_reset_handler_start));
	}
#endif

	__cortex_a9_save(mode);
	/* return from __cortex_a9_restore */
	barrier();
	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_switch);
	if (suspend_wfi_failed()) {
		tegra_wfi_fail_count[cpu]++;
		pr_err_ratelimited("WFI for LP2 failed for CPU %d: count %lu\n",
				    cpu, tegra_wfi_fail_count[cpu]);
	}

	restore_cpu_complex();
	start_critical_timings();

	spin_lock(&lp2_map_lock);
	tegra_cpu_lp2_map &= ~(1 << cpu);
	spin_unlock(&lp2_map_lock);

	remain = tegra_lp2_timer_remain();
	if (us)
		tegra_lp2_set_trigger(0);

	if (flags & TEGRA_POWER_CLUSTER_MASK)
		tegra_cluster_switch_epilog(mode);

	tegra_cluster_switch_time(flags, tegra_cluster_switch_time_id_epilog);
	/* pr_info_ratelimited("CPU %d return from LP2\n", cpu); */

#if INSTRUMENT_CLUSTER_SWITCH
	if (flags & TEGRA_POWER_CLUSTER_MASK) {
		pr_err("%s: prolog %lu us, switch %lu us, epilog %lu us, total %lu us\n",
		       is_lp_cluster() ? "G=>LP" : "LP=>G",
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_prolog] -
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_start],
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_switch] -
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_prolog],
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_epilog] -
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_switch],
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_epilog] -
		       tegra_cluster_switch_times[tegra_cluster_switch_time_id_start]);
	}
#endif

	return remain;
}
#endif

#ifdef CONFIG_PM
/* ensures that sufficient time is passed for a register write to
 * serialize into the 32KHz domain */
static void pmc_32kwritel(u32 val, unsigned long offs)
{
	writel(val, pmc + offs);
	udelay(130);
}

static u8 *iram_save = NULL;
static unsigned int iram_save_size = 0;
static void __iomem *iram_code = IO_ADDRESS(TEGRA_IRAM_CODE_AREA);

static void tegra_suspend_check_pwr_stats(void)
{
	/* cpus and l2 are powered off later */
	unsigned long pwrgate_partid_mask =
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC)
		(1 << TEGRA_POWERGATE_HEG)	|
		(1 << TEGRA_POWERGATE_SATA)	|
		(1 << TEGRA_POWERGATE_3D1)	|
#endif
		(1 << TEGRA_POWERGATE_3D)	|
		(1 << TEGRA_POWERGATE_VENC)	|
		(1 << TEGRA_POWERGATE_PCIE)	|
		(1 << TEGRA_POWERGATE_VDEC)	|
		(1 << TEGRA_POWERGATE_MPE);

	int partid;

	for (partid = 0; partid < TEGRA_NUM_POWERGATE; partid++)
		if ((1 << partid) & pwrgate_partid_mask)
			if (tegra_powergate_is_powered(partid))
				pr_warning("partition %s is left on before suspend\n",
					tegra_powergate_get_name(partid));

	return;
}

void tegra_suspend_dram(bool do_lp0)
{
	unsigned int mode = TEGRA_POWER_SDRAM_SELFREFRESH;
	unsigned long reg;
	unsigned int cpu;

	tegra_suspend_check_pwr_stats();

	/* copy the reset vector and SDRAM shutdown code into IRAM */
	memcpy(iram_save, iram_code, iram_save_size);
	memcpy(iram_code, (void *)__tegra_lp1_reset, iram_save_size);

	set_power_timers(pdata->cpu_timer, pdata->cpu_off_timer, 32768);

	reg = readl(pmc + PMC_CTRL);
	mode |= ((reg >> TEGRA_POWER_PMC_SHIFT) & TEGRA_POWER_PMC_MASK);

	if (pdata && pdata->board_suspend) {
		int lp_state = (do_lp0) ? 0 : 1;
		pdata->board_suspend(lp_state, TEGRA_SUSPEND_BEFORE_CPU);
	}

	if (!do_lp0) {
		cpu = cpu_number();

		mode |= TEGRA_POWER_CPU_PWRREQ_OE;
		if (pdata->separate_req)
			mode |= TEGRA_POWER_PWRREQ_OE;
		else
			mode &= ~TEGRA_POWER_PWRREQ_OE;
		mode &= ~TEGRA_POWER_EFFECT_LP0;

		tegra_legacy_irq_set_lp1_wake_mask();
	} else {
		u32 boot_flag = readl(pmc + PMC_SCRATCH0);
		pmc_32kwritel(boot_flag | 1, PMC_SCRATCH0);
		pmc_32kwritel(wb0_restore, PMC_SCRATCH1);
		writel(0x0, pmc + PMC_SCRATCH39);
		mode |= TEGRA_POWER_CPU_PWRREQ_OE;
		mode |= TEGRA_POWER_PWRREQ_OE;
		mode |= TEGRA_POWER_EFFECT_LP0;

		/* for platforms where the core & CPU power requests are
		 * combined as a single request to the PMU, transition to
		 * LP0 state by temporarily enabling both requests
		 */
		if (!pdata->separate_req) {
			reg |= ((mode & TEGRA_POWER_PMC_MASK) <<
				TEGRA_POWER_PMC_SHIFT);
			pmc_32kwritel(reg, PMC_CTRL);
			mode &= ~TEGRA_POWER_CPU_PWRREQ_OE;
		}

		tegra_set_lp0_wake_pads(pdata->wake_enb, pdata->wake_high,
			pdata->wake_any);
		tegra_lp0_suspend_mc();
	}

	suspend_cpu_complex();
	if (!do_lp0)
		tegra_cpu_lp1_map = 1;
	else {
#ifndef CONFIG_ARCH_TEGRA_2x_SOC
		reg = readl(pmc + PMC_SCRATCH4);
		if (is_lp_cluster())
			reg |= PMC_SCRATCH4_WAKE_CLUSTER_MASK;
		else
			reg &= (~PMC_SCRATCH4_WAKE_CLUSTER_MASK);
		pmc_32kwritel(reg, PMC_SCRATCH4);
#endif
		tegra_cpu_reset_handler_save();
	}

	flush_cache_all();
	tegra_cpu_reset_handler_flush(false);
#ifdef CONFIG_CACHE_L2X0
	l2x0_shutdown();
#endif

#ifdef CONFIG_TRUSTED_FOUNDATIONS
/* TRUSTED LOGIC SMC_STOP/Save State */
	if (!do_lp0) {
		extern void __tegra_cpu_reset_handler(void);
		extern void __tegra_cpu_reset_handler_start(void);
		callGenericSMC(0xFFFFFFFC, 0xFFFFFFE6,
				TEGRA_RESET_HANDLER_BASE +
				(__tegra_cpu_reset_handler -
				 __tegra_cpu_reset_handler_start));
	} else {
	callGenericSMC(0xFFFFFFFC, 0xFFFFFFE3, virt_to_phys(tegra_lp2_startup));
	}
#endif

	__cortex_a9_save(mode);

	if (!do_lp0) {
		tegra_cpu_lp1_map = 0;
		if (suspend_wfi_failed()) {
			tegra_wfi_fail_count[cpu]++;
			pr_err_ratelimited("WFI for LP1 failed for CPU %d: count %lu\n",
					    cpu, tegra_wfi_fail_count[cpu]);
		}
	} else {
		tegra_cpu_reset_handler_enable();
		tegra_cpu_reset_handler_restore();
		tegra_lp0_resume_mc();
	}

	restore_cpu_complex();

#ifdef CONFIG_CACHE_L2X0
	__cortex_a9_l2x0_reenable();
#endif

	if (!do_lp0) {
		memcpy(iram_code, iram_save, iram_save_size);
		tegra_legacy_irq_restore_mask();
	} else {
		/* for platforms where the core & CPU power requests are
		 * combined as a single request to the PMU, transition out
		 * of LP0 state by temporarily enabling both requests
		 */
		if (!pdata->separate_req) {
			reg = readl(pmc + PMC_CTRL);
			reg |= (TEGRA_POWER_CPU_PWRREQ_OE <<
							TEGRA_POWER_PMC_SHIFT);
			pmc_32kwritel(reg, PMC_CTRL);
			reg &= ~(TEGRA_POWER_PWRREQ_OE <<
							TEGRA_POWER_PMC_SHIFT);
			writel(reg, pmc + PMC_CTRL);
		}
	}

	wmb();
}

static int tegra_suspend_begin(suspend_state_t state)
{
	return regulator_suspend_prepare(state);
}

static int tegra_suspend_prepare_late(void)
{
	disable_irq(INT_SYS_STATS_MON);
	return tegra_iovmm_suspend();
}

static void tegra_suspend_wake(void)
{
	tegra_iovmm_resume();
	enable_irq(INT_SYS_STATS_MON);
}

static u8 uart_state[5];
unsigned long debug_uart_port_base = 0;
EXPORT_SYMBOL(debug_uart_port_base);

static int tegra_debug_uart_suspend(void)
{
	void __iomem *uart;
	u32 lcr;

	if (!debug_uart_port_base)
		return 0;

	uart = IO_ADDRESS(debug_uart_port_base);

	lcr = readb(uart + UART_LCR * 4);

	uart_state[0] = lcr;
	uart_state[1] = readb(uart + UART_MCR * 4);

	/* DLAB = 0 */
	writeb(lcr & ~UART_LCR_DLAB, uart + UART_LCR * 4);

	uart_state[2] = readb(uart + UART_IER * 4);

	/* DLAB = 1 */
	writeb(lcr | UART_LCR_DLAB, uart + UART_LCR * 4);

	uart_state[3] = readb(uart + UART_DLL * 4);
	uart_state[4] = readb(uart + UART_DLM * 4);

	writeb(lcr, uart + UART_LCR * 4);

	return 0;
}

static void tegra_debug_uart_resume(void)
{
	void __iomem *uart;
	u32 lcr;

	if (!debug_uart_port_base)
		return;

	uart = IO_ADDRESS(debug_uart_port_base);

	lcr = uart_state[0];

	writeb(uart_state[1], uart + UART_MCR * 4);

	/* DLAB = 0 */
	writeb(lcr & ~UART_LCR_DLAB, uart + UART_LCR * 4);

	writeb(UART_FCR_ENABLE_FIFO | UART_FCR_T_TRIG_01 | UART_FCR_R_TRIG_01,
			uart + UART_FCR * 4);
	writeb(uart_state[2], uart + UART_IER * 4);

	/* DLAB = 1 */
	writeb(lcr | UART_LCR_DLAB, uart + UART_LCR * 4);

	writeb(uart_state[3], uart + UART_DLL * 4);
	writeb(uart_state[4], uart + UART_DLM * 4);

	writeb(lcr, uart + UART_LCR * 4);
}

struct clk *debug_uart_clk = NULL;
EXPORT_SYMBOL(debug_uart_clk);

void tegra_console_uart_suspend(void)
{
	if (console_suspend_enabled && debug_uart_clk)
		clk_disable(debug_uart_clk);
}

void tegra_console_uart_resume(void)
{
	if (console_suspend_enabled && debug_uart_clk)
		clk_enable(debug_uart_clk);
}

#define MC_SECURITY_START	0x6c
#define MC_SECURITY_SIZE	0x70
#define MC_SECURITY_CFG2	0x7c

#define AHB_ARBITRATION_DISABLE		0x00
#define AHB_ARBITRATION_PRIORITY_CTRL	0x04
#define AHB_GIZMO_AHB_MEM		0x0c
#define AHB_GIZMO_APB_DMA		0x10
#define AHB_GIZMO_IDE			0x18
#define AHB_GIZMO_USB			0x1c
#define AHB_GIZMO_AHB_XBAR_BRIDGE	0x20
#define AHB_GIZMO_CPU_AHB_BRIDGE	0x24
#define AHB_GIZMO_COP_AHB_BRIDGE	0x28
#define AHB_GIZMO_XBAR_APB_CTLR		0x2c
#define AHB_GIZMO_VCP_AHB_BRIDGE	0x30
#define AHB_GIZMO_NAND			0x3c
#define AHB_GIZMO_SDMMC4		0x44
#define AHB_GIZMO_XIO			0x48
#define AHB_GIZMO_BSEV			0x60
#define AHB_GIZMO_BSEA			0x70
#define AHB_GIZMO_NOR			0x74
#define AHB_GIZMO_USB2			0x78
#define AHB_GIZMO_USB3			0x7c
#define AHB_GIZMO_SDMMC1		0x80
#define AHB_GIZMO_SDMMC2		0x84
#define AHB_GIZMO_SDMMC3		0x88
#define AHB_MEM_PREFETCH_CFG_X		0xd8
#define AHB_ARBITRATION_XBAR_CTRL	0xdc
#define AHB_MEM_PREFETCH_CFG3		0xe0
#define AHB_MEM_PREFETCH_CFG4		0xe4
#define AHB_MEM_PREFETCH_CFG1		0xec
#define AHB_MEM_PREFETCH_CFG2		0xf0
#define AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID	0xf8

static inline unsigned long gizmo_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static inline void gizmo_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_AHB_GIZMO_BASE + offset));
}

static u32 ahb_gizmo[29];

void tegra_ahbgizmo_suspend(void)
{
	ahb_gizmo[0] = gizmo_readl(AHB_ARBITRATION_DISABLE);
	ahb_gizmo[1] = gizmo_readl(AHB_ARBITRATION_PRIORITY_CTRL);
	ahb_gizmo[2] = gizmo_readl(AHB_GIZMO_AHB_MEM);
	ahb_gizmo[3] = gizmo_readl(AHB_GIZMO_APB_DMA);
	ahb_gizmo[4] = gizmo_readl(AHB_GIZMO_IDE);
	ahb_gizmo[5] = gizmo_readl(AHB_GIZMO_USB);
	ahb_gizmo[6] = gizmo_readl(AHB_GIZMO_AHB_XBAR_BRIDGE);
	ahb_gizmo[7] = gizmo_readl(AHB_GIZMO_CPU_AHB_BRIDGE);
	ahb_gizmo[8] = gizmo_readl(AHB_GIZMO_COP_AHB_BRIDGE);
	ahb_gizmo[9] = gizmo_readl(AHB_GIZMO_XBAR_APB_CTLR);
	ahb_gizmo[10] = gizmo_readl(AHB_GIZMO_VCP_AHB_BRIDGE);
	ahb_gizmo[11] = gizmo_readl(AHB_GIZMO_NAND);
	ahb_gizmo[12] = gizmo_readl(AHB_GIZMO_SDMMC4);
	ahb_gizmo[13] = gizmo_readl(AHB_GIZMO_XIO);
	ahb_gizmo[14] = gizmo_readl(AHB_GIZMO_BSEV);
	ahb_gizmo[15] = gizmo_readl(AHB_GIZMO_BSEA);
	ahb_gizmo[16] = gizmo_readl(AHB_GIZMO_NOR);
	ahb_gizmo[17] = gizmo_readl(AHB_GIZMO_USB2);
	ahb_gizmo[18] = gizmo_readl(AHB_GIZMO_USB3);
	ahb_gizmo[19] = gizmo_readl(AHB_GIZMO_SDMMC1);
	ahb_gizmo[20] = gizmo_readl(AHB_GIZMO_SDMMC2);
	ahb_gizmo[21] = gizmo_readl(AHB_GIZMO_SDMMC3);
	ahb_gizmo[22] = gizmo_readl(AHB_MEM_PREFETCH_CFG_X);
	ahb_gizmo[23] = gizmo_readl(AHB_ARBITRATION_XBAR_CTRL);
	ahb_gizmo[24] = gizmo_readl(AHB_MEM_PREFETCH_CFG3);
	ahb_gizmo[25] = gizmo_readl(AHB_MEM_PREFETCH_CFG4);
	ahb_gizmo[26] = gizmo_readl(AHB_MEM_PREFETCH_CFG1);
	ahb_gizmo[27] = gizmo_readl(AHB_MEM_PREFETCH_CFG2);
	ahb_gizmo[28] = gizmo_readl(AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID);
}

void tegra_ahbgizmo_resume(void)
{
	gizmo_writel(ahb_gizmo[0], AHB_ARBITRATION_DISABLE);
	gizmo_writel(ahb_gizmo[1], AHB_ARBITRATION_PRIORITY_CTRL);
	gizmo_writel(ahb_gizmo[2], AHB_GIZMO_AHB_MEM);
	gizmo_writel(ahb_gizmo[3], AHB_GIZMO_APB_DMA);
	gizmo_writel(ahb_gizmo[4], AHB_GIZMO_IDE);
	gizmo_writel(ahb_gizmo[5], AHB_GIZMO_USB);
	gizmo_writel(ahb_gizmo[6], AHB_GIZMO_AHB_XBAR_BRIDGE);
	gizmo_writel(ahb_gizmo[7], AHB_GIZMO_CPU_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[8], AHB_GIZMO_COP_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[9], AHB_GIZMO_XBAR_APB_CTLR);
	gizmo_writel(ahb_gizmo[10],AHB_GIZMO_VCP_AHB_BRIDGE);
	gizmo_writel(ahb_gizmo[11],AHB_GIZMO_NAND);
	gizmo_writel(ahb_gizmo[12],AHB_GIZMO_SDMMC4);
	gizmo_writel(ahb_gizmo[13],AHB_GIZMO_XIO);
	gizmo_writel(ahb_gizmo[14],AHB_GIZMO_BSEV);
	gizmo_writel(ahb_gizmo[15],AHB_GIZMO_BSEA);
	gizmo_writel(ahb_gizmo[16],AHB_GIZMO_NOR);
	gizmo_writel(ahb_gizmo[17],AHB_GIZMO_USB2);
	gizmo_writel(ahb_gizmo[18],AHB_GIZMO_USB3);
	gizmo_writel(ahb_gizmo[19],AHB_GIZMO_SDMMC1);
	gizmo_writel(ahb_gizmo[20],AHB_GIZMO_SDMMC2);
	gizmo_writel(ahb_gizmo[21],AHB_GIZMO_SDMMC3);
	gizmo_writel(ahb_gizmo[22],AHB_MEM_PREFETCH_CFG_X);
	gizmo_writel(ahb_gizmo[23],AHB_ARBITRATION_XBAR_CTRL);
	gizmo_writel(ahb_gizmo[24],AHB_MEM_PREFETCH_CFG3);
	gizmo_writel(ahb_gizmo[25],AHB_MEM_PREFETCH_CFG4);
	gizmo_writel(ahb_gizmo[26],AHB_MEM_PREFETCH_CFG1);
	gizmo_writel(ahb_gizmo[27],AHB_MEM_PREFETCH_CFG2);
	gizmo_writel(ahb_gizmo[28],AHB_ARBITRATION_AHB_MEM_WRQUE_MST_ID);
}

static int tegra_suspend_enter(suspend_state_t state)
{
	struct irq_desc *desc;
	void __iomem *mc = IO_ADDRESS(TEGRA_MC_BASE);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	void __iomem *emc = IO_ADDRESS(TEGRA_EMC_BASE);
#endif
	unsigned long flags;
	u32 mc_data[3] = {0, 0, 0};
	int irq;
	bool do_lp0 = (current_suspend_mode == TEGRA_SUSPEND_LP0);
	bool do_lp2 = (current_suspend_mode == TEGRA_SUSPEND_LP2);
	int lp_state;
	u64 rtc_before;
	u64 rtc_after;
	u64 secs;
	u32 ms;

	if (do_lp2)
		lp_state = 2;
	else if (do_lp0)
		lp_state = 0;
	else
		lp_state = 1;

	local_irq_save(flags);
	local_fiq_disable();

	pr_info("Entering suspend state LP%d\n", lp_state);
	if (pdata && pdata->board_suspend)
		pdata->board_suspend(lp_state, TEGRA_SUSPEND_BEFORE_PERIPHERAL);
	if (do_lp0) {
		tegra_lp0_cpu_mode(true);
		tegra_irq_suspend();
		tegra_dma_suspend();
		tegra_ahbgizmo_suspend();
		tegra_debug_uart_suspend();
		tegra_pinmux_suspend();
		tegra_timer_suspend();
		tegra_gpio_suspend();
		tegra_clk_suspend();

		mc_data[0] = readl(mc + MC_SECURITY_START);
		mc_data[1] = readl(mc + MC_SECURITY_SIZE);
		mc_data[2] = readl(mc + MC_SECURITY_CFG2);
	}

	for_each_irq_desc(irq, desc) {
		if ((desc->status & IRQ_WAKEUP) &&
		    (desc->status & IRQ_SUSPENDED) &&
		    (get_irq_chip(irq)->unmask)) {
			get_irq_chip(irq)->unmask(irq);
		}
	}

	rtc_before = tegra_rtc_read_ms();

	if (do_lp2)
		tegra_suspend_lp2(0, 0);
	else
		tegra_suspend_dram(do_lp0);

	rtc_after = tegra_rtc_read_ms();

	for_each_irq_desc(irq, desc) {
		if ((desc->status & IRQ_WAKEUP) &&
		    (desc->status & IRQ_SUSPENDED) &&
		    (get_irq_chip(irq)->mask)) {
			get_irq_chip(irq)->mask(irq);
		}
	}

	/* Clear DPD sample */
	writel(0x0, pmc + PMC_DPD_SAMPLE);

	if (pdata && pdata->board_resume)
		pdata->board_resume(lp_state, TEGRA_RESUME_AFTER_CPU);

	if (do_lp0) {
		writel(mc_data[0], mc + MC_SECURITY_START);
		writel(mc_data[1], mc + MC_SECURITY_SIZE);
		writel(mc_data[2], mc + MC_SECURITY_CFG2);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
		/* trigger emc mode write */
		writel(EMC_MRW_DEV_NONE, emc + EMC_MRW_0);
#endif
		tegra_clk_resume();
		tegra_gpio_resume();
		tegra_timer_resume();
		tegra_pinmux_resume();
		tegra_debug_uart_resume();
		tegra_ahbgizmo_resume();
		tegra_dma_resume();
		tegra_irq_resume();
		tegra_lp0_cpu_mode(false);
	}
	if (pdata && pdata->board_resume)
		pdata->board_resume(lp_state, TEGRA_RESUME_AFTER_PERIPHERAL);

	secs = rtc_after - rtc_before;
	ms = do_div(secs, 1000);
	{
		ktime_t delta = ktime_set((s32)secs, ms * 1000000);
		tegra_dvfs_rail_pause(tegra_cpu_rail, delta, false);
		if (do_lp0)
			tegra_dvfs_rail_pause(tegra_core_rail, delta, false);
		else
			tegra_dvfs_rail_pause(tegra_core_rail, delta, true);
	}
	pr_info("Suspended for %llu.%03u seconds\n", secs, ms);

	tegra_time_in_suspend[time_to_bin(secs)]++;

	local_fiq_enable();
	local_irq_restore(flags);

	return 0;
}

/*
 * Function pointers to optional board specific function
 */
void (*tegra_deep_sleep)(int);
EXPORT_SYMBOL(tegra_deep_sleep);

static int tegra_suspend_prepare(void)
{
	if ((current_suspend_mode == TEGRA_SUSPEND_LP0) && tegra_deep_sleep)
		tegra_deep_sleep(1);
	return 0;
}

static void tegra_suspend_finish(void)
{
	if ((current_suspend_mode == TEGRA_SUSPEND_LP0) && tegra_deep_sleep)
		tegra_deep_sleep(0);
}

static struct platform_suspend_ops tegra_suspend_ops = {
	.valid		= suspend_valid_only_mem,
	.prepare	= tegra_suspend_prepare,
	.finish		= tegra_suspend_finish,
	.begin		= tegra_suspend_begin,
	.prepare_late	= tegra_suspend_prepare_late,
	.wake		= tegra_suspend_wake,
	.enter		= tegra_suspend_enter,
};

static ssize_t suspend_mode_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	char *start = buf;
	char *end = buf + PAGE_SIZE;

	start += scnprintf(start, end - start, "%s ", \
				tegra_suspend_name[current_suspend_mode]);
	start += scnprintf(start, end - start, "\n");

	return start - buf;
}

static ssize_t suspend_mode_store(struct kobject *kobj,
					struct kobj_attribute *attr,
						const char *buf, size_t n)
{
	int len;
	const char *name_ptr;
	enum tegra_suspend_mode new_mode;

	name_ptr = buf;
	while (*name_ptr && !isspace(*name_ptr))
		name_ptr++;
	len = name_ptr - buf;
	if (!len)
		goto bad_name;

	for (new_mode = TEGRA_SUSPEND_NONE;				\
				new_mode < TEGRA_MAX_SUSPEND_MODE; ++new_mode) {
		if (!strncmp(buf, tegra_suspend_name[new_mode], len)) {
			current_suspend_mode = new_mode;
			break;
		}
	}

bad_name:
	return n;
}

static struct kobj_attribute suspend_mode_attribute =
	__ATTR(mode, 0644, suspend_mode_show, suspend_mode_store);
#endif

void __init tegra_init_suspend(struct tegra_suspend_platform_data *plat)
{
	u32 reg, mode;

	tegra_cpu_rail = tegra_dvfs_get_rail_by_name("vdd_cpu");
	tegra_core_rail = tegra_dvfs_get_rail_by_name("vdd_core");
	tegra_pclk = clk_get_sys(NULL, "pclk");
	BUG_ON(!tegra_pclk);
	pdata = plat;
	(void)reg;
	(void)mode;

#ifdef CONFIG_PM

	if ((tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) &&
	    (tegra_get_revision() == TEGRA_REVISION_A01) &&
	    (plat->suspend_mode == TEGRA_SUSPEND_LP0)) {
		/* Tegra 3 A01 supports only LP1 */
		pr_warning("Suspend mode LP0 is not supported on A01\n");
		pr_warning("Disabling LP0\n");
		plat->suspend_mode = TEGRA_SUSPEND_LP1;
	}

	if (plat->suspend_mode == TEGRA_SUSPEND_LP0 && tegra_lp0_vec_size &&
		tegra_lp0_vec_relocate) {
		unsigned char *reloc_lp0;
		unsigned long tmp;
		void __iomem *orig;
		reloc_lp0 = kmalloc(tegra_lp0_vec_size + L1_CACHE_BYTES - 1,
					GFP_KERNEL);
		WARN_ON(!reloc_lp0);
		if (!reloc_lp0) {
			pr_err("%s: Failed to allocate reloc_lp0\n",
				__func__);
			goto out;
		}

		orig = ioremap(tegra_lp0_vec_start, tegra_lp0_vec_size);
		WARN_ON(!orig);
		if (!orig) {
			pr_err("%s: Failed to map tegra_lp0_vec_start %08lx\n",
				__func__, tegra_lp0_vec_start);
			kfree(reloc_lp0);
			goto out;
		}

		tmp = (unsigned long) reloc_lp0;
		tmp = (tmp + L1_CACHE_BYTES - 1) & ~(L1_CACHE_BYTES - 1);
		reloc_lp0 = (unsigned char *)tmp;
		memcpy(reloc_lp0, orig, tegra_lp0_vec_size);
		iounmap(orig);
		tegra_lp0_vec_start = virt_to_phys(reloc_lp0);
	}

out:
	if (plat->suspend_mode == TEGRA_SUSPEND_LP0) {
		if (tegra_lp0_vec_size)
			wb0_restore = tegra_lp0_vec_start;
		else {
			pr_warning("Suspend mode LP0 requested, but missing lp0_vec\n");
			pr_warning("Disabling LP0\n");
			plat->suspend_mode = TEGRA_SUSPEND_LP1;
		}
	}
#else
	if ((plat->suspend_mode == TEGRA_SUSPEND_LP0) ||
	    (plat->suspend_mode == TEGRA_SUSPEND_LP1)) {
		pr_warning("Suspend mode LP0 or LP1 requested, CONFIG_PM not set\n");
		pr_warning("Limiting to LP2\n");
		plat->suspend_mode = TEGRA_SUSPEND_LP2;
	}
#endif

#ifdef CONFIG_CPU_IDLE
	if (plat->suspend_mode == TEGRA_SUSPEND_NONE)
		tegra_lp2_in_idle(false);
#endif

	tegra_context_area = kzalloc(CONTEXT_SIZE_BYTES * NR_CPUS, GFP_KERNEL);

	if (tegra_context_area && create_suspend_pgtable()) {
		kfree(tegra_context_area);
		tegra_context_area = NULL;
	}

#ifdef CONFIG_PM
	iram_save_size = (unsigned long)__tegra_iram_end;
	iram_save_size -= (unsigned long)__tegra_lp1_reset;

	iram_save = kmalloc(iram_save_size, GFP_KERNEL);
	if (!iram_save) {
		pr_err("%s: unable to allocate memory for SDRAM self-refresh "
		       "LP0/LP1 unavailable\n", __func__);
		plat->suspend_mode = TEGRA_SUSPEND_LP2;
	}
	/* CPU reset vector for LP0 and LP1 */
	__raw_writel(virt_to_phys(tegra_lp2_startup), pmc + PMC_SCRATCH41);

	/* Always enable CPU power request; just normal polarity is supported */
	reg = readl(pmc + PMC_CTRL);
	BUG_ON(reg & (TEGRA_POWER_CPU_PWRREQ_POLARITY <<
						TEGRA_POWER_PMC_SHIFT));
	reg |= (TEGRA_POWER_CPU_PWRREQ_OE << TEGRA_POWER_PMC_SHIFT);
	pmc_32kwritel(reg, PMC_CTRL);

	/* Configure core power request and system clock control if LP0
	   is supported */
	__raw_writel(pdata->core_timer, pmc + PMC_COREPWRGOOD_TIMER);
	__raw_writel(pdata->core_off_timer, pmc + PMC_COREPWROFF_TIMER);
	reg = readl(pmc + PMC_CTRL);
	mode = (reg >> TEGRA_POWER_PMC_SHIFT) & TEGRA_POWER_PMC_MASK;

	mode &= ~TEGRA_POWER_SYSCLK_POLARITY;
	mode &= ~TEGRA_POWER_PWRREQ_POLARITY;

	if (!pdata->sysclkreq_high)
		mode |= TEGRA_POWER_SYSCLK_POLARITY;
	if (!pdata->corereq_high)
		mode |= TEGRA_POWER_PWRREQ_POLARITY;

	/* configure output inverters while the request is tristated */
	reg |= (mode << TEGRA_POWER_PMC_SHIFT);
	pmc_32kwritel(reg, PMC_CTRL);

	/* now enable requests */
	reg |= (TEGRA_POWER_SYSCLK_OE << TEGRA_POWER_PMC_SHIFT);
	if (pdata->separate_req)
		reg |= (TEGRA_POWER_PWRREQ_OE << TEGRA_POWER_PMC_SHIFT);
	__raw_writel(reg, pmc + PMC_CTRL);

	if (pdata->suspend_mode == TEGRA_SUSPEND_LP0)
		lp0_suspend_init();

	suspend_set_ops(&tegra_suspend_ops);

	/* Create /sys/power/suspend/type */
	suspend_kobj = kobject_create_and_add("suspend", power_kobj);
	if (suspend_kobj) {
		if (sysfs_create_file(suspend_kobj, \
						&suspend_mode_attribute.attr))
			pr_err("%s: sysfs_create_file suspend type failed!", \
								 __func__);
	}
#endif
	current_suspend_mode = plat->suspend_mode;
}


#ifdef CONFIG_DEBUG_FS
static int tegra_suspend_debug_show(struct seq_file *s, void *data)
{
	seq_printf(s, "%s\n", tegra_suspend_name[*(int *)s->private]);
	return 0;
}

static int tegra_suspend_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_suspend_debug_show, inode->i_private);
}

static int tegra_suspend_debug_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	int buf_size;
	int i;
	struct seq_file *s = file->private_data;
	enum tegra_suspend_mode *val = s->private;

	memset(buf, 0x00, sizeof(buf));
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	for (i = 0; i < TEGRA_MAX_SUSPEND_MODE; i++) {
		if (!strnicmp(buf, tegra_suspend_name[i],
		    strlen(tegra_suspend_name[i]))) {
			if (i > pdata->suspend_mode)
				return -EINVAL;
			*val = i;
			return count;
		}
	}

	return -EINVAL;
}

static const struct file_operations tegra_suspend_debug_fops = {
	.open		= tegra_suspend_debug_open,
	.write		= tegra_suspend_debug_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int tegra_suspend_time_debug_show(struct seq_file *s, void *data)
{
	int bin;
	seq_printf(s, "time (secs)  count\n");
	seq_printf(s, "------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (tegra_time_in_suspend[bin] == 0)
			continue;
		seq_printf(s, "%4d - %4d %4u\n",
			bin ? 1 << (bin - 1) : 0, 1 << bin,
			tegra_time_in_suspend[bin]);
	}
	return 0;
}

static int tegra_suspend_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, tegra_suspend_time_debug_show, NULL);
}

static const struct file_operations tegra_suspend_time_debug_fops = {
	.open		= tegra_suspend_time_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_suspend_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("suspend_mode", 0755, NULL,
		(void *)&current_suspend_mode, &tegra_suspend_debug_fops);
	if (!d) {
		pr_info("Failed to create suspend_mode debug file\n");
		return -ENOMEM;
	}

	d = debugfs_create_file("suspend_time", 0755, NULL, NULL,
		&tegra_suspend_time_debug_fops);
	if (!d) {
		pr_info("Failed to create suspend_time debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(tegra_suspend_debug_init);
#endif
