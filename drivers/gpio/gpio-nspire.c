/*
 * linux/arch/arm/mach-nspire/gpio.c
 *
 * Author: Fabian Vogt <fabian@ritter-vogt.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#define NSPIRE_GPIO_SECTION_SIZE 0x40

#define NSPIRE_GPIO_INT_MASKED_STATUS_OFFSET	0x00
#define NSPIRE_GPIO_INT_STATUS_OFFSET 		0x04
#define NSPIRE_GPIO_INT_ENABLE_OFFSET 		0x08
#define NSPIRE_GPIO_INT_DISABLE_OFFSET 		0x0C
#define NSPIRE_GPIO_DIRECTION_OFFSET 		0x10
#define NSPIRE_GPIO_OUTPUT_OFFSET 		0x14
#define NSPIRE_GPIO_INPUT_OFFSET 		0x18
#define NSPIRE_GPIO_INT_STICKY_OFFSET		0x20

#define NSPIRE_GPIO_BIT(x) (x&7)
#define NSPIRE_GPIO_SECTION_OFFSET(x) (((x>>3)&3)*NSPIRE_GPIO_SECTION_SIZE)
#define NSPIRE_GPIO(x, t) IOMEM(controller->base + \
		NSPIRE_GPIO_SECTION_OFFSET(x) + NSPIRE_GPIO_##t##_OFFSET)

struct nspire_gpio_controller {
	unsigned		irq;
	void __iomem 		*base;
	spinlock_t 		lock;
	struct gpio_chip	chip;
	struct irq_chip_generic	*ic;
};

/* Only one present */
static struct nspire_gpio_controller *controller;

/* Functions for struct gpio_chip */
static int nspire_gpio_get(struct gpio_chip *chip, unsigned pin)
{
	/* Only reading allowed, so no spinlock needed */
	uint16_t val = readw(NSPIRE_GPIO(pin, INPUT));
		
	return (val>>NSPIRE_GPIO_BIT(pin)) & 0x1;
}

static void nspire_gpio_set(struct gpio_chip *chip, unsigned pin, int value)
{
	uint16_t val;
	spin_lock(&controller->lock);
	val = readw(NSPIRE_GPIO(pin, OUTPUT));
	if(value)
		val |= 1<<NSPIRE_GPIO_BIT(pin);
	else
		val &= ~(1<<NSPIRE_GPIO_BIT(pin));
	
	writew(val, NSPIRE_GPIO(pin, OUTPUT));
	spin_unlock(&controller->lock);
}

static int nspire_gpio_direction_input(struct gpio_chip *chip, unsigned pin)
{
	uint16_t val;
	if (pin >= chip->ngpio)
		return -EINVAL;

	spin_lock(&controller->lock);
	val = readw(NSPIRE_GPIO(pin, DIRECTION));
	val |= 1<<NSPIRE_GPIO_BIT(pin);
	writew(val, NSPIRE_GPIO(pin, DIRECTION));
	
	spin_unlock(&controller->lock);
	
	return 0;
}

static int nspire_gpio_direction_output(struct gpio_chip *chip,
				   unsigned pin, int value)
{
	uint16_t val;
	if (pin >= chip->ngpio)
		return -EINVAL;

	spin_lock(&controller->lock);
	val = readw(NSPIRE_GPIO(pin, OUTPUT));
	if(value)
		val |= 1<<NSPIRE_GPIO_BIT(pin);
	else
		val &= ~(1<<NSPIRE_GPIO_BIT(pin));
	
	writew(val, NSPIRE_GPIO(pin, OUTPUT));
	val = readw(NSPIRE_GPIO(pin, DIRECTION));
	val &= ~(1<<NSPIRE_GPIO_BIT(pin));
	writew(val, NSPIRE_GPIO(pin, DIRECTION));

	/* Set again, could be an PL061 */
	val = readw(NSPIRE_GPIO(pin, OUTPUT));
	if(value)
		val |= 1<<NSPIRE_GPIO_BIT(pin);
	else
		val &= ~(1<<NSPIRE_GPIO_BIT(pin));
	spin_unlock(&controller->lock);
	
	return 0;
}

static int nspire_gpio_to_irq(struct gpio_chip *chip, unsigned pin)
{
	return -ENXIO;
}

static struct gpio_chip nspire_gpio_chip = {
	.direction_input        = nspire_gpio_direction_input,
	.direction_output       = nspire_gpio_direction_output,
	.set                    = nspire_gpio_set,
	.get                    = nspire_gpio_get,
	.to_irq                 = nspire_gpio_to_irq,
	.base                   = 0,
	.owner			= THIS_MODULE,
};

/* struct platform_driver functions */
static int __devinit nspire_gpio_probe(struct platform_device *pdev)
{
	struct resource *res;
	int irq, error;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get GPIO IRQ\n");
		return -EINVAL;
	}
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No memory resource given\n");
		return -EINVAL;
	}

	controller = kzalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller) {
		dev_err(&pdev->dev, "Not enough memory available\n");
		return -ENOMEM;
	}
	
	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "I/O memory area busy\n");
		error = -EBUSY;
		goto err_free_mem;
	}
	
	controller->base = ioremap(res->start, resource_size(res));
	if (!controller->base) {
		dev_err(&pdev->dev, "Failed to ioremap memory area\n");
		error = -ENXIO;
		goto err_free_mem_region;
	}

	/* Copy one struct to another */
	controller->chip = nspire_gpio_chip;
	/* Hardcoded */
	controller->chip.ngpio = 32;
	controller->chip.label = dev_name(&pdev->dev);
	controller->chip.dev = &pdev->dev;
	controller->irq = irq;
	spin_lock_init(&(controller->lock));

	for (irq = 0; irq < controller->chip.ngpio; irq += 8) {
		/* Sticky interrupt status */
		writew(0xFF, NSPIRE_GPIO(irq, INT_STICKY));
		/* Reset interrupt status */
		writew(0xFF, NSPIRE_GPIO(irq, INT_STATUS));
		/* Disable interrupts */
		writew(0xFF, NSPIRE_GPIO(irq, INT_DISABLE));
	}

	irq = gpiochip_add(&(controller->chip));
	if(irq) {
		dev_err(&pdev->dev, "Failed to add gpio_chip\n");
		goto err_iounmap;
	}
	
	platform_set_drvdata(pdev, controller);

	printk(KERN_INFO "GPIO controller initialized!");

	return 0;

	err_iounmap:
		iounmap(controller->base);
	err_free_mem_region:
		release_mem_region(res->start, resource_size(res));
	err_free_mem:
		kfree(controller);
		
	return error;
}

static struct platform_driver nspire_gpio_driver = {
	.driver = {
		.name	= "gpio-nspire",
		.owner	= THIS_MODULE,
	},
	.probe		= nspire_gpio_probe,
};

/* General module functions */
static int __init nspire_gpio_init(void)
{
	return platform_driver_probe(&nspire_gpio_driver, nspire_gpio_probe);
}
module_init(nspire_gpio_init);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fabian Vogt <fabian@ritter-vogt.de>");
MODULE_DESCRIPTION("TI-nSpire GPIO Controller");