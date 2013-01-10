/*
 *	linux/drivers/rtc/rtc-nspire.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

#define RTC_CURR	0x00
#define RTC_ALRM	0x04
#define RTC_SET		0x08
#define RTC_INTMSK	0x0C
#define RTC_INTSTS	0x10
#define RTC_STS		0x14

struct nspire_rtc_pdata {
	void __iomem *iobase;
	int irq;

	struct rtc_device *rtc;
	unsigned long alarm;
	unsigned char alarm_enabled:1;
};

static int nspire_rtc_read_time(struct device *dev, struct rtc_time *tm) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);
	unsigned long rtc_time = readl(pdata->iobase + RTC_CURR);

	/* dev_info(&pdev->dev, "read time as %lu\n", rtc_time); */
	rtc_time_to_tm(rtc_time, tm);

	return 0;
}

static int nspire_rtc_set_time(struct device *dev, struct rtc_time *tm) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);
	unsigned long rtc_time;

	rtc_tm_to_time(tm, &rtc_time);
	/* dev_info(&pdev->dev, "set time to %lu\n", rtc_time); */
	writel(rtc_time, pdata->iobase + RTC_SET);

	return 0;
}

static int nspire_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *a) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);
	unsigned long rtc_time = pdata->alarm;

	rtc_time_to_tm(rtc_time, &a->time);
	a->pending = readl(pdata->iobase + RTC_INTSTS);

	return 0;
}

static int nspire_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *a) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);
	unsigned long rtc_time;

	rtc_tm_to_time(&a->time, &rtc_time);
	pdata->alarm = rtc_time;

	return 0;
}

static int nspire_rtc_alarm_enable(struct device *dev, unsigned int enabled) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);

	/* Acknowledge existing interrupts */
	writel(1, pdata->iobase + RTC_INTSTS);

	if (!enabled) {
		/* Write a value that is "impossible" to reach */
		writel(-1, pdata->iobase + RTC_ALRM);
		pdata->alarm_enabled = 0;
	} else {
		writel(pdata->alarm, pdata->iobase + RTC_ALRM);
		pdata->alarm_enabled = 1;
	}

	return 0;
}

static void nspire_rtc_release(struct device *dev) {
	struct platform_device *pdev = to_platform_device(dev);
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);

	pdata->alarm_enabled = 0;
	/* Disable alarm by writing impossible value */
	writel(-1, pdata->iobase + RTC_ALRM);
	/* Acknowledge existing interrupts */
	writel(1, pdata->iobase + RTC_INTSTS);
}


static struct rtc_class_ops nspire_rtc_ops = {
	.read_time 	= nspire_rtc_read_time,
	.set_time	= nspire_rtc_set_time,
	.read_alarm	= nspire_rtc_read_alarm,
	.set_alarm	= nspire_rtc_set_alarm,
	.alarm_irq_enable = nspire_rtc_alarm_enable,
	.release	= nspire_rtc_release
};

static irqreturn_t nspire_rtc_irq(int irq, void *dev_id) {
	struct nspire_rtc_pdata *pdata = dev_id;

	/* Acknowledge */
	writel(1, pdata->iobase + RTC_INTSTS);
	if (!pdata->alarm_enabled)
		rtc_update_irq(pdata->rtc, 1, (RTC_IRQF | RTC_AF));

	return IRQ_HANDLED;
}

static int nspire_rtc_probe(struct platform_device *pdev)
{
	int irq;
	struct resource *res;
	struct nspire_rtc_pdata *pdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!res)
		return -ENODEV;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	if (!devm_request_mem_region(&pdev->dev, res->start,
				     resource_size(res), pdev->name))
		return -EBUSY;

	pdata->iobase = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));

	/* Interrupt mask needs to be always on for the clock to tick */
	writel(1, pdata->iobase + RTC_INTMSK);
	/* Acknowledge existing interrupts */
	writel(1, pdata->iobase + RTC_INTSTS);

	platform_set_drvdata(pdev, pdata);

	if (irq >= 0 && !devm_request_irq(&pdev->dev, irq, nspire_rtc_irq,
		IRQF_SHARED, pdev->name, pdata)) {
		pdata->irq = irq;
	} else {
		dev_warn(&pdev->dev, "No interrupts for RTC.\n");
		pdata->irq = -1;
	}

	pdata->rtc = rtc_device_register(pdev->name, &pdev->dev,
		&nspire_rtc_ops, THIS_MODULE);

	if (IS_ERR(pdata->rtc)) {
		platform_set_drvdata(pdev, NULL);
		return PTR_ERR(pdata->rtc);
	}

	return 0;
}

static int nspire_rtc_remove(struct platform_device *pdev)
{
	struct nspire_rtc_pdata *pdata = platform_get_drvdata(pdev);

	rtc_device_unregister(pdata->rtc);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver nspire_rtc_driver = {
	.driver = {
		.name = "nspire-rtc",
		.owner = THIS_MODULE,
	},
	.probe	 = nspire_rtc_probe,
	.remove	 = nspire_rtc_remove,
};

module_platform_driver(nspire_rtc_driver)

MODULE_DESCRIPTION("RTC driver for TI-NSPIRE calculators");
MODULE_LICENSE("GPL");
