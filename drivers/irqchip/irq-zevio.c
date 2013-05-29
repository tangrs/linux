/*
 *  linux/drivers/irqchip/irq-zevio.c
 *
 *  Copyright (C) 2013 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#include <asm/mach/irq.h>
#include <asm/exception.h>

#include "irqchip.h"

#define IO_STATUS	0x000
#define IO_RAW_STATUS	0x004
#define IO_ENABLE	0x008
#define IO_DISABLE	0x00C
#define IO_CURRENT	0x020
#define IO_RESET	0x028
#define IO_MAX_PRIOTY	0x02C

#define IO_IRQ_BASE	0x000
#define IO_FIQ_BASE	0x100

#define IO_INVERT_SEL	0x200
#define IO_STICKY_SEL	0x204
#define IO_PRIORITY_SEL	0x300

#define MAX_INTRS	32
#define FIQ_START	MAX_INTRS


static void __iomem *irq_io_base;
static struct irq_domain *zevio_irq_domain;

static void zevio_irq_ack(struct irq_data *irqd)
{
	void __iomem *base = irq_io_base;

	if (irqd->hwirq < FIQ_START)
		base += IO_IRQ_BASE;
	else
		base += IO_FIQ_BASE;

	readl(base + IO_RESET);
}

static void zevio_irq_unmask(struct irq_data *irqd)
{
	void __iomem *base = irq_io_base;
	int irqnr = irqd->hwirq;

	if (irqnr < FIQ_START) {
		base += IO_IRQ_BASE;
	} else {
		irqnr -= MAX_INTRS;
		base += IO_FIQ_BASE;
	}

	writel((1<<irqnr), base + IO_ENABLE);
}

static void zevio_irq_mask(struct irq_data *irqd)
{
	void __iomem *base = irq_io_base;
	int irqnr = irqd->hwirq;

	if (irqnr < FIQ_START) {
		base += IO_IRQ_BASE;
	} else {
		irqnr -= FIQ_START;
		base += IO_FIQ_BASE;
	}

	writel((1<<irqnr), base + IO_DISABLE);
}

static struct irq_chip zevio_irq_chip = {
	.name		= "zevio_irq",
	.irq_ack	= zevio_irq_ack,
	.irq_mask	= zevio_irq_mask,
	.irq_unmask	= zevio_irq_unmask,
};


static int zevio_irq_map(struct irq_domain *dom, unsigned int virq,
		irq_hw_number_t hw)
{
	irq_set_chip_and_handler(virq, &zevio_irq_chip, handle_level_irq);
	set_irq_flags(virq, IRQF_VALID | IRQF_PROBE);

	return 0;
}

static struct irq_domain_ops zevio_irq_ops = {
	.map = zevio_irq_map,
	.xlate = irq_domain_xlate_onecell,
};

static void init_base(void __iomem *base)
{
	/* Disable all interrupts */
	writel(~0, base + IO_DISABLE);

	/* Accept interrupts of all priorities */
	writel(0xF, base + IO_MAX_PRIOTY);

	/* Reset existing interrupts */
	readl(base + IO_RESET);
}

static int process_base(void __iomem *base, struct pt_regs *regs)
{
	int irqnr;


	if (!readl(base + IO_STATUS))
		return 0;

	irqnr = readl(base + IO_CURRENT);
	irqnr = irq_find_mapping(zevio_irq_domain, irqnr);
	handle_IRQ(irqnr, regs);

	return 1;
}

asmlinkage void __exception_irq_entry zevio_handle_irq(struct pt_regs *regs)
{
	while (process_base(irq_io_base + IO_FIQ_BASE, regs))
		;
	while (process_base(irq_io_base + IO_IRQ_BASE, regs))
		;
}

static int __init zevio_of_init(struct device_node *node,
				struct device_node *parent)
{
	if (WARN_ON(irq_io_base))
		return -EBUSY;

	irq_io_base = of_iomap(node, 0);
	BUG_ON(!irq_io_base);

	/* Do not invert interrupt status bits */
	writel(~0, irq_io_base + IO_INVERT_SEL);

	/* Disable sticky interrupts */
	writel(0, irq_io_base + IO_STICKY_SEL);

	/* We don't use IRQ priorities. Set each IRQ to highest priority. */
	memset_io(irq_io_base + IO_PRIORITY_SEL, 0, MAX_INTRS * sizeof(u32));

	/* Init IRQ and FIQ */
	init_base(irq_io_base + IO_IRQ_BASE);
	init_base(irq_io_base + IO_FIQ_BASE);

	zevio_irq_domain = irq_domain_add_linear(node, MAX_INTRS,
						 &zevio_irq_ops, NULL);

	BUG_ON(!zevio_irq_domain);

	set_handle_irq(zevio_handle_irq);

	pr_info("TI-NSPIRE classic IRQ controller\n");
	return 0;
}

IRQCHIP_DECLARE(zevio_irq, "lsi,zevio-intc", zevio_of_init);
