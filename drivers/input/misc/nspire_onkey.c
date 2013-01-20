/*
 *	linux/drivers/input/misc/nspire_onkey.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#include <mach/nspire_mmio.h>

static struct input_dev *onkey_dev;
static struct delayed_work polling_work;

static void onkey_update(void)
{
	unsigned status = readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x28));
	status = !(status & (1<<4));

	input_report_key(onkey_dev, KEY_POWER, status);
	input_sync(onkey_dev);

	if (status)
		schedule_delayed_work(&polling_work, msecs_to_jiffies(10));
}

static void onkey_poll(struct work_struct *work __attribute__((unused)))
{
	onkey_update();
}

static irqreturn_t onkey_interrupt(int irq, void *dummy)
{
	if (!(readl(NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x14)) & (1<<0)))
		return IRQ_NONE;

	/* Clear interrupts */
	writel((1<<0), NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x14));

	onkey_update();

	return IRQ_HANDLED;
}

static int __init onkey_init(void)
{
	int err;

	/* Enable interrupts for power button */
	writel((1<<0), NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x10));

	/* Clear existing interrupts */
	writel((1<<0), NSPIRE_APB_VIRTIO(NSPIRE_APB_POWER + 0x14));

	onkey_dev = input_allocate_device();
	if (!onkey_dev) {
		pr_err("Unable to allocate input device for power button\n");
		return -ENOMEM;
	}

	onkey_dev->evbit[0] = BIT_MASK(EV_KEY);
	onkey_dev->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);

	err = input_register_device(onkey_dev);
	if (err) {
		pr_err("Unable to register input device for power button\n");
		goto err_free_dev;
	}

	/* We need a dummy void* priv for shared IRQs */
	if (request_irq(NSPIRE_IRQ_PWR, onkey_interrupt, IRQF_SHARED,
			"onkey", (void *)0xdeadbeef)) {
		pr_err("Unable to allocate IRQ for power button\n");
		err = -EBUSY;
		goto err_free_dev;
	}

	INIT_DELAYED_WORK(&polling_work, onkey_poll);

	pr_info("TI-NSPIRE ON Key\n");
	return 0;

err_free_dev:
	input_free_device(onkey_dev);

	return err;

}

static void __exit onkey_exit(void)
{
	input_free_device(onkey_dev);
	free_irq(NSPIRE_IRQ_PWR, onkey_interrupt);
}

module_init(onkey_init);
module_exit(onkey_exit);
