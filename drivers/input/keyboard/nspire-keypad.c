/*
 * Copyright (C) 2012 Daniel Tang
 *
 * Author: Daniel Tang
 *
 * License terms: GNU General Public License (GPL) version 2
 *
 * Nspire keypad controller driver
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/module.h>

#include <mach/keypad.h>

struct nspire_keypad {
	int irq;
	void __iomem *reg_base;
	struct input_dev *input;
	const struct nspire_keypad_data *pdata;
	spinlock_t lock;
};

static inline void nspire_report_state(struct nspire_keypad *keypad,
		int row, int col, unsigned state)
{
	state = keypad->pdata->active_low ? !state : !!state;

	if (keypad->pdata->evtcodes[row][col]) {
		input_report_key(keypad->input,
				keypad->pdata->evtcodes[row][col],
				state);
	}

}

static irqreturn_t nspire_keypad_irq(int irq, void *dev_id)
{
	struct nspire_keypad *keypad = dev_id;
	u16 current_state[8];
	int i, j;

	spin_lock(&keypad->lock);

	memcpy_fromio(current_state, keypad->reg_base + 0x10,
		sizeof(current_state));

	for (i = 0; i < 8; i++) {
		u16 current_bits = current_state[i];
		for (j = 0; j < 11; j++) {
			nspire_report_state(keypad, i, j,
					(current_bits & (1<<j)));
		}
	}
	input_sync(keypad->input);
	writel(0b111, keypad->reg_base + 0x8);

	spin_unlock(&keypad->lock);

	return IRQ_HANDLED;
}

static int nspire_keypad_chip_init(struct nspire_keypad *keypad)
{
	unsigned long val;

	/*
	 * We can assume the bootloader (i.e. TI-NSPIRE software) has
	 * already initialized this. Needs to be fixed if we're no longer
	 * booting in-place from TI-NSPIRE software.
	 */
	val = readl(keypad->reg_base);
	val &= ~(0b11);
	val |= 3; /* Set scan mode to 3 */
	writel(val, keypad->reg_base);

	/* Enable interrupts */
	writel((1<<1), keypad->reg_base + 0xc);

	/* Disable GPIO interrupts to prevent hanging on touchpad */
	/* Possibly used to detect touchpad events */
	writel(0, keypad->reg_base + 0x40);
	/* Acknowledge existing interrupts */
	writel(~0, keypad->reg_base + 0x44);

	return 0;
}

static int nspire_keypad_probe(struct platform_device *pdev)
{
	const struct nspire_keypad_data *plat = pdev->dev.platform_data;
	struct nspire_keypad *keypad;
	struct input_dev *input;
	struct resource *res;
	int irq;
	int i;
	int error;

	if (!plat) {
		dev_err(&pdev->dev, "invalid keypad platform data\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get keypad irq\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing platform resources\n");
		return -EINVAL;
	}

	keypad = kzalloc(sizeof(struct nspire_keypad), GFP_KERNEL);
	input = input_allocate_device();
	if (!keypad || !input) {
		dev_err(&pdev->dev, "failed to allocate keypad memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	keypad->irq = irq;
	keypad->pdata = plat;
	keypad->input = input;
	spin_lock_init(&keypad->lock);

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "failed to request I/O memory\n");
		error = -EBUSY;
		goto err_free_mem;
	}

	keypad->reg_base = ioremap(res->start, resource_size(res));
	if (!keypad->reg_base) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		error = -ENXIO;
		goto err_free_mem_region;
	}

	input->id.bustype = BUS_HOST;
	input->name = "nspire-keypad";
	input->dev.parent = &pdev->dev;
	input->evbit[0] = BIT_MASK(EV_KEY);
	for (i = 1; i < 128; i++)
		set_bit(i, input->keybit);
	clear_bit(0, input->keybit);

	error = nspire_keypad_chip_init(keypad);
	if (error) {
		dev_err(&pdev->dev, "unable to init keypad hardware\n");
		goto err_iounmap;
	}

	error = request_irq(keypad->irq, nspire_keypad_irq, 0,
			"nspire_keypad", keypad);
	if (error) {
		dev_err(&pdev->dev, "allocate irq %d failed\n", keypad->irq);
		goto err_iounmap;
	}

	error = input_register_device(input);
	if (error) {
		dev_err(&pdev->dev,
				"unable to register input device: %d\n", error);
		goto err_free_irq;
	}

	platform_set_drvdata(pdev, keypad);

	return 0;

err_free_irq:
	free_irq(keypad->irq, keypad);
err_iounmap:
	iounmap(keypad->reg_base);
err_free_mem_region:
	release_mem_region(res->start, resource_size(res));
err_free_mem:
	input_free_device(input);
	kfree(keypad);
	return error;
}

static int nspire_keypad_remove(struct platform_device *pdev)
{
	struct nspire_keypad *keypad = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	free_irq(keypad->irq, keypad);

	input_unregister_device(keypad->input);

	iounmap(keypad->reg_base);
	release_mem_region(res->start, resource_size(res));
	kfree(keypad);

	return 0;
}

static struct platform_driver nspire_keypad_driver = {
	.driver = {
		.name = "nspire-keypad",
		.owner  = THIS_MODULE,
	},
	.remove = nspire_keypad_remove,
	.probe = nspire_keypad_probe
};

module_platform_driver(nspire_keypad_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI-NSPIRE Keypad Driver");
