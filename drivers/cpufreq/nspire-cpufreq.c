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

#include <mach/nspire_clock.h>

/* This whole driver is a NOP - basically for reporting frequency only */
/* TODO: actually implement frequency scaling */

struct clk *cpu_clk;
atomic_t num_cpus;

static inline unsigned long nspire_get_cpufreq(void)
{
	/* There is only one */
	return clk_get_rate(cpu_clk) / 1000;
}

static int nspire_cpu_verify(struct cpufreq_policy *policy)
{
	unsigned long freq = nspire_get_cpufreq();

	/* Only one frequency allowed at the moment */
	cpufreq_verify_within_limits(policy, freq, freq);
	return 0;
}

static int nspire_cpu_percpu_init(struct cpufreq_policy *policy)
{
	/* Not expecting more than 1 CPU */
	WARN_ON(atomic_add_return(1, &num_cpus) > 1);

	/* One frequency */
	policy->cur = policy->min = policy->max = policy->cpuinfo.max_freq =
		policy->cpuinfo.min_freq = nspire_get_cpufreq();

	cpumask_setall(policy->cpus);
	return 0;
}

static int nspire_cpu_target(struct cpufreq_policy *policy,
		unsigned int target_freq, unsigned int relation)
{
	if (target_freq != nspire_get_cpufreq())
		return -EINVAL;

	return 0;
}

static struct cpufreq_driver nspire_cpufreq_driver = {
	.name		= "nspire_cpufreq",
	.init		= nspire_cpu_percpu_init,
	.verify		= nspire_cpu_verify,
	.target		= nspire_cpu_target,
};

static int __init nspire_cpufreq_init(void)
{
	cpu_clk = clk_get_sys("cpu", NULL);
	if (IS_ERR(cpu_clk)) {
		pr_err("cpu clock not found");
		return -ENOENT;
	}

	if (cpufreq_register_driver(&nspire_cpufreq_driver)) {
		pr_err("Failed to register CPUFreq driver");
		goto error;
	}
	return 0;
error:
	clk_put(cpu_clk);
	return -EINVAL;
}

late_initcall(nspire_cpufreq_init);
