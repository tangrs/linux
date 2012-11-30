/*
 *	linux/arch/arm/mach-nspire/classic.c
 *
 *	Copyright (C) 2012 Daniel Tang <tangrs@tangrs.id.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/cpumask.h>
#include <linux/serial_8250.h>
#include <linux/platform_device.h>

#include <asm/mach/time.h>
#include <asm/exception.h>

#include <mach/nspire_mmio.h>
#include <mach/irqs.h>

#include "common.h"

/* Interrupt handling */

static inline int check_interrupt(void __iomem *base, struct pt_regs *regs)
{
	if (readl(base + 0x0)) {
		int irqnr = readl(base + 0x24);
		unsigned prev_priority;
		handle_IRQ(irqnr, regs);

		/* Reset priorities */
		prev_priority = readl(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x28));
		writel(prev_priority, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x2c));
		return 1;
	}
	return 0;
}

asmlinkage void __exception_irq_entry
	nspire_classic_handle_irq(struct pt_regs *regs)
{
	int serviced;

	do {
		void __iomem *reg_base = IOMEM(NSPIRE_INTERRUPT_VIRT_BASE);
		serviced = 0;

		/* IRQ */
		serviced += check_interrupt(reg_base, regs);
		/* FIQ */
		serviced += check_interrupt(reg_base + 0x100, regs);
	} while (serviced > 0);
}

static void classic_irq_ack(struct irq_data *d)
{
	readl(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x28));
}

static void __init classic_allocate_gc(void)
{
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;

	gc = irq_alloc_generic_chip("NINT", 1, 0,
		IOMEM(NSPIRE_INTERRUPT_VIRT_BASE), handle_level_irq);

	ct = gc->chip_types;
	ct->chip.irq_ack = classic_irq_ack;
	ct->chip.irq_mask = irq_gc_mask_disable_reg;
	ct->chip.irq_unmask = irq_gc_unmask_enable_reg;

	ct->regs.mask = 0x8;
	ct->regs.enable = 0x8;
	ct->regs.disable = 0xc;

	irq_setup_generic_chip(gc, IRQ_MSK(NR_IRQS), IRQ_GC_INIT_MASK_CACHE,
		IRQ_NOREQUEST, 0);
}

void __init nspire_classic_init_irq(void)
{
	/* No stickies */
	writel(0, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x204));

	/* Disable all interrupts */
	writel(~0, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0xc));
	writel(~0, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x10c));

	/* Set all priorities to 0 */
	memset_io(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x300), 0, 0x7f);

	/* Accept interrupts of all priorities */
	writel(0xf, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x2c));
	writel(0xf, IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x12c));

	/* Clear existing interrupts */
	readl(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x28));
	readl(IOMEM(NSPIRE_INTERRUPT_VIRT_BASE + 0x128));

	/* Add chip */
	classic_allocate_gc();
}


/* Timer */

static int classic_timer_set_event(unsigned long delta,
				struct clock_event_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	writel(delta, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2));
	writel(1, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x8));
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x18));
	local_irq_restore(flags);

	return 0;
}
static void classic_timer_set_mode(enum clock_event_mode mode,
				 struct clock_event_device *evt)
{
	evt->mode = mode;
}

static struct clock_event_device nspire_clkevt = {
	.name		= "clockevent",
	.features	= CLOCK_EVT_FEAT_ONESHOT,
	.shift		= 32,
	.rating		= 400,
	.set_next_event = classic_timer_set_event,
	.set_mode	= classic_timer_set_mode,
	.cpumask		= cpu_all_mask,
};

static irqreturn_t classic_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *c = dev_id;

	/* Acknowledge */
	writel((1<<0), NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x20));

	if (c->mode != CLOCK_EVT_FEAT_PERIODIC)
		writel((1<<4) | 1, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x08));

	if (c->event_handler)
		c->event_handler(c);

	return IRQ_HANDLED;
}

static struct irqaction classic_timer_irq = {
	.name			= "timer2",
	.flags			= IRQF_DISABLED | IRQF_TIMER | IRQF_IRQPOLL,
	.handler		= classic_timer_interrupt,
	.dev_id			= &nspire_clkevt,
};


static void __init classic_timer_init(void)
{
	struct clk *timer_clk;

	/* Count down from 1 */
	writel(1, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2));

	/* Divider is zero */
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x4));

	/* Decreasing timer and interrupt */
	writel(1, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x8));

	/* Interrupt on timer value reaching 0 */
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x18));

	/* Acknowledge existing interrupts */
	writel(~0, NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x20));

	/* Set interrupt masks */
	writel((1<<0), NSPIRE_APB_VIRTIO(NSPIRE_APB_MISC + 0x24));

	setup_irq(NSPIRE_IRQ_TIMER2, &classic_timer_irq);

	timer_clk = clk_get(NULL, "timer2");
	clk_enable(timer_clk);

	/* Set clocksource to zero */
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0xc));

	/* Divider is zero */
	writel(0, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x10));

	/* Ever increasing timer */
	writel((1<<3) | 7, NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0x14));

	clocksource_mmio_init(NSPIRE_APB_VIRTIO(NSPIRE_APB_TIMER2 + 0xc),
		"clocksource", clk_get_rate(timer_clk), 200, 16,
		clocksource_mmio_readw_up);

	clockevents_config_and_register(&nspire_clkevt,
		clk_get_rate(timer_clk), 0x0001, 0xfffe);
}

struct sys_timer nspire_classic_sys_timer = {
	.init = classic_timer_init
};


/* Serial */
static struct plat_serial8250_port classic_serial_platform_data[] = {
	{
		.mapbase	= NSPIRE_APB_PHYS(NSPIRE_APB_UART),
		.irq		= NSPIRE_IRQ_UART,
		.uartclk	= 29491200,
		.iotype		= UPIO_MEM,
		.flags		= UPF_BOOT_AUTOCONF | UPF_SKIP_TEST |
				UPF_IOREMAP,
		.regshift	= 2
	},
	{ }
};

struct platform_device nspire_classic_serial_device = {
	.name		= "serial8250",
	.id			= PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = &classic_serial_platform_data
	}
};

/* Init */
void __init nspire_classic_init(void)
{
	platform_device_register(&nspire_keypad_device);
	platform_device_register(&nspire_classic_serial_device);

	nspire_init();
}
