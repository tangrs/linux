/*
 *  linux/drivers/input/keyboard/nspire-keypad.c
 *
 *  Copyright (C) 2013 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/input/matrix_keypad.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of.h>

#define KEYPAD_SCAN_MODE	0x00
#define KEYPAD_CNTL		0x04
#define KEYPAD_INT		0x08
#define KEYPAD_INTMSK		0x0C

#define KEYPAD_DATA		0x10
#define KEYPAD_GPIO		0x30

#define KEYPAD_UNKNOWN_INT	0x40
#define KEYPAD_UNKNOWN_INT_STS	0x44

#define KEYPAD_BITMASK_COLS	11
#define KEYPAD_BITMASK_ROWS	8

struct nspire_keypad {
	spinlock_t lock;

	void __iomem *reg_base;
	int irq;
	u32 int_mask;

	struct input_dev *input;
	struct clk *clk;

	struct matrix_keymap_data *keymap;
	int row_shift;

	/* Maximum delay estimated assuming 33MHz APB */
	u32 scan_interval;	/* In microseconds (~2000us max) */
	u32 row_delay;		/* In microseconds (~500us max) */

	bool active_low;
};

static inline void nspire_report_state(struct nspire_keypad *keypad,
		int row, int col, unsigned int state)
{
	int code = MATRIX_SCAN_CODE(row, col, keypad->row_shift);
	unsigned short *keymap = keypad->input->keycode;

	state = keypad->active_low ? !state : !!state;
	input_report_key(keypad->input, keymap[code], state);
}

static irqreturn_t nspire_keypad_irq(int irq, void *dev_id)
{
	struct nspire_keypad *keypad = dev_id;
	u32 int_sts;
	u16 state[8];
	int row, col;

	int_sts = readl(keypad->reg_base + KEYPAD_INT) & keypad->int_mask;

	if (!int_sts)
		return IRQ_NONE;

	spin_lock(&keypad->lock);

	memcpy_fromio(state, keypad->reg_base + KEYPAD_DATA, sizeof(state));

	for (row = 0; row < KEYPAD_BITMASK_ROWS; row++) {
		u16 bits = state[row];
		for (col = 0; col < KEYPAD_BITMASK_COLS; col++)
			nspire_report_state(keypad, row, col, bits & (1<<col));
	}
	input_sync(keypad->input);
	writel(0x3, keypad->reg_base + KEYPAD_INT);

	spin_unlock(&keypad->lock);

	return IRQ_HANDLED;
}

static int nspire_keypad_chip_init(struct nspire_keypad *keypad)
{
	unsigned long val = 0, cycles_per_us, delay_cycles, row_delay_cycles;

	cycles_per_us = (clk_get_rate(keypad->clk) / 1000000);
	if (cycles_per_us == 0)
		cycles_per_us = 1;

	delay_cycles = cycles_per_us * keypad->scan_interval;
	WARN_ON(delay_cycles >= (1<<16)); /* Overflow */
	delay_cycles &= 0xffff;

	row_delay_cycles = cycles_per_us * keypad->row_delay;
	WARN_ON(row_delay_cycles >= (1<<14)); /* Overflow */
	row_delay_cycles &= 0x3fff;


	val |= (3<<0); /* Set scan mode to 3 (continuous scan) */
	val |= (row_delay_cycles<<2); /* Delay between scanning each row */
	val |= (delay_cycles<<16); /* Delay between scans */
	writel(val, keypad->reg_base + KEYPAD_SCAN_MODE);

	val = (KEYPAD_BITMASK_ROWS & 0xff) | (KEYPAD_BITMASK_COLS & 0xff)<<8;
	writel(val, keypad->reg_base + KEYPAD_CNTL);

	/* Enable interrupts */
	keypad->int_mask = (1<<1);
	writel(keypad->int_mask, keypad->reg_base + 0xc);

	/* Disable GPIO interrupts to prevent hanging on touchpad */
	/* Possibly used to detect touchpad events */
	writel(0, keypad->reg_base + KEYPAD_UNKNOWN_INT);
	/* Acknowledge existing interrupts */
	writel(~0, keypad->reg_base + KEYPAD_UNKNOWN_INT_STS);

	return 0;
}

static int nspire_keypad_probe(struct platform_device *pdev)
{
	const struct device_node *of_node = pdev->dev.of_node;
	struct nspire_keypad *keypad;
	struct input_dev *input;
	struct resource *res;
	struct clk *clk;
	int irq, error;

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

	clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "unable to get clock\n");
		return -EINVAL;
	}

	clk_prepare(clk);
	error = clk_enable(clk);
	if (error)
		goto err_put_clk;

	keypad = kzalloc(sizeof(struct nspire_keypad), GFP_KERNEL);
	input = input_allocate_device();
	if (!keypad || !input) {
		dev_err(&pdev->dev, "failed to allocate keypad memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	error = of_property_read_u32(of_node, "scan-interval",
			&keypad->scan_interval);
	if (error) {
		dev_err(&pdev->dev, "failed to get scan-interval\n");
		goto err_free_mem;
	}

	error = of_property_read_u32(of_node, "row-delay",
			&keypad->row_delay);
	if (error) {
		dev_err(&pdev->dev, "failed to get row-delay\n");
		goto err_free_mem;
	}

	keypad->active_low = of_property_read_bool(of_node,
			"active-low");

	keypad->keymap = NULL;

	keypad->row_shift = get_count_order(KEYPAD_BITMASK_COLS);
	keypad->irq = irq;
	keypad->clk = clk;
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

	set_bit(EV_KEY, input->evbit);
	set_bit(EV_REP, input->evbit);

	error = matrix_keypad_build_keymap(keypad->keymap, "keymap",
			KEYPAD_BITMASK_ROWS, KEYPAD_BITMASK_COLS, NULL, input);
	if (error) {
		dev_err(&pdev->dev, "building keymap failed\n");
		goto err_iounmap;
	}

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

	dev_info(&pdev->dev, "TI-NSPIRE keypad at %#08x ("
			"scan_interval=%uus,row_delay=%uus"
			"%s)\n", res->start,
			keypad->row_delay,
			keypad->scan_interval,
			keypad->active_low ? ",active_low" : "");

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

	clk_disable(clk);
err_put_clk:
	clk_unprepare(clk);
	clk_put(clk);
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

	clk_disable(keypad->clk);
	clk_unprepare(keypad->clk);
	clk_put(keypad->clk);

	return 0;
}
#ifdef CONFIG_OF
static const struct of_device_id nspire_keypad_dt_match[] = {
	{ .compatible = "ti,nspire-keypad" },
	{ },
};
MODULE_DEVICE_TABLE(of, nspire_keypad_dt_match);
#endif

static struct platform_driver nspire_keypad_driver = {
	.driver = {
		.name = "nspire-keypad",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(nspire_keypad_dt_match),
	},
	.remove = nspire_keypad_remove,
	.probe = nspire_keypad_probe,
};

module_platform_driver(nspire_keypad_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TI-NSPIRE Keypad Driver");
