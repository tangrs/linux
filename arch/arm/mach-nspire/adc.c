/*
 *  linux/arch/arm/mach-nspire/adc.c
 *
 *  Copyright (C) 2012 Fabian Vogt <fabian@ritter-vogt.de> et al.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>

#include <mach/nspire_mmio.h>
#include <asm/mach-types.h>

/*The count is defined here, but you will have to change the output manually*/
#define NR_ADC_CHANNELS 7

static struct proc_dir_entry *adc_proc_entry;
uint32_t adc[NR_ADC_CHANNELS];
uint64_t last_refresh;

static int adc_read(char *buf, char **data, off_t offset,
			 int len, int *eof, void *privdata)
{
	uint8_t channel;
	/*Refresh only every 100ms; that's enough*/
	if (last_refresh + HZ/10 < jiffies || last_refresh == 0) {
		void __iomem *chnl_base = IOMEM(NSPIRE_ADC_VIRT_BASE + 0x100);
		/*Start measurement*/
		for (channel = 0; channel < NR_ADC_CHANNELS; channel++)
			writel(1, chnl_base + (0x20*channel));

		/* Wait a millisecond, better then waiting until it
		 * finishes, because we could wait forever, if something is not
		 * correctly set up
		 */
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ/1000);

		last_refresh = jiffies;

		for (channel = 0; channel < NR_ADC_CHANNELS; channel++)
			adc[channel] = readl(chnl_base + 0x10 + (0x20*channel));
	}

	len = snprintf(buf, PAGE_SIZE, "ADC0(LBAT)	%d\n"
					"ADC1(VBATT)	%d\n"
					"ADC2(VSYS)	%d\n"
					"ADC3(VKBID)	%d\n"
					"ADC4(VB12)	%d\n"
					"ADC5(VBUS)	%d\n"
					"ADC6(VSLED)	%d\n",
		adc[0], adc[1],	adc[2], adc[3],	adc[4], adc[5],	adc[6]);

	if (offset >= len) {
		*eof = 1;
		return 0;
	}
	memmove(buf, buf+offset, len-offset);
	return len-offset;
}

void __init adc_init(void)
{
	/*Enable ADC bus access*/
	uint32_t bus = readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x18));
	writel(bus & ~(1 << 4), NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x18));

	/*Nobody knows what this does, but it's needed*/
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x20));
}

int __init adc_procfs_init(void)
{
	const char *name = "adc";

	adc_proc_entry = create_proc_entry(name, 0444, NULL);
	if (!adc_proc_entry) {
		pr_alert("Error: Could not initialize /proc/%s\n", name);
		return -ENOMEM;
	}

	adc_proc_entry->read_proc = adc_read;
	pr_info("ADC voltages mapped to /proc/%s\n", name);
	return 0;
}
