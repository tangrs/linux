/*
 * linux/arch/arm/mach-nspire/gpio.c
 *
 * Author: Fabian Vogt <fabian@ritter-vogt.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/platform_data/gpio-nspire.h>

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
#define NSPIRE_APB_VIRTIO(x) IOMEM(0x90000000 + x)
#define NSPIRE_APB_GPIO 0

#define debug_gpio_regs(pin)

struct nspire_gpio_controller {
	unsigned		irq;
	unsigned		irq_base;
	void __iomem 		*base;
	spinlock_t 		lock;
	struct gpio_chip	chip;
	struct irq_chip_generic	*ic;
};

/* Only one present */
static struct nspire_gpio_controller *controller;

#ifndef debug_gpio_regs
static void debug_gpio_regs(unsigned pin)
{
	uint32_t gpio_val, gpio_val2, gpio_val3;
	uint16_t tmp;
	
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x18));
	gpio_val = tmp&0xFF;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x58));
	gpio_val |= (tmp&0xFF)<<8;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x98));
	gpio_val |= (tmp&0xFF)<<16;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0xD8));
	gpio_val |= (tmp&0xFF)<<24;
	
	gpio_val2 = 0;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x14));
	gpio_val2 = tmp&0xFF;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x54));
	gpio_val2 |= (tmp&0xFF)<<8;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x94));
	gpio_val2 |= (tmp&0xFF)<<16;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0xD4));
	gpio_val2 |= (tmp&0xFF)<<24;
	
	gpio_val3 = 0;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x10));
	gpio_val3 = tmp&0xFF;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x50));
	gpio_val3 |= (tmp&0xFF)<<8;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0x90));
	gpio_val3 |= (tmp&0xFF)<<16;
	tmp = readw(NSPIRE_APB_VIRTIO(NSPIRE_APB_GPIO + 0xD0));
	gpio_val3 |= (tmp&0xFF)<<24;
	
	printk(KERN_INFO "GPIO registers: direction: 0x%x input: 0x%lx output: 0x%lx\n", gpio_val3, gpio_val, gpio_val2);
	printk(KERN_INFO "Pin %d, bit %d, section %d -> offset(input) 0x%x\n", pin, NSPIRE_GPIO_BIT(pin), (pin>>3)&3, NSPIRE_GPIO(pin, INPUT));
}
#endif

/* Functions for struct gpio_chip */
static int nspire_gpio_get(struct gpio_chip *chip, unsigned pin)
{
	/* Only reading allowed, so no spinlock needed */
	uint16_t val = readw(NSPIRE_GPIO(pin, INPUT));

	debug_gpio_regs(pin);
		
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

	debug_gpio_regs(pin);
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

	debug_gpio_regs(pin);
	
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

	debug_gpio_regs(pin);
	
	return 0;
}

static int nspire_gpio_to_irq(struct gpio_chip *chip, unsigned pin)
{
	if(!controller->irq_base || pin < chip->ngpio)
		return -EINVAL;
	
	return pin + controller->irq_base;
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

/* Functions for IRQs */
static void nspire_gpio_irq(unsigned irq, struct irq_desc *desc)
{
	uint8_t pin, port;
	uint16_t status_val;
	uint16_t __iomem *status = NSPIRE_GPIO(0, INT_MASKED_STATUS);
	for (port = 0; port < controller->chip.ngpio; port += 8) {
		status_val = readw(status);
		/* Ack interrupt */
		writew(0xFF, status + NSPIRE_GPIO_INT_STATUS_OFFSET);
		status += NSPIRE_GPIO_SECTION_SIZE;
		if(likely(!status_val))
			continue;

		for(pin = 0; pin < 8; pin++) {
			if(status_val & 1 << pin)
				generic_handle_irq(controller->irq_base
						   + (port | pin));
		}
	}
}

static void nspire_gpio_enable_irq(struct irq_data *data)
{
	unsigned irq = data->irq;
	irq -= controller->irq_base;
	writew(1<<NSPIRE_GPIO_BIT(irq), NSPIRE_GPIO(irq, INT_ENABLE));
}

static void nspire_gpio_disable_irq(struct irq_data *data)
{
	unsigned irq = data->irq;
	irq -= controller->irq_base;
	writew(1<<NSPIRE_GPIO_BIT(irq), NSPIRE_GPIO(irq, INT_DISABLE));
}

static void nspire_gpio_ack_irq(struct irq_data *data)
{
	unsigned irq = data->irq;
	irq -= controller->irq_base;
	writew(1<<NSPIRE_GPIO_BIT(irq), NSPIRE_GPIO(irq, INT_STATUS));
}

static void nspire_gpio_mask_ack_irq(struct irq_data *data)
{
	unsigned irq = data->irq;
	irq -= controller->irq_base;
	writew(1<<NSPIRE_GPIO_BIT(irq), NSPIRE_GPIO(irq, INT_DISABLE));
	writew(1<<NSPIRE_GPIO_BIT(irq), NSPIRE_GPIO(irq, INT_STATUS));
}

static struct irq_chip nspire_gpio_irqchip = {
	.name		= "GPIO",
	.irq_ack	= nspire_gpio_ack_irq,
	.irq_mask_ack	= nspire_gpio_mask_ack_irq,
	.irq_enable	= nspire_gpio_enable_irq,
	.irq_disable	= nspire_gpio_disable_irq,
};

/* struct platform_driver functions */
static int __devinit nspire_gpio_probe(struct platform_device *pdev)
{
	const struct nspire_gpio_data *pdata = pdev->dev.platform_data;
	struct resource *res;
	int irq, error;

	if (!pdata || !pdata->ngpio) {
		dev_err(&pdev->dev, "Invalid nspire gpio data struct\n");
		return -EINVAL;
	}

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
	controller->chip.ngpio = pdata->ngpio;
	controller->chip.label = dev_name(&pdev->dev);
	controller->chip.dev = &pdev->dev;
	controller->irq = irq;
	controller->irq_base = pdata->irq_base;
	spin_lock_init(&(controller->lock));

	error = gpiochip_add(&(controller->chip));
	if(error) {
		dev_err(&pdev->dev, "Failed to add gpio_chip\n");
		goto err_iounmap;
	}
	
	platform_set_drvdata(pdev, controller);

	printk(KERN_INFO "GPIO controller initialized!");

	if(!controller->irq || !pdata->irq_base)
		return 0;
	
	for (irq = 0; irq < controller->chip.ngpio; irq += 8) {
		/* Sticky interrupt status */
		writew(0xFF, NSPIRE_GPIO(irq, INT_STICKY));
		/* Reset interrupt status */
		writew(0xFF, NSPIRE_GPIO(irq, INT_STATUS));
		/* Disable interrupts */
		writew(0xFF, NSPIRE_GPIO(irq, INT_DISABLE));
	}

	irq_set_chained_handler(controller->irq, nspire_gpio_irq);

	for (irq = 0; irq < controller->chip.ngpio; irq++) {
		irq_set_chip_and_handler(irq + controller->irq_base,
					 &nspire_gpio_irqchip, handle_simple_irq);
		set_irq_flags(irq + controller->irq_base, IRQF_VALID);
	}

	//writew(1 << NSPIRE_GPIO_BIT(21), NSPIRE_GPIO(21, INT_ENABLE));

	printk(KERN_INFO "GPIO IRQs initialized for IRQ %d!", controller->irq);
	
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