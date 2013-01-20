/*
 *	linux/drivers/cpufreq/nspire-cpufreq.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <mach/nspire_clock.h>
#include <mach/irqs.h>

/* This whole driver is a NOP - basically for reporting frequency only */
/* TODO: actually implement frequency scaling */

static struct nspire_clk_divisors safe_divisors[] = {
	{ .base_cpu = 2,	.cpu_ahb = 2 },
	{ .base_cpu = 4,	.cpu_ahb = 1 },
};

struct clk *cpu_clk;
atomic_t num_cpus;

static struct cpufreq_frequency_table freq_table[ARRAY_SIZE(safe_divisors)+1];

static inline unsigned long nspire_get_cpufreq(void)
{
	struct nspire_clk_speeds clks = nspire_get_clocks();

	return CLK_GET_CPU(&clks) / 1000;
}

static int nspire_cpu_verify(struct cpufreq_policy *policy)
{
	return cpufreq_frequency_table_verify(policy, freq_table);
}

static int nspire_cpu_percpu_init(struct cpufreq_policy *policy)
{
	/* Not expecting more than 1 CPU */
	WARN_ON(atomic_add_return(1, &num_cpus) > 1);

	cpufreq_frequency_table_cpuinfo(policy, freq_table);
	policy->cur = nspire_get_cpufreq();
	policy->cpuinfo.transition_latency = 1000;

	cpumask_setall(policy->cpus);

	return 0;
}

static int nspire_cpu_target(struct cpufreq_policy *policy,
		unsigned int target_freq, unsigned int relation)
{
	int ret, index, cpu;
	struct nspire_clk_speeds clks = nspire_get_clocks();
	struct cpufreq_freqs freqs;

	ret = cpufreq_frequency_table_target(policy, freq_table,
			target_freq, relation, &index);
	if (ret)
		return ret;

	freqs.old = CLK_GET_CPU(&clks) / 1000;
	clks.div = safe_divisors[freq_table[index].index];
	freqs.new = CLK_GET_CPU(&clks) / 1000;

	if (freqs.old == freqs.new)
		return 0;

	for_each_online_cpu(cpu) {
		freqs.cpu = cpu;
		cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);
	}

	nspire_set_clocks(&clks);

	for_each_online_cpu(cpu) {
		freqs.cpu = cpu;
		cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);
	}

	pr_info("CPU scaled: BASE = %luHz, CPU = %luHz, AHB = %luHz. "
		"Wrote %#08lX.\n", clks.base, CLK_GET_CPU(&clks),
		CLK_GET_AHB(&clks), nspire_clocks_to_io(&clks));

	return 0;
}

static struct cpufreq_driver nspire_cpufreq_driver = {
	.name		= "nspire_cpufreq",
	.init		= nspire_cpu_percpu_init,
	.verify		= nspire_cpu_verify,
	.target		= nspire_cpu_target,
};

static irqreturn_t clockspeed_interrupt(int irq, void *dummy)
{
	if (!(readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x14)) & (1<<1)))
		return IRQ_NONE;

	/* Clear interrupts */
	writel((1<<1), NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x14));

	return IRQ_HANDLED;
}

static int __init nspire_cpufreq_init(void)
{
	struct nspire_clk_speeds clks = nspire_get_clocks();
	unsigned long base_freq = clks.base;
	int i;

	/* We need a dummy void* priv for shared IRQs */
	if (request_irq(NSPIRE_IRQ_PWR, clockspeed_interrupt, IRQF_SHARED,
			"clockspeed", (void *)0xdeadbeef)) {
		pr_warn("Unable to get IRQ_PWR to ack clockspeed changes. "
			"You may get IRQ disabled problems\n");
	}

	for (i = 0; i < ARRAY_SIZE(safe_divisors); i++) {
		freq_table[i].index = i;
		freq_table[i].frequency =
				(base_freq / 1000) / safe_divisors[i].base_cpu;
	}
	freq_table[i].frequency = CPUFREQ_TABLE_END;

	if (cpufreq_register_driver(&nspire_cpufreq_driver)) {
		pr_err("Failed to register CPUFreq driver");
		return -EINVAL;
	}

	return 0;
}

late_initcall(nspire_cpufreq_init);
