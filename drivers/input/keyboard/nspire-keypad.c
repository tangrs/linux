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
	unsigned short keymap[8];
	const struct nspire_keypad_data *pdata;
	spinlock_t lock;
};

static irqreturn_t nspire_keypad_irq(int irq, void *dev_id)
{
	struct nspire_keypad *keypad = dev_id;
	short current_state[8];
	int i, j;

	memcpy_fromio(current_state, keypad->reg_base + 0x10, sizeof(current_state));
	for (i = 0; i < 8; i++) {
		short current_bits = current_state[i];
		short last_bits = keypad->keymap[i];
		for (j = 0; j < 11; j++) {
			if ((current_bits & (1<<j)) != (last_bits & (1<<j))) {
				if (keypad->pdata->evtcodes[i][j]) {
					/*printk(KERN_INFO "[%d %s]\n", keypad->pdata->evtcodes[i][j], current_bits & (1<<j) ? "u" : "d" );*/
					input_report_key(keypad->input, keypad->pdata->evtcodes[i][j], !!(current_bits & (1<<j)));
				}
			}
		}
	}
	memcpy(keypad->keymap, current_state, sizeof(keypad->keymap));
	input_sync(keypad->input);

	writel(0b111, keypad->reg_base + 0x8);
	return IRQ_HANDLED;
}

static int nspire_keypad_chip_init(struct nspire_keypad *keypad)
{
	unsigned long val;

	spin_lock(&keypad->lock);
	memcpy_fromio(keypad->keymap, keypad->reg_base + 0x10, sizeof(keypad->keymap));

	/* We can assume the bootloader (i.e. TI-NSPIRE software) has already initialized this */
	/* Needs to be fixed if we're no longer booting in-place from NSPIRE software */
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

	spin_unlock(&keypad->lock);

	return 0;
}

static int __init nspire_keypad_probe(struct platform_device *pdev)
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

	error = request_irq(keypad->irq, nspire_keypad_irq, 0, "nspire_keypad", keypad);
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

static int __devexit nspire_keypad_remove(struct platform_device *pdev)
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
	.remove = __devexit_p(nspire_keypad_remove),
};

static int __init nspire_keypad_init(void)
{
	pr_info("TI-NSPIRE keypad\n");
	return platform_driver_probe(&nspire_keypad_driver, nspire_keypad_probe);
}
module_init(nspire_keypad_init);

static void __exit nspire_keypad_exit(void)
{
	platform_driver_unregister(&nspire_keypad_driver);
}
module_exit(nspire_keypad_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI-NSPIRE Keypad Driver");
